#include "helper.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

extern volatile sig_atomic_t pending_sigchld;
static Jobs* jobs_global = NULL;

void set_foreground_job(Job* job, Jobs* jobs) {
  jobs->table[job->id] = NULL;
  if (job->state != JOB_FOREGROUND && job->id != 0) {
    jobs->num_jobs--;
    update_highest_job_id(jobs);
  }

  job->state = JOB_FOREGROUND;
  job->id = 0;
  jobs->table[0] = job;
  set_job_process_state(job, PROCESS_RUNNING);
}

void remove_foreground_job(Jobs* jobs, bool should_free) {
  if (should_free)
    free_job(jobs->table[0]);
  jobs->table[0] = NULL;
}

void add_job_to_table(Job* job, Jobs* jobs) {
  job->id = ++jobs->highest_job_id;
  jobs->table[job->id] = job;
  jobs->num_jobs++;
}

void remove_job_from_table(Job* job, Jobs* jobs) {
  if (jobs == NULL) {
    jobs = jobs_global;
  }

  jobs->table[job->id] = NULL;
  if (job->id > 0)
    jobs->num_jobs--;

  update_highest_job_id(jobs);
}

bool is_job_table_empty(Jobs* jobs) {
  return jobs->num_jobs == 0;
}

size_t get_highest_stopped_job_id(Jobs* jobs) {
  for (size_t i = jobs->highest_job_id; i > 0; i--) {
    Job* job = jobs->table[i];
    if (job->state == JOB_STOPPED)
      return i;
  }
  return 0;
}

void update_highest_job_id(Jobs* jobs) {
  for (; jobs->highest_job_id > 0; jobs->highest_job_id--) {
    Job* job = jobs->table[jobs->highest_job_id];

    if (job == NULL)
      continue;

    if (job->state == JOB_BACKGROUND || job->state == JOB_STOPPED)
      return;
  }
}

status_t handle_job(Job* job, Jobs* jobs) {
  switch (job->state) {
    case JOB_FOREGROUND:
      wait_for_processes(job);

      if (job->state == JOB_STOPPED) {
        add_job_to_table(job, jobs);
        printf("pa2: Job %ld has stopped\n", job->id);
      }

      tcsetpgrp(STDIN_FILENO, getpgid(0));

      status_t status = job->last_process->info.si_status;

      remove_foreground_job(jobs, job->state == JOB_DONE);
      return status;
    case JOB_BACKGROUND:
      add_job_to_table(job, jobs);
      return 0;
    default:
      fprintf(stderr, "Failed to create job\n");
      return 1;
  }
}

Process* wait_for_job_process(Job* job) {
  siginfo_t info;
  waitid(P_PGID, job->pgid, &info, WEXITED | WSTOPPED | WCONTINUED | WNOHANG);

  print_process(job->first_process);

  Process* process = get_process(job->first_process, info.si_pid);

  if (process != NULL) {
    process->is_running = false;
    process->info = info;
  }

  return process;
}

Job* find_job_by_pid(Jobs* jobs, pid_t pgid) {
  if (jobs == NULL) {
    jobs = jobs_global;
  }

  for (size_t i = 0; i < MAX_JOBS; i++) {
    Job* job = jobs->table[i];
    if (job == NULL) {
      continue;
    }

    for (Process* process = job->first_process; process != NULL;
         process = process->next_process) {
      if (process->pid == pgid) {
        return job;
      }
    }
  }
  return NULL;
}

Job* find_job_by_id(Jobs* jobs, uint64_t id) {
  if (jobs == NULL)
    jobs = jobs_global;

  if (id > MAX_JOBS)
    return NULL;

  return jobs->table[id];
}

Process* wait_for_any_process(Job** job) {
  siginfo_t info;
  if (waitid(P_ALL, 0, &info, WEXITED | WSTOPPED | WCONTINUED | WNOHANG) ==
          -1 ||
      info.si_pid == 0) {
    return NULL;
  }

  *job = find_job_by_pid(NULL, info.si_pid);

  if (*job == NULL) {
    fprintf(stderr, "pa2: No job found for PID %d\n", info.si_pid);
    return NULL;
  }

  Process* process = get_process((*job)->first_process, info.si_pid);

  if (process != NULL) {
    process->is_running = false;
    process->info = info;
    set_job_state(*job, process);
  }
  return process;
}

JobState set_job_state(Job* job, Process* process) {
  if (has_completed(job)) {
    return JOB_DONE;
  }

  if (process == NULL) {
    return job->state;
  }

  if (process->info.si_code == CLD_STOPPED) {
    job->state = JOB_STOPPED;
  } else if (process->info.si_code == CLD_CONTINUED) {
    if (job->state != JOB_FOREGROUND) {
      job->state = JOB_BACKGROUND;
    }
  }
  return job->state;
}

JobState wait_for_processes(Job* job) {
  JobState state = job->state;
  Job** job_loc = &jobs_global->table[job->id];

  sigset_t block_mask, suspend_mask;
  sigemptyset(&block_mask);
  sigaddset(&block_mask, SIGCHLD);
  sigprocmask(SIG_BLOCK, &block_mask, &suspend_mask);

  while (true) {
    if (pending_sigchld) handle_pending_sigchld();            
    
    if (*job_loc == NULL || has_completed(*job_loc) || has_stopped(*job_loc)) {
      state = (*job_loc == NULL) ? JOB_DONE : (*job_loc)->state;
      break;
    }

    sigsuspend(&suspend_mask);
  } 

  sigprocmask(SIG_SETMASK, &suspend_mask, NULL);
  return state;  
}

void set_jobs_global(Jobs* jobs) {
  jobs_global = jobs;
}