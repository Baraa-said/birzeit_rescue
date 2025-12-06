// types.h
#ifndef TYPES_H
#define TYPES_H

#include "config.h"

#define MAX_PATH_LENGTH 50

typedef struct {
    int x, y, z;
} Coord;

typedef struct {
    Coord genes[MAX_PATH_LENGTH];  // المسار
    int   length;                  // عدد الخطوات المستخدمة
    double fitness;                // قيمة الفتنس (بدائية)
} Path;

// البيانات المشتركة في الشيرد ميموري
typedef struct {
    Path  *population;   // array of Path (size = population_size)
    int   *grid;         // مش رح نستخدمه كثير هسا
    Coord *survivors;    // مش مهمين هسا
    Coord *obstacles;    // مش مهمين هسا

    int generation;      // رقم الجيل الحالي
    int workers_done;    // كم عامل خلص هذا الجيل

    double best_fitness; // أفضل فتنس لحد الآن
    Path   best_path;
} SharedData;

#endif
