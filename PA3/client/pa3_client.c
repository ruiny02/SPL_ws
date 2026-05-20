#include <arpa/inet.h>
#include <ctype.h>
#include <editline/readline.h>
#include <errno.h>
#include <helper.h>
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
  int32_t sockfd;
  // ??
  return sockfd;
}


// Sends the request to the server through sockfd.
// It is recommended to send each field of the Request struct separately.
void send_request(int32_t sockfd, Request* request) {
  // ???
}

// Receives the response from the server through sockfd.
// It is recommended to receive each field of the Response struct separately.
void receive_response(int32_t sockfd, Response* response) {
  // ???
}

void terminate(int32_t sockfd, const char* active_user) {
  if (active_user != nullptr) {
    // TODO: send logout request and cleanup request, response as the user is still active.
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

    size_t len;
    while (true) {
      char* line = nullptr;
      if (getline(&line, &len, file) == -1)
        break;

      line[strlen(line) - 1] = '\0';

      if (!line_is_empty(line))
        if (!evaluate(line, sockfd, &active_user))
          break;
    }
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
