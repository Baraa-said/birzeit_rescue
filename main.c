#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include "types.h"
#include "config.h"
#include "genetic.h"
#include "pool.h"
//baraa king new new
// Global variables
Config config;
int shmid, semid;
SharedData *shared;

// Evolution function
void run_evolution() {
    int stagnation_counter = 0;
    
    printf("Running genetic algorithm...\n\n");
    
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
            calculate_fitness(&new_pop[i]);
        }
        
        // Replace population
        for (int i = 0; i < config.population_size; i++) {
            free_path(&shared->population[i]);
            shared->population[i] = new_pop[i];
        }
        free(new_pop);
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

int main(int argc, char *argv[]) {
    const char *config_file = argc > 1 ? argv[1] : NULL;
    
    printf("=== Genetic Algorithm Rescue Robot Path Optimizer ===\n\n");
    
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
    printf("Generating initial population...\n");
    for (int i = 0; i < config.population_size; i++) {
        generate_random_path(&shared->population[i]);
        calculate_fitness(&shared->population[i]);
    }
    
    // Create worker processes
    pid_t *pids = malloc(config.num_processes * sizeof(pid_t));
    printf("Creating %d worker processes...\n", config.num_processes);
    create_process_pool(pids);
    
    // Run evolution
    run_evolution();
    
    // Wait for workers to finish
    wait_for_workers(pids);
    free(pids);
    
    // Display results
    display_results();
    
    // Cleanup
    cleanup_shared_memory();
    
    printf("\nOptimization complete!\n");
    
    return 0;
}