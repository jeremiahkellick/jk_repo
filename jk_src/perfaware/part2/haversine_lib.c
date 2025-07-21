#include <math.h>
#include <stdlib.h>

// #jk_build dependencies_begin
#include <jk_src/jk_lib/json/json.h>
#include <jk_src/perfaware/part4/custom_math_functions.h>
// #jk_build dependencies_end

#include "haversine_lib.h"

JK_PUBLIC double square(double A)
{
    double Result = (A * A);
    return Result;
}

JK_PUBLIC double radians_from_degrees(double Degrees)
{
    double Result = 0.01745329251994329577 * Degrees;
    return Result;
}

// NOTE(casey): EarthRadius is generally expected to be 6372.8
JK_PUBLIC double haversine_reference(double X0, double Y0, double X1, double Y1, double EarthRadius)
{
    /* NOTE(casey): This is not meant to be a "good" way to calculate the Haversine distance.
       Instead, it attempts to follow, as closely as possible, the formula used in the real-world
       question on which these homework exercises are loosely based.
    */

    double lat1 = Y0;
    double lat2 = Y1;
    double lon1 = X0;
    double lon2 = X1;

    double dLat = radians_from_degrees(lat2 - lat1);
    double dLon = radians_from_degrees(lon2 - lon1);
    lat1 = radians_from_degrees(lat1);
    lat2 = radians_from_degrees(lat2);

    double a = square(sin(dLat / 2.0)) + cos(lat1) * cos(lat2) * square(sin(dLon / 2));
    double c = 2.0 * asin(sqrt(a));

    double Result = EarthRadius * c;

    return Result;
}

JK_PUBLIC double haversine_reference_sum(HaversineContext context)
{
    double sum = 0.0;
    double sum_coefficient = 1.0 / (double)context.pair_count;

    for (uint64_t i = 0; i < context.pair_count; i++) {
        HaversinePair pair = context.pairs[i];
        sum += sum_coefficient
                * haversine_reference(pair.v[X0], pair.v[Y0], pair.v[X1], pair.v[Y1], EARTH_RADIUS);
    }

    return sum;
}

JK_PUBLIC uint64_t haversine_reference_verify(HaversineContext context)
{
    uint64_t error_count = 0;

    for (uint64_t i = 0; i < context.pair_count; i++) {
        HaversinePair pair = context.pairs[i];
        if (!approximately_equal(context.answers[i],
                    haversine_reference(
                            pair.v[X0], pair.v[Y0], pair.v[X1], pair.v[Y1], EARTH_RADIUS))) {
            error_count++;
        }
    }

    return error_count;
}

JK_PUBLIC double haversine(double X0, double Y0, double X1, double Y1, double EarthRadius)
{
    double lat1 = Y0;
    double lat2 = Y1;
    double lon1 = X0;
    double lon2 = X1;

    double dLat = radians_from_degrees(lat2 - lat1);
    double dLon = radians_from_degrees(lon2 - lon1);
    lat1 = radians_from_degrees(lat1);
    lat2 = radians_from_degrees(lat2);

    double a = square(jk_sin(dLat / 2.0)) + jk_cos(lat1) * jk_cos(lat2) * square(jk_sin(dLon / 2));
    double c = 2.0 * jk_asin(jk_sqrt(a));

    double Result = EarthRadius * c;

    return Result;
}

JK_PUBLIC double haversine_sum(HaversineContext context)
{
    double sum = 0.0;
    double sum_coefficient = 1.0 / (double)context.pair_count;

    for (uint64_t i = 0; i < context.pair_count; i++) {
        HaversinePair pair = context.pairs[i];
        sum += sum_coefficient
                * haversine(pair.v[X0], pair.v[Y0], pair.v[X1], pair.v[Y1], EARTH_RADIUS);
    }

    return sum;
}

JK_PUBLIC uint64_t haversine_verify(HaversineContext context)
{
    uint64_t error_count = 0;

    for (uint64_t i = 0; i < context.pair_count; i++) {
        HaversinePair pair = context.pairs[i];
        if (!approximately_equal(context.answers[i],
                    haversine(pair.v[X0], pair.v[Y0], pair.v[X1], pair.v[Y1], EARTH_RADIUS))) {
            error_count++;
        }
    }

    return error_count;
}

JK_PUBLIC HaversineContext haversine_setup(
        char *json_file_name, char *answers_file_name, JkArena *storage)
{
    HaversineContext context = {0};

    JkBuffer text = jk_platform_file_read_full(storage, json_file_name);
    JkBuffer answers_buffer = {0};
    if (answers_file_name) {
        answers_buffer = jk_platform_file_read_full(storage, answers_file_name);
    }

    JK_PLATFORM_PROFILE_ZONE_TIME_BEGIN(parse_haversine_pairs);

    JK_PLATFORM_PROFILE_ZONE_BANDWIDTH_BEGIN(parse_json, text.size);
    JkJson *json = jk_json_parse(text, storage);
    JK_PLATFORM_PROFILE_ZONE_END(parse_json);

    if (json == NULL) {
        fprintf(stderr, "haversine_setup: Failed to parse JSON\n");
        exit(1);
    }
    if (json->type != JK_JSON_OBJECT) {
        fprintf(stderr, "haversine_setup: JSON was not an object\n");
        exit(1);
    }
    JkJson *pairs_json = jk_json_member_get(json, "pairs");
    if (pairs_json == NULL) {
        fprintf(stderr, "haversine_setup: JSON object did not have a \"pairs\" member\n");
        exit(1);
    }
    if (pairs_json->type != JK_JSON_ARRAY) {
        fprintf(stderr, "haversine_setup: JSON object \"pairs\" member was not an array\n");
        exit(1);
    }

    context.pair_count = pairs_json->child_count;
    context.pairs = jk_arena_push(storage, context.pair_count * sizeof(context.pairs[0]));

    if (answers_buffer.size) {
        JK_ASSERT(answers_buffer.size == (context.pair_count + 1) * sizeof(double));
        context.answers = (double *)answers_buffer.data;
        context.sum_answer = context.answers[context.pair_count];
    }

    JK_PLATFORM_PROFILE_ZONE_TIME_BEGIN(lookup_and_convert);
    {
        size_t i = 0;
        for (JkJson *pair_json = pairs_json->first_child; pair_json;
                pair_json = pair_json->sibling, i++) {
            for (JkJson *coord_json = pair_json->first_child; coord_json;
                    coord_json = coord_json->sibling) {
                if (coord_json->name.size != 2) {
                    continue;
                }
                uint8_t x_or_y = coord_json->name.data[0] - 'x';
                uint8_t zero_or_one = coord_json->name.data[1] - '0';
                uint8_t j = (zero_or_one << 1) | x_or_y;
                if (x_or_y < 2 && zero_or_one < 2) {
                    context.pairs[i].v[j] = jk_parse_double(coord_json->value);
                }
            }
        }
    }
    JK_PLATFORM_PROFILE_ZONE_END(lookup_and_convert);

    JK_PLATFORM_PROFILE_ZONE_END(parse_haversine_pairs);

    return context;
}

JK_PUBLIC b32 approximately_equal(double a, double b)
{
    double epsilon = 0.0000001;
    double diff = a - b;
    return diff > -epsilon && diff < epsilon;
}

JK_PUBLIC char *coordinate_names[COORDINATE_COUNT] = {
    "x0",
    "y0",
    "x1",
    "y1",
};
