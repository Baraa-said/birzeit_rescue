// pool.c
// to run------->(./rescue_robot)
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

union semun {
    int              val;
    struct semid_ds *buf;
    unsigned short  *array;
};

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

// ----- worker logic -----
static void worker_process(int worker_id) {
    // seed مختلف لكل عامل
    srand(time(NULL) ^ (worker_id * 7919));
    printf("[WORKER %d] Started (PID=%d)\n", worker_id, getpid());
    fflush(stdout);

    // كل عامل يشتغل على chunk من الـ population
    int chunk = config.population_size / config.num_processes;
    int start = worker_id * chunk;
    int end   = (worker_id == config.num_processes - 1)
              ? config.population_size
              : start + chunk;

    while (1) {
        // 1) فحص إذا خلصنا كل الأجيال
        lock_sem();
        int gen = shared->generation;
        if (gen >= config.num_generations) {
            // ما عاد في شغل
            unlock_sem();
            break;
        }
        unlock_sem();

        // 2) احسب الفتنس لجزء هذا العامل
        for (int i = start; i < end; i++) {
            calculate_fitness(&shared->population[i]);
        }

        // 3) barrier + GA logic (آخر عامل يعمل التطور)
        lock_sem();
        shared->workers_done++;
        int is_last = (shared->workers_done == config.num_processes);

        if (is_last) {
            // هذا آخر عامل يخلص – مسؤول عن:
            //  (1) reset counter
            //  (2) update best
            //  (3) check stagnation
            //  (4) evolve population / أو إيقاف
            shared->workers_done = 0;

            // حدث أفضل حل
            update_best_solution(shared);

            // فحص الستاجنيشن: لو ما تحسنش الفتنس كذا جيل – نوقف
            if (check_stagnation()) {
                // نختم الأجيال: هيك كل العمال رح يطلعوا من اللوب
                shared->generation = config.num_generations;
            } else {
                // نطوّر السكان لجيل جديد
                evolve_population(shared->population);

                // زيدي رقم الجيل
                shared->generation++;
            }
        }
        unlock_sem();

        // نعطي النظام شوية راحة صغيرة
        usleep(1000);
    }

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
//**********************************************************************