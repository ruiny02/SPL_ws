// Note: This has been implemented already.
// (You can change it if you want).
// This basically prints the response in the format found in the PA3 spec.

// #include "user.h"
#include <ctype.h>
#include <helper.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

LoginErrorCode handle_login_response(const Request* request,
                                     const Response* response,
                                     const char** active_user) {
  switch (response->code) {
    case LOGIN_ERROR_SUCCESS:
      printf("User %s logged in successfully!\n", request->username);
      *active_user = strdup(request->username);
      break;
    case LOGIN_ERROR_ACTIVE_USER:
      fprintf(stderr, "User %s is already logged in!\n",
            request->username);
      break;
    case LOGIN_ERROR_ACTIVE_CLIENT:
      fprintf(stderr, "Client is already serving user %s!\n", *active_user);
      break;
    case LOGIN_ERROR_INCORRECT_PASSWORD:
      fprintf(stderr, "User %s provided incorrect password!\n", request->username);
      break;
    case LOGIN_ERROR_NO_PASSWORD:
      fprintf(stderr, "User %s did not provide a password!\n", request->username);
      break;
    case LOGIN_ERROR_NO_USERNAME_AND_PASSWORD:
      fprintf(stderr, "No username and password provided for login!\n");
      break;
    default:
      fprintf(stderr, "Unknown login error code: %d\n", response->code);
  }
  return response->code;
}

int32_t handle_book_response(const Request* request, const Response* response) {
  switch (response->code) {
    case BOOK_ERROR_SUCCESS:
      printf("Seat %s was booked successfully by user %s!\n", request->data,
             request->username);

      break;
    case BOOK_ERROR_USER_NOT_LOGGED_IN:
      fprintf(stderr, "User %s is not logged in!\n", request->username);
      break;
    case BOOK_ERROR_SEAT_UNAVAILABLE:
      fprintf(stderr, "Seat %s is unavailable for booking!\n", request->data);
      break;
    case BOOK_ERROR_SEAT_OUT_OF_RANGE:
      fprintf(stderr, "Seat %s is out of range [1,100]!\n", request->data);
      break;
    case BOOK_ERROR_NO_DATA:
      fprintf(stderr, "Please enter a seat number to book!\n");
      break;
    default:
      fprintf(stderr, "Unknown book error code: %d\n", response->code);
  }
  return response->code;
}
int32_t handle_confirm_booking_response(const Request* request,
                                        const Response* response) {
  switch (response->code) {
    case CONFIRM_BOOKING_ERROR_SUCCESS:
      if (response->data_size > 0) {
        if (strcmp(request->data, "available") == 0) {
          printf("Available seats: ");
        } else if (strcmp(request->data, "booked") == 0) {
          printf("Booked seats by user %s: ", request->username);
        }

        pa3_seat_t* seats = (pa3_seat_t*)response->data;

        for (size_t i = 0; i < response->data_size / sizeof(pa3_seat_t); i++) {
          if (i > 0) {
            printf(", ");
          }
          printf("%lu", seats[i]);
        }
        printf("\n");
      } else {
        printf("Booking confirmed, but no seats available!\n");
      }

      break;
    case CONFIRM_BOOKING_ERROR_USER_NOT_LOGGED_IN:
      fprintf(stderr, "User %s is not logged in!\n", request->username);
      break;
    case CONFIRM_BOOKING_ERROR_INVALID_DATA:
      fprintf(stderr, "Invalid data provided for booking confirmation!\n");
      break;
    case CONFIRM_BOOKING_ERROR_NO_DATA:
      fprintf(stderr, "Please enter 'available' or 'booked' for booking confirmation!\n");
      break;
    default:
      fprintf(stderr, "Unknown confirm booking error code: %d\n",
              response->code);
  }
  return response->code;
}

int32_t handle_cancel_booking_response(const Request* request, const Response* response) {
  switch (response->code) {
    case CANCEL_BOOKING_ERROR_SUCCESS:
      printf("Booking for seat %s was canceled successfully by user %s!\n",
             request->data, request->username);
      break;
    case CANCEL_BOOKING_ERROR_USER_NOT_LOGGED_IN:
      fprintf(stderr, "User %s is not logged in!\n", request->username);
      break;
    case CANCEL_BOOKING_ERROR_SEAT_NOT_BOOKED_BY_USER:
      fprintf(stderr, "Seat %s was not booked by user %s!\n", request->data,
              request->username);
      break;
    case CANCEL_BOOKING_ERROR_SEAT_OUT_OF_RANGE:
      fprintf(stderr, "Seat %s is out of range [1,100]!\n", request->data);
      break;
    case CANCEL_BOOKING_ERROR_NO_DATA:
      fprintf(stderr, "Please enter a seat number to cancel booking!\n");
      break;
    default:
      fprintf(stderr, "Unknown cancel booking error code: %d\n",
              response->code);
  }
  return response->code;
}

int32_t handle_logout_response(const Request* request,
                               const Response* response,
                               const char** active_user) {
  switch (response->code) {
    case LOGOUT_ERROR_SUCCESS:
      printf("User %s logged out successfully!\n", request->username);
      if (active_user != nullptr && *active_user != nullptr &&
          strcmp(*active_user, request->username) == 0) {
        free((void*)*active_user);
        *active_user = nullptr;
      }
      break;
    case LOGOUT_ERROR_USER_NOT_FOUND:
      fprintf(stderr, "User %s not found!\n", request->username);
      break;
    case LOGOUT_ERROR_USER_NOT_LOGGED_IN:
      fprintf(stderr, "User %s is not logged in!\n", request->username);
      break;
    default:
      fprintf(stderr, "Unknown logout error code: %d\n", response->code);
  }
  return response->code;
}

int32_t handle_query_response(const Request* request,
                              const Response* response) {
  switch (response->code) {
    case QUERY_ERROR_SUCCESS:
      if (response->data_size > 0) {
        SeatStats* seat = malloc(response->data_size);
        if (seat == nullptr) {
          perror("malloc");
          return -1;
        }
        memcpy(seat, response->data, response->data_size);
        printf("Seat %lu was booked %lu time%s and canceled %lu time%s!\n",
               seat->id, seat->amount_of_times_booked,
               (seat->amount_of_times_booked == 1) ? "" : "s",
               seat->amount_of_times_canceled,
               (seat->amount_of_times_canceled == 1) ? "" : "s");
        free(seat);
      }
      break;
    case QUERY_ERROR_SEAT_OUT_OF_RANGE:
      fprintf(stderr, "Seat %s is out of range [1,100]!\n", request->data);
      break;
    case QUERY_ERROR_NO_DATA:
      fprintf(stderr, "Please enter a seat number to query!\n");
      break;
    default:
      fprintf(stderr, "Unknown query error code: %d\n", response->code);
  }
  return response->code;
}

int32_t handle_response(Action action,
                        const Request* request,
                        const Response* response,
                        const char** active_user) {
  switch (action) {
    case ACTION_LOGIN:
      return handle_login_response(request, response, active_user);
    case ACTION_BOOK:
      return handle_book_response(request, response);
    case ACTION_CONFIRM_BOOKING:
      return handle_confirm_booking_response(request, response);
    case ACTION_CANCEL_BOOKING:
      return handle_cancel_booking_response(request, response);
    case ACTION_LOGOUT:
      return handle_logout_response(request, response, active_user);
    case ACTION_QUERY:
      return handle_query_response(request, response);
    default:
      fprintf(stderr, "Invalid action received!\n");
      return -1;
  }

  return -1;
}
