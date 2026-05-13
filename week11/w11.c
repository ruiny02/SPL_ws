#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/random.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

typedef struct {
  uint64_t* vector;
  uint64_t* matrix_row;
  size_t size;
  uint64_t result;
} thread_mvm_data;

typedef struct {
  uint64_t* array;
  size_t size;
} thread_mod_reduc_data;

typedef enum { COLUMN, ROW } vector_type;

// Helper functions
int32_t get_number_of_cores();  // gets the number of cores available
void fputs_matrix(const char* filename,
                  const uint64_t* const* const matrix,
                  size_t row_size,
                  size_t col_size);  // writes the matrix to a file

void fputs_vector(const char* filename,
                  const uint64_t* const vector,
                  size_t size,
                  vector_type vec_type);  // writes the vector to a file

void validate_result();  // checks if the result matches with the answer
#ifdef DEBUG
void single_threaded_matrix_multiplication(
    const uint64_t* const* const matrix,
    const uint64_t* const vector,
    uint64_t row_size,
    uint64_t column_size);  // does matmux with a single thread
#endif

/*** Insert your own matrix & vector data, functions ***/

// TODO: Implement modulo reduction
void* thread_modulo_reduction(void* arg) {
  thread_mod_reduc_data* data = (thread_mod_reduc_data*)arg;
  for (size_t i = 0; i < data->size; i++) {
    // Limit the values to be in range [0, 9]
  }
  return NULL;
}

// TODO: Implement matrix-vector multiplication
void* thread_matrix_vector_mult(void* arg) {
  thread_mvm_data* data = (thread_mvm_data*)arg;
  uint64_t sum = 0;
  // TODO: Modify sum
  // ...
  data->result = sum;
  return NULL;
}

int main(int argc, char* argv[]) {
  if (argc != 3) {
    printf("Usage: %s <row> <column>\n", argv[0]);
    exit(1);
  }

  // Assume row_size and col_size are valid
  uint64_t row_size = strtoll(argv[1], NULL, 10);
  uint64_t col_size = strtoll(argv[2], NULL, 10);

  const int32_t n_cores = get_number_of_cores();

  pthread_t* tids = malloc(row_size * sizeof(pthread_t));
  thread_mvm_data* t_mvm_data_arr = malloc(row_size * sizeof(thread_mvm_data));
  thread_mod_reduc_data* t_mod_reduc_data_arr =
      malloc(row_size * sizeof(thread_mod_reduc_data));

  uint64_t** matrix = malloc(row_size * sizeof(uint64_t*));
  for (size_t i = 0; i < row_size; i++) {
    matrix[i] = malloc(col_size * sizeof(uint64_t));
  }
  uint64_t* vector = malloc(col_size * sizeof(uint64_t));
  uint64_t* result = malloc(row_size * sizeof(uint64_t));

  // Initialize matrix & vector with random values
  for (size_t i = 0; i < row_size; i++) {
    getrandom(matrix[i], col_size * sizeof(uint64_t), 0);
  }
  getrandom(vector, col_size * sizeof(uint64_t), 0);

  /*** Insert your code ***/

  // TODO: Modify matrix & vector to be in range [0, 9] (digit)
  pthread_t vector_tid;
  thread_mod_reduc_data vector_data;
  // TODO: Create vector thread

  // TODO: Handle matrix
  for (size_t batch_i = 0; batch_i * n_cores < row_size; batch_i++) {
    size_t batch_start = batch_i * n_cores,
           batch_end = batch_start + n_cores < row_size ? batch_start + n_cores
                                                        : row_size;

    // TODO: Create threads
    for (size_t i = batch_start; i < batch_end; i++) {
    }

    // TODO: Join threads
    for (size_t i = batch_start; i < batch_end; i++) {
    }
  }

  // TODO: Join vector thread

  free(t_mod_reduc_data_arr);

  // Write matrix & vector to file
  fputs_matrix("input_matrix.txt", (const uint64_t* const*)matrix, row_size,
               col_size);

  fputs_vector("input_vector.txt", vector, col_size, ROW);

  memset(tids, 0, row_size * sizeof(pthread_t));

  for (size_t batch_i = 0; batch_i * n_cores < row_size; batch_i++) {
    size_t batch_start = batch_i * n_cores,
           batch_end = batch_start + n_cores < row_size ? batch_start + n_cores
                                                        : row_size;

    // TODO: Create threads
    for (size_t i = batch_start; i < batch_end; i++) {
    }

    // TODO: Join threads
    for (size_t i = batch_start; i < batch_end; i++) {
    }

    // TODO: Store result in result array
    for (size_t i = batch_start; i < batch_end; i++) {
    }
  }

  fputs_vector("result.txt", result, row_size, COLUMN);

#ifdef DEBUG
  single_threaded_matrix_multiplication((const uint64_t* const*)matrix, vector,
                                        row_size, col_size);
  validate_result();
#endif

  for (size_t i = 0; i < row_size; i++) {
    free(matrix[i]);
  }
  free(matrix);
  free(vector);
  free(result);
  free(t_mvm_data_arr);
  free(tids);
  return 0;
}

// Helper functions
int32_t get_number_of_cores() {
  cpu_set_t cpu_set;
  sched_getaffinity(0, sizeof(cpu_set), &cpu_set);
  return CPU_COUNT_S(sizeof(cpu_set), &cpu_set);
}

void fputs_matrix(const char* filename,
                  const uint64_t* const* const matrix,
                  size_t row_size,
                  size_t col_size) {
  int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fd == -1) {
    perror("open");
    exit(EXIT_FAILURE);
  }
  printf("Writing matrix to file %s!\n", filename);
  for (size_t i = 0; i < row_size; i++) {
    // write row
    const uint64_t* row = matrix[i];
    char* row_str;
    char** col_strings = malloc(col_size * sizeof(char*));
    for (size_t j = 0; j < col_size; j++) {
      col_strings[j] = NULL;
    }

    size_t row_str_len = 3;  // for [, ], and \n

    for (size_t j = 0; j < col_size; j++) {
      asprintf(&col_strings[j], "%lu", row[j]);

      row_str_len += strlen(col_strings[j]);
      if (j != col_size - 1)
        row_str_len++;  // +1 for tab
    }
    row_str = malloc(row_str_len);
    row_str[0] = '[';
    size_t offset = 1;
    for (size_t j = 0; j < col_size; j++) {
      size_t len = strlen(col_strings[j]);
      strncpy(row_str + offset, col_strings[j], len);
      offset += len;
      if (j != col_size - 1) {
        row_str[offset] = '\t';
        offset++;
      }
      if (col_strings[j] != NULL) {
        free(col_strings[j]);
        col_strings[j] = NULL;
      }
    }
    free(col_strings);

    row_str[offset] = ']';
    row_str[offset + 1] = '\n';

    ssize_t n_bytes = write(fd, row_str, row_str_len);
    if (n_bytes == -1) {
      perror("write");
      close(fd);
      free(row_str);
      exit(EXIT_FAILURE);
    }
    free(row_str);
  }
  close(fd);
}

void fputs_vector(const char* filename,
                  const uint64_t* const vector,
                  size_t size,
                  vector_type vec_type) {
  switch (vec_type) {
    case COLUMN:
      uint64_t const** matrix = malloc(size * sizeof(uint64_t*));
      if (matrix == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
      }
      for (size_t i = 0; i < size; i++) {
        matrix[i] = &vector[i];
      }
      fputs_matrix(filename, (const uint64_t* const*)matrix, size, 1);
      free(matrix);
      break;
    case ROW:
      fputs_matrix(filename,
                   (const uint64_t* const*)&(const uint64_t*[]){vector}, 1,
                   size);
      break;
    default:
      fprintf(stderr, "Invalid vector type!\n");
      exit(EXIT_FAILURE);
  }
}

void validate_result() {
  switch (fork()) {
    case -1:
      perror("fork");
      break;
    case 0:
      execlp("cmp", "cmp", "-s", "result.txt", "answer.txt", NULL);
      perror("execlp");
      break;
    default:
      int32_t status;
      if (wait(&status) == -1) {
        perror("wait");
        break;
      }

      if (WIFEXITED(status)) {
        switch (WEXITSTATUS(status)) {
          case 0:
            printf("The result is correct!\n");
            return;
          case 1:
            fprintf(stderr, "The result is wrong!\n");
            return;
        }
      }
      fprintf(stderr, "Unknown error occurred!\n");
      break;
  }
  exit(EXIT_FAILURE);
}

#ifdef DEBUG
// This function may serve as a hint for the multi-threaded version
void single_threaded_matrix_multiplication(const uint64_t* const* const matrix,
                                           const uint64_t* const vector,
                                           uint64_t row_size,
                                           uint64_t column_size) {
  uint64_t single_threaded_result[row_size];

  for (size_t row_i = 0; row_i < row_size; row_i++) {
    uint64_t sum = 0;
    const uint64_t* row = matrix[row_i];
    for (size_t column_i = 0; column_i < column_size; column_i++) {
      sum += row[column_i] * vector[column_i];
    }
    single_threaded_result[row_i] = sum;
  }

  uint64_t* single_threaded_result_ptr = single_threaded_result;

  fputs_vector("answer.txt", (const uint64_t*)single_threaded_result_ptr,
               row_size, COLUMN);
}
#endif
