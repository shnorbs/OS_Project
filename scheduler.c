#include "headers.h"
#include <signal.h>
#include <sys/wait.h>
#include <stdbool.h>
#include "Defined_DS.h"
// Define process states
#define Ready 0
#define RUNNING 1
#define  Blocked 2
#define FINISHED 3
#define NO_OF_PRIORITIES 11

int finished_counter=0;
int Waiting=0;
int Processes_entered=0;


// Tamer's global variables
int total_time = 0;
int busy_time = 0;
int lasttime = 0;

struct msgbuff
{
    Process p;
    int remainingTime;
};


// Define PCB structure
typedef struct PCB {
    Process process;     // Process details
    int state;           // Process state: WAITING, RUNNING, FINISHED, STOPPED
    int remaining_time;  // Remaining runtime
    int start_time;      // Start time
    int finish_time;     // Finish time
    int waiting_time;    // Waiting time
    int TA;
    int Last_execution;
    int executionTime;
    float WTA;
    pid_t pid;           // Process ID after fork

    // for mlfq
    int current_priority; // Track current priority level
    int original_priority; // Store original priority

} PCB;

int* Process_Wait;
float* WTA;

PriorityQueue* readyQueue;
PCB* currentPCB = NULL;// Currently running process
int Start_execution=0;
FILE* log_file; 
FILE* perf_file;


// MLFQ specific structures
struct MLFQ 
{
    CircularQueue *queues[NO_OF_PRIORITIES]; // Array of queues for each priority level
    int current_level; // Current priority level being serviced
    int time_quantum; // Base time quantum
};

// Initialize MLFQ
struct MLFQ* createMLFQ(int max_processes, int quantum) 
{
    struct MLFQ *mlfq = (struct MLFQ*)malloc(sizeof(struct MLFQ));
    mlfq->time_quantum = quantum;
    mlfq->current_level = 0;
    
    // Initialize queues for each priority level
    for (int i = 0; i < NO_OF_PRIORITIES; i++) 
    {
        mlfq->queues[i] = createCircularQueue(max_processes);
    }
    
    return mlfq;
}

// Add process to appropriate queue
void addToMLFQ(struct MLFQ *mlfq, struct PCB *pcb) 
{
    int priority = pcb->current_priority;
    enqueueCircularQueue(mlfq->queues[priority], pcb->process);
}

// Add a function to reset process priority to its original level
void resetProcessPriority(struct PCB *process) 
{
    process->current_priority = process->original_priority;
}


void scheduleNextProcess(Process nextProcess,PCB* pcb[]);
void logProcessfinished(FILE* log_file, PCB* pcb, const char* state, int time);
void logSchedulerPerformance(int processCount);
// Function prototypes
void handleProcessCompletion(int signum);
void logProcessEvent(FILE* log_file, PCB* pcb, const char* state, int time);
void clearResources(int signum);
void checkChildStatus();


void write_log (struct PCB *process, const char *state, int current_time)
{
    if (process->state == 2)    // check if finished
    {
        fprintf (log_file, "At time %d process %d %s arr %d total %d remain %d wait %d TA %d WTA %.2f%%\n", 
                current_time, 
                process->process.id, 
                state, 
                process->process.arrival_time, 
                process->process.runtime, 
                process->remaining_time, 
                process->waiting_time,
                process->TA,
                process->WTA); 
    }
    else
    {
        fprintf (log_file, "At time %d process %d %s arr %d total %d remain %d wait %d\n", 
                current_time, 
                process->process.id, 
                state, 
                process->process.arrival_time, 
                process->process.runtime, 
                process->remaining_time, 
                process->waiting_time); 
    }  
    fflush (log_file);         
}


void calculate_performance(int total_processes, struct PCB *pcb)
{
    float total_WTA = 0;
    float total_waiting = 0;

    for (int i = 0; i < total_processes; i++)
    {
        total_WTA += pcb[i].WTA;
        total_waiting += pcb[i].waiting_time;
    }

    float avg_WTA = total_WTA / total_processes;
    float avg_waiting = total_waiting / total_processes;

    float CPU_utilization = (float)busy_time / total_time * 100;

    fprintf(perf_file, "CPU utilization = %.2f%%\n", CPU_utilization);
    fprintf(perf_file, "Avg WTA = %.2f\n", avg_WTA);
    fprintf(perf_file, "Avg Waiting = %.2f\n", avg_waiting);

    fflush (perf_file);
}


int processCount = 0;
int main(int argc, char* argv[]) 
{
    signal(SIGINT, clearResources);
    signal(SIGCHLD, handleProcessCompletion);  // Handle child process termination
    initClk();
    int algo=atoi(argv[1]);
    int current_time;
    int process_count = atoi(argv[2]);
    int quanta = (argc > 3) ? atoi(argv[3]) : 0;
    if (quanta != 0)
        printf("quanta is %d\n", quanta);

    Process_Wait=(int*)malloc(sizeof(int)*process_count);
    WTA=(float*)malloc(sizeof(float)*process_count);
   
    // Initialize runtime-based priority queue
    // Adjust as needed
    // readyQueue = createPriorityQueue(process_count);
    int msgQid=msgget(MSQKEY,0666|IPC_CREAT);
    // Open log file
    log_file = fopen("scheduler.log", "w");
    if (!log_file) {
        perror("Failed to open scheduler.log");
        exit(1);
    }
    if(algo==1)
    {
        PCB pcb [process_count];
        int total_processes = 0; 
        readyQueue = createPriorityQueue(process_count);
        struct msgbuff1
        {
            Process p;
            //int remainingTime;
            
        }arrivingProcess;

        while (true) 
        {
            // Check for incoming processes
            int i = 0;
            current_time = getClk();
            while (msgrcv(msgQid, &arrivingProcess, sizeof(arrivingProcess), 0, IPC_NOWAIT) != -1) 
            {
                PCB newPCB = {
                    .process = arrivingProcess.p,
                    .state = Ready,
                    .remaining_time = arrivingProcess.p.runtime,
                    .start_time = -1,
                    .finish_time = -1,
                    .waiting_time = 0,
                    .pid = -1  // Not yet forked
                };
                pcb [i] = newPCB;
                insertRuntimePriorityQueue(readyQueue, pcb[i].process);
                processCount++;
                i++;
            }

            // If no process is running, select the next process
            if (currentPCB == NULL && readyQueue->size > 0) {
                Process nextProcess = removeRuntimePriorityQueue(readyQueue);

                // Fork and execute the process
                pid_t pid = fork();
                if (pid == 0) {
                    // Child process simulates execution
                    char runtimeChar[20];
                    sprintf(runtimeChar, "%d", nextProcess.runtime);
                    execl("./process.out", "./process.out", runtimeChar, (char*)NULL);
                    perror("Failed to execute process");
                    exit(1);
                } else if (pid > 0) {
                    // Parent process tracks the process
                    currentPCB = (PCB*)malloc(sizeof(PCB));
                    currentPCB->process = nextProcess;
                    currentPCB->pid = pid;
                    currentPCB->state = RUNNING;
                    currentPCB->start_time = getClk();
                    currentPCB->remaining_time = nextProcess.runtime;
                    
                    logProcessEvent(log_file, currentPCB, "started", currentPCB->start_time);
                } else {
                    perror("Fork failed");
                    exit(1);
                }
            }

            // Exit condition: no running process and no more processes in the queue
            if(!currentPCB && (lasttime+1) == getClk())
            {Waiting++;
                printf("Waiting= %d \n",Waiting);
                lasttime = getClk();
            
            }

            while(current_time > getClk());  // Simulate one time unit
        }
    }
    else if (algo == 2) 
    {
        readyQueue = createPriorityQueue(process_count);
        current_time = getClk();

        bool recieved=false;
        PCB* pcb[process_count];
        for (int i = 0; i < process_count; i++) {
            pcb[i] = NULL;
        }

        struct msgbuff {
            Process p;
        } arrivingProcess;

    while (true) 
    {
        // Check for incoming processes
        PCB* insertedPCB = NULL;
        if (msgrcv(msgQid, &arrivingProcess, sizeof(arrivingProcess), 0, IPC_NOWAIT) != -1) {
            PCB* newPCB = (PCB*)malloc(sizeof(PCB));
            *newPCB = (PCB){
                .process = arrivingProcess.p,
                .state = Ready,
                .remaining_time = arrivingProcess.p.runtime,
                .start_time = -1,
                .finish_time = -1,
                .waiting_time = 0,
                .pid = -1
            };
            
            insertedPCB = newPCB;
            
            printf("process%d recieved",arrivingProcess.p.id);
            
            
        }

        if (currentPCB) {
            // Preemption logic
            if (insertedPCB && currentPCB->process.priority > insertedPCB->process.priority) {
                printf("Preempting process %d (pid=%d)\n", currentPCB->process.id, currentPCB->pid);
                kill(currentPCB->pid, SIGSTOP);  // Stop current process
                 int i = 0;
                bool found = false;
                while (i < process_count && !found) {
                    if (pcb[i] && pcb[i]->process.id == currentPCB->process.id)
                    {int elapsed_time=getClk()-Start_execution;
                     pcb[i]->remaining_time-=elapsed_time;
                     Start_execution=0;
                       pcb[i]->Last_execution=getClk(); 
                    }
                    
                    i++;
                }
                currentPCB->process.prempted = true;
                currentPCB->state = Blocked;
                logProcessEvent(log_file, currentPCB, "Blocked", getClk());
                insertPriorityPriorityQueue(readyQueue, currentPCB->process);  // Reinsert current process
                currentPCB = NULL;
                scheduleNextProcess(insertedPCB->process, pcb);
                free(insertedPCB);
                insertedPCB = NULL;
                recieved=true;
        
            }
            else
            {
                 if (insertedPCB) {
                printf("I am inserted process %d",insertedPCB->process.id);
                insertPriorityPriorityQueue(readyQueue, insertedPCB->process);
                free(insertedPCB);
                insertedPCB = NULL;
                recieved=true;
            }

            }

        } 
        
        else {
            // If no current process, add new process to the queue
            if (insertedPCB) {
                printf("I am inserted process %d",insertedPCB->process.id);
                insertPriorityPriorityQueue(readyQueue, insertedPCB->process);
                free(insertedPCB);
                insertedPCB = NULL;
                recieved=true;
            }
        }

        // Schedule or resume the next process
        if (currentPCB == NULL && readyQueue->size > 0) {
            Process nextProcess = removePriorityPriorityQueue(readyQueue);

            if (!nextProcess.prempted) {
                scheduleNextProcess(nextProcess, pcb);
            } else {
                int i = 0;
                bool found = false;
                while (i < process_count && !found) {
                    if (pcb[i] && pcb[i]->process.id == nextProcess.id) {
                        printf("Resuming process %d (pid=%d)\n", nextProcess.id, pcb[i]->pid);
                        found = true;
                        nextProcess.prempted = false;
                        kill(pcb[i]->pid, SIGCONT);  // Resume preempted process
                        pcb[i]->waiting_time =getClk()-pcb[i]->Last_execution+(pcb[i]->start_time-pcb[i]->process.arrival_time);
                        pcb[i]->state = RUNNING;
                        logProcessEvent(log_file, pcb[i], "resumed", getClk());
                        currentPCB = pcb[i];
                    }
                    i++;
                }
            }
        }

        // Exit condition: no running process and no more processes in the queue
       if(finished_counter==process_count)
       break;
        if(!currentPCB&&!insertedPCB)
        {Waiting++;
            printf("Waiting= %d \n",Waiting);
        
        }

        while(current_time + 1 > getClk());  // Simulate one time unit
    }
}

else if (algo == 3)
{
    CircularQueue *cq =  createCircularQueue (process_count);   // ready queue

    log_file = fopen("scheduler.log", "w");
    perf_file = fopen("scheduler.perf", "w");

    int current_time = getClk();
    int total_processes = 0;
    int completed_processes = 0;
    int receivedProcessCount = 0;

    PCB *current_process = NULL;
    PCB pcb [process_count];

    struct msgbuff arrivingProcess;

    while (completed_processes < process_count)
    {   
        current_time = getClk();
        
        // Receive processes sent if any
        // fixed this by using while loop instead of if; this is why process 1 gave wrong output
        while (msgrcv (msgQid, &arrivingProcess, sizeof(arrivingProcess), 0, IPC_NOWAIT) != -1)
        { 
            printf ("Process %d received at time %d\n", arrivingProcess.p.id, getClk());
            struct PCB *new_pcb = &pcb[total_processes];

            new_pcb->process = arrivingProcess.p; 
            new_pcb->remaining_time = arrivingProcess.p.runtime; 
            new_pcb->state = 0;     // Waiting  
            new_pcb->waiting_time = 0; 
            new_pcb->executionTime = 0; 
            new_pcb->pid = -1; 

            pcb [total_processes] = *new_pcb;
            enqueueCircularQueue (cq, pcb [total_processes].process);
            total_processes++;
        }   
        

        if (current_process && current_process->state != 2)
        {
            enqueueCircularQueue (cq, current_process->process);
            current_process = NULL;
        }

        // loop again if queue is empty
        if (isCircularQueueEmpty(cq)) 
        { 
            continue; 
        }

        // Select process to run
        Process currentProcess = dequeueCircularQueue (cq);
        // Find PCB for this process 
        //struct PCB *current_process = NULL; 
        for (int i = 0; i < total_processes; i++) 
        {
            if (pcb[i].process.id == currentProcess.id) 
            {
                current_process = &pcb[i]; 
                break; 
            } 
        }


        // fork a new process or continue an existing one
        if (current_process->pid == -1)
        {
            current_process->start_time = current_time;
            current_process->pid = fork();
            if (current_process->pid == 0)
            {
                char remaining_time_str[10]; 
                //char quantum_str[10]; 

                sprintf(remaining_time_str, "%d", current_process->remaining_time); 
                //sprintf(quantum_str, "%d", quanta); 

                execl("./process.out", "./process.out", remaining_time_str, argv[1], argv[3], NULL); 
                perror("execl failed"); 
                exit(1);
            }
            current_process->waiting_time = current_time - current_process->process.arrival_time;
            write_log(current_process, "started", current_time);
        } 
        else 
        {
            kill (current_process->pid, SIGCONT);
            current_process->waiting_time = current_process->waiting_time + (current_time - current_process->Last_execution);
            write_log(current_process, "resumed", current_time);
        }


        current_process->state = 1;
        current_process->Last_execution = current_time;

        int run_time = (current_process->remaining_time > quanta) ? quanta : current_process->remaining_time;

        int start_time = getClk();
        int end_time = start_time + run_time;

        while (getClk() < end_time && current_process->remaining_time > 0);

        current_time = getClk();

        printf ("\n");

        current_process->remaining_time -= run_time; 
        current_process->executionTime += run_time; 
        busy_time += run_time;

        total_time = current_time;


        if (current_process->remaining_time <= 0)
        {
            current_process->TA = current_time - current_process->process.arrival_time;
            current_process->WTA = (float) current_process->TA / current_process->process.runtime;
            current_process->state = 2;     //finished

            write_log (current_process, "finished", current_time);

            int status;
            waitpid (current_process->pid, &status, 0);
            
            completed_processes++;
            current_process = NULL;
        }
        else 
        {
            kill (current_process->pid, SIGSTOP);
            write_log(current_process, "stopped", current_time);

            current_process->state = 0;  // Waiting
            current_process->Last_execution = current_time;

        }

    }
    calculate_performance (process_count, pcb);
}

else if (algo == 4)
{
    log_file = fopen("scheduler.log", "w");
    perf_file = fopen("scheduler.perf", "w");
    if (!log_file || !perf_file) 
    {
        perror("Error opening log/perf files");
        exit(1);
    }


    // Initialize MLFQ
    struct MLFQ *mlfq = createMLFQ(process_count, quanta);

    struct msgbuff arrivingProcess;
    struct PCB pcb[process_count];

    int current_time = getClk();
    int total_processes = 0;
    int completed_processes = 0;
    struct PCB *current_process = NULL;

    int prev_run_id;

    while (true) 
    {
        current_time = getClk();

        // Receive new processes
        // int rec_value = msgrcv(msgQid, &arrivingProcess, sizeof(arrivingProcess), 0, IPC_NOWAIT);
        // if (rec_value != -1) 
        while (msgrcv(msgQid, &arrivingProcess, sizeof(arrivingProcess), 0, IPC_NOWAIT) != -1)
        {
            printf("Process %d received at time %d\n", arrivingProcess.p.id, current_time);
            struct PCB *new_pcb = &pcb[total_processes];
            
            new_pcb->process = arrivingProcess.p;
            new_pcb->remaining_time = arrivingProcess.p.runtime;
            new_pcb->state = 0;
            new_pcb->waiting_time = 0;
            new_pcb->executionTime = 0;
            new_pcb->pid = -1;
            new_pcb->original_priority = arrivingProcess.p.priority;
            new_pcb->current_priority = arrivingProcess.p.priority;
            
            // Add to appropriate queue level
            pcb [total_processes] = *new_pcb;
            addToMLFQ(mlfq, &pcb [total_processes]);
            total_processes++;
        }

        if (current_process && current_process->state != 2)
        {
            addToMLFQ(mlfq, current_process);
            current_process = NULL;
        }

        // Find highest priority non-empty queue
        int current_level = 0;
        while (current_level < NO_OF_PRIORITIES && isCircularQueueEmpty(mlfq->queues[current_level])) 
        {
            current_level++;
        }
        
        if (current_level < NO_OF_PRIORITIES) 
        {
            Process currentProcess = dequeueCircularQueue(mlfq->queues[current_level]);
           // struct PCB *current_process = NULL;
            
            // Find PCB for this process
            for (int i = 0; i < total_processes; i++) 
            {
                if (pcb[i].process.id == currentProcess.id) 
                {
                    current_process = &pcb[i];
                    break;
                }
            }

            if (current_process) 
            {
                // Start or resume process
                if (current_process->pid == -1) 
                {
                    current_process->start_time = current_time;
                    current_process->pid = fork();
                    if (current_process->pid == 0) 
                    {
                        char remaining_time_str[10];
                        sprintf(remaining_time_str, "%d", current_process->remaining_time);
                        execl("./process.out", "./process.out", remaining_time_str, "4", argv[3], NULL);
                        exit(1);
                    }
                    current_process->waiting_time = current_time - current_process->process.arrival_time;
                    write_log(current_process, "started", current_time);
                } else 
                {
                    kill(current_process->pid, SIGCONT);

                    if (prev_run_id != current_process->process.id)
                        current_process->waiting_time += (current_time - current_process->Last_execution);
                    write_log(current_process, "resumed", current_time);
                }
                
                current_process->state = 1;
                current_process->Last_execution = current_time;
                
                // Run for quantum
                int start_time = getClk();
                 int run_time = (current_process->remaining_time > quanta) ? quanta : current_process->remaining_time;
                int end_time = start_time + run_time;
                
                while (getClk() < end_time && current_process->remaining_time > 0) 
                {
                    // Wait for quantum to complete
                }
                
                current_time = getClk();
                int execution_time = current_time - start_time;
                current_process->remaining_time -= execution_time;
                current_process->executionTime += execution_time;
                busy_time += execution_time;
                total_time = current_time;
                
                if (current_process->remaining_time <= 0) 
                {
                    // Process completed
                    current_process->TA = current_time - current_process->process.arrival_time;
                    current_process->WTA = (float)current_process->TA / current_process->process.runtime;
                    current_process->state = 2;
                    
                    write_log(current_process, "finished", current_time);
                    int status;
                    waitpid(current_process->pid, &status, 0);
                    completed_processes++;
                } else 
                {
                    // Process not finished, move to lower priority
                    kill(current_process->pid, SIGSTOP);

                    // If at the lowest priority, reset to original priority
                    if (current_process->current_priority >= NO_OF_PRIORITIES - 1) 
                    {
                        resetProcessPriority(current_process);
                    } else 
                    {
                        // Move to a lower priority queue
                        current_process->current_priority++;
                    }
                    prev_run_id = current_process->process.id;
                    current_process->Last_execution = current_time;

                    //addToMLFQ(mlfq, current_process);
                    write_log(current_process, "stopped", current_time);
                }
            }
        }
        
    }
    calculate_performance(process_count, pcb);
}

    
    // Finalize and cleanup 
    if (algo < 3)
        logSchedulerPerformance(finished_counter); //Not working for some reason implementation below
    fclose(log_file);
    destroyPriorityQueue(readyQueue);
    //clearResources(0);
    
    return 0;
}

// Handle child process termination
void handleProcessCompletion(int signum) {
    int status;
    pid_t pid = waitpid(-1, &status, WNOHANG);
    if (pid > 0 && currentPCB != NULL && currentPCB->pid == pid) {
        currentPCB->finish_time = getClk();
        currentPCB->state = FINISHED;

       
        
        kill(getppid(),SIGUSR1);
        finished_counter++;
        printf("finishedcounter=%d \n",finished_counter);
          // Calculate waiting time
        currentPCB->waiting_time = currentPCB->finish_time - currentPCB->process.arrival_time - currentPCB->process.runtime;   
        // Log completion
        logProcessfinished(log_file, currentPCB, "finished", currentPCB->finish_time);


       
        if(currentPCB)
        free(currentPCB);
        currentPCB = NULL;
    }
}

// Log process events
void logProcessEvent(FILE* log_file, PCB* pcb, const char* state, int time) {
    fprintf(log_file, "At time %d process %d %s arr %d total %d remain %d wait %d\n",
            time, pcb->process.id, state, pcb->process.arrival_time,
            pcb->process.runtime, pcb->remaining_time, pcb->waiting_time);
}
void logProcessfinished(FILE* log_file, PCB* pcb, const char* state, int time) {
    int TA_=time-pcb->process.arrival_time;
    float WTA_=(float)TA_/(pcb->process.runtime);
    Process_Wait[finished_counter-1]=pcb->waiting_time;
    WTA[finished_counter-1]=WTA_ ;
    fprintf(log_file, "At time %d process %d %s arr %d total %d remain %d wait %d TA %d WTA %f \n ",
            time, pcb->process.id, state, pcb->process.arrival_time,
            pcb->process.runtime, 0,pcb->waiting_time,TA_,WTA_ );
}

// Cleanup resources
void clearResources(int signum) {
    logSchedulerPerformance(processCount);
    printf("Cleaning up resources as Scheduler...\n");
     destroyPriorityQueue(readyQueue);
     free(WTA);
     free(Process_Wait);
     printf("Exiting\n");
     exit(1);
    
}
void logSchedulerPerformance(int processCount) {
    float totalWTA = 0;
    float totalWait = 0;

    for (int i = 0; i < processCount; i++) 
    { totalWTA+=WTA[i];
      totalWait+=Process_Wait[i];
    }

    float avgWTA = totalWTA / processCount;
    float avgWait = totalWait / processCount;
    int usedtime=getClk()-Waiting;
    float cpuUtilization = ((float)usedtime / getClk()) * 100;

    FILE* perf_file = fopen("scheduler.perf", "w");
    if (!perf_file) {
        perror("Failed to open scheduler.perf");
        return;
    }

    fprintf(perf_file, "CPU utilization = %.2f%%\n", cpuUtilization);
    fprintf(perf_file, "Avg WTA = %.2f\n", avgWTA);
    fprintf(perf_file, "Avg Waiting = %.2f\n", avgWait);

    fclose(perf_file) ;
    
    
}
void scheduleNextProcess(Process nextProcess,PCB* pcb[]) {
    pid_t pid = fork();
            if (pid == 0) {
                // Child process simulates execution
                char runtimeChar[20];
                sprintf(runtimeChar, "%d", nextProcess.runtime);
                execl("./process.out", "./process.out", runtimeChar, (char*)NULL);
                perror("Failed to execute process");
                exit(1);
            } else if (pid > 0) {
                // Parent process tracks the process
                currentPCB = (PCB*)malloc(sizeof(PCB));
                Start_execution=getClk();
                currentPCB->process = nextProcess;
                currentPCB->pid = pid;
                currentPCB->state = RUNNING;
                currentPCB->start_time = getClk();
                currentPCB->waiting_time=getClk()-currentPCB->process.arrival_time;
                currentPCB->remaining_time = nextProcess.runtime;
                pcb[Processes_entered]=currentPCB;
                Processes_entered++;
                printf(" I am process%d with pid=%d \n", currentPCB->process.id,currentPCB->pid);
                logProcessEvent(log_file, currentPCB, "started", currentPCB->start_time);
            } else {
                perror("Fork failed");
                exit(1);
            }
}
