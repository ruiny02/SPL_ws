#include <editline/history.h>
#include <editline/readline.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_ARGS 100

enum {
    SUCCESS,
    READLINE_EXIT,
    PARSE_ARGS_ERROR,
    EVALUATE_ERROR
};

static int shell_exit_status = EXIT_SUCCESS;
static int last_command_status = EXIT_SUCCESS;

/* Read one command line from the interactive prompt. */
static int read_command(char **cmd) {
    if ((*cmd = readline("$ ")) == NULL) {
        return READLINE_EXIT;
    }
    if (**cmd == '\0') {
        free(*cmd);
        *cmd = NULL;
    } else {
        add_history(*cmd);
    }
    return SUCCESS;
}

/* Split a command line into argv-style tokens. */
static int split_args(char *cmd, char *args[]) {
    char *save_ptr;
    char *token = strtok_r(cmd, " ", &save_ptr);
    int i = 0;

    while (token != NULL) {
        if (i >= MAX_ARGS - 1) {
            fprintf(stderr, "Error: Too many arguments\n");
            return PARSE_ARGS_ERROR;
        }
        args[i++] = token;
        token = strtok_r(NULL, " ", &save_ptr);
    }

    args[i] = NULL;
    return SUCCESS;
}

/* Parse the optional exit status for the built-in exit command. */
static int parse_exit_code(const char *text, int *status) {
    char *end_ptr;
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

/* Reap any children that outlive the shell loop. */
static void reap_children(void) {
    int status;

    while (waitpid(-1, &status, 0) > 0) {
    }
}

/* Run the built-in exit command and decide the shell status. */
static int run_exit(char *args[]) {
    int status = EXIT_SUCCESS;

    if (args[1] == NULL) {
        shell_exit_status = EXIT_SUCCESS;
        reap_children();
        return READLINE_EXIT;
    }
    if (args[2] != NULL) {
        fprintf(stderr, "exit: too many arguments\n");
        return EVALUATE_ERROR;
    }
    if (!parse_exit_code(args[1], &status)) {
        fprintf(stderr, "exit: %s: invalid integer\n", args[1]);
        return EVALUATE_ERROR;
    }

    shell_exit_status = status;
    reap_children();
    return READLINE_EXIT;
}

/* Fork and exec an external command in its own process group. */
static int run_external(char *args[]) {
    pid_t pid = fork();
    int status;

    if (pid < 0) {
        perror("fork");
        return EVALUATE_ERROR;
    }

    if (pid == 0) {
        setpgid(0, 0);
        execvp(args[0], args);
        if (errno == ENOENT) {
            fprintf(stderr, "%s: command not found\n", args[0]);
            _exit(127);
        }
        perror(args[0]);
        _exit(EXIT_FAILURE);
    }

    setpgid(pid, pid);

    while (waitpid(pid, &status, 0) < 0) {
        if (errno == EINTR) {
            continue;
        }
        perror("waitpid");
        return EVALUATE_ERROR;
    }

    if (WIFEXITED(status)) {
        last_command_status = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        last_command_status = 128 + WTERMSIG(status);
    }

    return SUCCESS;
}

/* Dispatch one parsed command line. */
static int eval_line(char *args[]) {
    if (args[0] == NULL) {
        return SUCCESS;
    }
    if (strcmp(args[0], "exit") == 0) {
        return run_exit(args);
    }
    return run_external(args);
}

/* Drive the minimal interactive shell used by PA2. */
int main(void) {
    int result = SUCCESS;

    do {
        char *cmd = NULL;
        char *args[MAX_ARGS];

        result = read_command(&cmd);
        if (cmd == NULL) {
            free(cmd);
            continue;
        }
        if (result == READLINE_EXIT) {
            free(cmd);
            break;
        }
        if ((result = split_args(cmd, args)) == SUCCESS) {
            result = eval_line(args);
        }
        free(cmd);
    } while (result != READLINE_EXIT);

    return shell_exit_status;
}
