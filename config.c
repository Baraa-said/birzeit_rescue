// config.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>   // عشان sysconf
#include "config.h"

Config config;

// دالة صغيرة تشيل المسافات من بداية ونهاية النص
static char *trim(char *s) {
    while (isspace((unsigned char)*s)) s++;
    if (*s == '\0') return s;

    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) end--;
    end[1] = '\0';

    return s;
}

// دالة ترجع عدد الأنوية (cores) المتاحة
static int detect_cores(void) {
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    if (n < 1) n = 1;      // أمان: أقل إشي 1
    if (n > 32) n = 32;    // حد أعلى منطقي
    return (int)n;
}

// قراءة الإعدادات من الملف
int read_config(const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        fprintf(stderr,
                "[config] ERROR: could not open '%s'. Please create it and fill parameters.\n",
                filename);
        return 0;
    }

    // صفرنا struct بالبداية
    memset(&config, 0, sizeof(Config));

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char *p = trim(line);
        if (*p == '\0' || *p == '#')
            continue;   // سطر فاضي أو comment

        char *eq = strchr(p, '=');
        if (!eq) continue;

        *eq = '\0';
        char *key = trim(p);
        char *val = trim(eq + 1);
        if (*key == '\0' || *val == '\0')
            continue;

        double dval = atof(val);
        int    ival = (int)dval;

        if (strcmp(key, "grid_x") == 0)              config.grid_x = ival;
        else if (strcmp(key, "grid_y") == 0)         config.grid_y = ival;
        else if (strcmp(key, "grid_z") == 0)         config.grid_z = ival;

        else if (strcmp(key, "population_size") == 0) config.population_size = ival;
        else if (strcmp(key, "num_generations") == 0) config.num_generations = ival;
        else if (strcmp(key, "num_processes") == 0)   config.num_processes   = ival;

        else if (strcmp(key, "max_path_length") == 0) config.max_path_length = ival;

        else if (strcmp(key, "num_survivors") == 0)   config.num_survivors   = ival;
        else if (strcmp(key, "num_obstacles") == 0)   config.num_obstacles   = ival;

        else if (strcmp(key, "w1") == 0)            config.w1 = dval;
        else if (strcmp(key, "w2") == 0)            config.w2 = dval;
        else if (strcmp(key, "w3") == 0)            config.w3 = dval;
        else if (strcmp(key, "w4") == 0)            config.w4 = dval;
    }

    fclose(f);

    // 🔹 بعد قراءة الملف: نضبط num_processes بطريقة ديناميكية
    int cores = detect_cores();

    if (config.num_processes <= 0) {
        // لو اليوزر ما حدد num_processes في الملف (أو حط 0)
        config.num_processes = cores;
    } else if (config.num_processes > cores) {
        // لو حط رقم أكبر من عدد الأنوية، نقلّصه
        fprintf(stderr,
                "[config] Warning: requested %d processes but only %d cores available. "
                "Using %d processes instead.\n",
                config.num_processes, cores, cores);
        config.num_processes = cores;
    }

    // كمان نضمن ما يكون عدد الـ processes أكبر من الـ population
    if (config.population_size > 0 &&
        config.num_processes > config.population_size) {
        config.num_processes = config.population_size;
    }

    // تأكد من القيم الأساسية
    if (config.grid_x <= 0 || config.grid_y <= 0 || config.grid_z <= 0 ||
        config.population_size <= 0 || config.num_generations <= 0 ||
        config.max_path_length <= 0) {

        fprintf(stderr,
                "[config] ERROR: missing or invalid mandatory parameters in '%s'.\n",
                filename);
        return 0;
    }

    return 1;  // كلشي تمام
}

void print_config(void) {
    printf("Configuration:\n");
    printf("  Grid: %dx%dx%d\n", config.grid_x, config.grid_y, config.grid_z);
    printf("  Population: %d\n", config.population_size);
    printf("  Generations: %d\n", config.num_generations);
    printf("  Processes: %d\n", config.num_processes);
    printf("  Max path length: %d\n", config.max_path_length);
    printf("  Survivors: %d, Obstacles: %d\n",
           config.num_survivors, config.num_obstacles);
    printf("  Weights: w1=%.2f, w2=%.2f, w3=%.2f, w4=%.2f\n",
           config.w1, config.w2, config.w3, config.w4);
}
//**********************************************************************