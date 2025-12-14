#ifndef GENETIC_H
#define GENETIC_H

#include "types.h"

typedef enum {
    START_TOP = 1,
    START_EDGES = 2,
    START_RANDOM = 3
} StartMode;

extern StartMode g_start_mode;

void init_grid(void);
void init_population(Path *population);

void generate_random_path(Path *path);
void calculate_fitness(Path *path);

void evolve_population_local(Path *population, int N);

int  is_valid(Coord c);
int  get_cell(Coord c);

#endif
