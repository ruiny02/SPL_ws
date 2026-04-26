#ifndef BUILTIN_COMMANDS_H
#define BUILTIN_COMMANDS_H
#include <string.h>
#include "jobs.h"
#include "parser.h"

status_t run_cd(Command* command);
status_t run_exit(Command* command, Jobs* jobs, status_t last_status);
status_t run_pwd(Command* command);
status_t run_jobs(Jobs* jobs);

status_t run_fg(Command* command, Jobs* jobs);
status_t run_bg(Command* command, Jobs* jobs);
status_t run_builtin_command(Command* command,
                             Jobs* jobs,
                             status_t last_status);
#endif
