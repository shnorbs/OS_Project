//#include "headers.h"
//#include "Defined_DS.h"


// here was the PCB struct but I moved it into headers.h for other files to access it too
// typedef struct PCB PCB;
// typedef struct WaitingQueue WaitingQueue;

// // PHASE 2
// // ========================= MEMORY ========================
// typedef struct MemoryBlock 
// {
//     int start_address;
//     int size;
//     bool is_free;
//     struct MemoryBlock* next;
// }
// MemoryBlock;


// typedef struct BuddyAllocator 
// {
//     MemoryBlock* memory_list;
//     int total_size;
//     FILE* memory_log;
// } 
// BuddyAllocator;


// // Initialize buddy allocator
// BuddyAllocator* initBuddyAllocator(int total_size) 
// {
//     BuddyAllocator* allocator = (BuddyAllocator*) malloc (sizeof(BuddyAllocator));
//     allocator->total_size = total_size;
    
//     // Create initial free block
//     allocator->memory_list = (MemoryBlock*) malloc (sizeof(MemoryBlock));
//     allocator->memory_list->start_address = 0;
//     allocator->memory_list->size = total_size;
//     allocator->memory_list->is_free = true;
//     allocator->memory_list->next = NULL;
    
//     // Open memory log file
//     allocator->memory_log = fopen("memory.log", "w");
//     return allocator;
// }


// // Find smallest power of 2 that fits the requested size
// int getNextPowerOf2(int size) 
// {
//     int power = 1;
//     while (power < size)
//         power *= 2;
//     return power;
// }


// // Allocate memory
// MemoryBlock* allocateMemory(BuddyAllocator* allocator, int size, int process_id) 
// {
//     int required_size = getNextPowerOf2(size);
//     MemoryBlock* current = allocator->memory_list;
    
//     while (current != NULL) 
//     {
//         if (current->is_free && current->size >= required_size) 
//         {
//             // Split blocks until we get the right size
//             while (current->size > required_size) 
//             {
//                 int new_size = current->size / 2;
//                 MemoryBlock* buddy = (MemoryBlock*) malloc (sizeof(MemoryBlock));
                
//                 buddy->size = new_size;
//                 buddy->start_address = current->start_address + new_size;
//                 buddy->is_free = true;
//                 buddy->next = current->next;
                
//                 current->size = new_size;
//                 current->next = buddy;
//             }
            
//             current->is_free = false;
//             fprintf(allocator->memory_log, "At time %d allocated %d bytes for process %d from %d to %d\n",
//                     getClk(), size, process_id, current->start_address, 
//                     current->start_address + current->size - 1);
//             return current;
//         }
//         current = current->next;
//     }
//     return NULL;
// }


// bool areBuddies(MemoryBlock* block1, MemoryBlock* block2) 
// {
//     if (!block1 || !block2) return false;
//     if (block1->size != block2->size) return false;
    
//     // Check if blocks are properly aligned buddies
//     return (block1->start_address ^ block2->start_address) == block1->size;
// }


// // Free memory
// void freeMemory(BuddyAllocator* allocator, MemoryBlock* block, int process_id) 
// {
//     if (!block) return;
    
//     block->is_free = true;
//     fprintf(allocator->memory_log, "At time %d freed %d bytes from process %d from %d to %d\n",
//             getClk(), block->size, process_id, block->start_address, 
//             block->start_address + block->size - 1);
            
//     // Keep merging buddies until no more merges are possible
//     bool merged;
//     do {
//         merged = false;
//         MemoryBlock* current = allocator->memory_list;
//         MemoryBlock* prev = NULL;
        
//         while (current && current->next) {
//             MemoryBlock* buddy = current->next;
            
//             if (current->is_free && buddy->is_free && areBuddies(current, buddy)) {
//                 // Merge the buddies
//                 current->size *= 2;
//                 current->next = buddy->next;
//                 free(buddy);
//                 merged = true;
//                 break;
//             }
            
//             prev = current;
//             current = current->next;
//         }
//     } while (merged);
// }



// // =================== WAITING QUEUE =====================
// typedef struct Node
// {
//     PCB * pcb;
//     struct Node * next; 
// } Node;


// struct WaitingQueue
// {
//     Node * front;
//     Node * rear;
//     int size;
// } ;


// WaitingQueue * createWaitingQueue()
// {
//     WaitingQueue * waitingQ = (WaitingQueue*) malloc (sizeof(WaitingQueue));
//     waitingQ->front = waitingQ->rear = NULL;
//     waitingQ->size = 0;
//     return waitingQ;
// }


// void enqueueWaiting (WaitingQueue * waitingQ, PCB * pcb)
// {
//     Node * newNode = (Node*) malloc (sizeof(Node));
//     newNode->pcb = pcb;
//     newNode->next = NULL;

//     if (waitingQ->rear == NULL)
//     {
//         waitingQ->front = waitingQ->rear = newNode;
//     }
//     else
//     {
//         waitingQ->rear->next = newNode;
//         waitingQ->rear = newNode;
//     }

//     waitingQ->size ++;
// }


// PCB * dequeueWaiting (WaitingQueue * waitingQ)
// {
//     if (waitingQ->front == NULL)
//     {
//         return NULL;
//     }
//     else
//     {
//         Node * temp = waitingQ->front;
//         PCB * dequeued = temp->pcb;
//         waitingQ->front = waitingQ->front->next;

//         if (waitingQ->front == NULL)
//         {
//             waitingQ->rear = NULL;
//         }

//         free (temp);
//         waitingQ->size --;
//         return dequeued;
//     }
// }



// Define PCB structure
// struct PCB {
//     Process process;     // Process details
//     int state;           // Process state: WAITING, RUNNING, FINISHED, STOPPED
//     int remaining_time;  // Remaining runtime
//     int start_time;      // Start time
//     int finish_time;     // Finish time
//     int waiting_time;    // Waiting time
//     int TA;
//     int Last_execution;
//     int executionTime;
//     float WTA;
//     pid_t pid;           // Process ID after fork

//     // for mlfq
//     int current_priority; // Track current priority level
//     int original_priority; // Store original priority

//     // PHASE 2
//     MemoryBlock* memory_block;

// };