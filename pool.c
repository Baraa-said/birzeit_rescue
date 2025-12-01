#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <time.h>
#include "pool.h"
#include "genetic.h"

// Semaphore operations
struct sembuf sem_lock = {0, -1, SEM_UNDO};
struct sembuf sem_unlock = {0, 1, SEM_UNDO};

void lock_sem() {
    semop(semid, &sem_lock, 1);
}

void unlock_sem() {
    semop(semid, &sem_unlock, 1);
}

// Initialize shared memory
void init_shared_memory() {
    // Calculate shared memory size
    int shm_size = sizeof(SharedData) + 
                   config.population_size * sizeof(Path) +
                   config.grid_x * config.grid_y * config.grid_z * sizeof(int) +
                   config.num_survivors * sizeof(Coord) +
                   config.num_obstacles * sizeof(Coord);
    
    // Create shared memory
    shmid = shmget(IPC_PRIVATE, shm_size, IPC_CREAT | 0666);
    if (shmid < 0) {
        perror("shmget");
        exit(1);
    }
    
    // Attach shared memory
    shared = (SharedData *)shmat(shmid, NULL, 0);
    if (shared == (void *)-1) {
        perror("shmat");
        exit(1);
    }
    
    // Set up pointers in shared memory
    char *ptr = (char *)shared + sizeof(SharedData);
    shared->population = (Path *)ptr;
    ptr += config.population_size * sizeof(Path);
    shared->grid = (int *)ptr;
    ptr += config.grid_x * config.grid_y * config.grid_z * sizeof(int);
    shared->survivors = (Coord *)ptr;
    ptr += config.num_survivors * sizeof(Coord);
    shared->obstacles = (Coord *)ptr;
    
    // Create semaphore
    semid = semget(IPC_PRIVATE, 1, IPC_CREAT | 0666);
    if (semid < 0) {
        perror("semget");
        exit(1);
    }
    semctl(semid, 0, SETVAL, 1);
    
    // Initialize shared data
    shared->generation = 0;
    shared->best_fitness = -1e9;
    shared->best_fitness_gen = 0;
    shared->workers_done = 0;
}

// Cleanup shared memory
void cleanup_shared_memory() {
    // Free all path genes in population
    for (int i = 0; i < config.population_size; i++) {
        if (shared->population[i].genes) {
            free(shared->population[i].genes);
        }
    }
    
    // Detach and remove shared memory
    shmdt(shared);
    shmctl(shmid, IPC_RMID, NULL);
    
    // Remove semaphore
    semctl(semid, 0, IPC_RMID);
}

// Worker process function
void worker_process(int worker_id) {
    srand(time(NULL) + worker_id);
    
    while (1) {
        lock_sem();
        
        // Check if we're done
        if (shared->generation >= config.num_generations) {
            unlock_sem();
            break;
        }
        
        // Calculate fitness for assigned paths
        int start = worker_id * (config.population_size / config.num_processes);
        int end = (worker_id + 1) * (config.population_size / config.num_processes);
        if (worker_id == config.num_processes - 1) {
            end = config.population_size;
        }
        
        unlock_sem();
        
        // Evaluate fitness for assigned range
        for (int i = start; i < end; i++) {
            calculate_fitness(&shared->population[i]);
        }
        
        // Synchronization barrier
        lock_sem();
        shared->workers_done++;
        
        // Last worker increments generation
        if (shared->workers_done == config.num_processes) {
            shared->workers_done = 0;
            shared->generation++;
        }
        unlock_sem();
        
        // Small delay to prevent busy waiting
        usleep(1000);
    }
    
    exit(0);
}

// Create process pool
void create_process_pool(pid_t *pids) {
    for (int i = 0; i < config.num_processes; i++) {
        pids[i] = fork();
        
        if (pids[i] < 0) {
            perror("fork");
            exit(1);
        }
        
        if (pids[i] == 0) {
            // Child process
            worker_process(i);
            // Worker process will exit when done
        }
    }
}

// Wait for all workers to finish
void wait_for_workers(pid_t *pids) {
    for (int i = 0; i < config.num_processes; i++) {
        waitpid(pids[i], NULL, 0);
    }
}