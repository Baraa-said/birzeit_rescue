#ifndef ASTAR_H
#define ASTAR_H

#include "types.h"

// A* baseline for comparison only: visits survivors in descending priority order.
// Returns 1 if builds a (possibly partial) path, 0 if none.
int astar_build_baseline(Path *out_path, Coord start);

#endif
