#ifndef TYPES_H
#define TYPES_H

#include "config.h"

#define MAX_PATH_LENGTH 200   // raised so config.max_path_length can be > 50 safely

#define EMPTY    0
#define OBSTACLE 1
#define SURVIVOR 2

typedef struct {
    int x, y, z;
} Coord;

typedef struct {
    Coord  genes[MAX_PATH_LENGTH];
    int    length;
    double fitness;

    int survivors_reached;  // UNIQUE survivors
    int coverage;           // unique visited cells
} Path;

typedef struct {
    Path  *population;
    int   *grid;
    Coord *survivors;
    Coord *obstacles;

    int generation;
    int workers_done;

    int stop_flag;          // NEW: main can stop workers early

    double best_fitness;
    Path   best_path;
} SharedData;

#endif
