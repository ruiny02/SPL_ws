// You're free to change any part of this code.
// You can add more arguments if needed to existing functions.

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define MAXLINE 512
#define MAX_USERNAME 20

#define SAFELY_RUN(call, exit_code) \
  if ((call) < 0) {                 \
    perror(#call);                  \
    exit(exit_code);                \
  }

enum ExitCode {
  EXIT_CODE_SUCCESS,
  EXIT_CODE_SOCKET_CREATION_FAILURE,
  EXIT_CODE_CONNECTION_FAILURE,
  EXIT_CODE_POLL_FAILURE,
  EXIT_CODE_WRITE_FAILURE,
  EXIT_CODE_READ_FAILURE,
  EXIT_CODE_INVALID_ARGUMENTS,
  EXIT_CODE_INVALID_PORT_NUMBER,
  EXIT_CODE_GETADDRINFO_FAILURE,
  EXIT_CODE_SERVER_DOWN,
  EXIT_CODE_GETLINE_FAILURE,
  // feel free to add more
};

enum MessageType {
  NEW_USER,
  USER_LEFT,
  USER_MESSAGE,
};

typedef struct {
  enum MessageType type;
  char message[MAXLINE];
  char username[MAX_USERNAME];
} message;

int conn_fd;
char username[MAXLINE];

// Helper functions
// You can remove these functions if you'd like
// I just added these functions to lessen the errors.
void exit_handler(void);
void sigint_handler(int sig);
void handle_server_termination(int sig);
long get_port(int argc, char* argv[]);
struct in_addr get_socket_address(char* host);
void get_username(char* username);

/* Functions you might have to modify */

void handle_input(int conn_fd, char* username) {
  char buf[MAXLINE];
  ssize_t bytes_read;

  switch (bytes_read = read(STDIN_FILENO, buf, MAXLINE)) {
    case -1:
      perror("read");
      exit(EXIT_CODE_READ_FAILURE);
    case 0:
      exit(EXIT_CODE_SUCCESS);
    default:
      if (strncmp(buf, "/quit\n", 6) == 0)
        exit(EXIT_CODE_SUCCESS);

      // TODO: Set up msg by copying username and message as well as setting the type, then send the message to the server
      message msg;

      SAFELY_RUN(write(conn_fd, (void*)&msg, sizeof(msg)),
                 EXIT_CODE_WRITE_FAILURE)
      break;
  }
}

void handle_server_response(int conn_fd) {
  message msg;
  switch (read(conn_fd, &msg, sizeof(msg))) {
    case -1:
      perror("read");
      exit(EXIT_CODE_READ_FAILURE);
    case 0:
      puts("Connection with the server has been terminated.");
      exit(EXIT_CODE_SERVER_DOWN);
    default:
      // Print message depending on the type
      switch (msg.type) {}
      break;
  }
}

int main(int argc, char* argv[]) {
  struct sigaction sa;
  sa.sa_handler = handle_server_termination;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sigaction(SIGPIPE, &sa, NULL);
  sa.sa_handler = sigint_handler;
  sigaction(SIGINT, &sa, NULL);
  struct sockaddr_in socket_address;

  get_username(username);

  SAFELY_RUN(conn_fd = socket(AF_INET, SOCK_STREAM, 0), 2)
  atexit(exit_handler);

  memset((char*)&socket_address, 0, sizeof(socket_address));
  socket_address.sin_family = AF_INET;
  socket_address.sin_addr = get_socket_address(argv[1]);
  socket_address.sin_port = htons(get_port(argc, argv));

  SAFELY_RUN(connect(conn_fd, (struct sockaddr*)&socket_address,
                     sizeof(socket_address)),
             3)

  // TODO: Send username to server
  //       Remember that the server should notify each client
  //       when a new user joins the chat room.
  message new_user_msg;
  memset(&new_user_msg, 0, sizeof(new_user_msg));

  // END TODO

  struct pollfd fds[1000];
  int ret;

  while (1) {
    fds[0] = (struct pollfd){
        .fd = STDIN_FILENO,
        .events = POLLIN};  // Checks if the user entered something

    fds[1] = (struct pollfd){
        .fd = conn_fd,
        .events = POLLIN};  // Checks if the server sent something

    SAFELY_RUN(ret = poll(fds, 2, -1), EXIT_CODE_POLL_FAILURE)

    if (fds[0].revents & POLLIN) handle_input(conn_fd, username);
    if (fds[1].revents & POLLIN) handle_server_response(conn_fd);
  }

  close(conn_fd);
  return EXIT_CODE_SUCCESS;
}

// Helper functions

void exit_handler(void) {
  message msg;
  memset(&msg, 0, sizeof(msg));
  msg.type = USER_LEFT;
  strncpy(msg.username, username, sizeof(msg.username) - 1);
  msg.username[sizeof(msg.username) - 1] = '\0';

  SAFELY_RUN(write(conn_fd, (void*)&msg, sizeof(msg)), EXIT_CODE_WRITE_FAILURE)
  puts("Bye!");
  close(conn_fd);
}

void sigint_handler(int sig) {
  if (sig == SIGINT) {
    exit(EXIT_CODE_SUCCESS);
  }
}

void handle_server_termination(int sig) {
  if (sig == SIGPIPE) {
    SAFELY_RUN(write(STDOUT_FILENO, "Server was terminated!\n", 24),
               EXIT_CODE_WRITE_FAILURE)
    exit(EXIT_CODE_SERVER_DOWN);
  }
}

long get_port(int argc, char* argv[]) {
  char* endptr;

  if (argc != 3) {
    fprintf(stderr, "Usage: %s <host> <port>\n", argv[0]);
    exit(EXIT_CODE_INVALID_ARGUMENTS);
  }

  long port = strtol(argv[2], &endptr, 10);

  if (*endptr != '\0') {
    printf("Invalid port number.\n");
    exit(EXIT_CODE_INVALID_PORT_NUMBER);
  }

  return port;
}

struct in_addr get_socket_address(char* host) {
  struct addrinfo hints, *res;
  int err;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;

  if ((err = getaddrinfo(host, NULL, &hints, &res)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err));
    exit(EXIT_CODE_GETADDRINFO_FAILURE);
  }

  struct in_addr addr = ((struct sockaddr_in*)res->ai_addr)->sin_addr;

  freeaddrinfo(res);

  return addr;
}

void get_username(char* username) {
  printf("Enter your username: ");
  size_t max_username = MAX_USERNAME;
  memset(username, 0, MAXLINE);

  long bytes_read = getline((char**)&username, &max_username, stdin);

  if (bytes_read == -1) {
    if (errno == 0) {
      exit(EXIT_CODE_SUCCESS);
    }

    perror("getline");
    exit(EXIT_CODE_GETLINE_FAILURE);
  }

  username[bytes_read - 1] = '\0';
}
