/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 boa-z */
#include "path2d/pg_bezier.h"
#include "path2d/pg_measure.h"
#include "test_util.h"

/* Independent double-precision Bernstein oracle; no pg_* in reference math. */
typedef struct {
    double x, y;
} ref_point_t;
static ref_point_t reference(const pg_point_t *p, unsigned degree, double t) {
    double u = 1.0 - t;
    double w[4];
    ref_point_t r = {0.0, 0.0};
    unsigned i;
    if (degree == 2u) {
        w[0] = u * u;
        w[1] = 2.0 * u * t;
        w[2] = t * t;
        w[3] = 0.0;
    } else {
        w[0] = u * u * u;
        w[1] = 3.0 * u * u * t;
        w[2] = 3.0 * u * t * t;
        w[3] = t * t * t;
    }
    for (i = 0; i <= degree; ++i) {
        r.x += w[i] * p[i].x;
        r.y += w[i] * p[i].y;
    }
    return r;
}
static double max_query_error;
static uint32_t seed = 0x50415448u;
static float coordinate(void) {
    seed = seed * 1664525u + 1013904223u;
    return (float)(seed >> 8) / 16777216.0f * 200.0f - 100.0f;
}
static void point_near(pg_point_t actual, ref_point_t expected, double tolerance) {
    if (fabs(actual.x - expected.x) > tolerance || fabs(actual.y - expected.y) > tolerance)
        printf("point actual=(%.9g,%.9g) reference=(%.9g,%.9g) tolerance=%.9g\n", (double)actual.x,
               (double)actual.y, expected.x, expected.y, tolerance);
    TU_NEAR(actual.x, expected.x, tolerance);
    TU_NEAR(actual.y, expected.y, tolerance);
}
static void check_curve(unsigned degree) {
    enum { STEPS = 4096 };
    pg_point_t p[4];
    pg_measure_sample_t workspace[1024];
    pg_cmd_t commands[2] = {PG_MOVE_TO(0, 0), PG_LINE_TO(0, 0)};
    pg_path_t path = {commands, 2};
    pg_measure_t measure;
    double cumulative[STEPS + 1];
    ref_point_t previous, current;
    pg_result_t result;
    unsigned i;
    for (i = 0; i < 4; ++i) {
        p[i].x = coordinate();
        p[i].y = coordinate();
    }
    commands[0].p1 = p[0];
    commands[1].type = degree == 2u ? PG_CMD_QUAD : PG_CMD_CUBIC;
    commands[1].p1 = p[1];
    commands[1].p2 = p[2];
    commands[1].p3 = p[3];
    for (i = 0; i <= 20; ++i) {
        float t = (float)i / 20.0f;
        float split = 0.37f;
        ref_point_t expected = reference(p, degree, t);
        if (degree == 2u) {
            pg_quad_t left, right;
            point_near(pg_quad_eval(p[0], p[1], p[2], t), expected, 4e-5);
            pg_quad_split(p[0], p[1], p[2], split, &left, &right);
            point_near(pg_quad_eval(left.p0, left.p1, left.p2, t),
                       reference(p, degree, (double)t * split), 5e-5);
            point_near(pg_quad_eval(right.p0, right.p1, right.p2, t),
                       reference(p, degree, split + (1.0 - split) * t), 5e-5);
        } else {
            pg_cubic_t left, right;
            point_near(pg_cubic_eval(p[0], p[1], p[2], p[3], t), expected, 4e-5);
            pg_cubic_split(p[0], p[1], p[2], p[3], split, &left, &right);
            point_near(pg_cubic_eval(left.p0, left.p1, left.p2, left.p3, t),
                       reference(p, degree, (double)t * split), 5e-5);
            point_near(pg_cubic_eval(right.p0, right.p1, right.p2, right.p3, t),
                       reference(p, degree, split + (1.0 - split) * t), 5e-5);
        }
    }
    cumulative[0] = 0.0;
    previous = reference(p, degree, 0.0);
    for (i = 1; i <= STEPS; ++i) {
        current = reference(p, degree, (double)i / STEPS);
        cumulative[i] = cumulative[i - 1] + hypot(current.x - previous.x, current.y - previous.y);
        previous = current;
    }
    result = pg_measure_init(&measure, &path, workspace, PG_ARRAY_SIZE(workspace), 0.005f);
    TU_EXPECT(result == PG_OK);
    if (result != PG_OK)
        return;
    /* Empirical regression envelopes for this seeded corpus, not API guarantees. */
    TU_NEAR(measure.total_length, cumulative[STEPS], cumulative[STEPS] * 0.0005);
    for (i = 1; i < measure.sample_count; ++i)
        TU_EXPECT(workspace[i].distance > workspace[i - 1].distance);
    for (i = 0; i <= 20; ++i) {
        double distance = cumulative[STEPS] * i / 20.0;
        unsigned j = 1;
        double t;
        pg_point_t position, tangent, normal;
        while (j < STEPS && cumulative[j] < distance)
            ++j;
        t = ((j - 1) + (distance - cumulative[j - 1]) / (cumulative[j] - cumulative[j - 1])) /
            STEPS;
        result = pg_measure_get_pos_tan_normalized(&measure, (float)i / 20.0f, &position, &tangent);
        TU_EXPECT(result == PG_OK);
        if (result != PG_OK)
            continue;
        current = reference(p, degree, t);
        {
            double error = hypot(position.x - current.x, position.y - current.y);
            if (error > max_query_error)
                max_query_error = error;
            /* Normalized-query budget plus corpus-specific roundoff allowance. */
            TU_EXPECT(error <= 0.005 + 0.0001);
        }
        TU_NEAR(hypot(tangent.x, tangent.y), 1.0, 2e-6);
        normal = pg_tangent_to_normal(tangent);
        TU_NEAR((double)tangent.x * normal.x + (double)tangent.y * normal.y, 0.0, 2e-6);
    }
}
static void nonuniform_parameter_regression(void) {
    const pg_cmd_t commands[] = {PG_MOVE_TO(0, 0), PG_QUAD_TO(0, 0, 100, 0)};
    const pg_path_t path = {commands, PG_ARRAY_SIZE(commands)};
    static pg_measure_sample_t samples[4097];
    pg_measure_t measure;
    pg_point_t position;
    pg_result_t result = pg_measure_init(&measure, &path, samples, PG_ARRAY_SIZE(samples), 0.0001f);
    TU_EXPECT(result == PG_OK);
    if (result != PG_OK)
        return;
    TU_NEAR(measure.total_length, 100.0, 1e-5);
    result = pg_measure_get_pos_tan(&measure, 50.0f, &position, NULL);
    TU_EXPECT(result == PG_OK);
    /* Geometric flatness must not hide nonlinear parameter speed. */
    TU_NEAR(position.x, 50.0, 0.0001);
}
int main(void) {
    unsigned i;
    for (i = 0; i < 24; ++i) {
        check_curve(2);
        check_curve(3);
    }
    nonuniform_parameter_regression();
    printf("reference: max_seeded_query_error=%.6f path_units\n", max_query_error);
    return TU_SUMMARY() ? 1 : 0;
}
