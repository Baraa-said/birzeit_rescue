#ifndef CONFIG_H
#define CONFIG_H

typedef struct {
    // Grid
    int grid_x, grid_y, grid_z;

    // GA
    int population_size;
    int num_generations;
    int max_path_length;

    // Parallelism
    int num_processes;   // 0 = auto

    // Environment
    int num_survivors;
    int num_obstacles;

    // Fitness weights
    double w1, w2, w3, w4;

    // GA operator parameters (user-defined)
    double elitism_percent;   // e.g. 0.10
    double mutation_rate;     // e.g. 0.20
    double crossover_rate;    // e.g. 0.80
    int    tournament_size;   // e.g. 3

    // Stopping criteria
    int stagnation_limit;     // generations without improvement (0 = disabled)
    int time_limit_seconds;   // seconds (0 = disabled)
} Config;

extern Config config;

void set_default_config(void);
int  read_config(const char *filename);   // returns 1 even if file missing (uses defaults)
void print_config(void);

#endif
