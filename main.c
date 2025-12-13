// main.c
// Updated to generate 5 data snapshots during evolution
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

#include "types.h"
#include "config.h"
#include "genetic.h"
#include "pool.h"

int main(int argc, char *argv[]) {
    // 1) Read configuration
    if (!read_config(argc > 1 ? argv[1] : "config.txt")) {
        fprintf(stderr, "Fatal: could not read config.\n");
        return 1;
    }
    print_config();

    // 2) Initialize shared memory + IPC
    init_shared_memory();

    // 3) Initialize environment + population
    init_environment(shared->grid, shared->survivors, shared->obstacles);
    init_population(shared->population);

    // 4) Create process pool
    pid_t pids[config.num_processes];
    create_process_pool(pids);

    // Take snapshots every 20 generations
    int snapshot_interval = 20;
    int num_snapshots = (config.num_generations / snapshot_interval);
    if (config.num_generations % snapshot_interval != 0) {
        num_snapshots++; // Add one more for the final generation
    }
    
    int *snapshot_generations = malloc(num_snapshots * sizeof(int));
    int snap_idx = 0;
    for (int gen = snapshot_interval; gen <= config.num_generations; gen += snapshot_interval) {
        snapshot_generations[snap_idx++] = gen;
    }
    // Make sure the last snapshot is at the final generation
    if (snapshot_generations[num_snapshots - 1] != config.num_generations) {
        snapshot_generations[num_snapshots - 1] = config.num_generations;
    }
    
    int next_snapshot = 0;
    
    printf("\n=== Data snapshots will be saved at generations: ");
    for (int i = 0; i < num_snapshots; i++) {
        printf("%d ", snapshot_generations[i]);
    }
    printf("===\n\n");

    // 5) Monitor generations and save snapshots
    int last_gen = -1;
    while (1) {
        lock_sem();
        int gen  = shared->generation;
        double best = shared->best_fitness;
        unlock_sem();

        if (gen != last_gen) {
            printf("Gen %3d: Best fitness = %.2f\n", gen, best);
            last_gen = gen;
            
            // Check if we should save a snapshot
            if (next_snapshot < num_snapshots && gen >= snapshot_generations[next_snapshot]) {
                lock_sem();
                printf("\n📸 Saving snapshot %d/%d (Generation %d)...\n", 
                       next_snapshot + 1, num_snapshots, gen);
                write_data_file(next_snapshot + 1);
                unlock_sem();
                next_snapshot++;
                printf("\n");
            }
        }

        if (gen >= config.num_generations) {
            break;
        }

        usleep(100000); // 0.1 seconds
    }

    // 6) Wait for workers and print final results
    wait_for_workers(pids);
    
    // Save final snapshot only if we haven't saved it yet
    lock_sem();
    int final_gen = shared->generation;
    if (next_snapshot < num_snapshots && 
        (next_snapshot == 0 || snapshot_generations[next_snapshot - 1] != final_gen)) {
        printf("\n📸 Saving final snapshot %d/%d (Generation %d)...\n", 
               next_snapshot + 1, num_snapshots, final_gen);
        write_data_file(next_snapshot + 1);
    }
    unlock_sem();
    
    print_final_result(shared);
    cleanup_shared_memory();
    
    free(snapshot_generations);

    printf("\n✅ Snapshots saved! Use visualize_robot.py to view them.\n");
    return 0;
}