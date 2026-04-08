#define _POSIX_C_SOURCE 200809L

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

volatile sig_atomic_t parent_pending_acks = 0; // int
volatile sig_atomic_t parent_pending_sigalrm = 0; // bool
volatile sig_atomic_t child_pending_signals = 0; // int
volatile sig_atomic_t child_pending_termination = 0; // bool

pid_t other_process_pid;

void set_signal(struct sigaction action, int signal, void (*handler)(int)) {
    action.sa_handler = handler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;

    if (sigaction(signal, &action, NULL) < 0) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
}

void parent_ack_handler(int sig) {
    (void)sig;
    parent_pending_acks++;
}

void parent_alarm_handler(int sig) {
    (void)sig;
    parent_pending_sigalrm = 1;
}

void child_signal_handler(int sig) {
    (void)sig;
    child_pending_signals++;
}

void child_sigint_handler(int sig) {
    (void)sig;
    child_pending_termination = 1;
}


void parent_handle_pending_signals(long* num_acks_received, long num_signal_to_send) {
    if (parent_pending_acks > 0) {
        *num_acks_received += parent_pending_acks;
        if (*num_acks_received > num_signal_to_send) {
            *num_acks_received = num_signal_to_send;
        }
        parent_pending_acks = 0;
    }

    if (*num_acks_received == num_signal_to_send) {
        printf("all signals have been sent!\n");
        kill(other_process_pid, SIGINT);
        exit(EXIT_SUCCESS);
    }

    if (parent_pending_sigalrm) {
        long remaining_signals = num_signal_to_send - *num_acks_received;

        parent_pending_sigalrm = 0;
        printf("sender: total remaining signal(s): %ld\n", remaining_signals);
        if (remaining_signals > 0) {
            kill(other_process_pid, SIGUSR1);
        }
        alarm(1);
    }
}


void child_handle_pending_signals(long* num_signal_received) {
    while (child_pending_signals > 0) {
        child_pending_signals--;
        (*num_signal_received)++;
        printf("receiver: received signal #%ld and sending ack\n", *num_signal_received);
        kill(other_process_pid, SIGUSR1);
    }

    if (child_pending_termination) {
        printf("receiver: received %ld signals\n", *num_signal_received);
        exit(EXIT_SUCCESS);
    }
}


int main(int argc, char* argv[]) {
  sigset_t blocked_signals;
  sigset_t wait_mask;

  setvbuf(stdout, NULL, _IONBF, 0);

  if (argc != 2) {
    fprintf(stderr, "Usage: %s <number of signals to send>\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  long num_signal_to_send = strtol(argv[1], NULL, 10);

  if (num_signal_to_send <= 0) {
    fprintf(stderr, "Invalid number of signals to send\n");
    exit(EXIT_FAILURE);
  }

  printf("number of signals to send: %ld\n", num_signal_to_send);

  struct sigaction action;
  memset(&action, 0, sizeof(action));
  sigemptyset(&blocked_signals);
  sigaddset(&blocked_signals, SIGUSR1);
  sigaddset(&blocked_signals, SIGALRM);
  sigaddset(&blocked_signals, SIGINT);
  sigprocmask(SIG_BLOCK, &blocked_signals, NULL);
  sigemptyset(&wait_mask);

  switch (other_process_pid = fork()) {
    case -1:
      perror("fork failed");
      exit(EXIT_FAILURE);
    case 0: // Child process (receiver)
      other_process_pid = getppid(); // Note: PID is now parent process (sender)!

      long num_signal_received = 0;
      /* TODO: set up handlers */

      // set up child_signal_handler signal handler (i.e. SIGUSR1)
      // set up terminate_handler signal handler (i.e. SIGINT)
      set_signal(action, SIGUSR1, child_signal_handler);
      set_signal(action, SIGINT, child_sigint_handler);

      while (1) {
        sigsuspend(&wait_mask);
        child_handle_pending_signals(&num_signal_received);
      }
      break;
    default: // Parent process (sender)
      // other_process_pid = child id
      long num_acks_received = 0;

      /* TODO: set up handlers */
      // set up parent_ack_handler signal handler (i.e. SIGUSR1)
      // signal  parent_alarm_handler signal handler (i.e. SIGALRM)
      set_signal(action, SIGUSR1, parent_ack_handler);
      set_signal(action, SIGALRM, parent_alarm_handler);

      // send back remaining signals after 1s
      // if sender doesn't receive all acks using alarm
      alarm(1);

      while (1) {
        sigsuspend(&wait_mask);
        parent_handle_pending_signals(&num_acks_received, num_signal_to_send);
      }
      break;
  }

  return EXIT_SUCCESS;
}
