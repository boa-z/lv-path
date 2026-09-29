/**
 * @file bench_measure.c
 * @brief Host micro-benchmark: LUT build, queries and slice throughput.
 *
 * Copyright (c) 2026 boa-z
 * SPDX-License-Identifier: MIT
 *
 * @note Uses clock() so the resolution is coarse on Windows; iterate counts
 *       amortize setup; rates below timer resolution are unavailable. The
 *       numbers are indicative host figures, not target guarantees.
 */
#include "path2d/pg_measure.h"
#include "path2d/pg_path.h"
#include "path2d/pg_writer.h"

#include <stdio.h>
#include <time.h>

#define WS_CAP 256u
#define BUILD_ITERS 20000L
#define QUERIES 2000000L
#define SLICE_ITERS 200000L
#define SLICE_CAP 64u

static pg_measure_sample_t g_ws[WS_CAP];
static pg_cmd_t g_slice_cmds[SLICE_CAP];

int main(void)
{
    static const pg_cmd_t s_curve[] = {
        PG_MOVE_TO(0.0f, 0.0f),
        PG_CUBIC_TO(0.0f, 100.0f, 100.0f, 100.0f, 100.0f, 200.0f),
        PG_CUBIC_TO(100.0f, 300.0f, 200.0f, 300.0f, 200.0f, 400.0f),
    };
    pg_path_t path = { s_curve, PG_ARRAY_SIZE(s_curve) };
    pg_measure_t m;
    pg_point_t pos;
    pg_point_t tan;
    pg_path_buffer_t buffer;
    pg_path_writer_t writer;
    double sum = 0.0;
    clock_t t0;
    double measure_secs;
    double query_secs;
    double slice_secs;
    long i;
    float total;

    t0 = clock();
    for (i = 0; i < BUILD_ITERS; i++) {
        if (pg_measure_init(&m, &path, g_ws, WS_CAP, 0.25f) != PG_OK) {
            printf("bench: measure_init failed\n");
            return 1;
        }
        sum += (double)m.sample_count;
    }
    measure_secs = (double)(clock() - t0) / (double)CLOCKS_PER_SEC;
    total = pg_measure_get_length(&m);

    t0 = clock();
    for (i = 0; i < QUERIES; i++) {
        float d = total * (float)(i % 1000L) / 1000.0f;

        if (pg_measure_get_pos_tan(&m, d, &pos, &tan) != PG_OK) {
            printf("bench: query failed\n");
            return 1;
        }
        sum += (double)pos.x + (double)pos.y + (double)tan.x;
    }
    query_secs = (double)(clock() - t0) / (double)CLOCKS_PER_SEC;

    t0 = clock();
    for (i = 0; i < SLICE_ITERS; i++) {
        float s = total * (float)(i % 100L) / 200.0f;
        float e = total * (float)((i % 100L) + 50L) / 200.0f;

        pg_path_buffer_init(&buffer, g_slice_cmds, SLICE_CAP);
        writer = pg_path_buffer_writer(&buffer);
        if (pg_measure_slice(&m, s, e, &writer) != PG_OK) {
            printf("bench: slice failed\n");
            return 1;
        }
        sum += (double)buffer.count;
    }
    slice_secs = (double)(clock() - t0) / (double)CLOCKS_PER_SEC;

    printf("bench: length=%.3f samples=%u workspace_bytes=%u measure_bytes=%u "
           "sample_bytes=%u command_bytes=%u\n", (double)total, m.sample_count,
           (unsigned)sizeof(g_ws), (unsigned)sizeof(m),
           (unsigned)sizeof(pg_measure_sample_t), (unsigned)sizeof(pg_cmd_t));
    printf("bench: builds=%ld elapsed_secs=%.6f\n", BUILD_ITERS, measure_secs);
    printf("bench: queries=%ld elapsed_secs=%.6f checksum=%.3f\n",
           QUERIES, query_secs, sum);
    printf("bench: slices=%ld elapsed_secs=%.6f\n", SLICE_ITERS, slice_secs);
    if (measure_secs > 0.0) printf("bench: builds/sec=%.0f\n", BUILD_ITERS/measure_secs);
    else printf("bench: builds/sec=unavailable (clock resolution)\n");
    if (query_secs > 0.0) printf("bench: queries/sec=%.0f\n", QUERIES/query_secs);
    else printf("bench: queries/sec=unavailable (clock resolution)\n");
    if (slice_secs > 0.0) printf("bench: slices/sec=%.0f\n", SLICE_ITERS/slice_secs);
    else printf("bench: slices/sec=unavailable (clock resolution)\n");
    return 0;
}
