#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <readline/history.h>
#include <readline/readline.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
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
int is_valid_file_name(const char* file_name);

void handle_exit();
void handle_sigint(int sig);
void setup_signal_handler();

int conn_fd = -1;

void setup_conn_fd(char* host, in_port_t port) {
  struct hostent* host_entry;
  struct sockaddr_in sock_addr;

  /* Create socket */
  conn_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (conn_fd < 0) {
    perror("socket");
    exit(1);
  }

  /* Get host information */
  host_entry = gethostbyname(host);
  if (host_entry == NULL) {
    fprintf(stderr, "invalid hostname %s\n", host);
    exit(1);
  }

  /* Initialize sock_addr */
  memset(&sock_addr, 0, sizeof(sock_addr));
  sock_addr.sin_family = AF_INET;
  memcpy(&sock_addr.sin_addr.s_addr, host_entry->h_addr_list[0],
         host_entry->h_length);
  sock_addr.sin_port = htons(port);

  /* Connect to host */
  if (connect(conn_fd, (struct sockaddr*)&sock_addr, sizeof(sock_addr)) < 0) {
    perror("connect");
    exit(1);
  }
}

int main(int argc, char* argv[]) {
  atexit(handle_exit);
  setup_signal_handler();

  if (argc < 3) {
    fprintf(
        stderr,
        "Received %d arguments. Please enter host address and port number!\n",
        argc - 1);
    exit(1);
  }

  errno = 0;
  char* end = NULL;
  long parsed_port = strtol(argv[2], &end, 10);
  if (errno == ERANGE || end == argv[2] || *end != '\0' ||
      parsed_port <= 0 || parsed_port > 65535) {
    fprintf(stderr, "invalid port number %s\n", argv[2]);
    exit(1);
  }
  in_port_t port = (in_port_t)parsed_port;

  setup_conn_fd(argv[1], port);

  while (1) {
    Data file_name_data;

    char* file_name = readline("File Name: ");
    if (file_name == NULL) {
      break;
    }
    if (strlen(file_name) > 0) {
      add_history(file_name);
    }

    if (strcmp(file_name, "quit") == 0) {
      free(file_name);
      break;
    }

    if (!is_valid_file_name(file_name)) {
      fprintf(stderr, "Invalid file name '%s'!\n", file_name);
      free(file_name);
      continue;
    }

    int fd = open(file_name, O_RDONLY);
    if (fd < 0) {
      fprintf(stderr, "Cannot open '%s' due to `%s`! Please try again!\n",
              file_name, strerror(errno));
      free(file_name);
      continue;
    }
    struct stat st;
    if (fstat(fd, &st) < 0) {
      fprintf(stderr, "Cannot stat '%s' due to `%s`! Please try again!\n",
              file_name, strerror(errno));
      close(fd);
      free(file_name);
      continue;
    }
    if (!S_ISREG(st.st_mode) || st.st_size >= 200000) {
      fprintf(stderr, "Invalid file size for '%s'!\n", file_name);
      close(fd);
      free(file_name);
      continue;
    }

    /* Initialize file_name_data */
    memset(&file_name_data, 0, sizeof(file_name_data));
    file_name_data.operation = SendFileName;
    file_name_data.size = strlen(file_name);
    snprintf(file_name_data.data, sizeof(file_name_data.data), "%s",
             file_name);

    Data serialized_file_name_data = serialize_data(&file_name_data);

    /* Send filename to host */
    if (write_all(conn_fd, &serialized_file_name_data, sizeof(Data)) < 0) {
      perror("write");
      close(fd);
      free(file_name);
      break;
    }

    /* Receive ack from host */

    // We assume ack_data's operation is always Ack, so no need to deserialize,
    // but in real-world applications, you have to double-check this...
    Data ack_data;
    if (read_full(conn_fd, &ack_data, sizeof(Data)) <= 0) {
      fprintf(stderr, "Failed to receive ack!\n");
      close(fd);
      free(file_name);
      break;
    }
    ack_data = deserialize_data(&ack_data);
    if (ack_data.operation != Ack) {
      fprintf(stderr, "Invalid ack!\n");
      close(fd);
      free(file_name);
      break;
    }

    /* Read the file and send data to host */
    size_t total_chars_sent = 0;
    printf("Sending %s...\n", file_name);

    Data file_data;
    memset(&file_data, 0, sizeof(file_data));
    file_data.operation = SendFileData;

    while (1) {
      // Break if n_read == 0 and exit if n_read < 0
      ssize_t n_read = read(fd, file_data.data, MAXLINE);
      if (n_read == 0) {
        break;
      }
      if (n_read < 0) {
        if (errno == EINTR) {
          continue;
        }
        perror("read");
        close(fd);
        free(file_name);
        exit(1);
      }
      file_data.operation = SendFileData;
      file_data.size = (uint32_t)n_read;
      Data serialized_file_data = serialize_data(&file_data);

      /* Send data to host */
      if (write_all(conn_fd, &serialized_file_data, sizeof(Data)) < 0) {
        perror("write");
        close(fd);
        free(file_name);
        exit(1);
      }

      total_chars_sent += file_data.size;
    }

    Data eof_data;
    memset(&eof_data, 0, sizeof(eof_data));
    eof_data.operation = EndOfFile;

    Data serialized_eof_data = serialize_data(&eof_data);

    /* Send EOF */
    if (write_all(conn_fd, &serialized_eof_data, sizeof(Data)) < 0) {
      perror("write");
      close(fd);
      free(file_name);
      break;
    }

    /* Print number of bytes sent */
    printf("Sent %zu bytes.\n", total_chars_sent);
    close(fd);
    free(file_name);
  }

  close(conn_fd);
  conn_fd = -1;

  return 0;
}

/* Helper functions */

void handle_sigint(int sig) {
  if (sig == SIGINT) {
    exit(0);
  }
}

void handle_exit() {
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

int is_valid_file_name(const char* file_name) {
  size_t len = strlen(file_name);
  if (len == 0 || len >= 50) {
    return 0;
  }
  if (strchr(file_name, '.') != NULL || strchr(file_name, '/') != NULL) {
    return 0;
  }
  return 1;
}
