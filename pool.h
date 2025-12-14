#ifndef POOL_H
#define POOL_H

#include <sys/types.h>
#include "types.h"

extern int shmid;
extern int semid;
extern SharedData *shared;

void init_shared_memory(void);
void cleanup_shared_memory(void);

void create_process_pool(pid_t *pids);
void wait_for_workers(pid_t *pids);

void lock_sem(void);
void unlock_sem(void);

void write_data_file(int snapshot_num);
void write_astar_file(const char *filename, const Path *p);

#endif
