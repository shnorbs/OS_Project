#include "headers.h"
#include <signal.h>

int remainingtime;
int quantum;
int current_pid;
int paused = 0;

void handle_stop(int sig) 
{
    paused = 1;
    printf("Process %d paused.\n", current_pid);
    while (paused) {
        sleep(1);  // Sleep while paused to simulate a halt
    }
}

void handle_cont(int sig) 
{
    paused = 0;
    printf("Process %d resumed.\n", current_pid);
}

int main(int argc, char *argv[]) {
    // Signal handling
    signal(SIGSTOP, handle_stop);
    signal(SIGCONT, handle_cont);
    
    current_pid = getpid();
    initClk();
    
    remainingtime = atoi(argv[1]);
    //int algo = atoi(argv[2]);
    //quantum = (argc > 3) ? atoi(argv[3]) : 1;
    
    printf("Process %d started with remaining time: %d\n", current_pid, remainingtime);
    int current_time = getClk();
    while (remainingtime > 0) {
        if (!paused) {            
            remainingtime--;


            while (getClk() != current_time + 1); // removed sleep(1) and used a while loop instead
            printf("Process %d remaining time: %d\n", current_pid, remainingtime);
            current_time++;
        }
    }
    
    destroyClk(false);
    return 0;
}
