#ifndef JOB_H
#define JOB_H

#include <signal.h>
#include "parser.h"


typedef struct Process {
  pid_t pid;
  siginfo_t info;
  bool is_running;
  Command* command;
  int stdin_fd;
  int stdout_fd;
  struct Process* next_process;
} Process;

Process* default_process();
Process* get_process(Process* process, pid_t pid);
void print_process(Process* process);
void free_process(Process* process);

typedef enum {
  JOB_NULL,
  JOB_FOREGROUND,
  JOB_BACKGROUND,
  JOB_STOPPED,
  JOB_DONE
} JobState;

const char* job_state_to_string(JobState state);

typedef struct {
  JobState state;
  uint64_t id;
  uint64_t num_processes;
  pid_t pgid;
  Process* first_process;
  Process* last_process;
  char* associated_command;
} Job;

void print_job(Job* job);

typedef enum {
  PROCESS_NOT_RUNNING,
  PROCESS_RUNNING,
} ProcessState;

Job* default_job();
void free_job(Job* job);

bool is_job_null(Job* job);
bool has_completed(Job* job);
bool has_stopped(Job* job);
JobState wait_for_processes(Job* job);
void set_job_process_state(Job* job, ProcessState state);
#endif