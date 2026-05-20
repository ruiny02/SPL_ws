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

// Use pthread_mutex_lock when accessing 'PollSet'
void add_to_pollset(PollSet* poll_set,
                    int32_t notification_fd,
                    int32_t connfd) {
  // ???
  notify_pollset(notification_fd);
  // ???
}

// This function is called within thread_func.
// Assuming you have already obtained the lock in thread_func, you do not need to lock the mutex here.
void remove_from_pollset(ThreadData* data, size_t* i_ptr) {
  // ???
}

// You have to poll the poll_set. When there is a file that is ready to be read,
// you have to lock the poll set to prevent cases where the poll set gets
// updated. You have to remember to unlock the poll set after you are done
// reading from it, including cases where you have to exit due to errors, or
// else your program might not be able to terminate without calling (p)kill.
void* thread_func(void* arg) {
  ThreadData* data = (ThreadData*)arg;
  while (!sigint_received) {
    // ???
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
