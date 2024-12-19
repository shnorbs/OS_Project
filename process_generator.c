#include "headers.h"
#include "Defined_DS.h"
#include "string.h"
#include <stdbool.h>

// Function to clear resources on termination
void clearResources(int signum);

// Global variables
int msgQid;
Process* processes;
char* File_name;

int main(int argc, char* argv[]) {

    signal(SIGINT, clearResources); // Handle termination signal
   

    // Correct memory allocation for File_name
    File_name = strdup(argv[1]);  // Allocate and copy argv[1]
    if (!File_name) {
        perror("Failed to allocate memory for File_name");
        return -1;
    }

   

    // Step 1: Read input file to count the number of processes
    FILE* file = fopen(File_name, "r");
    if (!file) {
        perror("Error opening processes.txt");
        free(File_name);
        return -1;
    }

    int process_count = 0;
    size_t len = 0;
    char* line = NULL;

    while (getline(&line, &len, file) != -1) {
        if (line[0] == '#') // Ignore comments
            continue;
        process_count++;
    }
    free(line);
    fclose(file);

    // Step 2: Allocate memory for processes
    processes = (Process*)malloc(process_count * sizeof(Process));
    if (!processes) {
        perror("Error allocating memory for processes");
        free(File_name);
        return -1;
    }

    // Step 3: Populate process array from the input file
    file = fopen(File_name, "r");
    if (!file) {
        perror("Error reopening processes.txt");
        free(processes);
        free(File_name);
        return -1;
    }

    int i = 0;
    line = NULL;
    while (getline(&line, &len, file) != -1) {
        if (line[0] == '#') // Ignore comments
            continue;

        if (sscanf(line, "%d\t%d\t%d\t%d", 
                   &processes[i].id, 
                   &processes[i].arrival_time, 
                   &processes[i].runtime, 
                   &processes[i].priority) == 4) {
            processes[i].prempted=false;
            i++;
        }
    }
    free(line);
    fclose(file);

    // Step 4: Start clock and scheduler processes
    int algo_num = atoi(argv[2]);  // Use the second argument for algorithm selection
    char quantum_str[10] = "";
    if (argc > 3)
        sprintf(quantum_str, "%d", atoi(argv[3]));  // Set quantum only if provided

    int clk_pid = fork();
    if (clk_pid == 0) {
        execl("./clk.out", "./clk.out", NULL);
        perror("Clock execution failed");
        exit(-1);
    }
    sleep(1);
    initClk();
    char process_count_str[10]; // Allocate enough space for an integer string
    sprintf(process_count_str, "%d", process_count); 
    int scheduler_pid = fork();
    if (scheduler_pid == 0) {
        if (argc > 3)
            execl("./scheduler.out", "./scheduler.out", argv[2],process_count_str, quantum_str, NULL);
        else
            execl("./scheduler.out", "./scheduler.out", argv[2],process_count_str, NULL);
        perror("Scheduler execution failed");
        exit(-1);
    }

    // Step 5: Message queue setup  
    msgQid = msgget(MSQKEY, 0666 | IPC_CREAT);
    if (msgQid == -1) {
        perror("Failed to create message queue");
        free(processes);
        free(File_name);
        destroyClk(true);
        return -1;
    }
    
    // Step 6: Send processes to the scheduler at the appropriate time
    for (int t = 0, sent = 0; sent < process_count; t++) {
        while (getClk() < t); // Wait for clock synchronization

        for (int j = 0; j < process_count; j++) {
            if (processes[j].arrival_time == t) {
                struct msgbuff {
                    Process p;
                } newMessage;
                newMessage.p = processes[j];

                if (msgsnd(msgQid, &newMessage, sizeof(newMessage), !IPC_NOWAIT) == -1) {
                    perror("Error sending process to scheduler");
                } else {
                    printf("Sent Process %d to scheduler at time %d\n", processes[j].id, t);
                    sent++;
                }
            }
        }
    }

    // Cleanup
   // while (getClk() < processes[process_count - 1].arrival_time + 10); // Wait for last process execution
    int status;
    waitpid (scheduler_pid, &status, 0);
    waitpid (clk_pid, &status, 0);
    clearResources(SIGINT);
    return 0;
}

void clearResources(int signum) {
    printf("Cleaning up resources as Process generator...\n");
    msgctl(msgQid, IPC_RMID, NULL); // Remove message queue
    if (processes)
        free(processes);
    if (File_name)
        free(File_name);
    //destroyClk(true); // Destroy the clock
    raise(SIGKILL);
}
