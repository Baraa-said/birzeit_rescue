#ifndef TYPES_H
#define TYPES_H

// Configuration parameters
typedef struct {
    int grid_x;
    int grid_y;
    int grid_z;
    int population_size;
    int num_generations;
    int max_path_length;
    int num_processes;
    int num_survivors;
    int num_obstacles;
    float w1; // survivor weight
    float w2; // coverage weight
    float w3; // length weight
    float w4; // risk weight
    float elitism_percent;
    float mutation_rate;
    float crossover_rate;
    int tournament_size;
    int stagnation_limit;
} Config;

// 3D coordinate
typedef struct {
    int x;
    int y;
    int z;
} Coord;

// Path (chromosome)
typedef struct {
    Coord *genes;
    int length;
    float fitness;
    int survivors_reached;
    int coverage;
} Path;

// Grid cell type
typedef enum {
    EMPTY = 0,
    OBSTACLE = 1,
    SURVIVOR = 2,
    VISITED = 3
} CellType;

// Shared memory structure
typedef struct {
    Path *population;
    int *grid;
    Coord *survivors;
    Coord *obstacles;
    int generation;
    int best_fitness_gen;
    float best_fitness;
    int workers_done;
} SharedData;

// Global shared variables
extern Config config;
extern int shmid, semid;
extern SharedData *shared;

#endif