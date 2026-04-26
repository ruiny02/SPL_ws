#include "helper.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

Process* default_process() {
  Process* process = malloc(sizeof *process);
  *process = (Process){.pid = -1,
                       .info = {},
                       .is_running = false,
                       .command = NULL,
                       .stdin_fd = -1,
                       .stdout_fd = -1,
                       .next_process = NULL};
  return process;
}

void free_process(Process* process) {
  free(process);
}

Process* get_process(Process* process, pid_t pid) {
  Process* current_process = process;

  while (current_process != NULL) {
    if (current_process->pid == pid) {
      return current_process;
    }
    current_process = current_process->next_process;
  }

  return NULL;
}

const char* si_code_to_string(int si_code) {
  switch (si_code) {
    case CLD_EXITED:
      return "CLD_EXITED";
    case CLD_KILLED:
      return "CLD_KILLED";
    case CLD_DUMPED:
      return "CLD_DUMPED";
    case CLD_STOPPED:
      return "CLD_STOPPED";
    case CLD_CONTINUED:
      return "CLD_CONTINUED";
    default:
      return "UNKNOWN";
  }
}

char* get_info(siginfo_t info) {
  char* info_str = malloc(256);
  snprintf(info_str, 256, "si_pid: %d, si_code: %s, si_status: %d", info.si_pid,
           si_code_to_string(info.si_code), info.si_status);
  return info_str;
}

void print_process(Process* process) {
  if (process == NULL) {
    printf("NULL\n");
    return;
  }

  char* info_str = get_info(process->info);

  printf(
      "Process(pid: %d,\tinfo: (%s),\tis_running: %s,\tcommand: "
      "%s,\n\tstdin_fd: %d,\tstdout_fd: %d,\tnext_process: %p)\n",
      process->pid, info_str, process->is_running ? "true" : "false",
      process->command ? process->command->args[0] : "NULL", process->stdin_fd,
      process->stdout_fd, (void*)process->next_process);

  free(info_str);
}

Job* default_job() {
  Job* job = malloc(sizeof *job);
  *job = (Job){.state = JOB_NULL,
               .id = 0,
               .num_processes = 0,
               .pgid = -1,
               .first_process = NULL,
               .last_process = NULL};

  return job;
}

void free_job(Job* job) {
  if (job == NULL) {
    return;
  }

  Process* current_process = job->first_process;
  while (current_process != NULL) {
    Process* next_process = current_process->next_process;
    free_process(current_process);
    current_process = next_process;
  }
  free(job->associated_command);
  free(job);
}

bool is_job_null(Job* job) {
  return job == NULL || job->state == JOB_NULL;
}

bool has_proceses_terminated(Process* process) {
  if (process == NULL || process->info.si_code == CLD_EXITED ||
      process->info.si_code == CLD_KILLED ||
      process->info.si_code == CLD_DUMPED) {
    return true;
  }
  return false;
}

bool has_completed(Job* job) {
  if (job == NULL) {
    return true;
  }

  if (job->state == JOB_DONE) {
    return true;
  }

  for (Process* current_process = job->first_process; current_process != NULL;
       current_process = current_process->next_process) {
    if (current_process->is_running ||
        !has_proceses_terminated(current_process)) {
      return false;
    }
  }
  job->state = JOB_DONE;
  return true;
}

bool has_stopped(Job* job) {
  return job->state == JOB_STOPPED;
}

const char* job_state_to_string(JobState state) {
  switch (state) {
    case JOB_NULL:
      return "NULL";
    case JOB_FOREGROUND:
      return "FOREGROUND";
    case JOB_BACKGROUND:
      return "BACKGROUND";
    case JOB_STOPPED:
      return "STOPPED";
    case JOB_DONE:
      return "DONE";
    default:
      return "UNKNOWN";
  }
}

void set_job_process_state(Job* job, ProcessState state) {
  for (Process* current_process = job->first_process; current_process != NULL;
       current_process = current_process->next_process) {
    current_process->is_running = (state == PROCESS_RUNNING);
  }
}

void print_job(Job* job) {
  if (job == NULL) {
    printf("NULL\n");
    return;
  }

  printf(
      "Job(id: %ld,\tstate: %s,\tnum_processes: %ld,\tpgid: %d,\n\t"
      "first_process: %p,\tlast_process: %p,\tassociated_command: %s)\n",
      job->id, job_state_to_string(job->state), job->num_processes, job->pgid,
      (void*)job->first_process, (void*)job->last_process,
      job->associated_command ? job->associated_command : "NULL");
}