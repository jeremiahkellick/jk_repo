#ifndef HAVERSINE_LIB_H
#define HAVERSINE_LIB_H

#include <jk_src/jk_lib/jk_lib.h>

#define EARTH_RADIUS 6372.8

JK_PUBLIC double haversine_reference(
        double x0, double y0, double x1, double y1, double earth_radius);

typedef enum Coordinate {
    X0,
    Y0,
    X1,
    Y1,
    COORDINATE_COUNT,
} Coordinate;

typedef struct HaversinePair {
    double v[COORDINATE_COUNT];
} HaversinePair;

typedef struct HaversineContext {
    uint64_t pair_count;
    HaversinePair *pairs;
    double *answers;
    double sum_answer;
} HaversineContext;

typedef double (*HaversineSumFunction)(HaversineContext context);

typedef uint64_t (*HaversineSumVerifyFunction)(HaversineContext context);

JK_PUBLIC double haversine_reference_sum(HaversineContext context);

JK_PUBLIC uint64_t haversine_reference_verify(HaversineContext context);

JK_PUBLIC char *coordinate_names[COORDINATE_COUNT];

JK_PUBLIC b32 approximately_equal(double a, double b);

#endif
