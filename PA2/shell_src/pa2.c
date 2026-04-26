#include <ctype.h>
#include <editline/readline.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "helper.h"

volatile sig_atomic_t pending_sigchld = 0;
static volatile sig_atomic_t pending_sigint = 0;

// TODO: set pending_sigint
void sigint_handler(int signum) {
}

// TODO: set pending_sigchld
void sigchld_handler(int signum) {
}

// TODO: Handle sigint by writing "\n" to stdout and resetting pending_sigint
// Return code: 130
status_t handle_pending_sigint() {
}


/**
 * TODO: Check for any completed processes
 * Return status does not matter for sigchld (Technically should be the exit status of the completed job if there is one, but for this shell, the return value is not used, so you can set this to anything). 
 *
 * Relevant functions:
 * - Process* wait_for_any_process(Job** job): checks if there are any completed processes in the passed job pointer, otherwise returns NULL.
 * - JobState set_job_state(Job* job, Process* process): updates the state of the job based on the process info. Returns the updated state of the job.
 * - bool has_completed(Job* job): checks if all processes in the job have completed.
 * - void remove_job_from_table(Job* job, Jobs*[Nullable] jobs); removes the job from the job table and decrements the number of jobs. If jobs is NULL, use the global jobs variable.
 * - void free_job(Job* job): frees the memory allocated for the job and its processes.  
 **/

status_t handle_pending_sigchld() {
  status_t status = -1;  
  Process* process = NULL;
  Job* job = NULL;

  do {
    // TODO: Reset pending_sigchld, and check for completed processes until there are no more.
  } while (pending_sigchld);  
  return status;
}

status_t setup_pipe(Process* process, int pipefd[2]);
status_t setup_redirection(Process* process);
status_t dup2_stdio(Process* process);

// TODO: Implement the built-in exit command
status_t run_exit(Command* command, Jobs* jobs, status_t last_status) {
}
// TODO: Implement the built-in CD command
// Helper function: get_home_dir() returns home directory of user (If no arguments passed or "~" is passed)
status_t run_cd(Command* command) {  
}

// TODO: Implement the built-in PWD command
status_t run_pwd() {
}

// TODO: Implement code to execute executables
status_t run_command(Command* command, Jobs* jobs, status_t last_status) {
  switch (command->type) {
    case COMMAND_BUILTIN:
      return run_builtin_command(command, jobs, last_status);
      break;
    case COMMAND_IMPLEMENTED:
      char* bin_dir = get_bin_dir();
      // run implemented commands
      // hint: implemented commands are in the same directory as the shell
      // todo: execvp(..., current_command->args);
      break;
    default:
      // run other commands using exec*
      break;
  }
  return EXIT_FAILURE;
}

// TODO: Write redirection code, run run_builtin_command() and restore fd
// Helper function: run_builtin_command(Command* command, Jobs* jobs, status_t last_status) runs the builtin command and returns its exit status. 

status_t run_single_builtin_command(Command* command,
                                    Jobs* jobs,
                                    status_t last_status) {
  status_t status = 0;
  int backup_fds[2] = {dup(STDIN_FILENO), dup(STDOUT_FILENO)};

  Job* job = default_job();
  set_foreground_job(job, jobs);

  Process* process = default_process();
  process->command = command;

  // todo: set redirection
  // todo: restore fd

  if (strncmp(command->args[0], "fg", 3) != 0) {
    remove_foreground_job(jobs, true);
  }

  free(process);
  return status;
}

// TODO: set up pipe through the pipe() function. You need to set the stdout_fd, and stdin_fd of the process as well.
status_t setup_pipe(Process* process, int pipefd[2]) {  
}

// TODO: set up redirection. You need to set stdin_fd and/or stdout_fd of the process as well.
// Filenames are stored in the command's stdin and command's stdout.
// Process fds are in process->stdin_fd and process->stdout_fd.
status_t setup_redirection(Process* process) {
}


//TODO: set up dup2 for stdin and/or stdout.
status_t dup2_stdio(Process* process) {
}

// TODO: set up signal handlers in child process.
status_t run_pipeline(Pipeline* pipeline,
                      Jobs* jobs,
                      char* cmd,
                      status_t last_status) {
  status_t status = 0;
  Job* job = default_job();
  job->associated_command = strdup(cmd);
  if (pipeline->is_in_background) {
    job->state = JOB_BACKGROUND;
  } else {
    set_foreground_job(job, jobs);
  }

  int32_t pipefd[2];

  Command* current_command = pipeline->first_command;
  Command* next_command = pipeline->first_command->next_command;

  while (current_command != NULL) {
    if (job->first_process == NULL) {
      job->first_process = job->last_process = default_process();
    } else {
      job->last_process = job->last_process->next_process;
    }

    Process* current_process = job->last_process;
    current_process->command = current_command;

    if (next_command != NULL) {
      current_process->next_process = default_process();
      
      if ((status = setup_pipe(current_process, pipefd)) != 0) {
        if (job->pgid > 0) {
          kill(-job->pgid, SIGTERM);
          while (true) {
            pid_t waited = waitpid(-job->pgid, NULL, 0);
            if (waited > 0) {
              continue;
            }
            if (waited == -1 && errno == EINTR) {
              continue;
            }
            break;
          }
        }
        free_job(job);
        return status;
      }
    }

    switch (current_process->pid = fork()) {
      case -1:
        perror("fork");
        exit(1);
      case 0:
        // Set up signal handlers in child.

        if ((status = setup_redirection(current_process)) != 0) {
          exit(status);
        }
        
        if ((status = dup2_stdio(current_process)) != 0) {
          exit(status);
        }

        exit(run_command(current_command, jobs, last_status));
    }

    if (job->pgid == -1) {
      job->pgid = current_process->pid;
      setpgid(current_process->pid, job->pgid);

      if (job->state == JOB_FOREGROUND)
        tcsetpgrp(STDIN_FILENO, job->pgid);
    } else {
      setpgid(current_process->pid, job->pgid);
    }

    current_process->is_running = true;

    if (job->first_process != current_process) {
      close(current_process->stdin_fd);
    }
    if (next_command != NULL) {
      close(pipefd[1]);
    }

    current_command = next_command;

    if (current_command != NULL) {
      next_command = current_command->next_command;
    }
  }

  status = handle_job(job, jobs);
  return status;
}

int main(const int argc, const char* argv[]) {
  rl_catch_signals = 0;
  Jobs jobs;
  memset(&jobs, 0, sizeof(jobs));
  set_jobs_global(&jobs);

  status_t last_status = 0;

  // TODO: set signal handlers in parent
  /**
   * Hint:
   * - Ignored signals: SIGTTOU, SIGTTIN, SIGTSTP
   * - Signal handlers: SIGINT, SIGCHLD
   * - Requires SA_RESTART for sa_flags: SIGINT, SIGCHLD
   **/
  struct sigaction sa;
  // end signal handlers

  if (argc > 1) {
    const char* filename = argv[1];
    FILE* file = fopen(filename, "r");
    if (file == NULL) {
      fprintf(stderr, "pa2sh: %s: %s\n", filename, strerror(errno));
      exit(1);
    }
   
    size_t len = 0;
    for (char* line = NULL; getline(&line, &len, file) != -1;) {
      if (!line_is_empty(line))
        last_status = evaluate(&line, &jobs, last_status);      
    }
  } else {
    while (1) {
      char* cmd;
      if (pending_sigchld) last_status = handle_pending_sigchld();
      if ((cmd = readline("$ ")) == NULL) {
        if (pending_sigint) {
          last_status = handle_pending_sigint();
          continue;
        }
        break;
      }
      
      if (line_is_empty(cmd)) {
        free(cmd);
        continue;
      }

      if (pending_sigchld) last_status = handle_pending_sigchld();

      add_history(cmd);
      last_status = evaluate(&cmd, &jobs, last_status);
      if (pending_sigchld) last_status = handle_pending_sigchld();
    }
  }

  return last_status;
}