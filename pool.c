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

// Fix for Linux/WSL: union semun may not be defined
union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
#if defined(__linux__)
    struct seminfo *__buf;
#endif
};

int shmid  = -1;
int semid  = -1;
SharedData *shared = NULL;

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

void write_data_file(int snapshot_num) {
    char filename[64];
    sprintf(filename, "robot_data_%d.txt", snapshot_num);

    FILE *f = fopen(filename, "w");
    if (!f) return;

    fprintf(f, "GRID: %d %d %d\n", config.grid_x, config.grid_y, config.grid_z);
    fprintf(f, "FITNESS: %.2f\n", shared->best_fitness);

    fprintf(f, "SURVIVORS: %d\n", config.num_survivors);
    for (int i = 0; i < config.num_survivors; i++)
        fprintf(f, "%d %d %d\n", shared->survivors[i].x, shared->survivors[i].y, shared->survivors[i].z);

    fprintf(f, "OBSTACLES: %d\n", config.num_obstacles);
    for (int i = 0; i < config.num_obstacles; i++)
        fprintf(f, "%d %d %d\n", shared->obstacles[i].x, shared->obstacles[i].y, shared->obstacles[i].z);

    fprintf(f, "PATH: %d\n", shared->best_path.length);
    for (int i = 0; i < shared->best_path.length; i++)
        fprintf(f, "%d %d %d\n",
                shared->best_path.genes[i].x,
                shared->best_path.genes[i].y,
                shared->best_path.genes[i].z);

    fclose(f);
}

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
        fprintf(f, "%d %d %d\n", shared->survivors[i].x, shared->survivors[i].y, shared->survivors[i].z);

    fprintf(f, "OBSTACLES: %d\n", config.num_obstacles);
    for (int i = 0; i < config.num_obstacles; i++)
        fprintf(f, "%d %d %d\n", shared->obstacles[i].x, shared->obstacles[i].y, shared->obstacles[i].z);

    fprintf(f, "PATH: %d\n", worker_best->length);
    for (int i = 0; i < worker_best->length; i++)
        fprintf(f, "%d %d %d\n",
                worker_best->genes[i].x,
                worker_best->genes[i].y,
                worker_best->genes[i].z);

    fclose(f);
}

void init_shared_memory(void) {
    int shm_size = (int)(sizeof(SharedData)
        + (size_t)config.population_size * sizeof(Path)
        + (size_t)config.grid_x * config.grid_y * config.grid_z * sizeof(int)
        + (size_t)config.num_survivors * sizeof(Coord)
        + (size_t)config.num_obstacles * sizeof(Coord));

    shmid = shmget(IPC_PRIVATE, (size_t)shm_size, IPC_CREAT | 0666);
    if (shmid < 0) { perror("shmget"); exit(1); }

    shared = (SharedData *)shmat(shmid, NULL, 0);
    if (shared == (void *)-1) { perror("shmat"); exit(1); }

    char *ptr = (char *)shared + sizeof(SharedData);

    shared->population = (Path *)ptr;
    ptr += (size_t)config.population_size * sizeof(Path);

    shared->grid = (int *)ptr;
    ptr += (size_t)config.grid_x * config.grid_y * config.grid_z * sizeof(int);

    shared->survivors = (Coord *)ptr;
    ptr += (size_t)config.num_survivors * sizeof(Coord);

    shared->obstacles = (Coord *)ptr;

    semid = semget(IPC_PRIVATE, 1, IPC_CREAT | 0666);
    if (semid < 0) { perror("semget"); exit(1); }

    union semun arg;
    arg.val = 1;
    if (semctl(semid, 0, SETVAL, arg) < 0) { perror("semctl SETVAL"); exit(1); }

    shared->generation   = 0;
    shared->workers_done = 0;
    shared->stop_flag    = 0;
    shared->best_fitness = -1e9;
}

void cleanup_shared_memory(void) {
    if (shared) shmdt(shared);
    if (shmid != -1) shmctl(shmid, IPC_RMID, NULL);
    if (semid != -1) semctl(semid, 0, IPC_RMID);
}

// forward declaration from genetic.c
void evolve_population_local(Path *population, int N);

static void worker_process(int worker_id) {
    srand((unsigned int)(time(NULL) ^ (worker_id * 7919) ^ (getpid() << 16)));

    int sub_pop_size = config.population_size / config.num_processes;
    if (sub_pop_size < 5) sub_pop_size = 5;

    Path *local_population = malloc((size_t)sub_pop_size * sizeof(Path));
    if (!local_population) _exit(1);

    for (int i = 0; i < sub_pop_size; i++) {
        generate_random_path(&local_population[i]);
        calculate_fitness(&local_population[i]);
    }

    Path local_best;
    local_best.fitness = -1e9;

    int local_gen = 0;

    while (1) {
        lock_sem();
        int stop = shared->stop_flag;
        int g = shared->generation;
        unlock_sem();

        if (stop || g >= config.num_generations)
            break;

        for (int i = 0; i < sub_pop_size; i++)
            if (local_population[i].fitness > local_best.fitness)
                local_best = local_population[i];

        evolve_population_local(local_population, sub_pop_size);

        local_gen++;

        if (local_gen % 5 == 0) {
            lock_sem();

            if (local_best.fitness > shared->best_fitness) {
                shared->best_fitness = local_best.fitness;
                shared->best_path = local_best;
            }

            shared->workers_done++;
            if (shared->workers_done == config.num_processes) {
                shared->workers_done = 0;
                shared->generation++;
            }

            unlock_sem();

            // barrier wait
            while (1) {
                lock_sem();
                int done = shared->workers_done;
                int stop2 = shared->stop_flag;
                unlock_sem();
                if (stop2 || done == 0) break;
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
    _exit(0);
}

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
