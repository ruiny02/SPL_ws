#include "builtin_commands.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

status_t run_builtin_command(Command* command,
                             Jobs* jobs,
                             status_t last_status) {
  if (strncmp(command->args[0], "exit", 5) == 0) {
    run_exit(command, jobs, last_status);
  } else if (strncmp(command->args[0], "cd", 3) == 0) {
    return run_cd(command);
  } else if (strncmp(command->args[0], "pwd", 4) == 0) {
    return run_pwd();
  } else if (strncmp(command->args[0], "jobs", 5) == 0) {
    return run_jobs(jobs);
  } else if (strncmp(command->args[0], "fg", 3) == 0) {
    return run_fg(command, jobs);
  } else if (strncmp(command->args[0], "bg", 3) == 0) {
    return run_bg(command, jobs);
  } else {
    fprintf(stderr, "%s: command not found\n", command->args[0]);
  }
  return 1;
}

status_t run_jobs(Jobs* jobs) {
  if (is_job_table_empty(jobs)) {
    fprintf(stderr, "jobs: There are no jobs\n");
    return 1;
  }

  puts("Job\tGroup\tState\tCommand");
  for (uint64_t i = jobs->highest_job_id; i > 0; i--) {
    Job* job = jobs->table[i];
    if (job == NULL)
      continue;

    if (job->state != JOB_NULL)
      printf("%ld\t%d\t%s\t%s\n", job->id, job->pgid,
             job->state == JOB_BACKGROUND ? "running" : "stopped",
             job->associated_command);
    else {      
      fprintf(stderr, "warning: Job %ld is NULL\n", job->id);
      return 1;
    }
  }
  return 0;
}

Job* get_job_to_move(Command* command, int64_t else_value) {
  Job* job = NULL;

  if (command->num_args > 1) {
    char* ptr;

    if (command->args[1][0] != '%') {
      int64_t pgid = strtoll(command->args[1], &ptr, 10);
      if (*ptr != '\0') {
        fprintf(stderr, "%s: %s: invalid integer\n", command->args[0], command->args[1]);
        return NULL;
      }
      job = find_job_by_pid(NULL, pgid);
    } else {
      int64_t job_id = strtoll(command->args[1] + 1, &ptr, 10);
      if (*ptr != '\0') {
        fprintf(stderr, "%s: %s: invalid integer\n", command->args[0], command->args[1]);
        return NULL;
      }
      job = find_job_by_id(NULL, job_id);
    }
  } else {
    job = find_job_by_id(NULL, else_value);
  }

  if (job == NULL) {
    if (command->num_args > 1) {
      fprintf(stderr, "%s: No suitable job: %s\n", command->args[0], command->args[1]);
    } else {
      fprintf(stderr, "%s: There are no suitable jobs\n", command->args[0]);
    }
    return NULL;
  }
  return job;
}

status_t run_fg(Command* command, Jobs* jobs) {
  remove_foreground_job(jobs, true);

  if (is_job_table_empty(jobs)) {
    fprintf(stderr, "%s: There are no suitable jobs\n", command->args[0]);
    return 2;
  }

  Job* job = get_job_to_move(command, jobs->highest_job_id);

  if (job == NULL) {
    return 2;
  }

  printf("Send job %ld (%s) to foreground\n", job->id, job->associated_command);

  set_foreground_job(job, jobs);
  tcsetpgrp(STDIN_FILENO, job->pgid);
  kill(-job->pgid, SIGCONT);

  return handle_job(job, jobs);
}

status_t run_bg(Command* command, Jobs* jobs) {
  if (is_job_table_empty(jobs)) {
    fprintf(stderr, "bg: There are no suitable jobs\n");
    return 1;
  }

  Job* job = get_job_to_move(command, get_highest_stopped_job_id(jobs));
  if (job == NULL) {
    return 2;
  }

  printf("Send job %ld (%s) to background\n", job->id, job->associated_command);

  set_job_process_state(job, PROCESS_RUNNING);
  job->state = JOB_BACKGROUND;
  kill(-job->pgid, SIGCONT);
  return 0;
}
