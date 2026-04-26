

#ifndef JOBS_H
#define JOBS_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "job.h"
#define MAX_JOBS 8192

typedef int32_t status_t;

typedef struct {
  Job* table[MAX_JOBS + 1];
  uint64_t num_jobs;
  uint64_t highest_job_id;
} Jobs;

void set_foreground_job(Job* job, Jobs* jobs);
void remove_foreground_job(Jobs* jobs, bool should_free);

void add_job_to_table(Job* job, Jobs* jobs);
void remove_job_from_table(Job* job, Jobs* jobs);

bool is_job_table_empty(Jobs* jobs);

size_t get_highest_stopped_job_id(Jobs* jobs);

void update_highest_job_id(Jobs* jobs);
status_t handle_job(Job* job, Jobs* jobs);

void set_jobs_global(Jobs* jobs);
// Process* wait_for_any_process();void remove_foreground_job(Jobs* jobs, bool
// should_free)
Process* wait_for_any_process(Job** job);
JobState set_job_state(Job* job, Process* process);

Job* find_job_by_pid(Jobs* jobs, pid_t pgid);
Job* find_job_by_id(Jobs* jobs, uint64_t id);

status_t handle_pending_sigchld();
#endif