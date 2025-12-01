#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include "genetic.h"

// Initialize grid with obstacles and survivors
void init_grid() {
    int grid_size = config.grid_x * config.grid_y * config.grid_z;
    
    // Clear grid
    for (int i = 0; i < grid_size; i++) {
        shared->grid[i] = EMPTY;
    }

    // Place obstacles
    for (int i = 0; i < config.num_obstacles; i++) {
        int x, y, z;
        do {
            x = rand() % config.grid_x;
            y = rand() % config.grid_y;
            z = rand() % config.grid_z;
        } while (shared->grid[z * config.grid_x * config.grid_y + y * config.grid_x + x] != EMPTY);
        
        shared->obstacles[i].x = x;
        shared->obstacles[i].y = y;
        shared->obstacles[i].z = z;
        shared->grid[z * config.grid_x * config.grid_y + y * config.grid_x + x] = OBSTACLE;
    }

    // Place survivors
    for (int i = 0; i < config.num_survivors; i++) {
        int x, y, z;
        do {
            x = rand() % config.grid_x;
            y = rand() % config.grid_y;
            z = rand() % config.grid_z;
        } while (shared->grid[z * config.grid_x * config.grid_y + y * config.grid_x + x] != EMPTY);
        
        shared->survivors[i].x = x;
        shared->survivors[i].y = y;
        shared->survivors[i].z = z;
        shared->grid[z * config.grid_x * config.grid_y + y * config.grid_x + x] = SURVIVOR;
    }
}

// Check if coordinate is valid
int is_valid(Coord c) {
    return c.x >= 0 && c.x < config.grid_x &&
           c.y >= 0 && c.y < config.grid_y &&
           c.z >= 0 && c.z < config.grid_z;
}

// Get cell type
int get_cell(Coord c) {
    if (!is_valid(c)) return OBSTACLE;
    return shared->grid[c.z * config.grid_x * config.grid_y + c.y * config.grid_x + c.x];
}

// Manhattan distance
int manhattan_distance(Coord a, Coord b) {
    return abs(a.x - b.x) + abs(a.y - b.y) + abs(a.z - b.z);
}

// Generate random path using A* inspired approach
void generate_random_path(Path *path) {
    path->genes = malloc(config.max_path_length * sizeof(Coord));
    path->length = 0;
    
    // Start at random position
    Coord current;
    do {
        current.x = rand() % config.grid_x;
        current.y = rand() % config.grid_y;
        current.z = rand() % config.grid_z;
    } while (get_cell(current) == OBSTACLE);
    
    path->genes[path->length++] = current;
    
    // Generate path with bias towards survivors
    for (int i = 1; i < config.max_path_length; i++) {
        Coord next = current;
        
        // 70% chance to move towards nearest survivor
        if (rand() % 100 < 70) {
            int min_dist = INT_MAX;
            int best_dir = -1;
            Coord directions[6] = {
                {1, 0, 0}, {-1, 0, 0},
                {0, 1, 0}, {0, -1, 0},
                {0, 0, 1}, {0, 0, -1}
            };
            
            for (int d = 0; d < 6; d++) {
                Coord candidate = {
                    current.x + directions[d].x,
                    current.y + directions[d].y,
                    current.z + directions[d].z
                };
                
                if (!is_valid(candidate) || get_cell(candidate) == OBSTACLE) continue;
                
                // Find nearest survivor
                int dist = INT_MAX;
                for (int s = 0; s < config.num_survivors; s++) {
                    int d = manhattan_distance(candidate, shared->survivors[s]);
                    if (d < dist) dist = d;
                }
                
                if (dist < min_dist) {
                    min_dist = dist;
                    best_dir = d;
                    next = candidate;
                }
            }
            
            if (best_dir == -1) break; // No valid moves
        } else {
            // Random move
            int attempts = 0;
            do {
                int dir = rand() % 6;
                next = current;
                if (dir == 0) next.x++;
                else if (dir == 1) next.x--;
                else if (dir == 2) next.y++;
                else if (dir == 3) next.y--;
                else if (dir == 4) next.z++;
                else next.z--;
                
                attempts++;
                if (attempts > 10) break;
            } while (!is_valid(next) || get_cell(next) == OBSTACLE);
            
            if (attempts > 10) break;
        }
        
        path->genes[path->length++] = next;
        current = next;
    }
}

// Calculate fitness
void calculate_fitness(Path *path) {
    int survivors_reached = 0;
    int coverage = 0;
    int risk = 0;
    int *visited = calloc(config.grid_x * config.grid_y * config.grid_z, sizeof(int));
    
    for (int i = 0; i < path->length; i++) {
        Coord c = path->genes[i];
        int idx = c.z * config.grid_x * config.grid_y + c.y * config.grid_x + c.x;
        
        // Count unique cells visited
        if (!visited[idx]) {
            visited[idx] = 1;
            coverage++;
        }
        
        // Check for survivors
        for (int s = 0; s < config.num_survivors; s++) {
            if (c.x == shared->survivors[s].x &&
                c.y == shared->survivors[s].y &&
                c.z == shared->survivors[s].z) {
                survivors_reached++;
            }
        }
        
        // Check collision risk (proximity to obstacles)
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
    
    path->fitness = config.w1 * survivors_reached + 
                   config.w2 * coverage - 
                   config.w3 * path->length - 
                   config.w4 * risk;
    path->survivors_reached = survivors_reached;
    path->coverage = coverage;
}

// Tournament selection
int tournament_selection() {
    int best_idx = rand() % config.population_size;
    float best_fitness = shared->population[best_idx].fitness;
    
    for (int i = 1; i < config.tournament_size; i++) {
        int idx = rand() % config.population_size;
        if (shared->population[idx].fitness > best_fitness) {
            best_fitness = shared->population[idx].fitness;
            best_idx = idx;
        }
    }
    
    return best_idx;
}

// Crossover
void crossover(Path *parent1, Path *parent2, Path *child) {
    int min_len = parent1->length < parent2->length ? parent1->length : parent2->length;
    int crossover_point = rand() % min_len;
    
    child->genes = malloc(config.max_path_length * sizeof(Coord));
    child->length = 0;
    
    // Copy first part from parent1
    for (int i = 0; i < crossover_point && i < parent1->length; i++) {
        child->genes[child->length++] = parent1->genes[i];
    }
    
    // Copy second part from parent2
    for (int i = crossover_point; i < parent2->length && child->length < config.max_path_length; i++) {
        child->genes[child->length++] = parent2->genes[i];
    }
}

// Mutation
void mutate(Path *path) {
    if ((float)rand() / RAND_MAX > config.mutation_rate) return;
    
    if (path->length < 2) return;
    
    int mutation_point = rand() % path->length;
    
    // Random move from mutation point
    Coord directions[6] = {
        {1, 0, 0}, {-1, 0, 0},
        {0, 1, 0}, {0, -1, 0},
        {0, 0, 1}, {0, 0, -1}
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

// Compare function for qsort
int compare_fitness(const void *a, const void *b) {
    Path *pa = (Path *)a;
    Path *pb = (Path *)b;
    if (pb->fitness > pa->fitness) return 1;
    if (pb->fitness < pa->fitness) return -1;
    return 0;
}

// Free path memory
void free_path(Path *path) {
    if (path->genes) {
        free(path->genes);
        path->genes = NULL;
    }
}