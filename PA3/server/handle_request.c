#include <helper.h>
#include <pa3_error.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "helper.h"

static void set_response(Response* response, int32_t code) {
  response->code = code;
  response->data_size = 0;
  response->data = nullptr;
}

static bool has_data(const Request* request) {
  return request->data != nullptr && request->data_size > 0;
}

static bool has_username(const Request* request) {
  return request->username != nullptr && request->username_length > 0;
}

static bool parse_seat_number(const Request* request, pa3_seat_t* seat_number) {
  if (!has_data(request)) {
    return false;
  }

  errno = 0;
  char* endptr = nullptr;
  unsigned long long value = strtoull(request->data, &endptr, 10);
  if (errno != 0 || endptr == request->data || *endptr != '\0' ||
      value < 1 || value > NUM_SEATS) {
    return false;
  }

  *seat_number = (pa3_seat_t)value;
  return true;
}

static bool request_matches_active_client(const Request* request,
                                          const char** active_user) {
  return active_user != nullptr && *active_user != nullptr &&
         has_username(request) && strcmp(*active_user, request->username) == 0;
}

static void save_active_user(const char** active_user, const char* username) {
  if (active_user == nullptr) {
    return;
  }

  free((void*)*active_user);
  *active_user = strdup(username);
  if (*active_user == nullptr) {
    perror("strdup");
    exit(EXIT_FAILURE);
  }
}

static pa3_uid_t lock_logged_in_user(const Request* request,
                                     Users* users,
                                     const char** active_user) {
  if (!request_matches_active_client(request, active_user)) {
    return -1;
  }

  pthread_mutex_lock(&users->array_mutex);
  pa3_uid_t uid = find_user(users, request->username);
  if (uid < 0) {
    pthread_mutex_unlock(&users->array_mutex);
    return -1;
  }

  pthread_mutex_lock(&users->user_mutex[uid]);
  pthread_mutex_unlock(&users->array_mutex);

  if (!users->array[uid].logged_in) {
    pthread_mutex_unlock(&users->user_mutex[uid]);
    return -1;
  }

  return uid;
}

LoginErrorCode handle_login_request(const Request* request,
                                    Response* response,
                                    Users* users,
                                    const char** active_user) {
  if (active_user != nullptr && *active_user != nullptr) {
    set_response(response, LOGIN_ERROR_ACTIVE_CLIENT);
    return LOGIN_ERROR_ACTIVE_CLIENT;
  }

  if (!has_username(request)) {
    set_response(response, LOGIN_ERROR_NO_USERNAME_AND_PASSWORD);
    return LOGIN_ERROR_NO_USERNAME_AND_PASSWORD;
  }

  if (!has_data(request)) {
    set_response(response, LOGIN_ERROR_NO_PASSWORD);
    return LOGIN_ERROR_NO_PASSWORD;
  }

  pthread_mutex_lock(&users->array_mutex);
  pa3_uid_t uid = find_user(users, request->username);

  if (uid >= 0) {
    pthread_mutex_lock(&users->user_mutex[uid]);
    pthread_mutex_unlock(&users->array_mutex);

    if (users->array[uid].logged_in) {
      pthread_mutex_unlock(&users->user_mutex[uid]);
      set_response(response, LOGIN_ERROR_ACTIVE_USER);
      return LOGIN_ERROR_ACTIVE_USER;
    }

    if (!validate_password(request->data, users->array[uid].hashed_password)) {
      pthread_mutex_unlock(&users->user_mutex[uid]);
      set_response(response, LOGIN_ERROR_INCORRECT_PASSWORD);
      return LOGIN_ERROR_INCORRECT_PASSWORD;
    }

    users->array[uid].logged_in = true;
    pthread_mutex_unlock(&users->user_mutex[uid]);
    save_active_user(active_user, request->username);
    set_response(response, LOGIN_ERROR_SUCCESS);
    return LOGIN_ERROR_SUCCESS;
  }

  char hashed_password[HASHED_PASSWORD_SIZE];
  hash_password(request->data, hashed_password);
  uid = (pa3_uid_t)add_user(users, request->username, hashed_password);
  pthread_mutex_lock(&users->user_mutex[uid]);
  users->array[uid].logged_in = true;
  pthread_mutex_unlock(&users->user_mutex[uid]);
  pthread_mutex_unlock(&users->array_mutex);

  save_active_user(active_user, request->username);
  set_response(response, LOGIN_ERROR_SUCCESS);
  return LOGIN_ERROR_SUCCESS;
}


BookErrorCode handle_book_request(const Request* request,
                                  Response* response,
                                  Users* users,
                                  Seat* seats,
                                  const char** active_user) {
  if (!has_data(request)) {
    set_response(response, BOOK_ERROR_NO_DATA);
    return BOOK_ERROR_NO_DATA;
  }

  pa3_seat_t seat_number;
  if (!parse_seat_number(request, &seat_number)) {
    set_response(response, BOOK_ERROR_SEAT_OUT_OF_RANGE);
    return BOOK_ERROR_SEAT_OUT_OF_RANGE;
  }

  pa3_uid_t uid = lock_logged_in_user(request, users, active_user);
  if (uid < 0) {
    set_response(response, BOOK_ERROR_USER_NOT_LOGGED_IN);
    return BOOK_ERROR_USER_NOT_LOGGED_IN;
  }

  Seat* seat = &seats[seat_number - 1];
  pthread_mutex_lock(&seat->mutex);
  if (seat->user_who_booked != nullptr) {
    pthread_mutex_unlock(&seat->mutex);
    pthread_mutex_unlock(&users->user_mutex[uid]);
    set_response(response, BOOK_ERROR_SEAT_UNAVAILABLE);
    return BOOK_ERROR_SEAT_UNAVAILABLE;
  }

  seat->user_who_booked = strdup(request->username);
  if (seat->user_who_booked == nullptr) {
    perror("strdup");
    exit(EXIT_FAILURE);
  }
  seat->stats.amount_of_times_booked++;
  pthread_mutex_unlock(&seat->mutex);
  pthread_mutex_unlock(&users->user_mutex[uid]);

  set_response(response, BOOK_ERROR_SUCCESS);
  return BOOK_ERROR_SUCCESS;
}

ConfirmBookingErrorCode handle_confirm_booking_request(const Request* request,
                                                       Response* response,
                                                       Users* users,
                                                       Seat* seats,
                                                       const char** active_user) {
  if (!has_data(request)) {
    set_response(response, CONFIRM_BOOKING_ERROR_NO_DATA);
    return CONFIRM_BOOKING_ERROR_NO_DATA;
  }

  bool wants_available = strcmp(request->data, "available") == 0;
  bool wants_booked = strcmp(request->data, "booked") == 0;
  if (!wants_available && !wants_booked) {
    set_response(response, CONFIRM_BOOKING_ERROR_INVALID_DATA);
    return CONFIRM_BOOKING_ERROR_INVALID_DATA;
  }

  pa3_uid_t uid = lock_logged_in_user(request, users, active_user);
  if (uid < 0) {
    set_response(response, CONFIRM_BOOKING_ERROR_USER_NOT_LOGGED_IN);
    return CONFIRM_BOOKING_ERROR_USER_NOT_LOGGED_IN;
  }

  pa3_seat_t selected[NUM_SEATS];
  size_t count = 0;
  for (size_t i = 0; i < NUM_SEATS; i++) {
    pthread_mutex_lock(&seats[i].mutex);
    bool include = wants_available
                       ? seats[i].user_who_booked == nullptr
                       : seats[i].user_who_booked != nullptr &&
                             strcmp(seats[i].user_who_booked,
                                    request->username) == 0;
    if (include) {
      selected[count++] = seats[i].stats.id;
    }
    pthread_mutex_unlock(&seats[i].mutex);
  }
  pthread_mutex_unlock(&users->user_mutex[uid]);

  set_response(response, CONFIRM_BOOKING_ERROR_SUCCESS);
  if (count > 0) {
    response->data_size = count * sizeof(pa3_seat_t);
    response->data = malloc(response->data_size);
    if (response->data == nullptr) {
      perror("malloc");
      exit(EXIT_FAILURE);
    }
    memcpy(response->data, selected, response->data_size);
  }
  return CONFIRM_BOOKING_ERROR_SUCCESS;
}

int32_t handle_cancel_booking_request(const Request* request,
                                      Response* response,
                                      Users* users,
                                      Seat* seats,
                                      const char** active_user) {
  if (!has_data(request)) {
    set_response(response, CANCEL_BOOKING_ERROR_NO_DATA);
    return CANCEL_BOOKING_ERROR_NO_DATA;
  }

  pa3_seat_t seat_number;
  if (!parse_seat_number(request, &seat_number)) {
    set_response(response, CANCEL_BOOKING_ERROR_SEAT_OUT_OF_RANGE);
    return CANCEL_BOOKING_ERROR_SEAT_OUT_OF_RANGE;
  }

  pa3_uid_t uid = lock_logged_in_user(request, users, active_user);
  if (uid < 0) {
    set_response(response, CANCEL_BOOKING_ERROR_USER_NOT_LOGGED_IN);
    return CANCEL_BOOKING_ERROR_USER_NOT_LOGGED_IN;
  }

  Seat* seat = &seats[seat_number - 1];
  pthread_mutex_lock(&seat->mutex);
  if (seat->user_who_booked == nullptr ||
      strcmp(seat->user_who_booked, request->username) != 0) {
    pthread_mutex_unlock(&seat->mutex);
    pthread_mutex_unlock(&users->user_mutex[uid]);
    set_response(response, CANCEL_BOOKING_ERROR_SEAT_NOT_BOOKED_BY_USER);
    return CANCEL_BOOKING_ERROR_SEAT_NOT_BOOKED_BY_USER;
  }

  free((void*)seat->user_who_booked);
  seat->user_who_booked = nullptr;
  seat->stats.amount_of_times_canceled++;
  pthread_mutex_unlock(&seat->mutex);
  pthread_mutex_unlock(&users->user_mutex[uid]);

  set_response(response, CANCEL_BOOKING_ERROR_SUCCESS);
  return CANCEL_BOOKING_ERROR_SUCCESS;
}

int32_t handle_logout_request(const Request* request,
                              Response* response,
                              Users* users,
                              const char** active_user) {
  if (!has_username(request)) {
    set_response(response, LOGOUT_ERROR_USER_NOT_FOUND);
    return LOGOUT_ERROR_USER_NOT_FOUND;
  }

  pthread_mutex_lock(&users->array_mutex);
  pa3_uid_t uid = find_user(users, request->username);
  if (uid < 0) {
    pthread_mutex_unlock(&users->array_mutex);
    set_response(response, LOGOUT_ERROR_USER_NOT_FOUND);
    return LOGOUT_ERROR_USER_NOT_FOUND;
  }

  pthread_mutex_lock(&users->user_mutex[uid]);
  pthread_mutex_unlock(&users->array_mutex);

  if (!users->array[uid].logged_in ||
      !request_matches_active_client(request, active_user)) {
    pthread_mutex_unlock(&users->user_mutex[uid]);
    set_response(response, LOGOUT_ERROR_USER_NOT_LOGGED_IN);
    return LOGOUT_ERROR_USER_NOT_LOGGED_IN;
  }

  users->array[uid].logged_in = false;
  pthread_mutex_unlock(&users->user_mutex[uid]);
  free((void*)*active_user);
  *active_user = nullptr;

  set_response(response, LOGOUT_ERROR_SUCCESS);
  return LOGOUT_ERROR_SUCCESS;
}

int32_t handle_query_request(const Request* request,
                             Response* response,
                             Seat* seats) {
  if (!has_data(request)) {
    set_response(response, QUERY_ERROR_NO_DATA);
    return QUERY_ERROR_NO_DATA;
  }

  pa3_seat_t seat_number;
  if (!parse_seat_number(request, &seat_number)) {
    set_response(response, QUERY_ERROR_SEAT_OUT_OF_RANGE);
    return QUERY_ERROR_SEAT_OUT_OF_RANGE;
  }

  Seat* seat = &seats[seat_number - 1];
  pthread_mutex_lock(&seat->mutex);
  SeatStats stats = seat->stats;
  pthread_mutex_unlock(&seat->mutex);

  set_response(response, QUERY_ERROR_SUCCESS);
  response->data_size = sizeof(stats);
  response->data = malloc(response->data_size);
  if (response->data == nullptr) {
    perror("malloc");
    exit(EXIT_FAILURE);
  }
  memcpy(response->data, &stats, response->data_size);
  return QUERY_ERROR_SUCCESS;
}

int32_t handle_request(const Request* request,
                       Response* response,
                       Users* users,
                       Seat* seats,
                       const char** active_user) {
  switch (request->action) {
    case ACTION_LOGIN:
      return handle_login_request(request, response, users, active_user);
    case ACTION_BOOK:
      return handle_book_request(request, response, users, seats, active_user);
    case ACTION_CONFIRM_BOOKING:
      return handle_confirm_booking_request(request, response, users, seats,
                                            active_user);
    case ACTION_CANCEL_BOOKING:
      return handle_cancel_booking_request(request, response, users, seats,
                                           active_user);
    case ACTION_LOGOUT:
      return handle_logout_request(request, response, users, active_user);
    case ACTION_QUERY:
      return handle_query_request(request, response, seats);
    case ACTION_TERMINATION:
    default:
      set_response(response, -1);
      return -1;
  }
}
