#ifndef CONFIG_H
#define CONFIG_H

typedef struct {
    int grid_x, grid_y, grid_z;

    int population_size;
    int num_generations;
    int num_processes;

    int max_path_length;

    int num_survivors;
    int num_obstacles;

    double w1, w2, w3, w4;
} Config;

extern Config config;

int read_config(const char *filename);
void print_config(void);

#endif
//**********************************************************************