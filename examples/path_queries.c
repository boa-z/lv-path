/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 boa-z
 * Caller-owned storage; no rendering, OS, or allocator required. */
#include "path2d/pg_flatten.h"
#include "path2d/pg_measure.h"

static const pg_cmd_t commands[] = {
    PG_MOVE_TO(0.0f, 0.0f),
    PG_QUAD_TO(25.0f, 50.0f, 50.0f, 0.0f),
    PG_CUBIC_TO(60.0f, -25.0f, 90.0f, 25.0f, 100.0f, 0.0f),
};
static const pg_path_t path = {commands, PG_ARRAY_SIZE(commands)};
static pg_measure_sample_t samples[256];
static pg_cmd_t output_commands[128];

int main(void) {
    pg_measure_t measure;
    pg_path_buffer_t buffer;
    pg_path_writer_t writer;
    pg_path_t output;
    pg_point_t position, tangent, normal;
    pg_result_t result;

    result = pg_measure_init(&measure, &path, samples, PG_ARRAY_SIZE(samples), 0.1f);
    if (result != PG_OK)
        return (int)result;
    result = pg_measure_get_pos_tan_normalized(&measure, 0.5f, &position, &tangent);
    if (result != PG_OK)
        return (int)result;
    normal = pg_tangent_to_normal(tangent);
    /* Application may now use position, tangent and normal in its own units. */
    (void)position;
    (void)normal;

    result = pg_path_buffer_init(&buffer, output_commands, PG_ARRAY_SIZE(output_commands));
    if (result != PG_OK)
        return (int)result;
    writer = pg_path_buffer_writer(&buffer);
    result = pg_measure_slice_normalized(&measure, 0.2f, 0.8f, &writer);
    if (result != PG_OK)
        return (int)result; /* Discard ALL partial output. */
    result = pg_path_buffer_to_path(&buffer, &output);
    if (result != PG_OK)
        return (int)result;
    /* Consume output before reusing its borrowed output_commands storage. */

    result = pg_path_buffer_init(&buffer, output_commands, PG_ARRAY_SIZE(output_commands));
    if (result != PG_OK)
        return (int)result;
    writer = pg_path_buffer_writer(&buffer);
    result = pg_path_flatten(&path, 0.1f, &writer);
    if (result != PG_OK)
        return (int)result;
    return (int)pg_path_buffer_to_path(&buffer, &output);
}
