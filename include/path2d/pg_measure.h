/**
 * @file pg_measure.h
 * @brief Arc-length measurement: LUT build, distance queries and slicing.
 *
 * Copyright (c) 2026 boa-z
 * SPDX-License-Identifier: MIT
 */
#ifndef PATH2D_MEASURE_H
#define PATH2D_MEASURE_H

#include "path2d/pg_types.h"
#include "path2d/pg_writer.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Minimum workspace capacity accepted by pg_measure_init(). */
#define PG_MEASURE_MIN_SAMPLES 2u

/**
 * One arc-length table entry.
 *
 * Samples are strictly increasing in distance. Consecutive samples either
 * belong to one command or straddle a command boundary. In the latter case,
 * the high sample owns the queried span; its initial parameter is t = 0.
 * Boundaries may include zero-length MOVE jumps between separate contours.
 */
typedef struct {
    float distance;         /**< Cumulative arc length at this sample, path units. */
    float t;                /**< Curve parameter within command_index, [0, 1]. */
    uint16_t command_index; /**< Index of the owning command in path->cmds. */
} pg_measure_sample_t;

/**
 * Measurement object. Borrows the path descriptor, its commands and workspace.
 * All three must remain alive and unchanged while queries run. Treat the
 * fields as read-only after initialization; no hidden allocation occurs.
 */
typedef struct {
    const pg_path_t *path;        /**< Measured descriptor (borrowed; commands may be const). */
    pg_measure_sample_t *samples; /**< Caller-owned LUT workspace (borrowed). */
    uint16_t sample_count;        /**< Samples written by init. */
    uint16_t sample_capacity;     /**< Workspace capacity in samples. */
    float total_length;           /**< Total arc length, path units. */
} pg_measure_t;

/**
 * @brief Builds the arc-length lookup table into caller workspace.
 *
 * Bounds local distance interpolation error and preflights whole-path length
 * error before recording (distance, t, command_index) samples. Measurement has
 * stricter acceptance than flattening. In exact arithmetic, length error is
 * <= T/4 and absolute-query arc residual <= 3T/4, where T is the effective
 * tolerance. Float roundoff is additional, not certified by this API; see
 * docs/geometry-contract.md for derivation, limits and resource costs.
 * Multi-subpath semantics: MOVEs only move the cursor, a MOVE to the
 * current position is a no-op, and all subpaths contribute to one continuous
 * distance (the jump itself has zero length). Zero-length prefixes (e.g.
 * L(0,0) before the first real span) produce no samples, so they never
 * become the LUT anchor. A curve returning to its start (a loop) is
 * subdivided and measured like any other geometry.
 *
 * @param[out] measure          Measure object to initialize.
 * @param[in]  path             Path to measure (validated, finite coords).
 * @param[out] workspace        Caller-owned sample array (borrowed, kept).
 * @param[in]  workspace_count  Workspace capacity; below
 *                              PG_MEASURE_MIN_SAMPLES is rejected.
 *                              Required capacity depends on the geometry and
 *                              tolerance. Handle PG_ERR_WORKSPACE_TOO_SMALL
 *                              explicitly; no fixed capacity fits all paths.
 * @param[in]  tolerance        Approximation budget in path units (clamped to
 *                              >= PG_MIN_TOLERANCE).
 * @return                      PG_OK on success;
 *                              PG_ERR_INVALID_ARG for NULL pointers or a
 *                              non-finite/non-positive tolerance;
 *                              PG_ERR_INVALID_PATH for malformed paths or
 *                              unrepresentable/non-increasing float lengths;
 *                              PG_ERR_WORKSPACE_TOO_SMALL when the table does
 *                              not fit (geometry is never truncated);
 *                              PG_ERR_TOLERANCE_NOT_MET when bounded depth or
 *                              preflight refinement cannot meet the budget;
 *                              PG_ERR_DEGENERATE for MOVE-only or
 *                              zero-length paths.
 *
 * @note Fail-atomic: on any failure the object is zeroed and unusable until a
 *       successful re-init. Workspace contents may have been modified and
 *       must not be consumed after failure.
 */
pg_result_t pg_measure_init(pg_measure_t *measure, const pg_path_t *path,
                            pg_measure_sample_t *workspace, uint16_t workspace_count,
                            float tolerance);

/**
 * @brief Returns the total arc length of an initialized measure.
 *
 * @param[in] measure  Initialized or zeroed measure; NULL yields 0.0f.
 * @return             Stored total length (zero after failed initialization).
 *
 * @note This getter does not validate state. Do not pass uninitialized storage.
 */
float pg_measure_get_length(const pg_measure_t *measure);

/**
 * @brief Returns the curve position and unit tangent at an arc distance.
 *
 * The LUT only locates the bracketing samples (binary search, O(log N)) and
 * interpolates the curve parameter; the returned position is then obtained by
 * evaluating the ORIGINAL curve at that parameter (not by interpolating LUT
 * points), and the tangent comes from the curve derivative - so direction
 * stays stable even with few samples. The parameter itself is therefore an
 * approximation. Cumulative arc residual is <= 3T/4 in exact arithmetic;
 * float roundoff is additional. This also bounds position error on a continuous
 * contour, but not across MOVE discontinuities. See docs/geometry-contract.md.
 * distance is clamped to [0, total_length]. No tangent angular-error or exact
 * physical-speed guarantee is made.
 *
 * @param[in]  measure   Initialized measure. Cannot be NULL.
 * @param[in]  distance  Arc distance in path units (clamped; NaN rejected).
 * @param[out] position  Output point. Cannot be NULL.
 * @param[out] tangent   Output unit tangent vector; may be NULL when the
 *                       caller only needs the position.
 * @return               PG_OK on success;
 *                       PG_ERR_INVALID_ARG for NULL measure/position, NaN
 *                       distance or an uninitialized measure;
 *                       PG_ERR_DEGENERATE if the measure holds no length;
 *                       PG_ERR_INVALID_PATH if evaluation is non-finite.
 *
 * @note Zero-derivative queries use a local sampled span in the same command,
 *       then its chord, then (1, 0). Outputs are usable only on PG_OK.
 *       Non-NULL position and tangent must point to distinct objects.
 *       Cost: one endpoint walk over commands plus O(log N) search,
 *       one evaluation and one derivative.
 */
pg_result_t pg_measure_get_pos_tan(const pg_measure_t *measure, float distance,
                                   pg_point_t *position, pg_point_t *tangent);

/**
 * @brief Returns the curve position and unit tangent at a normalized distance.
 *
 * Arc residual relative to normalized true length is <= T in exact arithmetic;
 * float rounding is additional. See docs/geometry-contract.md.
 *
 * @param[in]  measure     Initialized measure. Cannot be NULL.
 * @param[in]  normalized  Normalized distance [0.0, 1.0] (clamped;
 *                         NaN rejected). 0 and 1 map to the first
 *                         and last measurable points, excluding zero-length
 *                         leading/trailing commands.
 * @param[out] position    Output point coordinate. Cannot be NULL.
 * @param[out] tangent     Output normalized tangent vector; may be NULL.
 * @return                 PG_OK on success, PG_ERR_INVALID_ARG for NULL
 *                         measure/position, NaN input or an uninitialized
 *                         measure, PG_ERR_DEGENERATE if the measure holds no
 *                         length; PG_ERR_INVALID_PATH for non-finite evaluation.
 *
 * @note The tangent vector is normalized (length = 1.0).
 *       In a coordinate system where +Y points down, the positive normal
 *       (-t.y, t.x) points to the "right" side of the forward direction.
 */
pg_result_t pg_measure_get_pos_tan_normalized(const pg_measure_t *measure,
                                              float normalized,
                                              pg_point_t *position,
                                              pg_point_t *tangent);

/**
 * @brief Extracts the arc-length range [start, end] into a writer.
 *
 * Curves are restricted with De Casteljau range extraction, keeping their
 * original degree (never a polyline substitute). LINE/CLOSE pieces use
 * line_to calls. Subpath breaks appear as additional move_to calls, so a
 * slice spanning multiple subpaths never draws a connecting jump line.
 *
 * Contract:
 * - NaN start or end, or start > end: PG_ERR_INVALID_ARG.
 * - Out-of-range values are clamped to [0, total_length].
 * - start == end (after clamping): a single move_to at the located position,
 *   then PG_OK.
 * - Writer failures (typically PG_ERR_WORKSPACE_TOO_SMALL) abort the slice
 *   immediately and are propagated. The sink may contain partial output;
 *   discard it after any failure, even if its own callbacks succeeded.
 * - CLOSE is emitted as an explicit line; closed-topology metadata is lost.
 *
 * @param[in] measure  Initialized measure. Cannot be NULL.
 * @param[in] start    Range start distance, path units.
 * @param[in] end      Range end distance, path units.
 * @param[in] writer   Command sink; all four callbacks must be set.
 * @return             PG_OK on success, or the first error code from the
 *                     writer / the validation codes listed above.
 */
pg_result_t pg_measure_slice(const pg_measure_t *measure, float start,
                             float end, const pg_path_writer_t *writer);

/**
 * @brief Normalized variant of pg_measure_slice().
 *
 * @param[in] measure  Initialized measure. Cannot be NULL.
 * @param[in] start    Range start in [0, 1] (clamped; NaN rejected).
 * @param[in] end      Range end in [0, 1] (clamped; NaN rejected).
 * @param[in] writer   Command sink; all four callbacks must be set.
 * @return             Same codes as pg_measure_slice().
 *
 * @note start/end are compared before clamping, so start > end is rejected
 *       even when both values fall outside [0, 1].
 */
pg_result_t pg_measure_slice_normalized(const pg_measure_t *measure,
                                        float start, float end,
                                        const pg_path_writer_t *writer);

/**
 * @brief Normalizes a vector to unit length.
 *
 * @param[in] v  Input vector; non-finite vectors and magnitudes <= PG_EPSILON
 *               map to (1, 0).
 * @return       Unit vector (never NaN/Inf).
 */
pg_point_t pg_vec_normalize(pg_point_t v);

/**
 * @brief Rotates a tangent into its normal (-t.y, t.x).
 *
 * @param[in] tangent  Unit tangent vector.
 * @return             Normal vector (same length as the input).
 */
pg_point_t pg_tangent_to_normal(pg_point_t tangent);

#ifdef __cplusplus
}
#endif

#endif /* PATH2D_MEASURE_H */
