#include "helper.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>
#include <sys/types.h>
#include <pwd.h>
#include <limits.h>
#include <ctype.h>


char* get_bin_dir() {
  char path[PATH_MAX + 1];
  ssize_t len = readlink("/proc/self/exe", path, PATH_MAX);

  if (len == -1) {
    perror("readlink");
    return NULL;
  }

  path[len] = '\0';

  char* mutable_path = strdup(path);
  if (mutable_path == NULL) {
    return NULL;
  }

  char* dir = dirname(mutable_path);
  char* result = strdup(dir);
  free(mutable_path);
  return result;
}

char* get_home_dir() {
  char* directory;
 
  struct passwd pw;
  char* buffer = malloc(16384);
  struct passwd *result = NULL;
  getpwuid_r(getuid(), &pw, buffer, 16384, &result);

  directory = result ? strdup(result->pw_dir) : NULL;
  
  free(buffer);
  return directory;
}


status_t interpret(Pipeline* pipeline,
                   Jobs* jobs,
                   char* cmd,
                   status_t last_status) {
  if (pipeline->num_commands == 0)
    return 0;

  status_t status;
  if (is_single_builtin_command(pipeline)) {
    status =
        run_single_builtin_command(pipeline->first_command, jobs, last_status);
  } else {
    status = run_pipeline(pipeline, jobs, cmd, last_status);
  }

  return status;
}

status_t evaluate(char** cmd, Jobs* jobs, status_t last_status) {
  Token tokens[MAX_TOKENS];
  tokens[0] = (Token){.type = TOKEN_END, .data = NULL};
  Input input = {0};
  if (lex(*cmd, tokens) != 0) {
    goto cleanup;
  }

  if (parse(tokens, &input, last_status) != 0) {
    goto cleanup;
  }
  
  Pipeline* pipeline = input.first_command_line;

  while (pipeline != NULL) {
    if (!pipeline_is_empty(pipeline)) {
      last_status = interpret(pipeline, jobs, *cmd, last_status);
    }

    Pipeline* next_pipeline = pipeline->next_command_line;
    pipeline = next_pipeline;
  }

cleanup:
  free_input(&input);
  free_tokens(tokens);
  free(*cmd);
  *cmd = NULL;
  return last_status;
}

bool line_is_empty(const char* line) {
  for (; *line != '\0'; line++) {
    if (!isspace((unsigned char)*line)) {
      return false;
    }
  }
  return true;
}