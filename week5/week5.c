#include <stdio.h>              // printf
#include <stdlib.h>             // free
#include <string.h>             // strtok_r, sprintf
#include <sys/wait.h>           // waitpid
#include <unistd.h>             // execv
#include <editline/readline.h>  // readline
#include <editline/history.h>   // add_history

#define MAX_ARGS 100

enum { 
  READLINE_EXIT=-1,
  SUCCESS, 
  PARSE_ARGS_ERROR,
  EVALUATE_ERROR
}; // you can edit this or not use it at all.

// helper functions 
int get_input(char** cmd); // saves input to cmd, returns READLINE_EXIT on EOT (ctrl+D), SUCCESS on success
int parse_args(char* cmd, char* args[]); // parses `cmd` into `args`, returns PARSE_ARGS_ERROR on failure, SUCCESS on success

/** WRITE YOUR CODE BELOW **/
// You can add more functions here if needed like `handle_builtin_commands` (i.e., exit) or `execute_command` (i.e., execvp)


int evaluate(char* args[]) {
  // In shell, there are two main types of commands: built-in commands (e.g., exit) and external commands (e.g., ls, echo, etc.).
  
  // Built-in commands are handled directly by the shell. 
  // For this exercise, `exit` is the only built-in command we need to handle, which terminates the shell.
  // But for a future assignment, you need to implement more built-in commands.
  
  // External commmands are executed by creating a child process and using execvp to run the command in that child process.
  // Reminder: If execvp fails, you have to terminate the child process!

  // You have to handle both, and you can check if a command is built-in based on the program name (i.e., args[0]).

  args[0] = args[0]; // to avoid unused parameter warning; remove this when you write your actual code...
  return SUCCESS;
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
