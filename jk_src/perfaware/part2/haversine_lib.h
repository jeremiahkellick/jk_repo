#ifndef HAVERSINE_LIB_H
#define HAVERSINE_LIB_H

#include <jk_src/jk_lib/jk_lib.h>

#define EARTH_RADIUS 6372.8

JK_PUBLIC double haversine_reference(
        double X0, double Y0, double X1, double Y1, double EarthRadius);

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

JK_PUBLIC char *coordinate_names[COORDINATE_COUNT];

JK_PUBLIC b32 approximately_equal(double a, double b);

#endif
