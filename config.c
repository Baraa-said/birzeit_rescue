#include <stdio.h>
#include <string.h>
#include "config.h"

void read_config(const char *filename) {
    // Default values
    config.grid_x = 20;
    config.grid_y = 20;
    config.grid_z = 10;
    config.population_size = 50;
    config.num_generations = 100;
    config.max_path_length = 50;
    config.num_processes = 4;
    config.num_survivors = 10;
    config.num_obstacles = 100;
    config.w1 = 10.0;
    config.w2 = 2.0;
    config.w3 = 0.5;
    config.w4 = 5.0;
    config.elitism_percent = 0.1;
    config.mutation_rate = 0.2;
    config.crossover_rate = 0.8;
    config.tournament_size = 3;
    config.stagnation_limit = 20;

    if (filename == NULL) return;

    FILE *fp = fopen(filename, "r");
    if (!fp) {
        printf("Warning: Could not open config file '%s', using defaults\n", filename);
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        char key[128];
        float value;
        if (sscanf(line, "%s = %f", key, &value) == 2) {
            if (strcmp(key, "grid_x") == 0) config.grid_x = (int)value;
            else if (strcmp(key, "grid_y") == 0) config.grid_y = (int)value;
            else if (strcmp(key, "grid_z") == 0) config.grid_z = (int)value;
            else if (strcmp(key, "population_size") == 0) config.population_size = (int)value;
            else if (strcmp(key, "num_generations") == 0) config.num_generations = (int)value;
            else if (strcmp(key, "max_path_length") == 0) config.max_path_length = (int)value;
            else if (strcmp(key, "num_processes") == 0) config.num_processes = (int)value;
            else if (strcmp(key, "num_survivors") == 0) config.num_survivors = (int)value;
            else if (strcmp(key, "num_obstacles") == 0) config.num_obstacles = (int)value;
            else if (strcmp(key, "w1") == 0) config.w1 = value;
            else if (strcmp(key, "w2") == 0) config.w2 = value;
            else if (strcmp(key, "w3") == 0) config.w3 = value;
            else if (strcmp(key, "w4") == 0) config.w4 = value;
            else if (strcmp(key, "elitism_percent") == 0) config.elitism_percent = value;
            else if (strcmp(key, "mutation_rate") == 0) config.mutation_rate = value;
            else if (strcmp(key, "crossover_rate") == 0) config.crossover_rate = value;
            else if (strcmp(key, "tournament_size") == 0) config.tournament_size = (int)value;
            else if (strcmp(key, "stagnation_limit") == 0) config.stagnation_limit = (int)value;
        }
    }
    fclose(fp);
}

void print_config() {
    printf("Configuration:\n");
    printf("  Grid: %dx%dx%d\n", config.grid_x, config.grid_y, config.grid_z);
    printf("  Population: %d\n", config.population_size);
    printf("  Generations: %d\n", config.num_generations);
    printf("  Processes: %d\n", config.num_processes);
    printf("  Survivors: %d\n", config.num_survivors);
    printf("  Obstacles: %d\n", config.num_obstacles);
    printf("  Weights: w1=%.1f, w2=%.1f, w3=%.1f, w4=%.1f\n", 
           config.w1, config.w2, config.w3, config.w4);
    printf("  Elitism: %.1f%%, Mutation: %.1f%%, Crossover: %.1f%%\n\n",
           config.elitism_percent * 100, config.mutation_rate * 100, 
           config.crossover_rate * 100);
}