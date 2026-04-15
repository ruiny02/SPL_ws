#include <stdio.h>              // printf
#include <stdlib.h>             // free
#include <string.h>             // strcmp, strchr, snprintf
#include <sys/wait.h>           // waitpid
#include <unistd.h>             // execv, pipe, dup2, close
#include <editline/readline.h>  // readline
#include <editline/history.h>   // add_history
#include <errno.h>              // errno, ERANGE, EINTR
#include <fcntl.h>              // open

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

static int wait_for_pid(pid_t pid) {
  int status;

  while (waitpid(pid, &status, 0) < 0) {
    if (errno == EINTR) {
      continue;
    }

    perror("waitpid");
    return EVALUATE_ERROR;
  }

  return SUCCESS;
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

static void exec_program(char* args[]) {
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

static int execute_external_command(char* args[]) {
  pid_t pid = fork();

  if (pid < 0) {
    perror("fork");
    return EVALUATE_ERROR;
  }

  if (pid == 0) {
    exec_program(args);
  }

  return wait_for_pid(pid);
}

static int execute_redirection_command(char* args[], int operator_index) {
  int fd;
  int flags;
  int dup_target;
  pid_t pid;

  if (operator_index == 0 || args[operator_index + 1] == NULL || args[operator_index + 2] != NULL) {
    fprintf(stderr, "syntax error\n");
    return EVALUATE_ERROR;
  }

  if (strcmp(args[operator_index], "<") == 0) {
    flags = O_RDONLY;
    dup_target = STDIN_FILENO;
  } else if (strcmp(args[operator_index], ">") == 0) {
    flags = O_WRONLY | O_CREAT | O_TRUNC;
    dup_target = STDOUT_FILENO;
  } else {
    flags = O_WRONLY | O_CREAT | O_APPEND;
    dup_target = STDOUT_FILENO;
  }

  pid = fork();
  if (pid < 0) {
    perror("fork");
    return EVALUATE_ERROR;
  }

  if (pid == 0) {
    if (strcmp(args[operator_index], "<") == 0) {
      fd = open(args[operator_index + 1], flags);
    } else {
      fd = open(args[operator_index + 1], flags, 0666);
    }

    if (fd < 0) {
      perror(args[operator_index + 1]);
      _exit(EXIT_FAILURE);
    }

    if (dup2(fd, dup_target) < 0) {
      perror("dup2");
      close(fd);
      _exit(EXIT_FAILURE);
    }

    close(fd);
    args[operator_index] = NULL;
    exec_program(args);
  }

  return wait_for_pid(pid);
}

static int execute_pipe_command(char* args[], int operator_index) {
  int fd[2];
  pid_t left_pid;
  pid_t right_pid;
  char* rhs[MAX_ARGS];
  int rhs_index = 0;

  if (operator_index == 0 || args[operator_index + 1] == NULL) {
    fprintf(stderr, "syntax error\n");
    return EVALUATE_ERROR;
  }

  for (int i = operator_index + 1; args[i] != NULL; i++) {
    rhs[rhs_index++] = args[i];
  }
  rhs[rhs_index] = NULL;
  args[operator_index] = NULL;

  if (pipe(fd) < 0) {
    perror("pipe");
    return EVALUATE_ERROR;
  }

  left_pid = fork();
  if (left_pid < 0) {
    perror("fork");
    close(fd[0]);
    close(fd[1]);
    return EVALUATE_ERROR;
  }

  if (left_pid == 0) {
    if (dup2(fd[1], STDOUT_FILENO) < 0) {
      perror("dup2");
      _exit(EXIT_FAILURE);
    }

    close(fd[0]);
    close(fd[1]);
    exec_program(args);
  }

  right_pid = fork();
  if (right_pid < 0) {
    perror("fork");
    close(fd[0]);
    close(fd[1]);
    wait_for_pid(left_pid);
    return EVALUATE_ERROR;
  }

  if (right_pid == 0) {
    if (dup2(fd[0], STDIN_FILENO) < 0) {
      perror("dup2");
      _exit(EXIT_FAILURE);
    }

    close(fd[0]);
    close(fd[1]);
    exec_program(rhs);
  }

  close(fd[0]);
  close(fd[1]);

  if (wait_for_pid(left_pid) != SUCCESS) {
    wait_for_pid(right_pid);
    return EVALUATE_ERROR;
  }

  return wait_for_pid(right_pid);
}

static int find_operator_index(char* args[]) {
  int operator_index = -1;

  for (int i = 0; args[i] != NULL; i++) {
    if (strcmp(args[i], "<") == 0 || strcmp(args[i], ">") == 0 ||
        strcmp(args[i], ">>") == 0 || strcmp(args[i], "|") == 0) {
      if (operator_index >= 0) {
        fprintf(stderr, "syntax error\n");
        return -2;
      }
      operator_index = i;
    }
  }

  return operator_index;
}

int evaluate(char* args[]) {
  int operator_index;

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

  operator_index = find_operator_index(args);
  if (operator_index == -2) {
    return EVALUATE_ERROR;
  }

  if (operator_index < 0 && strcmp(args[0], "exit") == 0) {
    return handle_exit_command(args);
  }

  if (operator_index < 0) {
    return execute_external_command(args);
  }

  if (strcmp(args[operator_index], "|") == 0) {
    return execute_pipe_command(args, operator_index);
  }

  return execute_redirection_command(args, operator_index);
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
  int i = 0;
  char* cursor = cmd;

  while (*cursor != '\0') {
    while (*cursor == ' ') {
      *cursor = '\0';
      cursor++;
    }

    if (*cursor == '\0') {
      break;
    }

    if (i >= MAX_ARGS - 1) {
      fprintf(stderr, "Error: Too many arguments\n");
      return PARSE_ARGS_ERROR;
    }

    if (*cursor == '<') {
      args[i++] = "<";
      cursor++;
      continue;
    }

    if (*cursor == '|') {
      args[i++] = "|";
      cursor++;
      continue;
    }

    if (*cursor == '>') {
      if (*(cursor + 1) == '>') {
        args[i++] = ">>";
        cursor += 2;
      } else {
        args[i++] = ">";
        cursor++;
      }
      continue;
    }

    args[i++] = cursor;

    while (*cursor != '\0' && *cursor != ' ' && *cursor != '<' && *cursor != '>' && *cursor != '|') {
      cursor++;
    }

    if (*cursor == '\0') {
      break;
    }

    if (*cursor == ' ') {
      *cursor = '\0';
      cursor++;
      continue;
    }

    if (i >= MAX_ARGS - 1) {
      fprintf(stderr, "Error: Too many arguments\n");
      return PARSE_ARGS_ERROR;
    }

    if (*cursor == '<') {
      *cursor = '\0';
      args[i++] = "<";
      cursor++;
      continue;
    }

    if (*cursor == '|') {
      *cursor = '\0';
      args[i++] = "|";
      cursor++;
      continue;
    }

    if (*(cursor + 1) == '>') {
      *cursor = '\0';
      args[i++] = ">>";
      cursor += 2;
      continue;
    }

    *cursor = '\0';
    args[i++] = ">";
    cursor++;
  }

  args[i] = NULL;
  return SUCCESS;
}
