#ifndef TYPES_H
#define TYPES_H

#define MAX_PATH_LENGTH 500

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

    int survivors_reached; // unique
    int priority_sum;      // unique sum of priorities reached
    int coverage;          // unique visited cells
} Path;

typedef struct {
    Path  *population;
    int   *grid;

    Coord *survivors;
    int   *survivor_priority;

    Coord *obstacles;

    int generation;
    int workers_done;
    int stop_flag;

    double best_fitness;
    Path   best_path;
} SharedData;

#endif
