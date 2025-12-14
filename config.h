#ifndef CONFIG_H
#define CONFIG_H

typedef struct {
    int grid_x, grid_y, grid_z;

    int population_size;
    int num_generations;
    int max_path_length;

    int num_processes; // 0 = auto

    int num_survivors;
    int num_obstacles;

    double w1, w2, w3, w4;

    double elitism_percent;
    double mutation_rate;
    double crossover_rate;
    int tournament_size;

    int stagnation_limit;     // 0 disables
    int time_limit_seconds;   // 0 disables

    char survivor_priorities_csv[512]; // optional: "5,4,3,..."
    int priority_default;              // default priority if list missing

    // NEW: penalty weights to force rescuing survivors dominates fitness
    double missing_priority_penalty;   // e.g., 1000.0
    double full_rescue_bonus;          // e.g., 5000.0
} Config;

extern Config config;

void set_default_config(void);
int  read_config(const char *filename);
void print_config(void);

#endif
