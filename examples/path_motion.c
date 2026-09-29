/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 boa-z
 * Emit a point's motion along a curve as CSV. No renderer or timer is needed.
 * Distance approximation, float and direction limits are described in
 * docs/geometry-contract.md and examples/README.md.
 */
#include "path2d/pg_measure.h"

#include <stdio.h>

#define MOTION_STEPS 120u
#define MOTION_SECONDS 2.0f
#define SAMPLE_CAPACITY 256u

/* One continuous cubic contour, in caller-defined coordinate units. */
static const pg_cmd_t commands[] = {
    PG_MOVE_TO(0.0f, 0.0f),
    PG_CUBIC_TO(0.0f, 80.0f, 120.0f, 80.0f, 120.0f, 0.0f),
};
static const pg_path_t path = {commands, PG_ARRAY_SIZE(commands)};
static pg_measure_sample_t samples[SAMPLE_CAPACITY];

int main(void) {
    pg_measure_t measure;
    pg_point_t position, tangent, normal;
    pg_result_t result;
    float length;
    unsigned step;

    result = pg_measure_init(&measure, &path, samples, SAMPLE_CAPACITY, 0.1f);
    if (result != PG_OK) {
        fprintf(stderr, "path measurement failed: %s\n", pg_result_str(result));
        return 1;
    }
    length = pg_measure_get_length(&measure);
    if (printf("time_s,requested_distance,x,y,tangent_x,tangent_y,normal_x,normal_y\n") < 0)
        return 1;

    /* A real client supplies its own elapsed time. This loop simulates two
     * seconds at 60 samples per second without sleeping or retaining frames. */
    for (step = 0; step <= MOTION_STEPS; ++step) {
        float fraction = (float)step / (float)MOTION_STEPS;
        float time = MOTION_SECONDS * fraction;
        float distance = length * fraction;

        result = pg_measure_get_pos_tan(&measure, distance, &position, &tangent);
        if (result != PG_OK) {
            fprintf(stderr, "query %u failed: %s\n", step, pg_result_str(result));
            return 1; /* Any already emitted CSV is incomplete. */
        }
        normal = pg_tangent_to_normal(tangent);
        if (printf("%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f\n", (double)time, (double)distance,
                   (double)position.x, (double)position.y, (double)tangent.x, (double)tangent.y,
                   (double)normal.x, (double)normal.y) < 0)
            return 1;
    }
    /* Host stdio may allocate internally; the geometry core never calls it. */
    return fflush(stdout) == 0 ? 0 : 1;
}
