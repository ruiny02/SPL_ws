#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>


volatile sig_atomic_t should_exit = 0;
int error_code = 0;

#define ACK_TYPE 1000000ULL  // This can be used to separate ack messages from normal messages

#define ERASE_CURRENT_LINE "\33[2K\r" // This is a special string (ANSI escape code) that erases the current line in terminal

typedef struct {
  long msgtype;  // hint: receiver's user_id
  int sender_id;
  char text[512];
} msgbuf;

typedef struct {
  long msgtype;  // hint: ACK_TYPE + id of sender who sent original message
  char timestamp[512];
} msgbuf_ack;

void get_input(char** buf, size_t* size, char* unformatted_prompt, ...);
int get_id(char* text);
void sigint_handler(int sig);
void setup_handler();

int main() {
  setup_handler();
  int pid;
  int user_id = get_id("User ID: ");
  if (should_exit) return error_code;
  int receiver_id = get_id("Receiver ID: ");
  if (should_exit) return error_code;

  key_t key;
  int qid;
  // TODO: create key by "getting it"

  // TODO: create message queue by "getting it"

  switch (pid = fork()) {
    case -1:
      perror("fork");
      break;
    case 0:
      // The child is the receiver
      while (should_exit == 0) {
        msgbuf buf;

        // TODO: Receive a message with IPC_NOWAIT
        if (???) {
          msgbuf_ack ack;
          ack.msgtype = ACK_TYPE + receiver_id;

          printf(ERASE_CURRENT_LINE "User %d:\t%s", buf.sender_id,
                 buf.text);  // This line erases the current line and prints the
                             // received message
          printf("User %d:\t", user_id);
          fflush(stdout);

          // TODO: Send an ack message (current timestamp) to the sender
        }

        // TODO: Receive ack message using IPC_NOWAIT and store the messeg in
        // `read_time`
        msgbuf_ack read_time;
        if (???) {
          // NOTE: The printf statement below clears the current line and then
          // sends a message This printf only works properly when the sender and
          // receiver are running at the same itme Otherwise, it will clear the
          // input from the user's perspective, but not the actual stdin
          printf(ERASE_CURRENT_LINE "User %d read message at %s\n", receiver_id,
                 read_time.timestamp);
          printf("User %d:\t", user_id);
          fflush(stdout);
        }
      }
      _exit(error_code);
    default:
      // The parent is the sender
      while (should_exit == 0) {
        /** TODO:
         * 1. Get regular message from stdin (implemented)
         * 2. If the message is "quit", exit both parent and child (use
         * SIGINT)
         * 3. Send normal message
         */

        msgbuf buf;
        buf.msgtype = receiver_id;
        char* line = NULL;
        size_t len = 0;
        get_input(&line, &len, "User %d: ", user_id);
        if (should_exit) {
          free(line);
          break;
        }

        if (strncmp(line, "quit\n", 5) == 0) {
          kill(pid, SIGINT);
          waitpid(pid, NULL, 0);
          free(line);
          break;
        }

        // Your code

        free(line);
      }
      // TODO: Remove the message queue

      break;
  }


  return error_code;
}

// Helper functions
void get_input(char** buf, size_t* size, char* unformatted_prompt, ...) {
  va_list args;
  va_start(args, unformatted_prompt);
  char* prompt;
  if (vasprintf(&prompt, unformatted_prompt, args) == -1) {
    perror("vasprintf");
    error_code = 1;
    return;
  }
  va_end(args);

  fputs(prompt, stdout);

  if (getline(buf, size, stdin) == -1) {
    should_exit = 1;
    if (errno != 0) {
      error_code = 1;
    }    
  }
}

int get_id(char* text) {
  char* input = NULL;
  size_t len;

  get_input(&input, &len, text);

  int id = 0;
  if (input != NULL && !should_exit)
    id = strtoull(input, NULL, 10);

  free(input);
  return id;
}

void sigint_handler(int sig) {
  if (sig == SIGINT) {
    should_exit = 1;
  }
}

void setup_handler() {
  struct sigaction action;
  action.sa_handler = sigint_handler;
  sigemptyset(&action.sa_mask);
  action.sa_flags = 0;
  sigaction(SIGINT, &action, NULL);
}
