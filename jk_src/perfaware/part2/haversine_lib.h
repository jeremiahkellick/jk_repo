#ifndef HAVERSINE_LIB_H
#define HAVERSINE_LIB_H

#include <jk_src/jk_lib/platform/platform.h>

#define EARTH_RADIUS 6372.8

JK_PUBLIC double square(double A);

JK_PUBLIC double radians_from_degrees(double Degrees);

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

JK_PUBLIC double haversine(double X0, double Y0, double X1, double Y1, double EarthRadius);

JK_PUBLIC double haversine_sum(HaversineContext context);

JK_PUBLIC uint64_t haversine_verify(HaversineContext context);

JK_PUBLIC HaversineContext haversine_setup(
        char *json_file_name, char *answers_file_name, JkPlatformArena *storage);

JK_PUBLIC b32 approximately_equal(double a, double b);

JK_PUBLIC char *coordinate_names[COORDINATE_COUNT];

#endif
