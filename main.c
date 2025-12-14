#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#include "types.h"
#include "config.h"
#include "genetic.h"
#include "pool.h"

static StartMode ask_start_mode(void) {
    printf("Choose robot starting position:\n");
    printf("1) Top surface (z = grid_z-1)   [good if robot enters from above]\n");
    printf("2) Edges/boundary (any 3D border) [good if robot enters from sides]\n");
    printf("3) Random anywhere (non-obstacle) [more exploration, less realistic]\n");
    printf("Enter choice (1-3): ");
    fflush(stdout);

    int choice = 0;
    if (scanf("%d", &choice) != 1) choice = 3;

    if (choice < 1 || choice > 3) choice = 3;
    return (StartMode)choice;
}

int main(int argc, char *argv[]) {
    const char *cfg = (argc > 1) ? argv[1] : "config.txt";
    read_config(cfg);
    print_config();

    // Ask user start mode
    g_start_mode = ask_start_mode();
    printf("✅ Start mode selected: %d\n\n", (int)g_start_mode);

    init_shared_memory();

    // Seed randomness once in main (workers will reseed)
    srand((unsigned int)time(NULL));

    init_environment(shared->grid, shared->survivors, shared->obstacles);
    init_population(shared->population);

    pid_t pids[config.num_processes];
    create_process_pool(pids);

    // Snapshot strategy: every 20 global generations + final
    int snapshot_interval = 20;
    int next_snapshot_gen = snapshot_interval;
    int snapshot_count = 0;

    time_t start_time = time(NULL);
    double last_best = -1e9;
    int stagnation = 0;

    int last_seen_gen = -1;

    while (1) {
        lock_sem();
        int gen = shared->generation;
        double best = shared->best_fitness;
        int stop = shared->stop_flag;
        unlock_sem();

        if (stop) break;

        // global-gen change
        if (gen != last_seen_gen) {
            last_seen_gen = gen;

            // stagnation tracking (only if enabled)
            if (config.stagnation_limit > 0) {
                if (best <= last_best + 1e-6) stagnation++;
                else { stagnation = 0; last_best = best; }

                if (stagnation >= config.stagnation_limit) {
                    printf("\n⏹️  Stopping: stagnation reached (%d generations without improvement)\n",
                           config.stagnation_limit);
                    lock_sem();
                    shared->stop_flag = 1;
                    unlock_sem();
                    break;
                }
            }

            // snapshots
            if (gen > 0 && gen >= next_snapshot_gen) {
                snapshot_count++;
                printf("\n📸 Saving snapshot %d at global generation %d (best=%.2f)\n",
                       snapshot_count, gen, best);
                lock_sem();
                write_data_file(snapshot_count);
                unlock_sem();
                next_snapshot_gen += snapshot_interval;
            }

            // print progress
            if (gen % 5 == 0 && gen > 0) {
                printf("GLOBAL Gen %d | Best Fitness: %.2f\n", gen, best);
            }
        }

        // time limit
        if (config.time_limit_seconds > 0) {
            time_t now = time(NULL);
            if ((int)difftime(now, start_time) >= config.time_limit_seconds) {
                printf("\n⏹️  Stopping: time limit reached (%d seconds)\n", config.time_limit_seconds);
                lock_sem();
                shared->stop_flag = 1;
                unlock_sem();
                break;
            }
        }

        if (gen >= config.num_generations) break;

        usleep(100000);
    }

    // Final snapshot always
    lock_sem();
    if (snapshot_count == 0 || (shared->generation % snapshot_interval) != 0) {
        snapshot_count++;
        printf("\n📸 Saving FINAL snapshot %d (gen=%d)\n", snapshot_count, shared->generation);
        write_data_file(snapshot_count);
    }
    unlock_sem();

    wait_for_workers(pids);

    print_final_result(shared);

    cleanup_shared_memory();
    return 0;
}
