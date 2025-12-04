#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include "types.h"
#include "config.h"
#include "genetic.h"
#include "pool.h"
#include <unistd.h>


// Global variables
Config config;
int shmid, semid;
SharedData *shared;

// Get current time in microseconds
double get_time() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
}

// Evolution function (shared by both modes)
void run_evolution(int use_multiprocess) {
    int stagnation_counter = 0;
    
    printf("Running genetic algorithm (%s)...\n\n", 
           use_multiprocess ? "Multi-Process" : "Single-Process");
    
    for (int gen = 0; gen < config.num_generations; gen++) {
        // Sort population by fitness
        qsort(shared->population, config.population_size, sizeof(Path), compare_fitness);
        
        // Track best fitness
        float best = shared->population[0].fitness;
        if (best > shared->best_fitness) {
            shared->best_fitness = best;
            shared->best_fitness_gen = gen;
            stagnation_counter = 0;
        } else {
            stagnation_counter++;
        }
        
        // Display progress every 10 generations
        if (gen % 10 == 0) {
            printf("Gen %3d: Best Fitness = %.2f, Survivors = %d, Coverage = %d\n",
                   gen, shared->population[0].fitness,
                   shared->population[0].survivors_reached,
                   shared->population[0].coverage);
        }
        
        // Check stagnation
        if (stagnation_counter >= config.stagnation_limit) {
            printf("\nStagnation detected at generation %d. Stopping early.\n", gen);
            break;
        }
        
        // Create new population
        Path *new_pop = malloc(config.population_size * sizeof(Path));
        int elite_count = (int)(config.population_size * config.elitism_percent);
        
        // Elitism: preserve top solutions
        for (int i = 0; i < elite_count; i++) {
            new_pop[i].genes = malloc(config.max_path_length * sizeof(Coord));
            new_pop[i].length = shared->population[i].length;
            memcpy(new_pop[i].genes, shared->population[i].genes, 
                   shared->population[i].length * sizeof(Coord));
            new_pop[i].fitness = shared->population[i].fitness;
            new_pop[i].survivors_reached = shared->population[i].survivors_reached;
            new_pop[i].coverage = shared->population[i].coverage;
        }
        
        // Generate rest through crossover and mutation
        for (int i = elite_count; i < config.population_size; i++) {
            int p1 = tournament_selection();
            int p2 = tournament_selection();
            
            if ((float)rand() / RAND_MAX < config.crossover_rate) {
                crossover(&shared->population[p1], &shared->population[p2], &new_pop[i]);
            } else {
                new_pop[i].genes = malloc(config.max_path_length * sizeof(Coord));
                new_pop[i].length = shared->population[p1].length;
                memcpy(new_pop[i].genes, shared->population[p1].genes,
                       shared->population[p1].length * sizeof(Coord));
            }
            
            mutate(&new_pop[i]);
            
            // Single-process: calculate fitness here
            if (!use_multiprocess) {
                calculate_fitness(&new_pop[i]);
            }
        }
        
        // Replace population
        for (int i = 0; i < config.population_size; i++) {
            free_path(&shared->population[i]);
            shared->population[i] = new_pop[i];
        }
        free(new_pop);
        
        // Multi-process: trigger worker fitness calculation
        if (use_multiprocess) {
            shared->generation = gen;
            shared->workers_done = 0;
            
            // Wait for all workers to complete fitness calculation
            while (shared->workers_done < config.num_processes) {
                usleep(100);
            }
        }
    }
}

// Display results
void display_results() {
    // Final sort
    qsort(shared->population, config.population_size, sizeof(Path), compare_fitness);
    
    printf("\n=== Final Results ===\n");
    printf("Best Fitness: %.2f\n", shared->population[0].fitness);
    printf("Survivors Reached: %d / %d\n", 
           shared->population[0].survivors_reached, config.num_survivors);
    printf("Coverage: %d cells\n", shared->population[0].coverage);
    printf("Path Length: %d\n", shared->population[0].length);
    
    printf("\nOptimized Path (first 20 coordinates):\n");
    int display_len = shared->population[0].length < 20 ? shared->population[0].length : 20;
    for (int i = 0; i < display_len; i++) {
        printf("  [%2d,%2d,%2d]", 
               shared->population[0].genes[i].x,
               shared->population[0].genes[i].y,
               shared->population[0].genes[i].z);
        if ((i + 1) % 5 == 0) printf("\n");
    }
    if (shared->population[0].length > 20) {
        printf("  ... (%d more coordinates)\n", shared->population[0].length - 20);
    }
    
    printf("\nTop 5 Solutions:\n");
    for (int i = 0; i < 5 && i < config.population_size; i++) {
        printf("  #%d: Fitness=%.2f, Survivors=%d, Coverage=%d, Length=%d\n",
               i + 1,
               shared->population[i].fitness,
               shared->population[i].survivors_reached,
               shared->population[i].coverage,
               shared->population[i].length);
    }
}

// Save population for second run
void save_population(Path *backup, int size) {
    for (int i = 0; i < size; i++) {
        backup[i].genes = malloc(config.max_path_length * sizeof(Coord));
        backup[i].length = shared->population[i].length;
        memcpy(backup[i].genes, shared->population[i].genes,
               shared->population[i].length * sizeof(Coord));
        backup[i].fitness = shared->population[i].fitness;
        backup[i].survivors_reached = shared->population[i].survivors_reached;
        backup[i].coverage = shared->population[i].coverage;
    }
}

// Restore population for second run
void restore_population(Path *backup, int size) {
    for (int i = 0; i < size; i++) {
        if (shared->population[i].genes) {
            free(shared->population[i].genes);
        }
        shared->population[i].genes = malloc(config.max_path_length * sizeof(Coord));
        shared->population[i].length = backup[i].length;
        memcpy(shared->population[i].genes, backup[i].genes,
               backup[i].length * sizeof(Coord));
        shared->population[i].fitness = backup[i].fitness;
        shared->population[i].survivors_reached = backup[i].survivors_reached;
        shared->population[i].coverage = backup[i].coverage;
    }
}

// Free backup population
void free_backup(Path *backup, int size) {
    for (int i = 0; i < size; i++) {
        if (backup[i].genes) {
            free(backup[i].genes);
        }
    }
    free(backup);
}

int main(int argc, char *argv[]) {
    const char *config_file = argc > 1 ? argv[1] : NULL;
    
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║  Genetic Algorithm Rescue Robot - Performance Benchmark     ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");
    
    // Read configuration
    read_config(config_file);
    print_config();
    
    // Initialize shared memory and semaphores
    init_shared_memory();
    
    // Initialize random seed
    srand(time(NULL));
    
    // Initialize grid
    printf("Initializing environment...\n");
    init_grid();
    
    // Generate initial population
    printf("Generating initial population of %d paths...\n", config.population_size);
    for (int i = 0; i < config.population_size; i++) {
        generate_random_path(&shared->population[i]);
        calculate_fitness(&shared->population[i]);
    }
    
    // Backup initial population for fair comparison
    Path *initial_population = malloc(config.population_size * sizeof(Path));
    save_population(initial_population, config.population_size);
    
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("                    RUN 1: SINGLE-PROCESS MODE                 \n");
    printf("═══════════════════════════════════════════════════════════════\n\n");
    
    // Reset shared data
    shared->best_fitness = -1e9;
    shared->best_fitness_gen = 0;
    
    // Run single-process version
    double start_single = get_time();
    run_evolution(0);  // 0 = single-process
    double end_single = get_time();
    double time_single = end_single - start_single;
    
    // Save single-process results
    float best_fitness_single = shared->population[0].fitness;
    int survivors_single = shared->population[0].survivors_reached;
    
    display_results();
    
    printf("\n⏱️  Single-Process Time: %.3f seconds\n", time_single);
    
    // Restore initial population for fair comparison
    printf("\n\nRestoring initial population for second run...\n");
    restore_population(initial_population, config.population_size);
    
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("                    RUN 2: MULTI-PROCESS MODE                  \n");
    printf("═══════════════════════════════════════════════════════════════\n\n");
    
    // Reset shared data
    shared->best_fitness = -1e9;
    shared->best_fitness_gen = 0;
    shared->generation = 0;
    shared->workers_done = 0;
    
    // Create worker processes
    pid_t *pids = malloc(config.num_processes * sizeof(pid_t));
    printf("Creating %d worker processes...\n", config.num_processes);
    create_process_pool(pids);
    
    // Give workers time to start
    usleep(100000);
    
    printf("Worker PIDs: ");
    for (int i = 0; i < config.num_processes; i++) {
        printf("%d ", pids[i]);
    }
    printf("\n\n");
    
    // Run multi-process version
    double start_multi = get_time();
    run_evolution(1);  // 1 = multi-process
    double end_multi = get_time();
    double time_multi = end_multi - start_multi;
    
    // Save multi-process results
    float best_fitness_multi = shared->population[0].fitness;
    int survivors_multi = shared->population[0].survivors_reached;
    
    // Signal workers to stop
    shared->generation = config.num_generations;
    
    // Wait for workers to finish
    wait_for_workers(pids);
    free(pids);
    
    display_results();
    
    printf("\n⏱️  Multi-Process Time: %.3f seconds\n", time_multi);
    
    // Performance comparison
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║                   PERFORMANCE COMPARISON                     ║\n");
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    printf("║ Metric                │ Single-Process │ Multi-Process      ║\n");
    printf("╠═══════════════════════╪════════════════╪════════════════════╣\n");
    printf("║ Execution Time        │ %7.3f sec    │ %7.3f sec       ║\n", 
           time_single, time_multi);
    printf("║ Best Fitness          │ %10.2f     │ %10.2f         ║\n",
           best_fitness_single, best_fitness_multi);
    printf("║ Survivors Reached     │ %6d         │ %6d             ║\n",
           survivors_single, survivors_multi);
    printf("║ Processes Used        │      1         │ %6d             ║\n",
           config.num_processes);
    printf("╠═══════════════════════╧════════════════╧════════════════════╣\n");
    
    double speedup = time_single / time_multi;
    double efficiency = (speedup / config.num_processes) * 100.0;
    
    printf("║ Speedup: %.2fx                                              ║\n", speedup);
    printf("║ Parallel Efficiency: %.1f%%                                  ║\n", efficiency);
    
    if (speedup > 1.0) {
        printf("║ ✅ Multi-process is %.2fx FASTER                            ║\n", speedup);
    } else if (speedup < 1.0) {
        printf("║ ⚠️  Single-process is %.2fx faster                          ║\n", 1.0/speedup);
        printf("║    (Overhead may exceed benefits for this problem size)  ║\n");
    } else {
        printf("║ ⚖️  Both approaches have similar performance               ║\n");
    }
    
    printf("╚══════════════════════════════════════════════════════════════╝\n");
    
    // Cleanup
    free_backup(initial_population, config.population_size);
    cleanup_shared_memory();
    
    printf("\nBenchmark complete! Results saved above.\n");
    
    return 0;
}