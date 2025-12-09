// main.c
#include <stdio.h>
#include <unistd.h>

#include "types.h"
#include "config.h"
#include "genetic.h"
#include "pool.h"

int main(int argc, char *argv[]) {
    // 1) اقرأ الإعدادات من الملف
    if (!read_config(argc > 1 ? argv[1] : "config.txt")) {
        fprintf(stderr, "Fatal: could not read config.\n");
        return 1;
    }
    print_config();

    // 2) تهيئة الشيرد ميموري + IPC
    init_shared_memory();

    // 3) تهيئة البيئة + population
    init_environment(shared->grid, shared->survivors, shared->obstacles);
    init_population(shared->population);

    // 4) إنشاء الـ process pool
    pid_t pids[config.num_processes];
    create_process_pool(pids);

    // 5) مراقبة الأجيال وطباعة أفضل فتنس
    int last_gen = -1;
    while (1) {
        lock_sem();
        int gen  = shared->generation;
        double best = shared->best_fitness;
        unlock_sem();

        if (gen != last_gen) {
            printf("Gen %3d: Best fitness = %.2f\n", gen, best);
            last_gen = gen;
        }

        if (gen >= config.num_generations) {
            break;
        }

        usleep(100000); // 0.1 ثانية
    }

    // 6) انتظار انتهاء العمال وطباعة النتيجة
    wait_for_workers(pids);
    print_final_result(shared);
    cleanup_shared_memory();

    return 0;
}
