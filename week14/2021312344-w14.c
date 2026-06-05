#define _GNU_SOURCE

#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <syslog.h>
#include <unistd.h>

#define STUDENT_ID "2021312344"
#define PROGRAM_NAME STUDENT_ID "_echo"
#define EXECUTABLE_NAME STUDENT_ID "-w14"
#define PID_FILE "/tmp/" EXECUTABLE_NAME ".pid"
#define BUFFER_SIZE 4096
#define LISTEN_BACKLOG 64

static int listen_fd = -1;
static volatile sig_atomic_t stop_requested = 0;
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

struct client_info {
  int fd;
};

static void notify_parent(int notify_fd, char status) {
  if (notify_fd >= 0) {
    (void)write(notify_fd, &status, 1);
    (void)close(notify_fd);
  }
}

static void exit_with_syslog_error(int notify_fd, const char* message) {
  int saved_errno = errno;
  syslog(LOG_ERR, "%s: %s", message, strerror(saved_errno));
  notify_parent(notify_fd, '0');
  exit(EXIT_FAILURE);
}

static uint16_t parse_port(int argc, char* argv[]) {
  if (argc != 2) {
    fprintf(stderr, "Usage: %s <port>\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  errno = 0;
  char* endptr = NULL;
  unsigned long port = strtoul(argv[1], &endptr, 10);
  if (errno != 0 || endptr == argv[1] || *endptr != '\0' || port == 0 ||
      port > UINT16_MAX) {
    fprintf(stderr, "Invalid port number: %s\n", argv[1]);
    exit(EXIT_FAILURE);
  }

  return (uint16_t)port;
}

static void close_extra_fds(void) {
  DIR* dir = opendir("/proc/self/fd");
  if (dir != NULL) {
    int dir_fd = dirfd(dir);
    struct dirent* entry = NULL;

    while ((entry = readdir(dir)) != NULL) {
      if (entry->d_name[0] == '.') {
        continue;
      }

      errno = 0;
      char* endptr = NULL;
      unsigned long fd = strtoul(entry->d_name, &endptr, 10);
      if (errno != 0 || endptr == entry->d_name || *endptr != '\0' ||
          fd < STDERR_FILENO + 1UL || fd > INT_MAX ||
          (dir_fd >= 0 && (int)fd == dir_fd)) {
        continue;
      }

      (void)close((int)fd);
    }

    closedir(dir);
    return;
  }

  struct rlimit limit;
  if (getrlimit(RLIMIT_NOFILE, &limit) == 0) {
    rlim_t max_fd = limit.rlim_cur;
    if (max_fd == RLIM_INFINITY || max_fd > (rlim_t)INT_MAX) {
      max_fd = (rlim_t)INT_MAX;
    }

    for (int fd = STDERR_FILENO + 1; fd < (int)max_fd; fd++) {
      (void)close(fd);
    }
  }
}

static void reset_signal_handlers_and_mask(void) {
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = SIG_DFL;
  sigemptyset(&sa.sa_mask);

  for (int sig = 1; sig < NSIG; sig++) {
    if (sig == SIGKILL || sig == SIGSTOP) {
      continue;
    }
    (void)sigaction(sig, &sa, NULL);
  }

  sigset_t empty_mask;
  sigemptyset(&empty_mask);
  (void)sigprocmask(SIG_SETMASK, &empty_mask, NULL);
}

static void sanitize_environment_variables(void) {
  (void)setenv("PATH", "/usr/bin:/bin", 1);
}

static int redirect_standard_fds(void) {
  int null_fd = open("/dev/null", O_RDWR);
  if (null_fd == -1) {
    return -1;
  }

  if (dup2(null_fd, STDIN_FILENO) == -1 ||
      dup2(null_fd, STDOUT_FILENO) == -1 ||
      dup2(null_fd, STDERR_FILENO) == -1) {
    int saved_errno = errno;
    (void)close(null_fd);
    errno = saved_errno;
    return -1;
  }

  if (null_fd > STDERR_FILENO && close(null_fd) == -1) {
    return -1;
  }

  return 0;
}

static void daemonize(int* notify_fd_out) {
  int pipefd[2];
  if (pipe(pipefd) == -1) {
    perror("pipe");
    exit(EXIT_FAILURE);
  }

  pid_t pid = fork();
  if (pid == -1) {
    perror("fork");
    exit(EXIT_FAILURE);
  }

  if (pid > 0) {
    char status = '0';
    (void)close(pipefd[1]);
    ssize_t nread = read(pipefd[0], &status, 1);
    (void)close(pipefd[0]);
    _exit((nread == 1 && status == '1') ? EXIT_SUCCESS : EXIT_FAILURE);
  }

  (void)close(pipefd[0]);

  if (setsid() == -1) {
    notify_parent(pipefd[1], '0');
    _exit(EXIT_FAILURE);
  }

  pid = fork();
  if (pid == -1) {
    notify_parent(pipefd[1], '0');
    _exit(EXIT_FAILURE);
  }

  if (pid > 0) {
    _exit(EXIT_SUCCESS);
  }

  if (redirect_standard_fds() == -1) {
    notify_parent(pipefd[1], '0');
    _exit(EXIT_FAILURE);
  }

  umask(0);

  if (chdir("/") == -1) {
    notify_parent(pipefd[1], '0');
    _exit(EXIT_FAILURE);
  }

  *notify_fd_out = pipefd[1];
}

static void remove_pid_file(void) {
  (void)unlink(PID_FILE);
}

static int pid_file_points_to_running_process(pid_t* pid_out) {
  FILE* file = fopen(PID_FILE, "r");
  if (file == NULL) {
    return 0;
  }

  long pid_value = -1;
  int scanned = fscanf(file, "%ld", &pid_value);
  (void)fclose(file);

  if (scanned != 1 || pid_value <= 0 || pid_value > INT_MAX) {
    (void)unlink(PID_FILE);
    return 0;
  }

  pid_t pid = (pid_t)pid_value;
  if (kill(pid, 0) == 0 || errno == EPERM) {
    if (pid_out != NULL) {
      *pid_out = pid;
    }
    return 1;
  }

  if (errno == ESRCH) {
    (void)unlink(PID_FILE);
  }

  return 0;
}

static int write_pid_file(void) {
  pid_t existing_pid = -1;
  if (pid_file_points_to_running_process(&existing_pid)) {
    errno = EEXIST;
    return -1;
  }

  int fd = open(PID_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fd == -1) {
    return -1;
  }

  char pid_text[32];
  int n = snprintf(pid_text, sizeof(pid_text), "%ld\n", (long)getpid());
  if (n < 0 || n >= (int)sizeof(pid_text)) {
    (void)close(fd);
    errno = EINVAL;
    return -1;
  }

  ssize_t written = write(fd, pid_text, (size_t)n);
  if (written != n) {
    int saved_errno = (written == -1) ? errno : EIO;
    (void)close(fd);
    errno = saved_errno;
    return -1;
  }

  if (close(fd) == -1) {
    return -1;
  }

  return 0;
}

static void handle_stop_signal(int sig) {
  (void)sig;
  stop_requested = 1;
  if (listen_fd >= 0) {
    (void)close(listen_fd);
    listen_fd = -1;
  }
}

static void install_daemon_signal_handlers(void) {
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sigemptyset(&sa.sa_mask);

  sa.sa_handler = SIG_IGN;
  (void)sigaction(SIGPIPE, &sa, NULL);

  sa.sa_handler = handle_stop_signal;
  (void)sigaction(SIGTERM, &sa, NULL);
  (void)sigaction(SIGINT, &sa, NULL);
}

static int create_listening_socket(uint16_t port) {
  int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (fd == -1) {
    return -1;
  }

  int optval = 1;
  if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) ==
      -1) {
    int saved_errno = errno;
    (void)close(fd);
    errno = saved_errno;
    return -1;
  }

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(port);

  if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
    int saved_errno = errno;
    (void)close(fd);
    errno = saved_errno;
    return -1;
  }

  if (listen(fd, LISTEN_BACKLOG) == -1) {
    int saved_errno = errno;
    (void)close(fd);
    errno = saved_errno;
    return -1;
  }

  return fd;
}

static ssize_t write_all(int fd, const void* data, size_t size) {
  const char* cursor = data;
  size_t remaining = size;

  while (remaining > 0) {
    ssize_t written = write(fd, cursor, remaining);
    if (written == -1) {
      if (errno == EINTR) {
        continue;
      }
      return -1;
    }
    if (written == 0) {
      errno = EPIPE;
      return -1;
    }

    cursor += written;
    remaining -= (size_t)written;
  }

  return (ssize_t)size;
}

static void log_echoed_bytes(int fd, ssize_t nbytes) {
  struct sockaddr_in peer_addr;
  socklen_t peer_addr_len = sizeof(peer_addr);
  char client_ip[INET_ADDRSTRLEN] = "unknown";
  int client_port = 0;

  if (getpeername(fd, (struct sockaddr*)&peer_addr, &peer_addr_len) == 0 &&
      peer_addr.sin_family == AF_INET) {
    if (inet_ntop(AF_INET, &peer_addr.sin_addr, client_ip,
                  sizeof(client_ip)) == NULL) {
      strncpy(client_ip, "unknown", sizeof(client_ip));
      client_ip[sizeof(client_ip) - 1] = '\0';
    }
    client_port = ntohs(peer_addr.sin_port);
  }

  pthread_mutex_lock(&log_mutex);
  syslog(LOG_INFO, "Echoed %zd bytes to client %s:%d", nbytes, client_ip,
         client_port);
  pthread_mutex_unlock(&log_mutex);
}

static void* handle_client(void* arg) {
  struct client_info* info = arg;
  int client_fd = info->fd;
  free(info);

  char buffer[BUFFER_SIZE];
  while (!stop_requested) {
    ssize_t nread = read(client_fd, buffer, sizeof(buffer));
    if (nread > 0) {
      if (write_all(client_fd, buffer, (size_t)nread) == -1) {
        break;
      }
      log_echoed_bytes(client_fd, nread);
    } else if (nread == 0) {
      break;
    } else if (errno != EINTR) {
      if (errno != ECONNRESET) {
        pthread_mutex_lock(&log_mutex);
        syslog(LOG_WARNING, "read from client failed: %s", strerror(errno));
        pthread_mutex_unlock(&log_mutex);
      }
      break;
    }
  }

  (void)close(client_fd);
  return NULL;
}

static void accept_clients(void) {
  while (!stop_requested) {
    int client_fd = accept(listen_fd, NULL, NULL);
    if (client_fd == -1) {
      if (stop_requested || errno == EINTR || errno == EBADF) {
        continue;
      }
      syslog(LOG_WARNING, "accept failed: %s", strerror(errno));
      continue;
    }

    struct client_info* info = malloc(sizeof(*info));
    if (info == NULL) {
      syslog(LOG_WARNING, "malloc failed: %s", strerror(errno));
      (void)close(client_fd);
      continue;
    }
    info->fd = client_fd;

    pthread_t thread;
    int err = pthread_create(&thread, NULL, handle_client, info);
    if (err != 0) {
      syslog(LOG_WARNING, "pthread_create failed: %s", strerror(err));
      free(info);
      (void)close(client_fd);
      continue;
    }

    err = pthread_detach(thread);
    if (err != 0) {
      syslog(LOG_WARNING, "pthread_detach failed: %s", strerror(err));
    }
  }
}

int main(int argc, char* argv[]) {
  uint16_t port = parse_port(argc, argv);

  close_extra_fds();
  reset_signal_handlers_and_mask();
  sanitize_environment_variables();

  int notify_fd = -1;
  daemonize(&notify_fd);

  openlog(PROGRAM_NAME, LOG_PID, LOG_DAEMON);
  install_daemon_signal_handlers();

  if (write_pid_file() == -1) {
    exit_with_syslog_error(notify_fd, "write_pid_file");
  }
  atexit(remove_pid_file);

  listen_fd = create_listening_socket(port);
  if (listen_fd == -1) {
    exit_with_syslog_error(notify_fd, "create_listening_socket");
  }

  notify_parent(notify_fd, '1');
  syslog(LOG_INFO, "Started echo daemon on port %u", port);

  accept_clients();

  if (listen_fd >= 0) {
    (void)close(listen_fd);
  }
  syslog(LOG_INFO, "Stopped echo daemon");
  closelog();

  return EXIT_SUCCESS;
}
