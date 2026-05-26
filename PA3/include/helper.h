#ifndef COMMON_HELPER_H
#define COMMON_HELPER_H

#if (__STDC_VERSION__ >= 202000L && __GNUC__ >= 13)
#include <stddef.h>
#else
#define nullptr (void*)0
typedef void* nullptr_t;
#endif

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include "pa3_error.h"

typedef uint64_t pa3_seat_t;
extern bool sigint_received;

#define ERROR_CHECK(EXPR)                                      \
  do {                                                         \
    if ((EXPR) < 0) {                                          \
      perror(#EXPR);                                           \
      exit(EXIT_FAILURE);                                      \
    }                                                          \
  } while (0)

typedef enum {
  ACTION_INVALID = -1,
  ACTION_TERMINATION,
  ACTION_LOGIN,
  ACTION_BOOK,
  ACTION_CONFIRM_BOOKING,
  ACTION_CANCEL_BOOKING,
  ACTION_LOGOUT,
  ACTION_QUERY,
  ACTION_EXIT, // not used by server, added to make parsing easier
  ACTION_CLEAR_SCREEN // not used by server, added to make parsing easier
} Action;

typedef struct {
  uint64_t username_length;
  uint64_t data_size;
  Action action;
  char* username;
  char* data;
} Request;

void default_request(Request* request);
void free_request(Request* request);

typedef struct {
  uint64_t data_size;
  int32_t code;
  uint8_t* data;
} Response;

void free_response(Response* response);

typedef struct {
  pa3_seat_t id;
  uint64_t amount_of_times_booked;
  uint64_t amount_of_times_canceled;
} SeatStats;

typedef struct {
  SeatStats stats;
  const char* user_who_booked;
  pthread_mutex_t mutex;
} Seat;

void setup_sigint_handler();
ssize_t safe_write(int32_t fd, const void* buf, size_t count);
ssize_t safe_read(int32_t fd, void* buf, size_t count);
#endif
