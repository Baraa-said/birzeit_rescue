#ifndef GENETIC_H
#define GENETIC_H

#include "types.h"

// Grid functions
void init_grid();
int is_valid(Coord c);
int get_cell(Coord c);
int manhattan_distance(Coord a, Coord b);

// Path generation and evaluation
void generate_random_path(Path *path);
void calculate_fitness(Path *path);

// Genetic operators
int tournament_selection();
void crossover(Path *parent1, Path *parent2, Path *child);
void mutate(Path *path);

// Comparison for sorting
int compare_fitness(const void *a, const void *b);

// Free path memory
void free_path(Path *path);

#endif