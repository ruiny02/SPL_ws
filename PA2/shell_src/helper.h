#ifndef HELPER
#define HELPER

#include <stdint.h>
#include <stdbool.h>

typedef int32_t status_t;

#include "builtin_commands.h"
#include "job.h"
#include "jobs.h"
#include "parser.h"

char* get_bin_dir();
char* get_home_dir();

status_t interpret(Pipeline* pipeline, Jobs* jobs,
                   char* cmd, status_t last_status);
status_t evaluate(char** cmd, Jobs* jobs, status_t last_status);
bool line_is_empty(const char* line);

status_t run_pipeline(Pipeline* pipeline,
                      Jobs* jobs,
                      char* cmd,
                      status_t last_status);

status_t run_single_builtin_command(Command* command,
                                    Jobs* jobs,
                                    status_t last_status);
#endif