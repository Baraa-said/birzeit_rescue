#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include <time.h>
#include "genetic.h"
#include "pool.h"   // عشان extern SharedData *shared

// ثوابت خاصة بالـ GA (ما لمسنا config)
#define ELITISM_RATE     0.10   // top 10%
#define MUTATION_RATE    0.20   // 20% احتمال ميوتشن
#define TOURNAMENT_SIZE  3      // حجم التورنمنت

// متغيرات للستاجنيشن
static double last_best = -1e9;
static int    stagnation_counter = 0;

// لو ما عندك تعريف للـ EMPTY/OBSTACLE/SURVIVOR، تأكدي إنها موجودة في هيدر آخر
// مثال محتمل:
// #define EMPTY    0
// #define OBSTACLE 1
// #define SURVIVOR 2

// ------------ Helpers ------------

static inline int index3d(int x, int y, int z) {
    return z * config.grid_x * config.grid_y + y * config.grid_x + x;
}

static inline int coord_to_index(Coord c) {
    return index3d(c.x, c.y, c.z);
}

// Check if coordinate is inside grid
int is_valid(Coord c) {
    return c.x >= 0 && c.x < config.grid_x &&
           c.y >= 0 && c.y < config.grid_y &&
           c.z >= 0 && c.z < config.grid_z;
}

// Get cell type from shared grid
int get_cell(Coord c) {
    if (!is_valid(c)) return OBSTACLE;   // خارج الماب = عائق
    return shared->grid[coord_to_index(c)];
}

// Manhattan distance
int manhattan_distance(Coord a, Coord b) {
    return abs(a.x - b.x) + abs(a.y - b.y) + abs(a.z - b.z);
}

// ------------ Environment ------------

// Initialize grid with obstacles and survivors
void init_grid(void) {
    int grid_size = config.grid_x * config.grid_y * config.grid_z;

    // امسح الجريد
    for (int i = 0; i < grid_size; i++) {
        shared->grid[i] = EMPTY;
    }

    // ضع العوائق (بدون شرط supporting obstacle cells)
    for (int i = 0; i < config.num_obstacles; i++) {
        int x, y, z, idx;
        do {
            x = rand() % config.grid_x;
            y = rand() % config.grid_y;
            z = rand() % config.grid_z;
            idx = index3d(x, y, z);
        } while (shared->grid[idx] != EMPTY);

        shared->obstacles[i].x = x;
        shared->obstacles[i].y = y;
        shared->obstacles[i].z = z;
        shared->grid[idx] = OBSTACLE;
    }

    // ضع الناجين
    for (int i = 0; i < config.num_survivors; i++) {
        int x, y, z, idx;
        do {
            x = rand() % config.grid_x;
            y = rand() % config.grid_y;
            z = rand() % config.grid_z;
            idx = index3d(x, y, z);
        } while (shared->grid[idx] != EMPTY);

        shared->survivors[i].x = x;
        shared->survivors[i].y = y;
        shared->survivors[i].z = z;
        shared->grid[idx] = SURVIVOR;
    }
}

// الدكتور حاط init_environment في الهيدر – نخليها تستدعي init_grid
void init_environment(int *grid, Coord *survivors, Coord *obstacles) {
    (void)grid;
    (void)survivors;
    (void)obstacles;
    init_grid();
}

// ------------ Path & Population ------------

// توليد مسار عشوائي مع ميول باتجاه أقرب ناجي (A*-like)
void generate_random_path(Path *path) {
    int max_len = config.max_path_length;
    if (max_len > MAX_PATH_LENGTH) {
        max_len = MAX_PATH_LENGTH; // أمان
    }

    path->length = 0;
    path->fitness = 0.0;
    path->survivors_reached = 0;
    path->coverage = 0;

    // بداية من خلية مش عائق
    Coord current;
    do {
        current.x = rand() % config.grid_x;
        current.y = rand() % config.grid_y;
        current.z = rand() % config.grid_z;
    } while (get_cell(current) == OBSTACLE);

    path->genes[path->length++] = current;

    for (int step = 1; step < max_len; step++) {
        Coord next = current;

        // 70% نتحرك باتجاه أقرب ناجي
        if (rand() % 100 < 70 && config.num_survivors > 0) {
            int min_dist = INT_MAX;
            Coord best = current;

            Coord directions[6] = {
                { 1,  0,  0},
                {-1,  0,  0},
                { 0,  1,  0},
                { 0, -1,  0},
                { 0,  0,  1},
                { 0,  0, -1}
            };

            for (int d = 0; d < 6; d++) {
                Coord cand = {
                    current.x + directions[d].x,
                    current.y + directions[d].y,
                    current.z + directions[d].z
                };

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

            if (min_dist == INT_MAX) {
                break; // ما في حركة مفيدة
            }
            next = best;
        } else {
            // حركة عشوائية مع حد أعلى للمحاولات
            int attempts = 0;
            Coord cand;
            do {
                int dir = rand() % 6;
                cand = current;
                switch (dir) {
                    case 0: cand.x++; break;
                    case 1: cand.x--; break;
                    case 2: cand.y++; break;
                    case 3: cand.y--; break;
                    case 4: cand.z++; break;
                    case 5: cand.z--; break;
                }
                attempts++;
                if (attempts > 10) {
                    cand = current;
                    break;
                }
            } while (!is_valid(cand) || get_cell(cand) == OBSTACLE);

            if (attempts > 10) break;
            next = cand;
        }

        path->genes[path->length++] = next;
        current = next;
    }
}

// تهيئة البوبيوليشن
void init_population(Path *population) {
    srand(time(NULL));  // seed مرة واحدة لموضوع الجيناتيك

    for (int i = 0; i < config.population_size; i++) {
        generate_random_path(&population[i]);
        calculate_fitness(&population[i]);
    }

    shared->best_fitness = -1e9;
    last_best = -1e9;
    stagnation_counter = 0;
}

// ------------ Fitness ------------

void calculate_fitness(Path *path) {
    int survivors_reached = 0;
    int coverage = 0;
    int risk = 0;

    int grid_size = config.grid_x * config.grid_y * config.grid_z;
    int *visited = calloc(grid_size, sizeof(int));
    if (!visited) {
        fprintf(stderr, "Error: failed to allocate visited[]\n");
        exit(1);
    }

    for (int i = 0; i < path->length; i++) {
        Coord c = path->genes[i];

        if (!is_valid(c)) {
            // خارج حدود الماب = مسار سيء جداً
            path->fitness = -1e9;
            free(visited);
            return;
        }

        int idx = coord_to_index(c);

        // 1) coverage: عدد الخلايا الفريدة
        if (!visited[idx]) {
            visited[idx] = 1;
            coverage++;
        }

        // 2) survivors reached
        for (int s = 0; s < config.num_survivors; s++) {
            if (c.x == shared->survivors[s].x &&
                c.y == shared->survivors[s].y &&
                c.z == shared->survivors[s].z) {
                survivors_reached++;
            }
        }

        // 3) collision detection
        if (get_cell(c) == OBSTACLE) {
            path->fitness = -1e9;
            free(visited);
            return;
        }

        // 4) risk: عدد الخلايا المجاورة التي فيها عوائق
        for (int dx = -1; dx <= 1; dx++) {
            for (int dy = -1; dy <= 1; dy++) {
                for (int dz = -1; dz <= 1; dz++) {
                    Coord neighbor = {c.x + dx, c.y + dy, c.z + dz};
                    if (is_valid(neighbor) && get_cell(neighbor) == OBSTACLE) {
                        risk++;
                    }
                }
            }
        }
    }

    free(visited);

    path->survivors_reached = survivors_reached;
    path->coverage = coverage;

    path->fitness =
        config.w1 * survivors_reached +
        config.w2 * coverage          -
        config.w3 * path->length      -
        config.w4 * risk;
}

// ------------ GA operators ------------

// Tournament selection على shared->population
int tournament_selection(void) {
    int best_idx = rand() % config.population_size;
    double best_fitness = shared->population[best_idx].fitness;

    for (int i = 1; i < TOURNAMENT_SIZE; i++) {
        int idx = rand() % config.population_size;
        if (shared->population[idx].fitness > best_fitness) {
            best_fitness = shared->population[idx].fitness;
            best_idx = idx;
        }
    }
    return best_idx;
}

// تحديث أفضل حل في shared
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

    // منطق الستاجنيشن
    if (best <= last_best + 1e-6) {
        stagnation_counter++;
    } else {
        stagnation_counter = 0;
        last_best = best;
    }
}

// One-point crossover
void crossover(Path *parent1, Path *parent2, Path *child) {
    int min_len = (parent1->length < parent2->length)
                  ? parent1->length : parent2->length;

    if (min_len <= 1) {
        *child = *parent1; // نسخة بسيطة
        return;
    }

    int crossover_point = rand() % min_len;

    *child = *parent1;                   // كبداية
    child->length = 0;

    // أول جزء من parent1
    for (int i = 0; i < crossover_point && i < parent1->length; i++) {
        child->genes[child->length++] = parent1->genes[i];
    }

    // ثاني جزء من parent2
    for (int i = crossover_point;
         i < parent2->length && child->length < MAX_PATH_LENGTH;
         i++) {
        child->genes[child->length++] = parent2->genes[i];
    }
}

// Mutation: random move حول نقطة عشوائية
void mutate(Path *path) {
    double r = (double)rand() / RAND_MAX;
    if (r > MUTATION_RATE) return;
    if (path->length < 1) return;

    int mutation_point = rand() % path->length;

    Coord directions[6] = {
        { 1,  0,  0}, {-1,  0,  0},
        { 0,  1,  0}, { 0, -1,  0},
        { 0,  0,  1}, { 0,  0, -1}
    };

    int dir = rand() % 6;
    Coord new_coord = {
        path->genes[mutation_point].x + directions[dir].x,
        path->genes[mutation_point].y + directions[dir].y,
        path->genes[mutation_point].z + directions[dir].z
    };

    if (is_valid(new_coord) && get_cell(new_coord) != OBSTACLE) {
        path->genes[mutation_point] = new_coord;
    }
}

// qsort: نزولاً حسب الفتنس
int compare_fitness(const void *a, const void *b) {
    const Path *pa = (const Path *)a;
    const Path *pb = (const Path *)b;

    if (pb->fitness > pa->fitness) return 1;
    if (pb->fitness < pa->fitness) return -1;
    return 0;
}

// FIXED: Disable stagnation check for full evolution
int check_stagnation(void) {
    // Disable early stopping to allow full 100 generations for visualization
    return 0;
    
    // Or use a much higher limit if you want some stagnation detection:
    // return (stagnation_counter >= 50);
}

// بما أن genes array ثابت داخل Path، ما في free حقيقي
void free_path(Path *path) {
    (void)path;
}

// تطوير السكان - يعمل على أي population (محلي أو مشترك)
void evolve_population(Path *population) {
    // Get population size - use a smaller default for local populations
    int N = config.population_size / config.num_processes;
    if (N < 5) N = 5; // Minimum for evolution
    
    // Handle case where we're evolving the full shared population
    // This happens in the original non-independent mode
    if (population == shared->population) {
        N = config.population_size;
    }

    // رتبي حسب الفتنس
    qsort(population, N, sizeof(Path), compare_fitness);

    int elite_count = (int)(ELITISM_RATE * N);
    if (elite_count < 1) elite_count = 1;

    Path *new_pop = malloc(N * sizeof(Path));
    if (!new_pop) {
        fprintf(stderr, "Error: failed to allocate new population\n");
        exit(1);
    }

    // 1) النخبة (keep best solutions)
    for (int i = 0; i < elite_count; i++) {
        new_pop[i] = population[i];
    }

    // 2) أطفال (create offspring through crossover and mutation)
    for (int i = elite_count; i < N; i++) {
        // Select parents from current population
        int p1_idx = rand() % N;
        int p2_idx = rand() % N;
        
        // Tournament selection for better parents
        for (int t = 1; t < TOURNAMENT_SIZE; t++) {
            int candidate = rand() % N;
            if (population[candidate].fitness > population[p1_idx].fitness) {
                p1_idx = candidate;
            }
        }
        
        for (int t = 1; t < TOURNAMENT_SIZE; t++) {
            int candidate = rand() % N;
            if (population[candidate].fitness > population[p2_idx].fitness) {
                p2_idx = candidate;
            }
        }
        
        Path *parent1 = &population[p1_idx];
        Path *parent2 = &population[p2_idx];

        Path child;
        crossover(parent1, parent2, &child);
        mutate(&child);
        calculate_fitness(&child);

        new_pop[i] = child;
    }

    // 3) استبدال (replace old population with new)
    for (int i = 0; i < N; i++) {
        population[i] = new_pop[i];
    }

    free(new_pop);
}

// ------------ Output ------------

void print_final_result(SharedData *sd) {
    printf("\n=== Final Results ===\n");
    printf("Best fitness:       %.2f\n", sd->best_fitness);
    printf("Best path length:   %d\n",  sd->best_path.length);
    printf("Survivors reached:  %d\n",  sd->best_path.survivors_reached);
    printf("Coverage (cells):   %d\n",  sd->best_path.coverage);
}