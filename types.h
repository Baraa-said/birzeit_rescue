#ifndef TYPES_H
#define TYPES_H

#include "config.h"

#define MAX_PATH_LENGTH 50   // الحد الأقصى الثابت للجينات في المسار
#define EMPTY    0
#define OBSTACLE 1
#define SURVIVOR 2

typedef struct {
    int x, y, z;
} Coord;

// مسار = كروموسوم = سلسلة إحداثيات
typedef struct {
    Coord  genes[MAX_PATH_LENGTH]; // المسار كاملاً
    int    length;                 // عدد الخطوات المستخدمة فعلياً
    double fitness;                // قيمة الفتنس

    int survivors_reached;         // كم ناجي مر عليهم الروبوت
    int coverage;                  // كم خلية مختلفة زارها الروبوت
} Path;

// البيانات المشتركة في الشيرد ميموري
typedef struct {
    Path  *population;   // array of Path (size = population_size)
    int   *grid;         // 3D grid ممسوح على 1D
    Coord *survivors;    // مواقع الناجين
    Coord *obstacles;    // مواقع العوائق

    int generation;      // رقم الجيل الحالي (كم جيل *اكتمل*)
    int workers_done;    // كم عامل خلص هذا الجيل

    double best_fitness; // أفضل فتنس لحد الآن
    Path   best_path;    // أفضل مسار لحد الآن
} SharedData;

#endif
