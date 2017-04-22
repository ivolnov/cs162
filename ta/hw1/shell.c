#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include "tokenizer.h"

/* Convenience macro to silence compiler warnings about unused function parameters. */
#define unused __attribute__((unused))

/* Whether the shell is connected to an actual terminal or not. */
bool shell_is_interactive;

/* File descriptor for the shell input */
int shell_terminal;

/* Terminal mode settings for the shell */
struct termios shell_tmodes;

/* Process group id for the shell */
pid_t shell_pgid;

/* buffer to store a full path of an executable */
char exec_full_path[4096]; // longest file path on EXT4 they say...

int cmd_exit(struct tokens *tokens);
int cmd_help(struct tokens *tokens);
int cmd_pwd(struct tokens *tokens);
int cmd_cd(struct tokens *tokens);
int cmd_wait(struct tokens *tokens);
int cmd_fg(struct tokens *tokens);
int cmd_bg(struct tokens *tokens);

/* Built-in command functions take token array (see parse.h) and return int */
typedef int cmd_fun_t(struct tokens *tokens);

/* Built-in command struct and lookup table */
typedef struct fun_desc {
  cmd_fun_t *fun;
  char *cmd;
  char *doc;
} fun_desc_t;

fun_desc_t cmd_table[] = {
  {cmd_help, "?", "show this help menu"},
  {cmd_exit, "exit", "exit the command shell"},
  {cmd_pwd, "pwd", "print current working directory"},
  {cmd_cd, "cd", "change directory: cd [relative or full path name]"},
  {cmd_wait, "wait", "blocks until all background processes terminate"},
  {cmd_fg, "fg", "moves the process to the foreground"},
  {cmd_bg, "bg", "moves the process to the background"},
};

/* Storage of all job related data */
typedef struct job job;
struct job {
    int id;
    job *next;
    pid_t pid;
    char *exe;
    size_t argc;
    char **argv;
    int fg;
    struct termios tmod;
};

void job_destroy(job *job);

/* Background jobs' list head pointer and its size */
job *bg_jobs = NULL;
int bg_job_cnt = 1;

/* Utility to iterate over list for its tail */
job *jobs_tail() {

    job *tail = NULL;

    for (job *j = bg_jobs; j; j = j->next) {
        tail = j;
    }

    return tail;
}

/* Utility to get job by its pid */
job *job_with(pid_t pid) {
    for (job *j = bg_jobs; j; j = j->next) {
        if (pid == j->pid) {
            return j;
        }
    }
    return NULL;
}

/* Removes the job struct from jobs list by re-pointing its previous element to the one in front */
void remove_from_jobs(job *job) {
    if (job == bg_jobs) {
        bg_jobs = NULL;
        bg_job_cnt = 1;
    } else {
        struct job *prev = NULL;
        for (struct job *j = bg_jobs; j; j = j->next) {
            if (j == job) {
                prev->next = job->next;
            }
            prev = j;
        }
    }
}

/* Prints something like: [1]+  Done                    echo hello */
void print_job_info(job *j, char *status) {
    int i = 1;
    fprintf(stdout, "[%u]+  %-20s%s", j->id, status, j->argv[0]);

    while (j->argv[i]) {
        fprintf(stdout, " %s", j->argv[i++]);
    }

    fprintf(stdout, "\n");
}

/* Waits for children in non blocking mode. Destroys terminated ones. */
void check_bg_jobs() {
    int status;
    pid_t pid;

    while ((pid = waitpid(WAIT_ANY, &status, WNOHANG|WUNTRACED)) > 0) {

        job *child = job_with(pid);

        if (WIFSTOPPED(status)) {
            print_job_info(child, "Stopped");
        } else {
            print_job_info(child, "Done");
            remove_from_jobs(child);
            job_destroy(child);
        }
    }
}

/* print errno */
void error() {
    fprintf(stderr, "%s\n", strerror(errno));
}

/* Waits for job termination or stop. Acts accordingly.*/
int wait_for(job *job) {
    int status = 0;

    tcgetattr(shell_terminal, &shell_tmodes);
    tcsetpgrp(shell_terminal, job->pid);

    waitpid(job->pid, &status, WUNTRACED);

    tcgetattr(shell_terminal, &job->tmod);
    tcsetattr(shell_terminal, TCSADRAIN, &shell_tmodes);
    tcsetpgrp(shell_terminal, shell_pgid);

    if (WIFSTOPPED(status)) {
        if (bg_jobs) {
            jobs_tail()->next = job;
        } else {
            bg_jobs = job;
        }
        job->id = bg_job_cnt++;
        job->fg = 0;
        print_job_info(job, "Stopped");
    } else {
        if (!WIFEXITED(status)) {
            fprintf(stderr, "%i\n", WEXITSTATUS(status));
        }
        print_job_info(job, "Done");
        job_destroy(job);
    }

    return WEXITSTATUS(status);
}

/* Makes process to ignore SIGINT, SIGQUIT, SIGTSTP, SIGTTIN, SIGTTOU */
void ignore_sigs() {
    struct sigaction ign_action;

    ign_action.sa_sigaction = (void (*)(int, siginfo_t *, void *)) SIG_IGN;

    sigaction(SIGINT, &ign_action, NULL);
    sigaction(SIGQUIT, &ign_action, NULL);
    sigaction(SIGTSTP, &ign_action, NULL);
    sigaction(SIGTTIN, &ign_action, NULL);
    sigaction(SIGTTOU, &ign_action, NULL);
}

/* restores defaults for SIGINT, SIGQUIT, SIGTSTP, SIGTTIN, SIGTTOU */
void default_sigs_on() {
    struct sigaction std_action;

    std_action.sa_sigaction = (void (*)(int, siginfo_t *, void *)) SIG_DFL;

    sigaction(SIGINT, &std_action, NULL);
    sigaction(SIGQUIT, &std_action, NULL);
    sigaction(SIGTSTP, &std_action, NULL);
    sigaction(SIGTTIN, &std_action, NULL);
    sigaction(SIGTTOU, &std_action, NULL);
}

/* Stores full path name of a shell command according to the $PATH in a given char array pointer. */
int full_path_name(char *cmd, char *path_buffer) {
    char *dir_name = NULL;
    char *env = getenv("PATH");
    char path[strlen(env)];

    strcpy(path, env);

    dir_name = strtok(path, ":");

    do {
        strcpy(path_buffer, dir_name);
        strcat(path_buffer, "/");
        strcat(path_buffer, cmd);
        if (access(path_buffer, F_OK) == 0) {
            return 0;
        }
        dir_name = strtok(NULL, ":");
    } while (dir_name);

    fprintf(stderr, "%s: command not found\n", cmd);

    return 1;
}

/* Redirects stdin and stdout to or from a file according to '>' or '<' arrows in argv. */
void redirect_io(job *job) {

    FILE *fp = NULL;
    int file_param_offset = job->fg ? 2 : 3;
    int arrow_param_offset = job->fg ? 3 : 4;

    if (job->argc < arrow_param_offset + 1) {
        return;
    }

    char *file = job->argv[job->argc - file_param_offset];
    char *arrow = job->argv[job->argc - arrow_param_offset];
    int new_fd = (strcmp(arrow, "<") == 0) ? STDIN_FILENO : (strcmp(arrow, ">") == 0) ? STDOUT_FILENO : STDERR_FILENO;

    if (new_fd == STDERR_FILENO) {
        return;
    }

    if (!(fp = fopen(file, new_fd ? "w+" : "r"))) {
        error();
    } else {
        dup2(fileno(fp), new_fd);
        fclose(fp);
        job->argv[job->argc - arrow_param_offset] = NULL;
    }
}

int _invalid(struct tokens *tokens) {
    char *token = NULL;
    size_t length = tokens_get_length(tokens);
    size_t arrow_position = 0;
    size_t ampersand_position = 0;


    for (size_t i = 0; i < length; i++) {
        token = tokens_get_token(tokens, i);
        if (!arrow_position && (!strcmp(token, ">") || !strcmp(token, "<"))) {
            arrow_position = i;
        } else if (!ampersand_position && !strcmp(token, "&")) {
            ampersand_position = i;
        }
    }

    if (ampersand_position && ampersand_position != length - 1) {
        fprintf(stderr, "syntax error near unexpected token `%s'\n", tokens_get_token(tokens, ampersand_position));
        return 1;
    }

    if (arrow_position) {
        if ((ampersand_position && arrow_position != length - 3) || (!ampersand_position && arrow_position != length - 2))
        {
            fprintf(stderr, "syntax error near unexpected token `%s'\n", tokens_get_token(tokens, arrow_position));
            return 1;
        }
    }

    if(full_path_name(tokens_get_token(tokens, 0), exec_full_path) != 0) {
        fprintf(stderr, "%s: command not found\n", tokens_get_token(tokens, 0));
        return 1;
    };

    return 0;
}

/* Builds an amazing job struct instead of useless tokens */
job *job_build(struct tokens *tokens) {

    if (_invalid(tokens)) {
        return NULL;
    }

    job *job = malloc(sizeof *job);
    job->next = NULL;
    job->pid = 0;
    job->exe = malloc(strlen(exec_full_path) * sizeof(char));
    job->argc = tokens_get_length(tokens) + 1; // extra one for NULL.
    job->argv = malloc(job->argc * sizeof(char*));
    job->fg = strcmp(tokens_get_token(tokens, job->argc - 2), "&");

    strcpy(job->exe, exec_full_path);

    for (size_t i = 0; i < job->argc - 1; i++) {
        char *token = tokens_get_token(tokens, i);
        job->argv[i] = malloc(strlen(token) * sizeof(char));
        strcpy(job->argv[i], token);
    }

    if (job->fg) {
        job->argv[job->argc - 1] = NULL;
    } else {
        job->argv[job->argc - 2] = NULL;
    }

    tcgetattr(shell_terminal, &job->tmod);

    return job;
}

/* Frees memory occupied by the job */
void job_destroy(struct job *job) {
    for(size_t i = 0; i < job->argc; i++ ) {
        free(job->argv[i]);
    }
    free(job->exe);
    free(job->argv);
    free(job);
}

/* Prints current working directory. Uses parent shell pwd utility. */
int cmd_pwd(struct tokens *tokens) {
  char cwd[1024];
  getcwd(cwd, sizeof(cwd));
  printf("%s\n", cwd);
  return 0;
}

/* Changes current working directory. Uses parent shell cd utility. */
int cmd_cd(struct tokens *tokens) {
  int rc = chdir(tokens_get_token(tokens, 1));
  if (rc) {
    error();
  } else {
    cmd_pwd(tokens);
  }
  return rc;
}

/* Waits for all processes in bg to finish. */
int cmd_wait(struct tokens *tokens) {
    while(bg_jobs) {
        check_bg_jobs();
        sleep(1); // Energy save mode :)
    }
    return 0;
}

/* Moves the process to the foreground. The last stopped one or the one with the specified pid. */
int cmd_fg(struct tokens *tokens) {
    size_t argc = tokens_get_length(tokens);

    if (argc > 2 || (argc == 2 && !isdigit(tokens_get_token(tokens, 1)))) {
        fprintf(stderr, "usage: fg [pid]\n");
        return -1;
    }

    int pid = argc > 1 ? atoi(tokens_get_token(tokens, 1)): 0;
    job *job = pid > 0 ? job_with(pid) : jobs_tail();

    if (!job) {
        return -1;
    }

    remove_from_jobs(job);
    job->fg = 1;

    print_job_info(job, "");

    kill(-job->pid, SIGCONT);

    wait_for(job);

    return 0;
};

/* Moves the last stopped one or the one with the specified pid to the background. */
int cmd_bg(struct tokens *tokens) {
    size_t argc = tokens_get_length(tokens);

    if (argc > 2 || (argc == 2 && !isdigit(tokens_get_token(tokens, 1)))) {
        fprintf(stderr, "usage: bg [pid]\n");
        return -1;
    }

    int pid = argc > 1 ? atoi(tokens_get_token(tokens, 1)): 0;
    job *job = pid > 0 ? job_with(pid) : jobs_tail();

    if (!job) {
        return -1;
    }

    kill(-job->pid, SIGCONT);

    return 0;
}

/* Runs given executable with given arguments.*/
void child(job *job) {
    if (shell_is_interactive) {
        default_sigs_on();
        setpgid(0, 0);
    }

    if (execv(job->exe, job->argv)) {
        error();
        exit(1);
    }
}

/* Performs parent's process logic */
int parent(job *job) {
    int rc = 0;

    if (shell_is_interactive) {
        setpgid(job->pid, job->pid);
    }

    if (job->fg) {
        wait_for(job);
    } else {
        struct job *tail = jobs_tail();

        if (!tail) {
            bg_jobs = job;
        } else {
            tail->next = job;
        }

        job->id = bg_job_cnt++;

        fprintf(stdout, "[%i] %u\n", job->id, job->pid);
    }

    return rc;
}

/* Executes arbitrary commands (not built into shell). */
int cmd_exec(job *job) {
    int rc = 0;

    if ((job->pid = fork())) {
        rc = parent(job);
    } else {
        redirect_io(job);
        child(job);
    }

    return rc;
}

/* Prints a helpful description for the given command */
int cmd_help(unused struct tokens *tokens) {
  for (unsigned int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
    printf("%s - %s\n", cmd_table[i].cmd, cmd_table[i].doc);
  return 1;
}

/* Exits this shell */
int cmd_exit(unused struct tokens *tokens) {
  exit(0);
}

/* Looks up the built-in command, if it exists. */
int lookup(char cmd[]) {
  for (unsigned int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
    if (cmd && (strcmp(cmd_table[i].cmd, cmd) == 0))
      return i;
  return -1;
}

/* Intialization procedures for this shell */
void init_shell() {
  /* Our shell is connected to standard input. */
  shell_terminal = STDIN_FILENO;

  /* Check if we are running interactively */
  shell_is_interactive = isatty(shell_terminal);

  if (shell_is_interactive) {
    /* If the shell is not currently in the foreground, we must pause the shell until it becomes a
     * foreground process. We use SIGTTIN to pause the shell. When the shell gets moved to the
     * foreground, we'll receive a SIGCONT. */
    while (tcgetpgrp(shell_terminal) != getpgrp())
      kill(-shell_pgid, SIGTTIN);

    /* Saves the shell's process id */
    shell_pgid = getpid();

    /* Take control of the terminal */
    tcsetpgrp(shell_terminal, shell_pgid);

    /* Save the current termios to a variable, so it can be restored later. */
    tcgetattr(shell_terminal, &shell_tmodes);
  }
}

int main(unused int argc, unused char *argv[]) {
  init_shell();
  ignore_sigs();

  static char line[4096];
  int line_num = 0;

  /* Please only print shell prompts when standard input is not a tty */
  if (shell_is_interactive)
    fprintf(stdout, "%d>> ", line_num);

  while (fgets(line, 4096, stdin)) {
    /* Split our line into words. */
    struct tokens *tokens = tokenize(line);

    /* Find which built-in function to run. */
    int fundex = lookup(tokens_get_token(tokens, 0));

    if (fundex >= 0) {
      cmd_table[fundex].fun(tokens);
    } else {
      /* REPLACE this to run commands as programs. */
      if (tokens_get_length(tokens)) {
          job *job = job_build(tokens);
          if (job) {
              cmd_exec(job);
          }
      }
    }

    if (shell_is_interactive)
      /* Please only print shell prompts when standard input is not a tty */
      check_bg_jobs();
      fprintf(stdout, "%d>> ", ++line_num);

    /* Clean up memory */
    tokens_destroy(tokens); //TODO: that thing destroys your job data man !
  }

  return 0;
}
