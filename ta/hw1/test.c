//
// Created by ivolnov on 08.03.17.
//

#include <stdlib.h>

#include <stdio.h>
#include <unistd.h>
#include <wait.h>


void foo() {
    int status;
    pid_t pid = waitpid(WAIT_ANY, &status, WNOHANG|WUNTRACED);
    if (pid > 0) {
        fprintf(stderr, "pid->%u  status: %i\n", pid, WEXITSTATUS(status));
    }
}

int main(void) {
    char *intervals[5];
    intervals[0] = "10";
    intervals[1] = "8";
    intervals[2] = "6";
    intervals[3] = "4";
    intervals[4] = "2";

    char *argv[3];
    argv[0] = "sleep";
    argv[2] = NULL;

    for (int i=0; i < 5; i++){
        if (fork() == 0) {
            setpgid(getpid(), getpid());
            argv[1] = intervals[i];
            execvp("sleep", argv);
            perror(NULL);
            exit(-1);
        }
    }

    sleep(5);

    while (1) {
        foo();
    }
}

