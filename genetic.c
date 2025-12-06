// genetic.c
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "genetic.h"

static double last_best = -1e9;
static int stagnation_counter = 0;

void init_environment(int *grid, Coord *survivors, Coord *obstacles) {
    (void)grid;
    (void)survivors;
    (void)obstacles;
    // حالياً مش رح نستخدمهم – بس نرضي الكمبايلر
}

void init_population(Path *population) {
    srand(time(NULL));

    for (int i = 0; i < config.population_size; i++) {
        Path *p = &population[i];
        p->length = rand() % MAX_PATH_LENGTH;
        if (p->length < 5) p->length = 5;

        for (int j = 0; j < p->length; j++) {
            p->genes[j].x = rand() % config.grid_x;
            p->genes[j].y = rand() % config.grid_y;
            p->genes[j].z = rand() % config.grid_z;
        }
        p->fitness = 0.0;
    }
}

// فتنس بدائي بس عشان نشوف أرقام
void calculate_fitness(Path *p) {
    double sum = 0.0;
    for (int i = 0; i < p->length; i++) {
        sum += p->genes[i].x + p->genes[i].y + p->genes[i].z;
    }
    p->fitness = sum;
}

void update_best_solution(SharedData *shared) {
    double best = shared->best_fitness;
    int best_idx = -1;

    for (int i = 0; i < config.population_size; i++) {
        if (shared->population[i].fitness > best) {
            best = shared->population[i].fitness;
            best_idx = i;
        }
    }

    if (best_idx >= 0) {
        shared->best_fitness = best;
        shared->best_path = shared->population[best_idx];
    }

    // منطق بسيط لستاجنيشن
    if (best <= last_best + 1e-6) {
        stagnation_counter++;
    } else {
        stagnation_counter = 0;
        last_best = best;
    }
}

int check_stagnation(void) {
    // لو اتحسنش الفتنس لعدة أجيال – نوقف
    return (stagnation_counter >= 5);
}

// حالياً ما بنعمل تطور حقيقي – بس نضيف شوية عشوائية
void evolve_population(Path *population) {
    for (int i = 0; i < config.population_size; i++) {
        Path *p = &population[i];
        int idx = rand() % p->length;
        p->genes[idx].x = rand() % config.grid_x;
        p->genes[idx].y = rand() % config.grid_y;
        p->genes[idx].z = rand() % config.grid_z;
    }
}

void print_final_result(SharedData *shared) {
    printf("\n=== Final Results (Stub) ===\n");
    printf("Best fitness: %.2f\n", shared->best_fitness);
    printf("Best path length: %d\n", shared->best_path.length);
}
