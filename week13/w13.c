#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
  size_t put_index;  // rear
  size_t get_index;  // front
  size_t length;     // size
  size_t capacity;
  size_t total_n_iterations;
  pthread_mutex_t lock;
  pthread_cond_t not_full;
  pthread_cond_t not_empty;
  size_t data[];
} queue_t;

void* produce(void* args);
void* consume(void* args);
void put_data(queue_t* queue, size_t data);
size_t get_data(queue_t* queue);

queue_t* qinit(size_t total_n_iterations, size_t capacity) {
  if (capacity == 0) {
    fprintf(stderr, "capacity must be greater than 0\n");
    exit(EXIT_FAILURE);
  }
  if (total_n_iterations == 0) {
    fprintf(stderr, "total_n_iterations must be greater than 0\n");
    exit(EXIT_FAILURE);
  }

  queue_t* queue = (queue_t*)malloc(sizeof(queue_t) + sizeof(size_t[capacity]));

  if (queue == NULL) {
    fprintf(stderr, "Failed to allocate memory for queue\n");
    exit(EXIT_FAILURE);
  }

  queue->capacity = capacity;
  queue->total_n_iterations = total_n_iterations;

  queue->put_index = queue->get_index = queue->length = 0;
  pthread_mutex_init(&queue->lock, NULL);
  pthread_cond_init(&queue->not_full, NULL);
  pthread_cond_init(&queue->not_empty, NULL);
  return queue;
}

void qdelete(queue_t* queue) {
  pthread_mutex_destroy(&queue->lock);
  pthread_cond_destroy(&queue->not_full);
  pthread_cond_destroy(&queue->not_empty);
  free(queue);
}

int main(int argc, char* argv[]) {
  if (argc != 3) {
    fprintf(stderr, "usage: %s <total_n_iterations> <queue_size>\n", argv[0]);
    return 1;
  }
  queue_t* queue =
      qinit(strtoull(argv[1], NULL, 10), strtoull(argv[2], NULL, 10));

  pthread_t producer, consumer;
  pthread_create(&producer, NULL, produce, (void*)queue);
  pthread_create(&consumer, NULL, consume, (void*)queue);

  pthread_join(producer, NULL);
  pthread_join(consumer, NULL);

  qdelete(queue);
}

void* produce(void* args) {
  queue_t* queue = (queue_t*)args;
  for (size_t i = 0; i < queue->total_n_iterations; i++) {
    size_t data = i;
    put_data(queue, data);
  }
  pthread_exit(NULL);
}

void* consume(void* args) {
  queue_t* queue = (queue_t*)args;
  for (size_t i = 0; i < queue->total_n_iterations; i++) {
    get_data(queue);
  }
  pthread_exit(NULL);
}

/** Add your code here **/

// Put data unless queue is full
void put_data(queue_t* queue, size_t data) {
  // 1. Lock
  pthread_mutex_lock(&queue->lock);

  // 2. Wait until queue is not full
  while (queue->length == queue->capacity) {
    pthread_cond_wait(&queue->not_full, &queue->lock);
  }

  // 3. Put Algorithm for Circular Array
  // 3.a. Assign the value `data` to the position pointed by `put_index`
  queue->data[queue->put_index] = data;

  // 3.b. Modularly increase `put_index` by 1 so that it is within `capacity` at
  // all times
  queue->put_index = (queue->put_index + 1) % queue->capacity;

  // 3.c. Increase `length` by 1
  queue->length++;

  printf("put data %zu to queue\n", data);

  // 4. Send a signal that queue is not empty
  pthread_cond_signal(&queue->not_empty);

  // 5. Unlock
  pthread_mutex_unlock(&queue->lock);
}

// Get data unless queue is empty
size_t get_data(queue_t* queue) {
  size_t data;
  // 1. Lock
  pthread_mutex_lock(&queue->lock);

  // 2: Wait until queue is not empty
  while (queue->length == 0) {
    pthread_cond_wait(&queue->not_empty, &queue->lock);
  }

  // 3. Get Algorithm for Circular Array
  // 3.a. Get the data at index `get_index` and then modularly add `get_index`
  // by 1 so that it is within capacity at all times
  data = queue->data[queue->get_index];
  queue->get_index = (queue->get_index + 1) % queue->capacity;

  // 3.b. Decrease `length` by 1
  queue->length--;

  // 3.c. When `length` is zero, set `get_index` and `put_index` to 0
  if (queue->length == 0) {
    queue->get_index = 0;
    queue->put_index = 0;
  }

  printf("get data %zu from queue\n", data);

  // 4. Send a signal that queue is not full
  pthread_cond_signal(&queue->not_full);

  // 5. Unlock
  // Note: Signal before releasing the lock because the signal thread might
  // acquire the lock again.
  pthread_mutex_unlock(&queue->lock);

  // 6. Return data
  return data;
}

/** End **/
