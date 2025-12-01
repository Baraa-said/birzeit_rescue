#ifndef POOL_H
#define POOL_H

#include <sys/types.h>

// Semaphore operations
void lock_sem();
void unlock_sem();

// Shared memory management
void init_shared_memory();
void cleanup_shared_memory();

// Process pool management
void create_process_pool(pid_t *pids);
void wait_for_workers(pid_t *pids);
void worker_process(int worker_id);

#endif