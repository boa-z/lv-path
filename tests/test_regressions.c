/**
 * @file test_regressions.c
 * @brief Numerical range and multi-contour contract regressions.
 * Copyright (c) 2026 boa-z
 * SPDX-License-Identifier: MIT
 */
#include "path2d/pg_measure.h"
#include "path2d/pg_path.h"
#include "test_util.h"

#include <float.h>

static pg_measure_sample_t samples[256];

static void test_large_finite_geometry(void) {
    const pg_cmd_t cmds[] = {PG_MOVE_TO(0, 0), PG_LINE_TO(1e20f, 1e20f)};
    const pg_path_t path = {cmds, PG_ARRAY_SIZE(cmds)};
    pg_measure_t measure;
    pg_point_t position, tangent, normal;

    TU_EXPECT(pg_measure_init(&measure, &path, samples, 256, 0.1f) == PG_OK);
    TU_NEAR(pg_measure_get_length(&measure) / 1e20f, 1.41421356f, 1e-6f);
    TU_EXPECT(pg_measure_get_pos_tan_normalized(&measure, 0.5f, &position, &tangent) == PG_OK);
    TU_NEAR(position.x / 1e20f, 0.5f, 1e-6f);
    TU_NEAR(tangent.x, 0.70710678f, 1e-6f);
    TU_NEAR(tangent.y, 0.70710678f, 1e-6f);
    normal = pg_tangent_to_normal(tangent);
    TU_NEAR(normal.x * tangent.x + normal.y * tangent.y, 0.0f, 1e-6f);

    tangent = pg_vec_normalize((pg_point_t){FLT_MAX, FLT_MAX});
    TU_NEAR(tangent.x, 0.70710678f, 1e-6f);
    TU_NEAR(tangent.y, 0.70710678f, 1e-6f);
    tangent = pg_vec_normalize((pg_point_t){tu_inf(), 1.0f});
    TU_POINT_NEAR(tangent, 1.0f, 0.0f, 0.0f);
}

static void test_unrepresentable_length(void) {
    const pg_cmd_t overflow[] = {PG_MOVE_TO(0, 0), PG_LINE_TO(FLT_MAX * 0.75f, 0),
                                 PG_LINE_TO(0, 0)};
    const pg_cmd_t stagnation[] = {PG_MOVE_TO(0, 0), PG_LINE_TO(1e8f, 0), PG_MOVE_TO(0, 0),
                                   PG_LINE_TO(1, 0)};
    const pg_path_t paths[] = {{overflow, PG_ARRAY_SIZE(overflow)},
                               {stagnation, PG_ARRAY_SIZE(stagnation)}};
    unsigned i;

    for (i = 0; i < PG_ARRAY_SIZE(paths); i++) {
        pg_measure_t measure;
        TU_EXPECT(pg_path_validate(&paths[i]) == PG_OK); /* structurally valid */
        TU_EXPECT(pg_measure_init(&measure, &paths[i], samples, 256, 0.1f) == PG_ERR_INVALID_PATH);
        TU_EXPECT(measure.path == NULL && measure.samples == NULL && measure.sample_count == 0 &&
                  measure.total_length == 0.0f);
    }
}

static void test_tangent_at_disconnected_contour(void) {
    const pg_cmd_t cmds[] = {PG_MOVE_TO(-1000, 0), PG_LINE_TO(-900, 0), PG_MOVE_TO(0, 0),
                             PG_CUBIC_TO(0, 0, 0, 100, 100, 100)};
    const pg_path_t path = {cmds, PG_ARRAY_SIZE(cmds)};
    pg_measure_t measure;
    pg_point_t position, tangent;

    TU_EXPECT(pg_measure_init(&measure, &path, samples, 256, 0.1f) == PG_OK);
    TU_EXPECT(pg_measure_get_pos_tan(&measure, 100.0f, &position, &tangent) == PG_OK);
    TU_POINT_NEAR(position, 0.0f, 0.0f, 1e-6f);
    /* The MOVE jump is not a direction of travel. The new curve leaves +Y. */
    TU_EXPECT(tangent.y > 0.99f && fabsf(tangent.x) < 0.05f);
}

static void test_slice_near_parameter_zero(void) {
    const pg_cmd_t quad[] = {PG_MOVE_TO(0, 0), PG_QUAD_TO(500000, 0, 1000000, 0)};
    const pg_cmd_t cubic[] = {PG_MOVE_TO(0, 0), PG_CUBIC_TO(1000000, 0, 2000000, 0, 3000000, 0)};
    const pg_path_t paths[] = {{quad, 2}, {cubic, 2}};
    pg_cmd_t output[4];
    unsigned i;

    for (i = 0; i < PG_ARRAY_SIZE(paths); i++) {
        pg_measure_t measure;
        pg_path_buffer_t buffer;
        pg_path_writer_t writer;
        TU_EXPECT(pg_measure_init(&measure, &paths[i], samples, 256, 0.1f) == PG_OK);
        TU_EXPECT(pg_path_buffer_init(&buffer, output, 4) == PG_OK);
        writer = pg_path_buffer_writer(&buffer);
        TU_EXPECT(pg_measure_slice(&measure, 0.03f, 0.06f, &writer) == PG_OK);
        TU_EXPECT(buffer.count == 2);
        TU_POINT_NEAR(output[0].p1, 0.03f, 0.0f, 1e-7f);
        if (i == 0) {
            TU_EXPECT(output[1].type == PG_CMD_QUAD);
            TU_POINT_NEAR(output[1].p1, 0.045f, 0.0f, 1e-7f);
            TU_POINT_NEAR(output[1].p2, 0.06f, 0.0f, 1e-7f);
        } else {
            TU_EXPECT(output[1].type == PG_CMD_CUBIC);
            TU_POINT_NEAR(output[1].p1, 0.04f, 0.0f, 1e-7f);
            TU_POINT_NEAR(output[1].p2, 0.05f, 0.0f, 1e-7f);
            TU_POINT_NEAR(output[1].p3, 0.06f, 0.0f, 1e-7f);
        }
    }
}

static void test_uninitialized_slice(void) {
    pg_measure_t measure = {0};
    pg_cmd_t output[4];
    pg_path_buffer_t buffer;
    pg_path_writer_t writer;

    TU_EXPECT(pg_path_buffer_init(&buffer, output, 4) == PG_OK);
    writer = pg_path_buffer_writer(&buffer);
    TU_EXPECT(pg_measure_slice(&measure, 0.0f, 1.0f, &writer) == PG_ERR_INVALID_ARG);
    TU_EXPECT(pg_measure_slice_normalized(&measure, 0.0f, 1.0f, &writer) == PG_ERR_INVALID_ARG);
    TU_EXPECT(buffer.count == 0);
}

static void final_contour_tangent(void) {
    const pg_cmd_t commands[] = {
        PG_MOVE_TO(-1000, 0),
        PG_LINE_TO(-900, 0),
        PG_MOVE_TO(0, 0),
        PG_QUAD_TO(0, 100, 0, 100),
    };
    const pg_path_t path = {commands, PG_ARRAY_SIZE(commands)};
    pg_measure_sample_t samples[256];
    pg_measure_t measure;
    pg_point_t position, tangent;
    TU_EXPECT(pg_measure_init(&measure, &path, samples, 256, 0.1f) == PG_OK);
    TU_EXPECT(pg_measure_get_pos_tan(&measure, 200.0f, &position, &tangent) == PG_OK);
    TU_NEAR(tangent.x, 0.0f, 1e-6f);
    TU_NEAR(tangent.y, 1.0f, 1e-6f);
}

int main(void) {
    final_contour_tangent();
    TU_EXPECT(!tu_near(tu_nan(), 0.0, 1e-3));
    TU_EXPECT(!tu_near(tu_inf(), tu_inf(), 1e-3));
    test_large_finite_geometry();
    test_unrepresentable_length();
    test_tangent_at_disconnected_contour();
    test_slice_near_parameter_zero();
    test_uninitialized_slice();
    return TU_SUMMARY() ? 1 : 0;
}
