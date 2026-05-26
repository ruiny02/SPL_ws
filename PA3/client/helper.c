#include <ctype.h>
#include <helper.h>
#include <pa3_error.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "helper.h"
#include "handle_response.h"

#define CLEAR_SCREEN "\033[H\033[J"

#define HANDLE_PARSING_ERROR(ARG, ERROR_MSG, ...) \
  do {                              \
    fprintf(stderr, ERROR_MSG"\n", ##__VA_ARGS__);    \
    parsing_error = ARG;           \
    goto cleanup_error;            \
  } while (0)


// Converts string to Action enum.
// Punctuation and whitespace are removed.
Action to_action(const char* action_str) {
  Action action = ACTION_INVALID;

  char* action_str_copy = strdup(action_str);
  char* action_str_pointer = (char*)action_str_copy;

  for (char* action_str_source = (char*)action_str; *action_str_source;
       ++action_str_source) {
    if (isspace((unsigned char)*action_str_source) ||
        ispunct((unsigned char)*action_str_source)) {
      continue;
    }
    *action_str_pointer = *action_str_source;
    action_str_pointer++;
  }
  *action_str_pointer = '\0';

  if (strcmp(action_str_copy, "login") == 0) {
    action = ACTION_LOGIN;
  } else if (strcmp(action_str_copy, "book") == 0) {
    action = ACTION_BOOK;
  } else if (strcmp(action_str_copy, "confirmbooking") == 0) {
    action = ACTION_CONFIRM_BOOKING;
  } else if (strcmp(action_str_copy, "cancelbooking") == 0) {
    action = ACTION_CANCEL_BOOKING;
  } else if (strcmp(action_str_copy, "logout") == 0) {
    action = ACTION_LOGOUT;
  } else if (strcmp(action_str_copy, "query") == 0) {
    action = ACTION_QUERY;
  } else if ((strcmp(action_str_copy, "exit") == 0) || (strcmp(action_str_copy, "quit") == 0)) {
    action = ACTION_EXIT;
  } else if (strcmp(action_str_copy, "clear") == 0) {
    action = ACTION_CLEAR_SCREEN;
  }
  free(action_str_copy);
  return action;
}

// Parses input into a `Request`.
ParsingError parse_request(Request* request,
                           char* input,
                           const char** active_user) {
  ParsingError parsing_error = PARSING_SUCCESS;
  if (request == nullptr) {
    fprintf(stderr, "Request pointer is nullptr!\n");
    return PARSING_NO_REQUEST;
  }

  default_request(request);

  char* input_copy = strdup(input);
  char* saveptr;

  char* token = strtok_r(input_copy, " \t\r\n", &saveptr);
  if (token != nullptr) {
    request->action = to_action(token);
    if (request->action == ACTION_INVALID) {
      HANDLE_PARSING_ERROR(PARSING_INVALID_ACTION, "Invalid action received!");
    }
    if (request->action == ACTION_LOGIN) {
      if (active_user != nullptr && *active_user != nullptr) {
        HANDLE_PARSING_ERROR(PARSING_ALREADY_LOGGED_IN,
                             "Client is already serving user %s!", *active_user);
      }
    } else if (request->action == ACTION_EXIT || request->action == ACTION_CLEAR_SCREEN) {
      // no-op
    } else if (request->action == ACTION_QUERY) {
      // query does not require an active user
    } else {
      if (active_user == nullptr || *active_user == nullptr) {
        HANDLE_PARSING_ERROR(PARSING_NOT_LOGGED_IN, "User is not logged in!");
      }
      request->username = strdup(*active_user);
      request->username_length = strlen(*active_user);
    }
  } else {
    HANDLE_PARSING_ERROR(PARSING_NO_ACTION,
                         "Please enter the action!");
  }

  token = strtok_r(nullptr, " \t\r\n", &saveptr);

  if (token == nullptr) goto cleanup;

  if (request->action == ACTION_LOGIN) {
    request->username = strdup(token);
    request->username_length = strlen(token);
  } else {
    request->data = strdup(token);
    request->data_size = strlen(token);
    goto cleanup;
  }

  token = strtok_r(nullptr, " \t\r\n", &saveptr);
  if (token != nullptr) {
    request->data = strdup(token); // password
    request->data_size = strlen(token); // password length
  }
  goto cleanup;

cleanup_error:
  free_request(request);
cleanup:
  free(input_copy);
  free(input);
  return parsing_error;
}

void free_input(char** input) {
  if (input == nullptr || *input == nullptr) return;

  free(*input);
  *input = nullptr;
}

bool line_is_empty(const char* line) {
  for (; *line != '\0'; line++) {
    if (!isspace((unsigned char)*line)) {
      return false;
    }
  }
  return true;
}

// Evaluates an input line
bool evaluate(char* input, int32_t sockfd, const char** active_user) {
      Request request;
      ParsingError parsing_error = parse_request(&request, input, active_user);

      if (parsing_error != PARSING_SUCCESS)
        return true;
      else if (request.action == ACTION_EXIT) {
        return false;
      } else if (request.action == ACTION_CLEAR_SCREEN) {
        printf(CLEAR_SCREEN);
        return true;
      }

      send_request(sockfd, &request);

      Response response;
      receive_response(sockfd, &response);
      handle_response(request.action, &request, &response,active_user);
      free_request(&request);
      free_response(&response);
      return true;
}
