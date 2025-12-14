#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include <time.h>

#include "genetic.h"
#include "pool.h"   // extern SharedData *shared

StartMode g_start_mode = START_RANDOM;

static inline int index3d(int x, int y, int z) {
    return z * config.grid_x * config.grid_y + y * config.grid_x + x;
}

static inline int coord_to_index(Coord c) {
    return index3d(c.x, c.y, c.z);
}

int is_valid(Coord c) {
    return c.x >= 0 && c.x < config.grid_x &&
           c.y >= 0 && c.y < config.grid_y &&
           c.z >= 0 && c.z < config.grid_z;
}

int get_cell(Coord c) {
    if (!is_valid(c)) return OBSTACLE;
    return shared->grid[coord_to_index(c)];
}

int manhattan_distance(Coord a, Coord b) {
    return abs(a.x - b.x) + abs(a.y - b.y) + abs(a.z - b.z);
}

void init_grid(void) {
    int grid_size = config.grid_x * config.grid_y * config.grid_z;

    for (int i = 0; i < grid_size; i++)
        shared->grid[i] = EMPTY;

    // Obstacles
    for (int i = 0; i < config.num_obstacles; i++) {
        int x, y, z, idx;
        do {
            x = rand() % config.grid_x;
            y = rand() % config.grid_y;
            z = rand() % config.grid_z;
            idx = index3d(x, y, z);
        } while (shared->grid[idx] != EMPTY);

        shared->obstacles[i] = (Coord){x, y, z};
        shared->grid[idx] = OBSTACLE;
    }

    // Survivors
    for (int i = 0; i < config.num_survivors; i++) {
        int x, y, z, idx;
        do {
            x = rand() % config.grid_x;
            y = rand() % config.grid_y;
            z = rand() % config.grid_z;
            idx = index3d(x, y, z);
        } while (shared->grid[idx] != EMPTY);

        shared->survivors[i] = (Coord){x, y, z};
        shared->grid[idx] = SURVIVOR;
    }
}

void init_environment(int *grid, Coord *survivors, Coord *obstacles) {
    (void)grid; (void)survivors; (void)obstacles;
    init_grid();
}

static int is_edge_cell(Coord c) {
    return (c.x == 0 || c.x == config.grid_x - 1 ||
            c.y == 0 || c.y == config.grid_y - 1 ||
            c.z == 0 || c.z == config.grid_z - 1);
}

static Coord pick_start_coord(void) {
    Coord c;

    if (g_start_mode == START_TOP) {
        // Top surface = z = grid_z-1
        do {
            c.x = rand() % config.grid_x;
            c.y = rand() % config.grid_y;
            c.z = config.grid_z - 1;
        } while (get_cell(c) == OBSTACLE);
        return c;
    }

    if (g_start_mode == START_EDGES) {
        // Any boundary cell in 3D (edges/sides)
        do {
            c.x = rand() % config.grid_x;
            c.y = rand() % config.grid_y;
            c.z = rand() % config.grid_z;
        } while (!is_edge_cell(c) || get_cell(c) == OBSTACLE);
        return c;
    }

    // Random
    do {
        c.x = rand() % config.grid_x;
        c.y = rand() % config.grid_y;
        c.z = rand() % config.grid_z;
    } while (get_cell(c) == OBSTACLE);
    return c;
}

void generate_random_path(Path *path) {
    int max_len = config.max_path_length;
    if (max_len > MAX_PATH_LENGTH) max_len = MAX_PATH_LENGTH;

    path->length = 0;
    path->fitness = 0.0;
    path->survivors_reached = 0;
    path->coverage = 0;

    Coord current = pick_start_coord();
    path->genes[path->length++] = current;

    Coord directions[6] = {
        { 1,  0,  0}, {-1,  0,  0},
        { 0,  1,  0}, { 0, -1,  0},
        { 0,  0,  1}, { 0,  0, -1}
    };

    for (int step = 1; step < max_len; step++) {
        Coord next = current;

        // 70% guided move toward nearest survivor (simple heuristic)
        if ((rand() % 100) < 70 && config.num_survivors > 0) {
            int min_dist = INT_MAX;
            Coord best = current;

            for (int d = 0; d < 6; d++) {
                Coord cand = { current.x + directions[d].x,
                               current.y + directions[d].y,
                               current.z + directions[d].z };

                if (!is_valid(cand) || get_cell(cand) == OBSTACLE) continue;

                int dist = INT_MAX;
                for (int s = 0; s < config.num_survivors; s++) {
                    int dd = manhattan_distance(cand, shared->survivors[s]);
                    if (dd < dist) dist = dd;
                }

                if (dist < min_dist) {
                    min_dist = dist;
                    best = cand;
                }
            }

            if (min_dist == INT_MAX) break;
            next = best;
        } else {
            // random step with limited attempts
            int attempts = 0;
            Coord cand;
            do {
                int dir = rand() % 6;
                cand = (Coord){ current.x + directions[dir].x,
                                current.y + directions[dir].y,
                                current.z + directions[dir].z };
                attempts++;
                if (attempts > 10) { cand = current; break; }
            } while (!is_valid(cand) || get_cell(cand) == OBSTACLE);

            if (attempts > 10) break;
            next = cand;
        }

        path->genes[path->length++] = next;
        current = next;
    }
}

void calculate_fitness(Path *path) {
    int coverage = 0;
    int risk = 0;

    // unique survivors tracking
    int survivors_reached = 0;
    char *surv_hit = NULL;
    if (config.num_survivors > 0) {
        surv_hit = calloc((size_t)config.num_survivors, 1);
        if (!surv_hit) { fprintf(stderr, "alloc failed\n"); exit(1); }
    }

    int grid_size = config.grid_x * config.grid_y * config.grid_z;
    int *visited = calloc((size_t)grid_size, sizeof(int));
    if (!visited) { fprintf(stderr, "alloc failed\n"); exit(1); }

    for (int i = 0; i < path->length; i++) {
        Coord c = path->genes[i];

        if (!is_valid(c)) {
            path->fitness = -1e9;
            free(visited);
            free(surv_hit);
            return;
        }

        if (get_cell(c) == OBSTACLE) {
            path->fitness = -1e9;
            free(visited);
            free(surv_hit);
            return;
        }

        int idx = coord_to_index(c);

        if (!visited[idx]) {
            visited[idx] = 1;
            coverage++;
        }

        // unique survivors
        for (int s = 0; s < config.num_survivors; s++) {
            if (!surv_hit[s] &&
                c.x == shared->survivors[s].x &&
                c.y == shared->survivors[s].y &&
                c.z == shared->survivors[s].z) {
                surv_hit[s] = 1;
                survivors_reached++;
                break;
            }
        }

        // risk = nearby obstacles (3x3x3)
        for (int dx = -1; dx <= 1; dx++) {
            for (int dy = -1; dy <= 1; dy++) {
                for (int dz = -1; dz <= 1; dz++) {
                    Coord n = { c.x + dx, c.y + dy, c.z + dz };
                    if (is_valid(n) && get_cell(n) == OBSTACLE)
                        risk++;
                }
            }
        }
    }

    free(visited);
    free(surv_hit);

    path->survivors_reached = survivors_reached;
    path->coverage = coverage;

    path->fitness =
        config.w1 * survivors_reached +
        config.w2 * coverage          -
        config.w3 * path->length      -
        config.w4 * risk;
}

static int compare_fitness_desc(const void *a, const void *b) {
    const Path *pa = (const Path *)a;
    const Path *pb = (const Path *)b;
    if (pb->fitness > pa->fitness) return 1;
    if (pb->fitness < pa->fitness) return -1;
    return 0;
}

static int tournament_pick(Path *population, int N) {
    int best = rand() % N;
    for (int i = 1; i < config.tournament_size; i++) {
        int cand = rand() % N;
        if (population[cand].fitness > population[best].fitness)
            best = cand;
    }
    return best;
}

static void crossover(const Path *p1, const Path *p2, Path *child) {
    int min_len = (p1->length < p2->length) ? p1->length : p2->length;
    if (min_len <= 1) { *child = *p1; return; }

    int cp = rand() % min_len;
    child->length = 0;

    for (int i = 0; i < cp && child->length < MAX_PATH_LENGTH; i++)
        child->genes[child->length++] = p1->genes[i];

    for (int i = cp; i < p2->length && child->length < MAX_PATH_LENGTH; i++)
        child->genes[child->length++] = p2->genes[i];
}

static void mutate(Path *p) {
    double r = (double)rand() / (double)RAND_MAX;
    if (r > config.mutation_rate) return;
    if (p->length < 1) return;

    int mp = rand() % p->length;

    Coord dirs[6] = {
        { 1,  0,  0}, {-1,  0,  0},
        { 0,  1,  0}, { 0, -1,  0},
        { 0,  0,  1}, { 0,  0, -1}
    };
    int d = rand() % 6;

    Coord nc = { p->genes[mp].x + dirs[d].x,
                 p->genes[mp].y + dirs[d].y,
                 p->genes[mp].z + dirs[d].z };

    if (is_valid(nc) && get_cell(nc) != OBSTACLE)
        p->genes[mp] = nc;
}

void evolve_population(Path *population) {
    // Not used in this project version (we use evolve_population_local in workers)
    (void)population;
}


void evolve_population_local(Path *population, int N) {
    qsort(population, (size_t)N, sizeof(Path), compare_fitness_desc);

    int elite = (int)(config.elitism_percent * N);
    if (elite < 1) elite = 1;

    Path *new_pop = malloc((size_t)N * sizeof(Path));
    if (!new_pop) { fprintf(stderr, "alloc failed\n"); exit(1); }

    for (int i = 0; i < elite; i++)
        new_pop[i] = population[i];

    for (int i = elite; i < N; i++) {
        int p1 = tournament_pick(population, N);
        int p2 = tournament_pick(population, N);

        Path child;
        double cr = (double)rand() / (double)RAND_MAX;

        if (cr <= config.crossover_rate) {
            crossover(&population[p1], &population[p2], &child);
        } else {
            child = population[p1];
        }

        mutate(&child);
        calculate_fitness(&child);
        new_pop[i] = child;
    }

    memcpy(population, new_pop, (size_t)N * sizeof(Path));
    free(new_pop);
}

void init_population(Path *population) {
    for (int i = 0; i < config.population_size; i++) {
        generate_random_path(&population[i]);
        calculate_fitness(&population[i]);
    }

    shared->best_fitness = -1e9;
}

void update_best_solution(SharedData *sd) {
    double best = sd->best_fitness;
    int best_idx = -1;

    for (int i = 0; i < config.population_size; i++) {
        if (sd->population[i].fitness > best) {
            best = sd->population[i].fitness;
            best_idx = i;
        }
    }
    if (best_idx >= 0) {
        sd->best_fitness = best;
        sd->best_path = sd->population[best_idx];
    }
}

void print_final_result(SharedData *sd) {
    printf("\n=== Final Results ===\n");
    printf("Best fitness:       %.2f\n", sd->best_fitness);
    printf("Best path length:   %d\n", sd->best_path.length);
    printf("Survivors reached:  %d\n", sd->best_path.survivors_reached);
    printf("Coverage (cells):   %d\n", sd->best_path.coverage);
}
