// genetic.h
#ifndef GENETIC_H
#define GENETIC_H

#include "types.h"
#include "config.h"

void init_environment(int *grid, Coord *survivors, Coord *obstacles);
void init_population(Path *population);
void calculate_fitness(Path *p);
void update_best_solution(SharedData *shared);
int  check_stagnation(void);
void evolve_population(Path *population);
void print_final_result(SharedData *shared);

#endif
