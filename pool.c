#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>

#include "pool.h"
#include "config.h"
#include "genetic.h"

// union semun for SysV semctl
union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
#if defined(__linux__)
    struct seminfo *__buf;
#endif
};

int shmid = -1;
int semid = -1;
SharedData *shared = NULL;

static struct sembuf sem_lock_op   = {0, -1, SEM_UNDO};
static struct sembuf sem_unlock_op = {0,  1, SEM_UNDO};

void lock_sem(void) {
    if (semop(semid, &sem_lock_op, 1) == -1) { perror("semop lock"); _exit(1); }
}
void unlock_sem(void) {
    if (semop(semid, &sem_unlock_op, 1) == -1) { perror("semop unlock"); _exit(1); }
}

void write_data_file(int snapshot_num) {
    char filename[64];
    sprintf(filename, "robot_data_%d.txt", snapshot_num);

    FILE *f = fopen(filename, "w");
    if (!f) return;

    fprintf(f, "GRID: %d %d %d\n", config.grid_x, config.grid_y, config.grid_z);
    fprintf(f, "GEN: %d\n", shared->generation);
    fprintf(f, "FITNESS: %.2f\n", shared->best_fitness);

    fprintf(f, "SURVIVORS: %d\n", config.num_survivors);
    for (int i = 0; i < config.num_survivors; i++)
        fprintf(f, "%d %d %d %d\n",
                shared->survivors[i].x, shared->survivors[i].y, shared->survivors[i].z,
                shared->survivor_priority[i]);

    fprintf(f, "OBSTACLES: %d\n", config.num_obstacles);
    for (int i = 0; i < config.num_obstacles; i++)
        fprintf(f, "%d %d %d\n", shared->obstacles[i].x, shared->obstacles[i].y, shared->obstacles[i].z);

    fprintf(f, "PATH: %d\n", shared->best_path.length);
    for (int i = 0; i < shared->best_path.length; i++)
        fprintf(f, "%d %d %d\n",
                shared->best_path.genes[i].x, shared->best_path.genes[i].y, shared->best_path.genes[i].z);

    fclose(f);
}

void write_astar_file(const char *filename, const Path *p) {
    FILE *f = fopen(filename, "w");
    if (!f) return;

    fprintf(f, "GRID: %d %d %d\n", config.grid_x, config.grid_y, config.grid_z);
    fprintf(f, "GEN: -1\n");
    fprintf(f, "FITNESS: %.2f\n", p->fitness);

    fprintf(f, "SURVIVORS: %d\n", config.num_survivors);
    for (int i = 0; i < config.num_survivors; i++)
        fprintf(f, "%d %d %d %d\n",
                shared->survivors[i].x, shared->survivors[i].y, shared->survivors[i].z,
                shared->survivor_priority[i]);

    fprintf(f, "OBSTACLES: %d\n", config.num_obstacles);
    for (int i = 0; i < config.num_obstacles; i++)
        fprintf(f, "%d %d %d\n", shared->obstacles[i].x, shared->obstacles[i].y, shared->obstacles[i].z);

    fprintf(f, "PATH: %d\n", p->length);
    for (int i = 0; i < p->length; i++)
        fprintf(f, "%d %d %d\n", p->genes[i].x, p->genes[i].y, p->genes[i].z);

    fclose(f);
}

void init_shared_memory(void) {
    size_t shm_size =
        sizeof(SharedData) +
        (size_t)config.population_size * sizeof(Path) +
        (size_t)config.grid_x * config.grid_y * config.grid_z * sizeof(int) +
        (size_t)config.num_survivors * sizeof(Coord) +
        (size_t)config.num_survivors * sizeof(int) +
        (size_t)config.num_obstacles * sizeof(Coord);

    shmid = shmget(IPC_PRIVATE, shm_size, IPC_CREAT | 0666);
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

    shared->survivor_priority = (int *)ptr;
    ptr += (size_t)config.num_survivors * sizeof(int);

    shared->obstacles = (Coord *)ptr;

    semid = semget(IPC_PRIVATE, 1, IPC_CREAT | 0666);
    if (semid < 0) { perror("semget"); exit(1); }

    union semun arg;
    arg.val = 1;
    if (semctl(semid, 0, SETVAL, arg) < 0) { perror("semctl"); exit(1); }

    shared->generation   = 0;
    shared->workers_done = 0;
    shared->stop_flag    = 0;
    shared->best_fitness = -1e18;
}

void cleanup_shared_memory(void) {
    if (shared) shmdt(shared);
    if (shmid != -1) shmctl(shmid, IPC_RMID, NULL);
    if (semid != -1) semctl(semid, 0, IPC_RMID);
}

static void worker_process(int worker_id) {
    srand((unsigned int)(time(NULL) ^ (worker_id * 7919) ^ (getpid() << 16)));

    int subN = config.population_size / config.num_processes;
    if (subN < 10) subN = 10;

    Path *local = (Path *)malloc((size_t)subN * sizeof(Path));
    if (!local) _exit(1);

    for (int i = 0; i < subN; i++) {
        generate_random_path(&local[i]);
        calculate_fitness(&local[i]);
    }

    Path local_best = local[0];
    int local_gen = 0;

    while (1) {
        lock_sem();
        int stop = shared->stop_flag;
        int g = shared->generation;
        unlock_sem();

        if (stop || g >= config.num_generations) break;

        for (int i = 0; i < subN; i++)
            if (local[i].fitness > local_best.fitness) local_best = local[i];

        evolve_population_local(local, subN);
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

            // barrier wait with timeout safety
            int spins = 0;
            while (1) {
                lock_sem();
                int done = shared->workers_done;
                int stop2 = shared->stop_flag;
                unlock_sem();
                if (stop2 || done == 0) break;
                usleep(200);
                spins++;
                if (spins > 20000) { // ~4s
                    lock_sem();
                    shared->stop_flag = 1;
                    unlock_sem();
                    break;
                }
            }
        }
    }

    lock_sem();
    if (local_best.fitness > shared->best_fitness) {
        shared->best_fitness = local_best.fitness;
        shared->best_path = local_best;
    }
    unlock_sem();

    free(local);
    _exit(0);
}

void create_process_pool(pid_t *pids) {
    for (int i = 0; i < config.num_processes; i++) {
        pid_t pid = fork();
        if (pid == 0) worker_process(i);
        pids[i] = pid;
    }
}

void wait_for_workers(pid_t *pids) {
    for (int i = 0; i < config.num_processes; i++)
        waitpid(pids[i], NULL, 0);
}
