#include <arpa/inet.h>
#include <ctype.h>
#include <editline/readline.h>
#include <errno.h>
#include <helper.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pa3_error.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "handle_response.h"
#include "helper.h"

const char* active_user = nullptr;
bool sigint_received = false;

// Use these two instead of read/write
// [ALREADY IMPLMENTED] Helper function: can write all data to fd safely even when sigint is received
ssize_t safe_write(int32_t fd, const void* buf, size_t count);

// [ALREADY IMPLMENTED] Helper function: can read all data from fd safely even when sigint is received
ssize_t safe_read(int32_t fd, void* buf, size_t count);


// Sets up a socket on the given hostname and port and returns the sockfd.
int32_t get_socket(char* hostname, uint64_t port) {
  char port_str[32];
  snprintf(port_str, sizeof(port_str), "%llu", (unsigned long long)port);

  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  struct addrinfo* result = nullptr;
  int32_t gai_error = getaddrinfo(hostname, port_str, &hints, &result);
  if (gai_error != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(gai_error));
    exit(EXIT_FAILURE);
  }

  int32_t sockfd = -1;
  for (struct addrinfo* rp = result; rp != nullptr; rp = rp->ai_next) {
    sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (sockfd < 0) {
      continue;
    }
    if (connect(sockfd, rp->ai_addr, rp->ai_addrlen) == 0) {
      break;
    }
    close(sockfd);
    sockfd = -1;
  }

  freeaddrinfo(result);
  if (sockfd < 0) {
    perror("connect");
    exit(EXIT_FAILURE);
  }
  return sockfd;
}

static void write_or_exit(int32_t fd, const void* buf, size_t count) {
  if (safe_write(fd, buf, count) != (ssize_t)count) {
    perror("write");
    exit(EXIT_FAILURE);
  }
}

static void read_or_exit(int32_t fd, void* buf, size_t count) {
  if (safe_read(fd, buf, count) != (ssize_t)count) {
    perror("read");
    exit(EXIT_FAILURE);
  }
}

// Sends the request to the server through sockfd.
// It is recommended to send each field of the Request struct separately.
void send_request(int32_t sockfd, Request* request) {
  write_or_exit(sockfd, &request->action, sizeof(request->action));
  write_or_exit(sockfd, &request->username_length,
                sizeof(request->username_length));
  write_or_exit(sockfd, &request->data_size, sizeof(request->data_size));
  if (request->username_length > 0) {
    write_or_exit(sockfd, request->username, request->username_length);
  }
  if (request->data_size > 0) {
    write_or_exit(sockfd, request->data, request->data_size);
  }
}

// Receives the response from the server through sockfd.
// It is recommended to receive each field of the Response struct separately.
void receive_response(int32_t sockfd, Response* response) {
  response->code = -1;
  response->data_size = 0;
  response->data = nullptr;

  read_or_exit(sockfd, &response->code, sizeof(response->code));
  read_or_exit(sockfd, &response->data_size, sizeof(response->data_size));
  if (response->data_size > 0) {
    response->data = malloc(response->data_size);
    if (response->data == nullptr) {
      perror("malloc");
      exit(EXIT_FAILURE);
    }
    read_or_exit(sockfd, response->data, response->data_size);
  }
}

void terminate(int32_t sockfd, const char* active_user) {
  if (active_user != nullptr) {
    Request request;
    default_request(&request);
    request.action = ACTION_LOGOUT;
    request.username = strdup(active_user);
    if (request.username == nullptr) {
      perror("strdup");
      exit(EXIT_FAILURE);
    }
    request.username_length = strlen(request.username);

    send_request(sockfd, &request);

    Response response;
    receive_response(sockfd, &response);
    handle_response(request.action, &request, &response, &active_user);

    free_request(&request);
    free_response(&response);
  }
}

int main(int argc, char* argv[]) {
  setup_sigint_handler();

  if (argc < 3) {
    fprintf(stderr, "usage: %s <IP address> <port>\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  int32_t sockfd = get_socket(argv[1], strtoull(argv[2], nullptr, 10));

  if (argc > 3) {
    const char* filename = argv[3];
    FILE* file = fopen(filename, "r");
    if (file == nullptr) {
      fprintf(stderr, "%s: %s: %s\n", argv[0], filename, strerror(errno));
      exit(1);
    }

    size_t len = 0;
    while (true) {
      char* line = nullptr;
      if (getline(&line, &len, file) == -1) {
        free(line);
        break;
      }

      line[strcspn(line, "\r\n")] = '\0';

      if (!line_is_empty(line)) {
        if (!evaluate(line, sockfd, &active_user)) {
          break;
        }
      } else {
        free(line);
      }
    }
    fclose(file);
  } else {
    while (true) {
      char* input = nullptr;

      if (((input = readline("")) == nullptr) || sigint_received)
        break;

      add_history(input);
      if (!evaluate(input, sockfd, &active_user)) break;
    }
  }

  terminate(sockfd, active_user);
  close(sockfd);
  return 0;
}
