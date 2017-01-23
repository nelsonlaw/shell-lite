/************************************************************
* Student name and No.: Luo Kai Ren
* Development platform: Ubuntu 14.04 VM
* Last modified date: Oct 20, 2016
* Compilation: gcc main.c -o myshell -Wall
*************************************************************/


#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <pthread.h>

pthread_mutex_t count_mutex;

int builtin_exit(char **args);

int builtin_timex(char **args);

int check_lines_length(char **lines);

int check_and(char *line);

int check_pipe(char *line);

int builtin_run(char *line);

int run(char *line, int force_zombie);

void usr_handler();

void int_handler();

void sigchld_handler();

void sigchld_timex_handler();

void increment_process_count();

int launch_pipe(int in, int out, char *line);

char **split_line(char *line, int type);

void loop(void);

int line_reading = 0;

int completed_process_num = 0;


/**
   @brief List of builtin command names.
 */
char *builtin_str[] = {
        "exit",
        "timeX"
};

/**
   @brief List of builtin command functions.
 */

int (*builtin_func[])(char **) = {
        &builtin_exit,
        &builtin_timex
};

/**
   @brief Count the number of builtin functions.
   @return number of builtin functions.
 */

int builtin_num() {
    return sizeof(builtin_str) / sizeof(char *);
}


/**
   @brief Builtin command: timex. Setup timex environment.
   @param args Null terminated list of arguments (including program name).
   @return 3: success 1:error
 */

int builtin_timex(char **args) {
    if (check_lines_length(args) == 1) {
        fprintf(stderr, "myshell: \"timeX\" cannot be a standalone command\n");
        return 1;
    }
    int args_length = check_lines_length(args);
    char *pos = strchr(args[args_length - 1], '&');
    if ((strcmp(args[args_length - 1], "&") == 0) || (pos != NULL)) {
        fprintf(stderr, "myshell: \"timeX\" cannot be run in background mode\n");
        return 1;
    }
    return 3;
}


/**
   @brief Builtin command: exit.
   @param args Null terminated list of arguments (including program name).
   @return 2: exit success 1:error
 */
int builtin_exit(char **args) {
    if (check_lines_length(args) != 1) {
        fprintf(stderr, "myshell: \"exit\" with other arguments!!!\n");
        return 1;
    }
    return 2;
}


/**
  @brief Launch a program and wait for it to terminate if it is a foreground program.
  @param args Null terminated list of arguments (including program name).
  @param n number of program need to wait
  @param timex_mode if timex mode enabled 1, sigchld won't be blocked. (disabled 0)
  @return Always return 1: success
 */
int launch_nopipe(char **args, int n, int timex_mode) {
    pid_t pid;
    int background = 0;

    int args_length = check_lines_length(args);
    char *pos = strchr(args[args_length - 1], '&');
    if (strcmp(args[args_length - 1], "&") == 0) {
        background = 1;
        args[args_length - 1] = NULL;
    } else if (pos != NULL) {
        background = 1;
        *pos = '\0';
    }


    pid = fork();
    if (pid == 0) {
        // Child process
        sigset_t sigset;
        sigemptyset(&sigset);
        sigaddset(&sigset, SIGUSR1);

        sigpending(&sigset);
        if (execvp(args[0], args) == -1) {
            fprintf(stderr, "myshell: '%s' ", args[0]);
            perror(NULL);
            exit(EXIT_FAILURE);
        }
    } else if (pid < 0) {
        // Error forking
        perror("myshell");
    } else {
        // Parent process

        if (background) {
            setpgid(pid, 0);
        } else if (timex_mode) {
            //do nothing
        } else {
            kill(pid, SIGUSR1);
            int i;
            for (i = 0; i < n; i++) wait(NULL);
        }

    }

    return 1;
}

/**
  @brief Launch a program and connect it with pipe in and out
  @param in input end of a pipe
  @param out output end of a pipe
  @param line Null terminated list of arguments(including program name).
  @return 1: success 0:error
 */

int launch_pipe(int in, int out, char *line) {
    pid_t pid;
    char **args = split_line(line, 0);
    if ((pid = fork()) == 0) {
        if (in != 0) {
            dup2(in, 0);
            close(in);
        }

        if (out != 1) {
            dup2(out, 1);
            close(out);
        }
        if (execvp(args[0], args) == -1) {
            fprintf(stderr, "myshell: Fail to execute '%s' ", args[0]);
            perror(NULL);
            exit(EXIT_FAILURE);
        }
        return 0;
    } else if (pid < 0) {
        // Error forking
        perror("myshell");
    } else {
        setpgid(pid, 0);
    }

    return 1;
}

/**
   @brief Execute shell built-in or launch program.
   @param line Null terminated list of arguments(including program name).
   @return  3:timeX 2:exit 1: error 0: found and run good -1: not found
 */
int builtin_run(char *line) {
    int i;

    char **args = split_line(line, 0);

    for (i = 0; i < builtin_num(); i++) {
        if (strcmp(args[0], builtin_str[i]) == 0) {
            return (*builtin_func[i])(args);
        }
    }
    return -1;
}


/**
   @brief Read a line of input from stdin.
   @return The line from stdin.
 */
char *get_line(void) {
    char *line = NULL;
    size_t bufsize = 0;
    line_reading = 1; //A trick to prevent getline blocking future input.
    getline(&line, &bufsize, stdin);
    line_reading = 0;
    return line;
}

/**
   @brief Split a line into arguments tokens
   @param line command line.
   @param type 0:delimit by "[space]\t\r\n" 1:"|"
   @return Null-terminated arguments tokens.
 */

char **split_line(char *line, int type) {
    int param_maxlen = 30;
    int index = 0;
    char *param_delim;
    if (type == 0) {
        param_delim = " \t\r\n";
    } else if (type == 1) {
        param_delim = "|";
    } else {
        return NULL;
    }
    char **params = malloc(param_maxlen * sizeof(char *));
    char *param;

    char *ptr = malloc(sizeof(char *));
    param = strtok_r(line, param_delim, &ptr);
    while (param != NULL) {
        params[index] = param;
        index++;
        param = strtok_r(NULL, param_delim, &ptr);
    }
    params[index] = NULL;
    return params;
}

/**
   @brief A placeholder for SIGUSR1 handler
 */

void usr_handler() {

}

/**
   @brief SIGINT handler: clear current state
 */

void int_handler() {
    if (line_reading) {
        printf("\n## myshell $ ");
        fflush(stdin);
        fflush(stdout);
    }
}

/**
   @brief SIGCHLD handler: print process statue information
 */

void sigchld_handler() {
    siginfo_t info;
    char str[255];
    char command_name[255];
    waitid(P_ALL, 0, &info, (WEXITED | WNOHANG | WNOWAIT));
    if (info.si_pid > 0) {
        snprintf(str, 255, "/proc/%d/comm", info.si_pid);
        //printf("%s\n", str);
        FILE *file = fopen(str, "rb");
        if (file) {
            fread(command_name, 1, 255, file);
            char *pos;
            if ((pos = strchr(command_name, '\n')) != NULL) *pos = '\0';
            fclose(file);
            printf("[%ld] %s Done\n", (long) info.si_pid, command_name);
        }
        waitpid(info.si_pid, NULL, 0);
    }
}

/**
   @brief SIGCHLD handler: print process statue information and benchmark number
 */


void sigchld_timex_handler() {
    siginfo_t info;
    char stat[255];

    int z, pid;
    char str[50], comm[256];
    unsigned long h, ut, st, cutime, cstime;
    float uptime, idletime;
    long l;
    unsigned long long int starttime;

    waitid(P_ALL, 0, &info, (WEXITED | WNOHANG | WNOWAIT));
    if (info.si_pid > 0) {
        printf("\nPID\tCMD\t\tRTIME\t\tUTIME\t\tSTIME\n");
        snprintf(stat, 255, "/proc/%d/stat", info.si_pid);

        //printf("%s\n", stat);
        FILE *file = fopen(stat, "r");
        if (file) {
            fscanf(file, "%d %s %c %d %d %d %d %d %u %lu %lu %lu %lu %lu %lu %ld %ld %ld %ld %ld %ld %llu", &pid, comm,
                   &str[0], &z, &z, &z, &z,
                   &z,
                   (unsigned *) &z, &h, &h, &h, &h, &ut, &st, &cutime, &cstime, &l, &l, &l, &l, &starttime);
            fclose(file);
        }

        snprintf(stat, 255, "/proc/%d/comm", info.si_pid);
        FILE *file2 = fopen(stat, "r");
        if (file2) {
            fscanf(file2, "%s", comm);
            fclose(file2);
        }

        FILE *file3 = fopen("/proc/uptime", "r");
        if (file3) {
            fscanf(file3, "%f %f", &uptime, &idletime);
            fclose(file3);
        }


        printf("%d\t%s\t\t%.2lf s\t\t%.2f s\t\t%.2f s\n", pid, comm,
               uptime - (starttime * 1.0f / sysconf(_SC_CLK_TCK)),
               ut * 1.0f / sysconf(_SC_CLK_TCK), st * 1.0f / sysconf(_SC_CLK_TCK));
        waitpid(info.si_pid, NULL, 0);
    }
    increment_process_count();
}

/**
   @brief A more reliable way to count finished processes then waiting for signal.
 */

void increment_process_count() {
    pthread_mutex_lock(&count_mutex);
    completed_process_num += 1;
    pthread_mutex_unlock(&count_mutex);
}

/**
   @brief count number of arguments tokens
   @param lines Null terminated list of arguments(including program name).
   @return number of arguments tokens
 */

int check_lines_length(char **lines) {
    int size;
    for (size = 0; lines[size] != NULL; size++);
    return size;
}

/**
   @brief check usage of &
   @param line Null terminated command line(including program name).
   @return 0:no problem 1:error
 */

int check_and(char *line) {

    int result = 0;
    char *pos = strchr(line, '&');
    if (pos == NULL) { // no & sign
        result = 0;
    } else if ((pos - line + 2) == strlen(line)) { // & sign is last char 2: one for \n, one for index shift
        result = 0;
    } else { // other case
        fprintf(stderr, "myshell: '&' should not appear in the middle of the command line\n");
        result = 1;
    }


    return result;
}

/**
   @brief check usage of |
   @param line Null terminated command line(including program name).
   @return 0:no problem 1:error
 */

int check_pipe(char *line) {
    int result = 0;
    char *pipe_pos = strchr(line, '|');
    if ((pipe_pos - line + 2) == strlen(line)) {
        fprintf(stderr, "myshell: Incomplete '|' sequence\n");
        result = 1;
    } else {
        result = 0;
    }
    return result;
}

/**
   @brief Core function to run a command line
   @param line Null terminated command line(including program name).
   @param timex_mode if timex mode enabled 1, sigchld won't be blocked. (disabled 0)
   @return 0:exit 1:continue
 */

int run(char *line, int timex_mode) {

    char **lines;
    char *backup_line;
    int builtin_type;

    backup_line = malloc(sizeof(line) * 16);
    memcpy(backup_line, line, sizeof(line) * 16);

    lines = split_line(line, 1);
    if (timex_mode) {
        builtin_type = -1;
    } else {
        char temp[256];
        strcpy(temp,lines[0]);
        builtin_type = builtin_run(temp);
    }

    int i;
    int n = check_lines_length(lines);
    int in, fd[2], backup;

    sigset_t sigchld_mask;

    /* Setup signal environment for different command */
    /* builitin_type 3:timeX 2:exit 1: error 0: found and run good -1: not a builtin command */
    if (builtin_type == 1) { return 1; }
    else if (builtin_type == 2) { return 0; }
    else if (builtin_type == 3) {
        signal(SIGCHLD, sigchld_timex_handler);
        backup_line = (backup_line + 6);
        run(backup_line, 1);
        while (completed_process_num < n) {}
        completed_process_num = 0;
        return 1;
    } else if (builtin_type == -1 && !timex_mode) {
        signal(SIGCHLD, sigchld_handler);
        sigemptyset(&sigchld_mask);
        sigaddset(&sigchld_mask, SIGCHLD);
        sigprocmask(SIG_BLOCK, &sigchld_mask, NULL);
    }

    in = 0;

    /* backup the stdin */
    pipe(fd);
    backup = fd[0];
    dup2(in, backup);
    close(fd[1]);

    /* If there is blank command in middle, do nothing and return  */
    for (i = 0; i < n; ++i) {
        if (lines[i][0] == '\n') {
            return 1;
        };
    }

    /* init pipes and connect them */
    for (i = 0; i < n - 1; ++i) {
        pipe(fd);
        launch_pipe(in, fd[1], lines[i]);
        close(fd[1]);
        in = fd[0];
    }
    if (in != 0)
        dup2(in, 0);

    /* Last command not need to connect to a output pipe */
    char **backup_lines = split_line(backup_line, 1);
    char **args = split_line(backup_lines[i], 0);
    int result = launch_nopipe(args, n, timex_mode);

    if (builtin_type == -1 && !timex_mode) {
        sigprocmask(SIG_UNBLOCK, &sigchld_mask, NULL);
    }
    /* recover stdin */
    dup2(backup, 0);

    return result;
}

/**
   @brief Loop getting command line and executing it.
 */
void loop(void) {
    char *line;
    int cont;


    do {
        printf("## myshell $ ");
        line = get_line();
        if (check_and(line) || check_pipe(line) || strcmp(line, "\n") == 0) {
            cont = 1;
        } else {
            cont = run(line, 0);
        }
        free(line);
    } while (cont);
}


int main(int argc, char **argv) {

    // SIGINT handler setup
    signal(SIGINT, int_handler);
    signal(SIGCHLD, sigchld_handler);
    signal(SIGUSR1, usr_handler);


    loop();


    return 0;
}

