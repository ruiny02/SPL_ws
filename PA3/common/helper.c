#include "helper.h"
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

void sigint_handler(int32_t signum) {
  if (signum == SIGINT)
    sigint_received = true;
}

void setup_sigint_handler() {
  struct sigaction action = {.sa_handler = sigint_handler, .sa_flags = 0};
  sigemptyset(&action.sa_mask);
  sigaction(SIGINT, &action, nullptr);
}

void default_request(Request* request) {
  request->username = nullptr;
  request->username_length = 0;
  request->data = nullptr;
  request->data_size = 0;
  request->action = ACTION_INVALID;
}

void free_request(Request* request) {
  if (request->username != nullptr) {
    free(request->username);
    request->username = nullptr;
  }
  if (request->data != nullptr) {
    free(request->data);
    request->data = nullptr;
  }
}

void free_response(Response* response) {
  if (response->data != nullptr) {
    free(response->data);
    response->data = nullptr;
  }
}

ssize_t safe_write(int32_t fd, const void* buf, size_t count) {
  const uint8_t* buf_ptr = (const uint8_t*)buf; // treat buffer as byte sequence
  size_t current_size = 0;
  while (current_size < count) {
      ssize_t n_written = write(fd, buf_ptr + current_size, count - current_size);
      if (n_written > 0) current_size += n_written;
      else if (n_written == 0 || errno != EINTR) return -1;
  }
  return (ssize_t)current_size;
}

ssize_t safe_read(int32_t fd, void* buf, size_t count) {
  uint8_t* buf_ptr = (uint8_t*)buf; // treat buffer as byte sequence
  size_t current_size = 0;
  while (current_size < count) {
      ssize_t n_read = read(fd, buf_ptr + current_size, count - current_size);
      if (n_read > 0) current_size += n_read;
      else if (n_read == 0) break;
      else if (errno != EINTR) return -1;
  }
  return (ssize_t)current_size;
}
