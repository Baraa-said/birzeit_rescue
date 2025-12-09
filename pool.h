// pool.h
#ifndef POOL_H
#define POOL_H

#include <sys/types.h>
#include "types.h"
#include "config.h"

extern int shmid;
extern int semid;
extern SharedData *shared;

void init_shared_memory(void);
void cleanup_shared_memory(void);
void create_process_pool(pid_t *pids);
void wait_for_workers(pid_t *pids);

// نخليهم متاحين للـ main
void lock_sem(void);
void unlock_sem(void);

#endif
//**********************************************************************