#include <stdio.h>              // printf
#include <stdlib.h>             // free
#include <string.h>             // strtok_r, sprintf
#include <sys/wait.h>           // waitpid
#include <unistd.h>             // execv
#include <editline/readline.h>  // readline
#include <editline/history.h>   // add_history
#include <errno.h>              // errno, ERANGE, EINTR

#define MAX_ARGS 100

enum {
  SUCCESS,
  READLINE_EXIT,
  PARSE_ARGS_ERROR,
  EVALUATE_ERROR
}; // you can edit this or not use it at all.

// helper functions
int get_input(char** cmd); // saves input to cmd, returns READLINE_EXIT on EOT (ctrl+D), SUCCESS on success
int parse_args(char* cmd, char* args[]); // parses `cmd` into `args`, returns PARSE_ARGS_ERROR on failure, SUCCESS on success

/** WRITE YOUR CODE BELOW **/
// You can add more functions here if needed like `handle_builtin_commands` (i.e., exit) or `execute_command` (i.e., execvp)
static int shell_exit_status = EXIT_SUCCESS;

static void wait_for_all_children(void) {
  int status;

  while (waitpid(-1, &status, 0) > 0) {
  }
}

static int parse_exit_status(const char* text, int* status) {
  char* end_ptr;
  long value;

  errno = 0;
  value = strtol(text, &end_ptr, 10);
  if (end_ptr == text || *end_ptr != '\0') {
    return 0;
  }

  if (errno == ERANGE || value < 0 || value > 255) {
    *status = 255;
    return 1;
  }

  *status = (int)value;
  return 1;
}

static int handle_exit_command(char* args[]) {
  int status = EXIT_SUCCESS;

  if (args[1] == NULL) {
    shell_exit_status = EXIT_SUCCESS;
    wait_for_all_children();
    return READLINE_EXIT;
  }

  if (args[2] != NULL) {
    fprintf(stderr, "exit: too many arguments\n");
    return EVALUATE_ERROR;
  }

  if (!parse_exit_status(args[1], &status)) {
    fprintf(stderr, "exit: %s: invalid integer\n", args[1]);
    return EVALUATE_ERROR;
  }

  shell_exit_status = status;
  wait_for_all_children();
  return READLINE_EXIT;
}

static int execute_external_command(char* args[]) {
  pid_t pid = fork();
  int status;

  if (pid < 0) {
    perror("fork");
    return EVALUATE_ERROR;
  }

  if (pid == 0) {
    int execvp_error;
    int execv_error;

    execvp(args[0], args);
    execvp_error = errno;
    execv_error = execvp_error;

    if (strchr(args[0], '/') == NULL) {
      size_t length = strlen(args[0]) + 3;
      char path[length];

      snprintf(path, sizeof(path), "./%s", args[0]);
      execv(path, args);
      execv_error = errno;
    }

    if (execv_error == ENOENT) {
      fprintf(stderr, "%s: command not found\n", args[0]);
      _exit(127);
    }

    errno = execv_error;
    perror(args[0]);
    _exit(EXIT_FAILURE);
  }

  while (waitpid(pid, &status, 0) < 0) {
    if (errno == EINTR) {
      continue;
    }

    perror("waitpid");
    return EVALUATE_ERROR;
  }

  return SUCCESS;
}


int evaluate(char* args[]) {
  // In shell, there are two main types of commands: built-in commands (e.g., exit) and external commands (e.g., ls, echo, etc.).

  // Built-in commands are handled directly by the shell.
  // For this exercise, `exit` is the only built-in command we need to handle, which terminates the shell.
  // But for a future assignment, you need to implement more built-in commands.

  // External commmands are executed by creating a child process and using execvp to run the command in that child process.
  // Reminder: If execvp fails, you have to terminate the child process!

  // You have to handle both, and you can check if a command is built-in based on the program name (i.e., args[0]).

  if (args[0] == NULL) {
    return SUCCESS;
  }

  if (strcmp(args[0], "exit") == 0) {
    return handle_exit_command(args);
  }

  return execute_external_command(args);
}

int main() {
  int result = SUCCESS;

  do {
    char* cmd = NULL;

    result = get_input(&cmd);

    if (cmd == NULL)
      goto loop_end;

    char* args[MAX_ARGS];

    if (((result = parse_args(cmd, args)) != SUCCESS))
      goto loop_end;

    result = evaluate(args);
  loop_end:
    free(cmd);
  } while (result != READLINE_EXIT);

  return shell_exit_status;
}

// helper functions

// You have to free the memory allocated by readline
int get_input(char** cmd) {
  if ((*cmd = readline("$ ")) == NULL) {
    return READLINE_EXIT;
  }
  if (**cmd == '\0') {
    free(*cmd);
    *cmd = NULL;
  } else add_history(*cmd);
  return SUCCESS;
}

int parse_args(char* cmd, char* args[]) {
  char* save_ptr;
  char* ptr = strtok_r(cmd, " ", &save_ptr);
  int i = 0;

  while (ptr != NULL) {
    if (i >= MAX_ARGS - 1) {
      fprintf(stderr, "Error: Too many arguments\n");
      return PARSE_ARGS_ERROR;
    }
    args[i++] = ptr;
    ptr = strtok_r(NULL, " ", &save_ptr);
  }

  args[i] = NULL;
  return SUCCESS;
}
