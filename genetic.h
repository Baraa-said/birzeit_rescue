#ifndef GENETIC_H
#define GENETIC_H

#include "types.h"
#include "config.h"

typedef enum {
    START_TOP = 1,
    START_EDGES = 2,
    START_RANDOM = 3
} StartMode;

extern StartMode g_start_mode;

// Environment
void init_grid(void);
int  is_valid(Coord c);
int  get_cell(Coord c);
int  manhattan_distance(Coord a, Coord b);
void init_environment(int *grid, Coord *survivors, Coord *obstacles);

// GA
void init_population(Path *population);
void generate_random_path(Path *path);
void calculate_fitness(Path *path);
void evolve_population(Path *population);
void update_best_solution(SharedData *sd);
void print_final_result(SharedData *sd);

#endif
