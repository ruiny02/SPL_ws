#include <arpa/inet.h>
#include <errno.h>
#include <helper.h>
#include <netinet/in.h>
#include <pa3_error.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include "helper.h"

bool sigint_received = false;

/** ------ ALREADY IMPLEMENTED HELPER FUNCTIONS ------ **/
// Use these two instead of read/write
// [ALREADY IMPLEMENTED] Helper function: can write all data to fd safely even when sigint is received
ssize_t safe_write(int32_t fd, const void* buf, size_t count);

// [ALREADY IMPLEMENTED] Helper function: can read all data from fd safely even when sigint is received
ssize_t safe_read(int32_t fd, void* buf, size_t count);

/** ------ END ALREADY IMPLEMENTED HELPER FUNCTIONS ------ **/

static void deactivate_user(Users* users, const char* username) {
  if (username == nullptr) {
    return;
  }

  pthread_mutex_lock(&users->array_mutex);
  pa3_uid_t uid = find_user(users, username);
  if (uid < 0) {
    pthread_mutex_unlock(&users->array_mutex);
    return;
  }

  pthread_mutex_lock(&users->user_mutex[uid]);
  pthread_mutex_unlock(&users->array_mutex);
  users->array[uid].logged_in = false;
  pthread_mutex_unlock(&users->user_mutex[uid]);
}

static ssize_t find_fd_in_pollset(PollSet* poll_set, int32_t fd) {
  for (size_t i = 0; i < poll_set->size; i++) {
    if (poll_set->set[i].fd == fd) {
      return (ssize_t)i;
    }
  }
  return -1;
}

static bool receive_request(int32_t fd, Request* request) {
  default_request(request);

  ssize_t n_read = safe_read(fd, &request->action, sizeof(request->action));
  if (n_read == 0) {
    return false;
  }
  if (n_read != (ssize_t)sizeof(request->action)) {
    return false;
  }
  if (safe_read(fd, &request->username_length,
                sizeof(request->username_length)) !=
      (ssize_t)sizeof(request->username_length)) {
    return false;
  }
  if (safe_read(fd, &request->data_size, sizeof(request->data_size)) !=
      (ssize_t)sizeof(request->data_size)) {
    return false;
  }

  if (request->username_length > 0) {
    request->username = calloc(request->username_length + 1, sizeof(char));
    if (request->username == nullptr) {
      perror("calloc");
      exit(EXIT_FAILURE);
    }
    if (safe_read(fd, request->username, request->username_length) !=
        (ssize_t)request->username_length) {
      return false;
    }
  }

  if (request->data_size > 0) {
    request->data = calloc(request->data_size + 1, sizeof(char));
    if (request->data == nullptr) {
      perror("calloc");
      exit(EXIT_FAILURE);
    }
    if (safe_read(fd, request->data, request->data_size) !=
        (ssize_t)request->data_size) {
      return false;
    }
  }

  return true;
}

static bool send_response(int32_t fd, const Response* response) {
  if (safe_write(fd, &response->code, sizeof(response->code)) !=
      (ssize_t)sizeof(response->code)) {
    return false;
  }
  if (safe_write(fd, &response->data_size, sizeof(response->data_size)) !=
      (ssize_t)sizeof(response->data_size)) {
    return false;
  }
  if (response->data_size > 0 &&
      safe_write(fd, response->data, response->data_size) !=
          (ssize_t)response->data_size) {
    return false;
  }
  return true;
}

// Use pthread_mutex_lock when accessing 'PollSet'
void add_to_pollset(PollSet* poll_set,
                    int32_t notification_fd,
                    int32_t connfd) {
  pthread_mutex_lock(&poll_set->mutex);
  if (poll_set->size >= CLIENTS_PER_THREAD) {
    pthread_mutex_unlock(&poll_set->mutex);
    close(connfd);
    return;
  }

  size_t i = poll_set->size;
  poll_set->set[i].fd = connfd;
  poll_set->set[i].events = POLLIN;
  poll_set->set[i].revents = 0;
  poll_set->active_users[i] = nullptr;
  poll_set->size++;
  pthread_mutex_unlock(&poll_set->mutex);
  notify_pollset(notification_fd);
}

// This function is called within thread_func.
// Assuming you have already obtained the lock in thread_func, you do not need to lock the mutex here.
void remove_from_pollset(ThreadData* data, size_t* i_ptr) {
  PollSet* poll_set = data->poll_set;
  size_t i = *i_ptr;
  if (i >= poll_set->size) {
    return;
  }

  if (poll_set->active_users[i] != nullptr) {
    deactivate_user(data->users, poll_set->active_users[i]);
    free((void*)poll_set->active_users[i]);
    poll_set->active_users[i] = nullptr;
  }

  close(poll_set->set[i].fd);

  size_t last = poll_set->size - 1;
  if (i != last) {
    poll_set->set[i] = poll_set->set[last];
    poll_set->active_users[i] = poll_set->active_users[last];
  }

  poll_set->set[last].fd = -1;
  poll_set->set[last].events = 0;
  poll_set->set[last].revents = 0;
  poll_set->active_users[last] = nullptr;
  poll_set->size--;

  if (*i_ptr > 0) {
    (*i_ptr)--;
  }
}

static void remove_fd_from_pollset(ThreadData* data, int32_t fd) {
  pthread_mutex_lock(&data->poll_set->mutex);
  ssize_t i = find_fd_in_pollset(data->poll_set, fd);
  if (i >= 0) {
    size_t index = (size_t)i;
    remove_from_pollset(data, &index);
  }
  pthread_mutex_unlock(&data->poll_set->mutex);
}

// You have to poll the poll_set. When there is a file that is ready to be read,
// you have to lock the poll set to prevent cases where the poll set gets
// updated. You have to remember to unlock the poll set after you are done
// reading from it, including cases where you have to exit due to errors, or
// else your program might not be able to terminate without calling (p)kill.
void* thread_func(void* arg) {
  ThreadData* data = (ThreadData*)arg;
  while (!sigint_received) {
    struct pollfd local_set[CLIENTS_PER_THREAD];
    nfds_t local_size;

    pthread_mutex_lock(&data->poll_set->mutex);
    local_size = (nfds_t)data->poll_set->size;
    memcpy(local_set, data->poll_set->set,
           sizeof(struct pollfd) * data->poll_set->size);
    pthread_mutex_unlock(&data->poll_set->mutex);

    int32_t poll_result = poll(local_set, local_size, -1);
    if (poll_result < 0) {
      if (errno == EINTR) {
        continue;
      }
      perror("poll");
      break;
    }

    for (nfds_t i = 0; i < local_size && poll_result > 0; i++) {
      if (local_set[i].revents == 0) {
        continue;
      }
      poll_result--;

      if (local_set[i].fd == data->pipe_out_fd) {
        char buffer[64];
        ssize_t n_read = read(data->pipe_out_fd, buffer, sizeof(buffer));
        if (n_read == 0 && sigint_received) {
          break;
        }
        continue;
      }

      int32_t fd = local_set[i].fd;
      if (local_set[i].revents & (POLLERR | POLLHUP | POLLNVAL)) {
        remove_fd_from_pollset(data, fd);
        continue;
      }

      Request request;
      if (!receive_request(fd, &request)) {
        free_request(&request);
        remove_fd_from_pollset(data, fd);
        continue;
      }

      pthread_mutex_lock(&data->poll_set->mutex);
      ssize_t actual_i = find_fd_in_pollset(data->poll_set, fd);
      const char** active_user =
          actual_i >= 0 ? &data->poll_set->active_users[actual_i] : nullptr;
      pthread_mutex_unlock(&data->poll_set->mutex);

      if (active_user == nullptr) {
        free_request(&request);
        continue;
      }

      Response response = {.data_size = 0, .code = -1, .data = nullptr};
      int32_t code =
          handle_request(&request, &response, data->users, data->seats,
                         active_user);
      if (code == -1) {
        fprintf(stderr, "Invalid action received: %d\n", request.action);
      }

      bool sent = send_response(fd, &response);
      free_request(&request);
      free_response(&response);

      if (!sent) {
        remove_fd_from_pollset(data, fd);
      }
    }
  }
  pthread_exit(nullptr);
}

int main(int argc, char* argv[]) {
  setup_sigint_handler();

  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    return 1;
  }

  int listenfd;
  struct sockaddr_in saddr, caddr;

  Users users;
  setup_users(&users);

  Seat* seats = default_seats();
  int32_t n_cores = get_num_cores();

  pthread_t* tid_arr = malloc(sizeof(pthread_t) * n_cores);
  ThreadData* data_arr = malloc(sizeof(ThreadData) * n_cores);
  int32_t (*pipe_fds)[2] = malloc(sizeof(int32_t[2]) * n_cores);

  for (int i = 0; i < n_cores; i++) {
    ERROR_CHECK(pipe(pipe_fds[i]));

    data_arr[i].thread_index = i;
    data_arr[i].pipe_out_fd = pipe_fds[i][0];
    data_arr[i].poll_set = create_poll_set(pipe_fds[i][0]);
    data_arr[i].users = &users;
    data_arr[i].seats = seats;
    ERROR_CHECK(pthread_create(&tid_arr[i], nullptr, thread_func, &data_arr[i]));
  }

  ERROR_CHECK(listenfd = socket(AF_INET, SOCK_STREAM, 0));
  int opt = 1;
  ERROR_CHECK(setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)));

  memset(&saddr, 0, sizeof(saddr));
  saddr.sin_family = AF_INET;
  saddr.sin_addr.s_addr = htonl(INADDR_ANY);
  saddr.sin_port = htons(strtoull(argv[1], nullptr, 10));

  ERROR_CHECK(bind(listenfd, (struct sockaddr*)&saddr, sizeof(saddr)));
  ERROR_CHECK(listen(listenfd, 10));

  struct pollfd main_thread_poll_set[2];
  memset(main_thread_poll_set, 0, sizeof(main_thread_poll_set));
  main_thread_poll_set[0].fd = STDIN_FILENO;
  main_thread_poll_set[0].events = POLLIN;
  main_thread_poll_set[1].fd = listenfd;
  main_thread_poll_set[1].events = POLLIN;

  while (!sigint_received) {
    if (poll(main_thread_poll_set, 2, -1) < 0) {
      if (errno == EINTR) {
        continue;
      }
      perror("poll");
      exit(EXIT_FAILURE);
    }

    if (main_thread_poll_set[0].revents & POLLIN) {
      if (check_stdin_for_termination() == true) {
        kill(getpid(), SIGINT);
        continue;
      }
    } else if (main_thread_poll_set[1].revents & POLLIN) {
      uint32_t caddrlen = sizeof(caddr);
      int connfd = accept(listenfd, (struct sockaddr*)&caddr, &caddrlen);
      if (connfd < 0) {
        if (errno == EINTR) {
          continue;
        }
        puts("accept() failed");
        exit(EXIT_FAILURE);
      }

      printf("Accepted connection from client\n");
      ssize_t pollset_i;
      do {
        pollset_i = find_suitable_pollset(data_arr, n_cores);
      } while (pollset_i == -1);
      add_to_pollset(data_arr[pollset_i].poll_set, pipe_fds[pollset_i][1],
                     connfd);
    }
  }

  return terminate_after_cleanup(pipe_fds, tid_arr, data_arr, n_cores, listenfd,
                                 &users, seats);
}
