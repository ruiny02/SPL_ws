#include <ctype.h>
#include <editline/readline.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include "helper.h"

volatile sig_atomic_t pending_sigchld = 0;
static volatile sig_atomic_t pending_sigint = 0;

// TODO: set pending_sigint
void sigint_handler(int signum) {
  (void) signum;
  pending_sigint = 1;
}

// TODO: set pending_sigchld
void sigchld_handler(int signum) {
  (void) signum;
  pending_sigchld = 1;
}

// TODO: Handle sigint by writing "\n" to stdout and resetting pending_sigint
// Return code: 130
status_t handle_pending_sigint() {
  pending_sigint = 0;
  write(STDOUT_FILENO, "\n", 1);
  return 130;
}

/**
 * TODO: Check for any completed processes
 * Check for any completed processes.
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
  do {
    // TODO: Reset pending_sigchld, and check for completed processes until there are no more.
    pending_sigchld = 0;

    while (true) {
      Job* job = NULL;
      Process* process = wait_for_any_process(&job);
      if (process == NULL) {
        break;
      }

      bool table_job = job != NULL && job->id > 0;
      set_job_state(job, process);
      if (table_job && has_completed(job)) {
        printf("pa2: Job %lu, '%s' has ended\n", (unsigned long) job->id,
               job->associated_command);
        fflush(stdout);
        remove_job_from_table(job, NULL);
        free_job(job);
      }
    }
  } while (pending_sigchld);  
  return 0;
}

status_t setup_pipe(Process* process, int pipefd[2]);
status_t setup_redirection(Process* process);
status_t dup2_stdio(Process* process);

// TODO: Implement the built-in exit command
status_t run_exit(Command* command, Jobs* jobs, status_t last_status) {
  /* The "still jobs active" guard belongs to the shell-exit path only.
   * Inside a pipeline subprocess this builtin sees a forked copy of the
   * parent's jobs table; refusing to exit there would clobber the
   * pipeline's exit status (PDF: "exit only exits the subprocess").
   * The single-builtin caller does the active-jobs check before us. */
  (void) jobs;

  if (command->num_args > 2) {
    fprintf(stderr, "exit: too many arguments\n");
    return 1;
  }

  if (command->num_args == 1) {
    exit(last_status);
  }

  char* end = NULL;
  errno = 0;
  long code = strtol(command->args[1], &end, 10);
  if (errno != 0 || command->args[1][0] == '\0' || *end != '\0') {
    fprintf(stderr, "exit: %s: invalid integer\n", command->args[1]);
    return 1;
  }

  if (code < 0 || code > 255) {
    code = 255;
  }
  exit((int) code);
}

// TODO: Implement the built-in CD command
// Helper function: get_home_dir() returns home directory of user (If no arguments passed or "~" is passed)
status_t run_cd(Command* command) {  
  const char* target = NULL;
  char* home = NULL;

  if (command->num_args == 1 || strcmp(command->args[1], "~") == 0) {
    home = get_home_dir();
    target = home;
  } else {
    target = command->args[1];
  }

  if (target == NULL) {
    fprintf(stderr, "cd: HOME not set\n");
    free(home);
    return 1;
  }

  if (chdir(target) == -1) {
    fprintf(stderr, "cd: %s: %s\n", target, strerror(errno));
    free(home);
    return 1;
  }

  char cwd[PATH_MAX];
  if (getcwd(cwd, sizeof(cwd)) != NULL) {
    setenv("PWD", cwd, 1);
  }

  free(home);
  return 0;
}

// TODO: Implement the built-in PWD command
status_t run_pwd(Command* command) {
  bool physical = false;
  char cwd[PATH_MAX];
  char* logical = getenv("PWD");

  for (size_t i = 1; i < command->num_args; i++) {
    if (strcmp(command->args[i], "-P") == 0) {
      physical = true;
    } else if (strcmp(command->args[i], "-L") == 0) {
      physical = false;
    }
  }

  if (!physical && logical != NULL && logical[0] != '\0') {
    puts(logical);
    return 0;
  }

  if (getcwd(cwd, sizeof(cwd)) == NULL) {
    fprintf(stderr, "pwd: %s\n", strerror(errno));
    return 1;
  }
  puts(cwd);
  return 0;
}

// TODO: Implement code to execute executables
status_t run_command(Command* command, Jobs* jobs, status_t last_status) {
  switch (command->type) {
    case COMMAND_BUILTIN:
      return run_builtin_command(command, jobs, last_status);
    case COMMAND_IMPLEMENTED: {
      char* bin_dir = get_bin_dir();
      if (bin_dir != NULL) {
        size_t path_len = strlen(bin_dir) + strlen(command->args[0]) + 2;
        char* path = malloc(path_len);
        if (path == NULL) {
          perror("pa2_shell");
          free(bin_dir);
          return EXIT_FAILURE;
        }
        snprintf(path, path_len, "%s/%s", bin_dir, command->args[0]);
        execv(path, command->args);
        int saved_errno = errno;
        free(path);
        free(bin_dir);
        if (saved_errno != ENOENT) {
          errno = saved_errno;
          break;
        }
      }
      // run implemented commands
      // hint: implemented commands are in the same directory as the shell
      // todo: execvp(..., current_command->args);
      execvp(command->args[0], command->args);
      break;
    }
    default:
      // run other commands using exec*
      execvp(command->args[0], command->args);
      break;
  }

  if (errno == ENOENT) {
    fprintf(stderr, "%s: command not found\n", command->args[0]);
    return 127;
  }

  fprintf(stderr, "pa2_shell: %s: %s\n", command->args[0], strerror(errno));
  return EXIT_FAILURE;
}

// TODO: Write redirection code, run run_builtin_command() and restore fd
// Helper function: run_builtin_command(Command* command, Jobs* jobs, status_t last_status) runs the builtin command and returns its exit status. 

status_t run_single_builtin_command(Command* command,
                                    Jobs* jobs,
                                    status_t last_status) {
  /* exit can only refuse based on active jobs from the shell context;
   * if a child runs it inside a pipeline it just exits the subprocess.
   * The check moved out of run_exit so the pipeline path stays clean. */
  if (strcmp(command->args[0], "exit") == 0 && !is_job_table_empty(jobs)) {
    fprintf(stderr, "There are still jobs active.\n");
    return 1;
  }

  status_t status = 0;
  int backup_fds[2] = {dup(STDIN_FILENO), dup(STDOUT_FILENO)};

  Job* job = default_job();
  set_foreground_job(job, jobs);

  Process* process = default_process();
  process->command = command;

  if (backup_fds[0] == -1 || backup_fds[1] == -1) {
    perror("dup");
    status = 1;
    goto cleanup;
  }

  // todo: set redirection
  status = setup_redirection(process);
  if (status == 0) {
    status = dup2_stdio(process);
  }

  if (status == 0) {
    status = run_builtin_command(command, jobs, last_status);
  }

  fflush(stdout);
  fflush(stderr);

  // todo: restore fd
  if (dup2(backup_fds[0], STDIN_FILENO) == -1) {
    perror("dup2");
    status = 1;
  }
  if (dup2(backup_fds[1], STDOUT_FILENO) == -1) {
    perror("dup2");
    status = 1;
  }

cleanup:
  if (backup_fds[0] != -1) {
    close(backup_fds[0]);
  }
  if (backup_fds[1] != -1) {
    close(backup_fds[1]);
  }
  if (jobs->table[0] == job) {
    remove_foreground_job(jobs, true);
  }

  free(process);
  return status;
}

// TODO: set up pipe through the pipe() function. You need to set the stdout_fd, and stdin_fd of the process as well.
status_t setup_pipe(Process* process, int pipefd[2]) {  
  if (pipe(pipefd) == -1) {
    fprintf(stderr, "pa2_shell: pipe: %s\n", strerror(errno));
    return 1;
  }

  process->stdout_fd = pipefd[1];
  process->next_process->stdin_fd = pipefd[0];
  return 0;
}

// TODO: set up redirection. You need to set stdin_fd and/or stdout_fd of the process as well.
// Filenames are stored in the command's stdin and command's stdout.
// Process fds are in process->stdin_fd and process->stdout_fd.
status_t setup_redirection(Process* process) {
  Command* command = process->command;

  if (command->stdin != NULL) {
    int fd = open(command->stdin, O_RDONLY);
    if (fd == -1) {
      fprintf(stderr, "pa2_shell: %s: %s\n", command->stdin,
              strerror(errno));
      return 1;
    }
    if (process->stdin_fd != -1) {
      close(process->stdin_fd);
    }
    process->stdin_fd = fd;
  }

  if (command->stdout != NULL) {
    int flags = O_WRONLY | O_CREAT;
    flags |= command->append_stdout ? O_APPEND : O_TRUNC;

    int fd = open(command->stdout, flags, 0666);
    if (fd == -1) {
      fprintf(stderr, "pa2_shell: %s: %s\n", command->stdout,
              strerror(errno));
      return 1;
    }
    if (process->stdout_fd != -1) {
      close(process->stdout_fd);
    }
    process->stdout_fd = fd;
  }

  return 0;
}


//TODO: set up dup2 for stdin and/or stdout.
status_t dup2_stdio(Process* process) {
  if (process->stdin_fd != -1) {
    if (dup2(process->stdin_fd, STDIN_FILENO) == -1) {
      fprintf(stderr, "pa2_shell: dup2: %s\n", strerror(errno));
      return 1;
    }
    close(process->stdin_fd);
    process->stdin_fd = -1;
  }

  if (process->stdout_fd != -1) {
    if (dup2(process->stdout_fd, STDOUT_FILENO) == -1) {
      fprintf(stderr, "pa2_shell: dup2: %s\n", strerror(errno));
      return 1;
    }
    close(process->stdout_fd);
    process->stdout_fd = -1;
  }

  return 0;
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
    job->num_processes++;

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

    fflush(NULL);

    switch (current_process->pid = fork()) {
      case -1:
        perror("fork");
        exit(1);
      case 0:
        // Set up signal handlers in child.
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGTTIN, SIG_DFL);
        signal(SIGTTOU, SIG_DFL);
        signal(SIGCHLD, SIG_DFL);

        setpgid(0, job->pgid == -1 ? 0 : job->pgid);
        if (job->state == JOB_FOREGROUND) {
          tcsetpgrp(STDIN_FILENO, getpgrp());
        }

        if (next_command != NULL &&
            current_process->next_process->stdin_fd != -1) {
          close(current_process->next_process->stdin_fd);
        }

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
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = SIG_IGN;
  sigemptyset(&sa.sa_mask);
  sigaction(SIGQUIT, &sa, NULL);
  sigaction(SIGTSTP, &sa, NULL);
  sigaction(SIGTTIN, &sa, NULL);
  sigaction(SIGTTOU, &sa, NULL);

  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = sigint_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  sigaction(SIGINT, &sa, NULL);

  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = sigchld_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  sigaction(SIGCHLD, &sa, NULL);
  // end signal handlers

  if (argc > 1) {
    const char* filename = argv[1];
    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
      fprintf(stderr, "pa2sh: %s: %s\n", filename, strerror(errno));
      exit(1);
    }

    /* Slurp the whole script into memory, then close the descriptor.
     * Builtin commands that run inside a pipeline fork the shell, and
     * libc's exit() in the child would fflush() any inherited read
     * stream, lseek()ing the shared open file description back to the
     * already-consumed offset and causing the parent to re-read lines. */
    size_t cap = 4096;
    size_t total = 0;
    char* buf = malloc(cap);
    if (buf == NULL) {
      perror("pa2sh");
      close(fd);
      exit(1);
    }
    while (1) {
      if (total + 1 >= cap) {
        cap *= 2;
        char* grown = realloc(buf, cap);
        if (grown == NULL) {
          perror("pa2sh");
          free(buf);
          close(fd);
          exit(1);
        }
        buf = grown;
      }
      ssize_t n = read(fd, buf + total, cap - total - 1);
      if (n == 0) break;
      if (n < 0) {
        if (errno == EINTR) continue;
        fprintf(stderr, "pa2sh: %s: %s\n", filename, strerror(errno));
        free(buf);
        close(fd);
        exit(1);
      }
      total += (size_t) n;
    }
    buf[total] = '\0';
    close(fd);

    char* cursor = buf;
    while (cursor < buf + total) {
      char* newline = strchr(cursor, '\n');
      size_t line_len = newline != NULL ? (size_t)(newline - cursor) : strlen(cursor);
      char* line = strndup(cursor, line_len);
      if (line == NULL) {
        perror("pa2sh");
        break;
      }
      cursor += line_len + (newline != NULL ? 1 : 0);

      if (pending_sigchld) handle_pending_sigchld();
      if (pending_sigint) {
        last_status = handle_pending_sigint();
        free(line);
        continue;
      }
      if (!line_is_empty(line)) {
        last_status = evaluate(&line, &jobs, last_status);
      } else {
        free(line);
      }
      if (pending_sigchld) handle_pending_sigchld();
    }
    free(buf);
    if (pending_sigchld) handle_pending_sigchld();
  } else {
    while (1) {
      char* cmd;
      if (pending_sigchld) handle_pending_sigchld();
      if (pending_sigint) {
        last_status = handle_pending_sigint();
        continue;
      }
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

      if (pending_sigchld) handle_pending_sigchld();

      add_history(cmd);
      last_status = evaluate(&cmd, &jobs, last_status);
      if (pending_sigchld) handle_pending_sigchld();
    }
  }

  /* Clean up any surviving background/stopped jobs so we don't leave
   * orphan child processes behind (PDF: child processes are resources
   * and must be freed before the program terminates). */
  for (size_t i = 0; i <= jobs.highest_job_id; i++) {
    Job* job = jobs.table[i];
    if (job == NULL) continue;
    if (job->pgid > 0) {
      kill(-job->pgid, SIGCONT);
      kill(-job->pgid, SIGTERM);
      while (waitpid(-job->pgid, NULL, 0) > 0) { /* reap */ }
    }
    free_job(job);
    jobs.table[i] = NULL;
  }

  return last_status;
}
