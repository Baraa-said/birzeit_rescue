// pool.c
// Updated with independent worker evolution (Island Model GA)
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <time.h>

#include "types.h"
#include "config.h"
#include "genetic.h"
#include "pool.h"

int shmid  = -1;
int semid  = -1;
SharedData *shared = NULL;

// ----- semaphore helpers -----
static struct sembuf sem_lock_op   = {0, -1, SEM_UNDO};
static struct sembuf sem_unlock_op = {0,  1, SEM_UNDO};

// union semun {
//     int              val;
//     struct semid_ds *buf;
//     unsigned short  *array;
// };

void lock_sem(void) {
    if (semop(semid, &sem_lock_op, 1) == -1) {
        perror("semop lock");
        _exit(1);
    }
}

void unlock_sem(void) {
    if (semop(semid, &sem_unlock_op, 1) == -1) {
        perror("semop unlock");
        _exit(1);
    }
}

// ----- Data file writing -----
void write_data_file(int snapshot_num) {
    char filename[64];
    sprintf(filename, "robot_data_%d.txt", snapshot_num);
    
    FILE *f = fopen(filename, "w");
    if (!f) {
        fprintf(stderr, "Error: Could not create %s\n", filename);
        return;
    }
    
    // Write grid dimensions
    fprintf(f, "GRID: %d %d %d\n", config.grid_x, config.grid_y, config.grid_z);
    
    // Write best fitness
    fprintf(f, "FITNESS: %.2f\n", shared->best_fitness);
    
    // Write survivors
    fprintf(f, "SURVIVORS: %d\n", config.num_survivors);
    for (int i = 0; i < config.num_survivors; i++) {
        fprintf(f, "%d %d %d\n", 
                shared->survivors[i].x,
                shared->survivors[i].y,
                shared->survivors[i].z);
    }
    
    // Write obstacles
    fprintf(f, "OBSTACLES: %d\n", config.num_obstacles);
    for (int i = 0; i < config.num_obstacles; i++) {
        fprintf(f, "%d %d %d\n",
                shared->obstacles[i].x,
                shared->obstacles[i].y,
                shared->obstacles[i].z);
    }
    
    // Write best path
    fprintf(f, "PATH: %d\n", shared->best_path.length);
    for (int i = 0; i < shared->best_path.length; i++) {
        fprintf(f, "%d %d %d\n",
                shared->best_path.genes[i].x,
                shared->best_path.genes[i].y,
                shared->best_path.genes[i].z);
    }
    
    fclose(f);
    printf("  → Snapshot %d saved to %s\n", snapshot_num, filename);
}

// ----- shared memory init/cleanup -----
void init_shared_memory(void) {
    int shm_size = sizeof(SharedData)
        + config.population_size * sizeof(Path)
        + config.grid_x * config.grid_y * config.grid_z * sizeof(int)
        + config.num_survivors * sizeof(Coord)
        + config.num_obstacles * sizeof(Coord);

    shmid = shmget(IPC_PRIVATE, shm_size, IPC_CREAT | 0666);
    if (shmid < 0) {
        perror("shmget");
        exit(1);
    }

    shared = (SharedData *)shmat(shmid, NULL, 0);
    if (shared == (void *)-1) {
        perror("shmat");
        exit(1);
    }

    char *ptr = (char *)shared + sizeof(SharedData);

    shared->population = (Path *)ptr;
    ptr += config.population_size * sizeof(Path);

    shared->grid = (int *)ptr;
    ptr += config.grid_x * config.grid_y * config.grid_z * sizeof(int);

    shared->survivors = (Coord *)ptr;
    ptr += config.num_survivors * sizeof(Coord);

    shared->obstacles = (Coord *)ptr;

    // semaphore
    semid = semget(IPC_PRIVATE, 1, IPC_CREAT | 0666);
    if (semid < 0) {
        perror("semget");
        exit(1);
    }

    union semun arg;
    arg.val = 1;
    if (semctl(semid, 0, SETVAL, arg) == -1) {
        perror("semctl SETVAL");
        exit(1);
    }

    shared->generation   = 0;
    shared->workers_done = 0;
    shared->best_fitness = -1e9;
}

void cleanup_shared_memory(void) {
    if (shared != NULL) {
        shmdt(shared);
        shared = NULL;
    }
    if (shmid != -1) {
        shmctl(shmid, IPC_RMID, NULL);
        shmid = -1;
    }
    if (semid != -1) {
        semctl(semid, 0, IPC_RMID);
        semid = -1;
    }
}

// ----- INDEPENDENT WORKER LOGIC (Island Model) -----
static void worker_process(int worker_id) {
    // Unique seed for each worker
    srand(time(NULL) ^ (worker_id * 7919) ^ (getpid() << 16));
    printf("[WORKER %d] Started (PID=%d) - Working independently\n", worker_id, getpid());
    fflush(stdout);

    // Each worker manages its own sub-population
    int sub_pop_size = config.population_size / config.num_processes;
    if (sub_pop_size < 5) sub_pop_size = 5; // Minimum 5 individuals per worker
    
    // Allocate local population for this worker
    Path *local_population = malloc(sub_pop_size * sizeof(Path));
    if (!local_population) {
        fprintf(stderr, "[WORKER %d] Failed to allocate local population\n", worker_id);
        _exit(1);
    }
    
    // Initialize local population independently
    for (int i = 0; i < sub_pop_size; i++) {
        generate_random_path(&local_population[i]);
        calculate_fitness(&local_population[i]);
    }
    
    // Track local best
    Path local_best;
    local_best.fitness = -1e9;
    
    printf("[WORKER %d] Managing %d paths independently\n", worker_id, sub_pop_size);
    fflush(stdout);
    
    int generation = 0;
    
    while (1) {
        // Check if we should stop
        lock_sem();
        int global_gen = shared->generation;
        if (global_gen >= config.num_generations) {
            unlock_sem();
            break;
        }
        unlock_sem();
        
        // === INDEPENDENT EVOLUTION ===
        // Each worker evolves its own population independently
        
        // 1) Find local best solution
        for (int i = 0; i < sub_pop_size; i++) {
            if (local_population[i].fitness > local_best.fitness) {
                local_best = local_population[i];
            }
        }
        
        // 2) Evolve local population (independent genetic operations)
        evolve_population(local_population);
        
        // Recalculate fitness for all individuals
        for (int i = 0; i < sub_pop_size; i++) {
            calculate_fitness(&local_population[i]);
        }
        
        generation++;
        
        // === PERIODIC SYNCHRONIZATION ===
        // Every 5 generations, workers share their best solutions
        if (generation % 5 == 0) {
            lock_sem();
            
            // Share local best with global population
            if (local_best.fitness > shared->best_fitness) {
                shared->best_fitness = local_best.fitness;
                shared->best_path = local_best;
                printf("[WORKER %d] Found new global best! Fitness: %.2f, Survivors: %d\n", 
                       worker_id, local_best.fitness, local_best.survivors_reached);
                fflush(stdout);
            }
            
            // Worker synchronization barrier
            shared->workers_done++;
            int is_last = (shared->workers_done == config.num_processes);
            
            if (is_last) {
                // Last worker increments generation counter
                shared->workers_done = 0;
                shared->generation++;
                
                // Optional: Implement migration (share solutions between islands)
                // This creates diversity by exchanging best solutions
            }
            
            unlock_sem();
            
            // Wait for all workers to sync
            while (1) {
                lock_sem();
                int done = shared->workers_done;
                unlock_sem();
                if (done == 0) break; // All workers synced
                usleep(100);
            }
        }
        
        // Small delay to prevent CPU thrashing
        usleep(500);
    }
    
    // Final contribution to global solution
    lock_sem();
    if (local_best.fitness > shared->best_fitness) {
        shared->best_fitness = local_best.fitness;
        shared->best_path = local_best;
        printf("[WORKER %d] Final best: Fitness=%.2f, Survivors=%d, Coverage=%d\n",
               worker_id, local_best.fitness, 
               local_best.survivors_reached, local_best.coverage);
    }
    unlock_sem();
    
    free(local_population);
    printf("[WORKER %d] Completed evolution independently\n", worker_id);
    fflush(stdout);
    
    _exit(0);
}

// ----- pool creation / waiting -----
void create_process_pool(pid_t *pids) {
    for (int i = 0; i < config.num_processes; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            exit(1);
        }
        if (pid == 0) {
            worker_process(i);
        } else {
            pids[i] = pid;
        }
    }
}

void wait_for_workers(pid_t *pids) {
    for (int i = 0; i < config.num_processes; i++) {
        if (pids[i] > 0) {
            waitpid(pids[i], NULL, 0);
        }
    }
}