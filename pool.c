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
    if (!f) return;

    fprintf(f, "GRID: %d %d %d\n", config.grid_x, config.grid_y, config.grid_z);
    fprintf(f, "FITNESS: %.2f\n", shared->best_fitness);

    fprintf(f, "SURVIVORS: %d\n", config.num_survivors);
    for (int i = 0; i < config.num_survivors; i++)
        fprintf(f, "%d %d %d\n",
                shared->survivors[i].x,
                shared->survivors[i].y,
                shared->survivors[i].z);

    fprintf(f, "OBSTACLES: %d\n", config.num_obstacles);
    for (int i = 0; i < config.num_obstacles; i++)
        fprintf(f, "%d %d %d\n",
                shared->obstacles[i].x,
                shared->obstacles[i].y,
                shared->obstacles[i].z);

    fprintf(f, "PATH: %d\n", shared->best_path.length);
    for (int i = 0; i < shared->best_path.length; i++)
        fprintf(f, "%d %d %d\n",
                shared->best_path.genes[i].x,
                shared->best_path.genes[i].y,
                shared->best_path.genes[i].z);

    fclose(f);
}

// ----- Write individual worker result -----
void write_worker_data_file(int worker_id, Path *worker_best) {
    char filename[64];
    sprintf(filename, "robot_data_worker_%d.txt", worker_id);

    FILE *f = fopen(filename, "w");
    if (!f) return;

    fprintf(f, "GRID: %d %d %d\n", config.grid_x, config.grid_y, config.grid_z);
    fprintf(f, "FITNESS: %.2f\n", worker_best->fitness);
    fprintf(f, "WORKER_ID: %d\n", worker_id);

    fprintf(f, "SURVIVORS: %d\n", config.num_survivors);
    for (int i = 0; i < config.num_survivors; i++)
        fprintf(f, "%d %d %d\n",
                shared->survivors[i].x,
                shared->survivors[i].y,
                shared->survivors[i].z);

    fprintf(f, "OBSTACLES: %d\n", config.num_obstacles);
    for (int i = 0; i < config.num_obstacles; i++)
        fprintf(f, "%d %d %d\n",
                shared->obstacles[i].x,
                shared->obstacles[i].y,
                shared->obstacles[i].z);

    fprintf(f, "PATH: %d\n", worker_best->length);
    for (int i = 0; i < worker_best->length; i++)
        fprintf(f, "%d %d %d\n",
                worker_best->genes[i].x,
                worker_best->genes[i].y,
                worker_best->genes[i].z);

    fclose(f);

    // printf("💾 [WORKER %d] Path saved (Fitness: %.2f)\n", worker_id, worker_best->fitness);
}

// ----- shared memory init / cleanup -----
void init_shared_memory(void) {
    int shm_size = sizeof(SharedData)
        + config.population_size * sizeof(Path)
        + config.grid_x * config.grid_y * config.grid_z * sizeof(int)
        + config.num_survivors * sizeof(Coord)
        + config.num_obstacles * sizeof(Coord);

    shmid = shmget(IPC_PRIVATE, shm_size, IPC_CREAT | 0666);
    if (shmid < 0) exit(1);

    shared = (SharedData *)shmat(shmid, NULL, 0);
    if (shared == (void *)-1) exit(1);

    char *ptr = (char *)shared + sizeof(SharedData);

    shared->population = (Path *)ptr;
    ptr += config.population_size * sizeof(Path);

    shared->grid = (int *)ptr;
    ptr += config.grid_x * config.grid_y * config.grid_z * sizeof(int);

    shared->survivors = (Coord *)ptr;
    ptr += config.num_survivors * sizeof(Coord);

    shared->obstacles = (Coord *)ptr;

    semid = semget(IPC_PRIVATE, 1, IPC_CREAT | 0666);
    if (semid < 0) exit(1);

    union semun arg;
    arg.val = 1;
    semctl(semid, 0, SETVAL, arg);

    shared->generation   = 0;
    shared->workers_done = 0;
    shared->best_fitness = -1e9;
}

void cleanup_shared_memory(void) {
    if (shared) shmdt(shared);
    if (shmid != -1) shmctl(shmid, IPC_RMID, NULL);
    if (semid != -1) semctl(semid, 0, IPC_RMID);
}

// ----- WORKER PROCESS -----
static void worker_process(int worker_id) {
    srand(time(NULL) ^ (worker_id * 7919) ^ (getpid() << 16));

    // printf("[WORKER %d] Started (PID=%d)\n", worker_id, getpid());

    int sub_pop_size = config.population_size / config.num_processes;
    if (sub_pop_size < 5) sub_pop_size = 5;

    Path *local_population = malloc(sub_pop_size * sizeof(Path));
    if (!local_population) _exit(1);

    for (int i = 0; i < sub_pop_size; i++) {
        generate_random_path(&local_population[i]);
        calculate_fitness(&local_population[i]);
    }

    Path local_best;
    local_best.fitness = -1e9;

    int generation = 0;

    while (1) {
        lock_sem();
        if (shared->generation >= config.num_generations) {
            unlock_sem();
            break;
        }
        unlock_sem();

        for (int i = 0; i < sub_pop_size; i++)
            if (local_population[i].fitness > local_best.fitness)
                local_best = local_population[i];

        evolve_population(local_population);

        for (int i = 0; i < sub_pop_size; i++)
            calculate_fitness(&local_population[i]);

        generation++;

        if (generation % 5 == 0) {
            lock_sem();

            if (local_best.fitness > shared->best_fitness) {
                shared->best_fitness = local_best.fitness;
                shared->best_path = local_best;

                // printf("🏆 NEW GLOBAL BEST by worker %d\n", worker_id);
            }

            shared->workers_done++;
            if (shared->workers_done == config.num_processes) {
                shared->workers_done = 0;
                shared->generation++;
            }

            unlock_sem();

            while (1) {
                lock_sem();
                int done = shared->workers_done;
                unlock_sem();
                if (done == 0) break;
                usleep(100);
            }
        }

        usleep(500);
    }

    lock_sem();
    if (local_best.fitness > shared->best_fitness) {
        shared->best_fitness = local_best.fitness;
        shared->best_path = local_best;
    }
    unlock_sem();

    write_worker_data_file(worker_id, &local_best);

    free(local_population);

    // printf("[WORKER %d] Shutting down after %d generations\n", worker_id, generation);

    _exit(0);
}

// ----- pool creation / waiting -----
void create_process_pool(pid_t *pids) {
    for (int i = 0; i < config.num_processes; i++) {
        pid_t pid = fork();
        if (pid == 0) worker_process(i);
        else pids[i] = pid;
    }
}

void wait_for_workers(pid_t *pids) {
    for (int i = 0; i < config.num_processes; i++)
        waitpid(pids[i], NULL, 0);
}
