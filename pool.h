// pool.h
#ifndef POOL_H
#define POOL_H

#include <sys/types.h>
#include "types.h"
#include "config.h"

extern int shmid;
extern int semid;
extern SharedData *shared;

// Shared memory management
void init_shared_memory(void);
void cleanup_shared_memory(void);

// Process pool management
void create_process_pool(pid_t *pids);
void wait_for_workers(pid_t *pids);

// Semaphore operations
void lock_sem(void);
void unlock_sem(void);

// Data output for visualization
void write_data_file(int snapshot_num);

#endif // POOL_H