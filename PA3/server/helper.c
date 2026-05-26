#include "helper.h"
#if __has_include(<argon2.h>)
#include <argon2.h>
#else
#define ARGON2_OK 0
int argon2id_hash_encoded(const uint32_t t_cost,
                          const uint32_t m_cost,
                          const uint32_t parallelism,
                          const void* pwd,
                          const size_t pwdlen,
                          const void* salt,
                          const size_t saltlen,
                          const size_t hashlen,
                          char* encoded,
                          const size_t encodedlen);
int argon2id_verify(const char* encoded, const void* pwd, const size_t pwdlen);
#endif
#include <fcntl.h>
#include <sched.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/random.h>
#include <unistd.h>

// Password-related functions

void generate_salt(uint8_t* salt) {
  if (getrandom(salt, SALT_SIZE, 0) != SALT_SIZE) {
    perror("getrandom");
    exit(EXIT_FAILURE);
  }
}

void hash_password(const char* password, char* hashed_password) {
  uint8_t salt[SALT_SIZE];
  generate_salt(salt);
  char hash[HASHED_PASSWORD_SIZE];
  argon2id_hash_encoded(2, MEMORY_USAGE, 1, password, strlen(password), salt,
                        SALT_SIZE, HASH_SIZE, hash, HASHED_PASSWORD_SIZE);
  strncpy(hashed_password, hash, HASHED_PASSWORD_SIZE);
  hashed_password[HASHED_PASSWORD_SIZE - 1] = '\0';
}

bool validate_password(const char* password_to_validate,
                       const char* hashed_password) {
  return argon2id_verify(hashed_password, password_to_validate,
                         strlen(password_to_validate)) == ARGON2_OK;
}

// User-related functions
User default_user() {
  return (User){
      .username = nullptr, .hashed_password = nullptr, .logged_in = false};
}

void setup_users(Users* users) {
  users->size = 0;
  users->capacity = NUM_USERS;
  users->array = malloc(sizeof(User) * users->capacity);
  users->user_mutex = malloc(sizeof(pthread_mutex_t) * users->capacity);
  if (users->array == nullptr || users->user_mutex == nullptr) {
      perror("malloc");
      exit(EXIT_FAILURE);
  }
  for (int i = 0; i < NUM_USERS; i++) {
      users->array[i] = default_user();
      pthread_mutex_init(&users->user_mutex[i], NULL);
  }
  pthread_mutex_init(&users->array_mutex, NULL);
}
ssize_t find_user(const Users* users, const char* username) {
  for (size_t i = 0; i < users->size; i++)
    if (strcmp(users->array[i].username, username) == 0)
      return i;
  return -1;
}

size_t add_user(Users* users,
                const char* username,
                const char* hashed_password) {
  if (users->size >= users->capacity) {
      fprintf(stderr, "User capacity exceeded. Exiting.\n");
      exit(EXIT_FAILURE);
  }
  size_t uid = users->size;

  char* username_copy = strdup(username);
  char* hashed_password_copy = strdup(hashed_password);
  if (username_copy == nullptr || hashed_password_copy == nullptr) {
      perror("strdup");
      free(username_copy);
      free(hashed_password_copy);
      exit(EXIT_FAILURE);
  }

  users->array[uid].username = username_copy;
  users->array[uid].hashed_password = hashed_password_copy;
  users->array[uid].logged_in = false;
  users->size++;
  return uid;
}

void free_users(Users* users) {
  size_t i = 0;
  for (; i < users->size; i++) {
    free((char*)users->array[i].username);
    free((char*)users->array[i].hashed_password);
    pthread_mutex_destroy(&users->user_mutex[i]);
  }

  for (; i < users->capacity; i++)
   pthread_mutex_destroy(&users->user_mutex[i]);

  free(users->user_mutex);
  free(users->array);
  pthread_mutex_destroy(&users->array_mutex);
}

// Seat-related functions

Seat* default_seats() {
  Seat* seats = malloc(sizeof(Seat) * NUM_SEATS);
  if (seats == nullptr) {
    perror("malloc");
    exit(EXIT_FAILURE);
  }

  for (int i = 0; i < NUM_SEATS; i++) {
    seats[i].stats = (SeatStats){ .id = i + 1, .amount_of_times_booked = 0, .amount_of_times_canceled = 0 };
    seats[i].user_who_booked = nullptr;
    pthread_mutex_init(&seats[i].mutex, nullptr);
  }

  return seats;
}

// Poll set-related functions
PollSet* create_poll_set(int32_t self_pipe_fd) {
  PollSet* poll_set = calloc(1, sizeof(PollSet));
  if (poll_set == nullptr) {
    perror("calloc");
    exit(EXIT_FAILURE);
  }
  pthread_mutex_init(&poll_set->mutex, nullptr);
  poll_set->set[0].fd = self_pipe_fd;
  poll_set->set[0].events = POLLIN;
  poll_set->size = 1;

  return poll_set;
}

ssize_t find_suitable_pollset(ThreadData* data_arr, int32_t n_cores) {
  ssize_t min_i = -1;
  size_t min_size = 0;
  for (int i = 0; i < n_cores; i++) {
    pthread_mutex_lock(&data_arr[i].poll_set->mutex);
    size_t poll_set_size = data_arr[i].poll_set->size;
    pthread_mutex_unlock(&data_arr[i].poll_set->mutex);

    if (poll_set_size < CLIENTS_PER_THREAD) {
      if (min_i == -1 || poll_set_size < min_size) {
        min_i = i;
        min_size = poll_set_size;
      }
    }
  }
  return min_i;
}

void notify_pollset(int32_t notification_fd) {
  write(notification_fd, "", 1);
}

// Others
int32_t get_num_cores() {
  cpu_set_t cpu_set;
  sched_getaffinity(0, sizeof(cpu_set), &cpu_set);
  return CPU_COUNT_S(sizeof(cpu_set), &cpu_set);
}

int32_t terminate_after_cleanup(int32_t (*pipe_fds)[2],
                                pthread_t* tid_arr,
                                ThreadData* data_arr,
                                int32_t n_cores,
                                int listenfd,
                                Users* users,
                                Seat* seats) {
  for (int i = 0; i < n_cores; i++) {
    write(pipe_fds[i][1], "", 1);
    close(pipe_fds[i][1]);
    pthread_join(tid_arr[i], nullptr);
    close(pipe_fds[i][0]);

    PollSet* poll_set = data_arr[i].poll_set;
    for (size_t j = 1; j < poll_set->size; j++) {
      close(poll_set->set[j].fd);
      if (poll_set->active_users[j] != nullptr) {
        free((void*)poll_set->active_users[j]);
        poll_set->active_users[j] = nullptr;
      }
    }
    pthread_mutex_destroy(&poll_set->mutex);
    free(poll_set);
  }

  for (int i = 0; i < NUM_SEATS; i++) {
    pthread_mutex_destroy(&seats[i].mutex);
    if (seats[i].user_who_booked != nullptr) {
      free((void*)seats[i].user_who_booked);
    }
  }
  free(seats);


  close(listenfd);
  free(tid_arr);
  free(pipe_fds);
  free(data_arr);
  free_users(users);
  puts("Server terminated!");
  return 0;
}

bool check_stdin_for_termination() {
  bool should_exit = false;

  char buffer[MAXLINE];
  ssize_t n_read = read(STDIN_FILENO, buffer, sizeof(buffer));
  if (n_read < 0) {
    perror("read");
    should_exit = true;
  } else if (n_read == 0) {
    should_exit = true;
  } else {
    buffer[n_read - 1] = '\0';

    if (strncmp(buffer, "exit", 4) == 0 || strncmp(buffer, "quit", 4) == 0 ||
        strncmp(buffer, "\0", 1) == 0) {
      should_exit = true;
    }
  }
  return should_exit;
}
