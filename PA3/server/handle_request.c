#include <helper.h>
#include <pa3_error.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "helper.h"

LoginErrorCode handle_login_request(const Request* request,
                                    Response* response,
                                    Users* users) {}


BookErrorCode handle_book_request(const Request* request,
                                  Response* response,
                                  Users* users,
                                  Seat* seats) {
  // Use pthread_mutex_lock when accessing 'Seat'
}

ConfirmBookingErrorCode handle_confirm_booking_request(const Request* request,
                                                       Response* response,
                                                       Users* users,
                                                       Seat* seats) {}

int32_t handle_cancel_booking_request(const Request* request,
                                      Response* response,
                                      Users* users,
                                      Seat* seats) {
  // Use pthread_mutex_lock when accessing 'Seat'
}

int32_t handle_logout_request(const Request* request,
                              Response* response,
                              Users* users) {}

int32_t handle_query_request(const Request* request,
                             Response* response,
                             Seat* seats) {
  // Use pthread_mutex_lock when accessing 'Seat'
}

int32_t handle_request(const Request* request,
                       Response* response,
                       Users* users,
                       Seat* seats) {
  switch (request->action) {
    case ACTION_LOGIN:
      return handle_login_request(request, response, users);
    case ACTION_BOOK:
      return handle_book_request(request, response, users, seats);
    case ACTION_CONFIRM_BOOKING:
      return handle_confirm_booking_request(request, response, users, seats);
    case ACTION_CANCEL_BOOKING:
      return handle_cancel_booking_request(request, response, users, seats);
    case ACTION_LOGOUT:
      return handle_logout_request(request, response, users);
    case ACTION_QUERY:
      return handle_query_request(request, response, seats);
    case ACTION_TERMINATION:
    //
    default:
      return -1;
  }
}
