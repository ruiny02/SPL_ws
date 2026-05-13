#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define MAXLINE 80

typedef enum { SendFileName, Ack, SendFileData, EndOfFile } Operation;

// These pragmas are used to not pad the struct
#pragma pack(push, 1)
typedef struct {
  uint32_t operation;
  char data[MAXLINE];
  uint32_t size;
} Data;
#pragma pack(pop)

// Preparing data to be sent over the network
Data serialize_data(Data* data);
// Preparing data to be used in the program
Data deserialize_data(Data* data);
int read_full(int fd, void* buf, size_t size);
int write_all(int fd, const void* buf, size_t size);

void handle_exit();
void handle_sigint(int sig);
void setup_signal_handler();

int listen_fd = -1, conn_fd = -1;

int main(int argc, char* argv[]) {
  atexit(handle_exit);
  setup_signal_handler();

  socklen_t conn_addr_len;
  struct sockaddr_in sock_addr, conn_addr;

  if (argc < 2) {
    fprintf(stderr, "Received %d arguments. Please enter port number!\n",
            argc - 1);
    exit(1);
  }
  errno = 0;
  char* end = NULL;
  long parsed_port = strtol(argv[1], &end, 10);
  if (errno == ERANGE || end == argv[1] || *end != '\0' ||
      parsed_port <= 0 || parsed_port > 65535) {
    fprintf(stderr, "invalid port number %s\n", argv[1]);
    exit(1);
  }
  in_port_t port = (in_port_t)parsed_port;

  /* Create listen socket */
  listen_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (listen_fd < 0) {
    perror("socket");
    exit(1);
  }

  /* [END] Create listen socket */
  int yes = 1;
  if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) <
      0) {
    perror("setsockopt");
    exit(1);
  }
  /* Bind sockaddr (IP, etc.) to listen socket */
  memset(&sock_addr, 0, sizeof(sock_addr));
  sock_addr.sin_family = AF_INET;
  sock_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  sock_addr.sin_port = htons(port);
  if (bind(listen_fd, (struct sockaddr*)&sock_addr, sizeof(sock_addr)) < 0) {
    perror("bind");
    exit(1);
  }

  /* Listen to listen socket */
  if (listen(listen_fd, 5) < 0) {
    perror("listen");
    exit(1);
  }

  while (1) {
    /* Accept connection request from client */
    conn_addr_len = sizeof(conn_addr);
    conn_fd =
        accept(listen_fd, (struct sockaddr*)&conn_addr, &conn_addr_len);
    if (conn_fd < 0) {
      if (errno == EINTR) {
        continue;
      }
      perror("accept");
      continue;
    }

    // Handle client
    while (1) {
      /* Get filename from client */
      Data serialized_filename_data;

      int n_read =
          read_full(conn_fd, &serialized_filename_data, sizeof(Data));
      if (n_read == 0) {
        break;
      }
      if (n_read < 0) {
        perror("read");
        break;
      }

      Data filename_data = deserialize_data(&serialized_filename_data);
      filename_data.data[MAXLINE - 1] = '\0';
      if (filename_data.operation != SendFileName) {
        fprintf(stderr, "invalid operation\n");
        break;
      }

      /* Print filename */
      printf("File Name: %s\n", filename_data.data);
      fflush(stdout);

      /* Create a new file called <filename>_copy  */
      char copy_filename[MAXLINE + 6];
      snprintf(copy_filename, sizeof(copy_filename), "%s_copy",
               filename_data.data);
      int fd = open(copy_filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
      if (fd < 0) {
        perror("open");
        break;
      }

      /* Send ack */
      Data ack_data;
      memset(&ack_data, 0, sizeof(ack_data));
      ack_data.operation = Ack;
      Data serialized_ack_data = serialize_data(&ack_data);

      if (write_all(conn_fd, &serialized_ack_data, sizeof(Data)) < 0) {
        perror("write");
        close(fd);
        break;
      }

      /* Receive data and save data to <filename> _copy */
      size_t total_chars_received = 0;
      int quit = 0;  // quit if n_read_size <= 0
      // If n_read_size == 0, print "Client disconnected."

      while (1) {
        Data serialized_data;
        n_read = read_full(conn_fd, &serialized_data, sizeof(Data));
        if (n_read == 0) {
          quit = 1;
          break;
        }
        if (n_read < 0) {
          perror("read");
          quit = 1;
          break;
        }
        Data data = deserialize_data(&serialized_data);

        // Check for operation.
        // If Eof, break
        // If SendFileData, write to file and add length of bytes of file to sum
        if (data.operation == EndOfFile) {
          break;
        }
        if (data.operation != SendFileData || data.size > MAXLINE) {
          fprintf(stderr, "invalid file data\n");
          quit = 1;
          break;
        }
        if (total_chars_received + data.size >= 200000) {
          fprintf(stderr, "file is too large\n");
          quit = 1;
          break;
        }
        if (write_all(fd, data.data, data.size) < 0) {
          perror("write");
          quit = 1;
          break;
        }
        total_chars_received += data.size;
      }

      /* Print numbers of bytes received. */
      printf("Received %zu bytes.\n", total_chars_received);
      fflush(stdout);
      close(fd);

      /* Break from loop once client quits */
      if (quit)
        break;
    }

    close(conn_fd);
    conn_fd = -1;
  }

  close(conn_fd);
  close(listen_fd);
  conn_fd = listen_fd = -1;

  return 0;
}

/* Helper functions */

void handle_sigint(int sig) {
  if (sig == SIGINT) {
    exit(0);
  }
}

void handle_exit() {
  if (listen_fd != -1) {
    close(listen_fd);
  }

  if (conn_fd != -1) {
    close(conn_fd);
  }
}

void setup_signal_handler() {
  struct sigaction act;
  act.sa_handler = handle_sigint;
  act.sa_flags = 0;
  sigemptyset(&act.sa_mask);
  sigaction(SIGINT, &act, NULL);
}

// Preparing data to be sent over the network
Data serialize_data(Data* data) {
  Data serialized_data;
  serialized_data.operation = htonl((uint32_t)data->operation);
  serialized_data.size = htonl((uint32_t)data->size);
  memcpy(serialized_data.data, data->data, MAXLINE);
  return serialized_data;
}

// Preparing data to be used in the program
Data deserialize_data(Data* data) {
  Data deserialized_data;
  deserialized_data.operation = ntohl(data->operation);
  deserialized_data.size = ntohl(data->size);
  memcpy(deserialized_data.data, data->data, MAXLINE);
  return deserialized_data;
}

int read_full(int fd, void* buf, size_t size) {
  char* cur = buf;
  size_t done = 0;

  while (done < size) {
    ssize_t n_read = read(fd, cur + done, size - done);
    if (n_read == 0) {
      return done == 0 ? 0 : -1;
    }
    if (n_read < 0) {
      if (errno == EINTR) {
        continue;
      }
      return -1;
    }
    done += (size_t)n_read;
  }

  return 1;
}

int write_all(int fd, const void* buf, size_t size) {
  const char* cur = buf;
  size_t done = 0;

  while (done < size) {
    ssize_t n_written = write(fd, cur + done, size - done);
    if (n_written < 0) {
      if (errno == EINTR) {
        continue;
      }
      return -1;
    }
    done += (size_t)n_written;
  }

  return 0;
}
