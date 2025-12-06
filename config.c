// config.c
// to run------->(./rescue_robot)
#include <stdio.h>
#include "config.h"

Config config;

void read_config(const char *filename) {
    (void)filename; // حالياً نتجاهل الملف ونستخدم قيم افتراضية

    config.grid_x = 20;
    config.grid_y = 20;
    config.grid_z = 10;

    config.population_size = 50;
    config.num_generations = 20;
    config.num_processes   = 4;

    config.max_path_length = 50;

    config.num_survivors   = 10;
    config.num_obstacles   = 100;

    config.w1 = 10.0;
    config.w2 = 2.0;
    config.w3 = 0.5;
    config.w4 = 5.0;
}

void print_config(void) {
    printf("Configuration:\n");
    printf("  Grid: %dx%dx%d\n", config.grid_x, config.grid_y, config.grid_z);
    printf("  Population: %d\n", config.population_size);
    printf("  Generations: %d\n", config.num_generations);
    printf("  Processes: %d\n", config.num_processes);
}
