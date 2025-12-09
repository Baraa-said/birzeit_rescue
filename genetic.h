#ifndef GENETIC_H
#define GENETIC_H

#include "types.h"
#include "config.h"

// -------- Grid / Environment --------
void init_grid(void);
int  is_valid(Coord c);
int  get_cell(Coord c);
int  manhattan_distance(Coord a, Coord b);
void init_environment(int *grid, Coord *survivors, Coord *obstacles);

// -------- Population & Fitness --------
void init_population(Path *population);
void calculate_fitness(Path *path);
void update_best_solution(SharedData *shared);
int  check_stagnation(void);
void evolve_population(Path *population);
void print_final_result(SharedData *shared);

// -------- Path generation --------
void generate_random_path(Path *path);

// -------- Genetic operators --------
int  tournament_selection(void);
void crossover(Path *parent1, Path *parent2, Path *child);
void mutate(Path *path);

// -------- Utilities --------
int  compare_fitness(const void *a, const void *b);
void free_path(Path *path);

#endif // GENETIC_H
