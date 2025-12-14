#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#include "config.h"

Config config;

static char *trim(char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    if (*s == 0) return s;
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return s;
}

void set_default_config(void) {
    // Reasonable defaults (project-safe)
    config.grid_x = 10;
    config.grid_y = 10;
    config.grid_z = 3;

    config.population_size = 40;
    config.num_generations = 200;
    config.max_path_length = 50;

    config.num_processes = 0; // auto

    config.num_survivors = 7;
    config.num_obstacles = 10;

    config.w1 = 10.0;
    config.w2 = 2.0;
    config.w3 = 0.5;
    config.w4 = 4.0;

    config.elitism_percent = 0.10;
    config.mutation_rate   = 0.20;
    config.crossover_rate  = 0.80;
    config.tournament_size = 3;

    config.stagnation_limit  = 20;
    config.time_limit_seconds = 0;
}

static int clamp_int(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static double clamp_double(double v, double lo, double hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

int read_config(const char *filename) {
    set_default_config();

    FILE *f = fopen(filename, "r");
    if (!f) {
        fprintf(stderr, "⚠️  Warning: could not open '%s'. Using default configuration.\n", filename);
        // Still OK (requirement: defaults if config missing)
    } else {
        char line[256];
        while (fgets(line, sizeof(line), f)) {
            char *p = trim(line);
            if (*p == '\0' || *p == '#') continue;

            char *eq = strchr(p, '=');
            if (!eq) continue;

            *eq = '\0';
            char *key = trim(p);
            char *val = trim(eq + 1);

            // ints
            if (strcmp(key, "grid_x") == 0) config.grid_x = atoi(val);
            else if (strcmp(key, "grid_y") == 0) config.grid_y = atoi(val);
            else if (strcmp(key, "grid_z") == 0) config.grid_z = atoi(val);

            else if (strcmp(key, "population_size") == 0) config.population_size = atoi(val);
            else if (strcmp(key, "num_generations") == 0) config.num_generations = atoi(val);
            else if (strcmp(key, "max_path_length") == 0) config.max_path_length = atoi(val);

            else if (strcmp(key, "num_processes") == 0) config.num_processes = atoi(val);

            else if (strcmp(key, "num_survivors") == 0) config.num_survivors = atoi(val);
            else if (strcmp(key, "num_obstacles") == 0) config.num_obstacles = atoi(val);

            else if (strcmp(key, "tournament_size") == 0) config.tournament_size = atoi(val);

            else if (strcmp(key, "stagnation_limit") == 0) config.stagnation_limit = atoi(val);
            else if (strcmp(key, "time_limit_seconds") == 0) config.time_limit_seconds = atoi(val);

            // doubles
            else if (strcmp(key, "w1") == 0) config.w1 = atof(val);
            else if (strcmp(key, "w2") == 0) config.w2 = atof(val);
            else if (strcmp(key, "w3") == 0) config.w3 = atof(val);
            else if (strcmp(key, "w4") == 0) config.w4 = atof(val);

            else if (strcmp(key, "elitism_percent") == 0) config.elitism_percent = atof(val);
            else if (strcmp(key, "mutation_rate") == 0) config.mutation_rate = atof(val);
            else if (strcmp(key, "crossover_rate") == 0) config.crossover_rate = atof(val);
        }
        fclose(f);
    }

    // sanitize
    config.grid_x = clamp_int(config.grid_x, 1, 500);
    config.grid_y = clamp_int(config.grid_y, 1, 500);
    config.grid_z = clamp_int(config.grid_z, 1, 500);

    config.population_size = clamp_int(config.population_size, 2, 100000);
    config.num_generations = clamp_int(config.num_generations, 1, 1000000);

    config.max_path_length = clamp_int(config.max_path_length, 1, 100000);

    config.num_survivors = clamp_int(config.num_survivors, 0, 100000);
    config.num_obstacles = clamp_int(config.num_obstacles, 0, 100000);

    config.elitism_percent = clamp_double(config.elitism_percent, 0.0, 0.95);
    config.mutation_rate   = clamp_double(config.mutation_rate, 0.0, 1.0);
    config.crossover_rate  = clamp_double(config.crossover_rate, 0.0, 1.0);
    config.tournament_size = clamp_int(config.tournament_size, 2, 50);

    config.stagnation_limit   = clamp_int(config.stagnation_limit, 0, 1000000);
    config.time_limit_seconds = clamp_int(config.time_limit_seconds, 0, 1000000);

    // auto processes if 0
    if (config.num_processes <= 0) {
        long cores = sysconf(_SC_NPROCESSORS_ONLN);
        if (cores < 1) cores = 1;
        config.num_processes = (int)cores;
    }

    if (config.num_processes > config.population_size)
        config.num_processes = config.population_size;

    if (config.num_processes < 1) config.num_processes = 1;

    return 1;
}

void print_config(void) {
    printf("\n=== Configuration ===\n");
    printf("Grid: %dx%dx%d\n", config.grid_x, config.grid_y, config.grid_z);
    printf("Population: %d\n", config.population_size);
    printf("Generations: %d\n", config.num_generations);
    printf("Max path length: %d\n", config.max_path_length);
    printf("Processes: %d\n", config.num_processes);
    printf("Survivors: %d\n", config.num_survivors);
    printf("Obstacles: %d\n", config.num_obstacles);

    printf("Weights: w1=%.2f w2=%.2f w3=%.2f w4=%.2f\n", config.w1, config.w2, config.w3, config.w4);

    printf("GA params: elitism=%.2f mutation=%.2f crossover=%.2f tournament=%d\n",
           config.elitism_percent, config.mutation_rate, config.crossover_rate, config.tournament_size);

    printf("Stopping: stagnation_limit=%d time_limit_seconds=%d\n",
           config.stagnation_limit, config.time_limit_seconds);

    printf("=====================\n\n");
}
