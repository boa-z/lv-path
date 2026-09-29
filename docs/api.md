# API and memory contracts

The public headers are the declaration reference. The complete
[example](../examples/path_queries.c) is compiled and executed by CTest.

## Public surface

| Header | Operations |
| --- | --- |
| `pg_types.h` | Types, command initializers, error strings, version and limits |
| `pg_path.h` | `pg_path_validate` |
| `pg_bezier.h` | Quadratic/cubic evaluation, derivatives and splits |
| `pg_flatten.h` | `pg_path_flatten` |
| `pg_measure.h` | Init, total length, distance/normalized position and tangent, slices, vector normalization, tangent-to-normal rotation |
| `pg_writer.h` | Callback sink and fixed-capacity command buffer |

Paths begin with MOVE. Each command uses only its defined point fields. CLOSE
returns to the current contour start. MOVE changes the cursor without adding
length; repeated and disconnected contours share one cumulative distance domain.
Validation checks structure and consumed coordinates, not every possible future
floating-point intermediate or sufficient workspace.

## API review decisions (2026-09-29)

This follow-up preserves every public declaration and adds no library entry point.
Generic pg (path geometry) names describe commands, measurements, Beziers and
writers; there is no widget, display or application state. The path2d header
prefix and compatibility target remain to avoid a rename without maintainer input.

| Area | Decision / boundary |
| --- | --- |
| Ownership | Keep borrowed immutable input and caller-owned workspace; descriptor lifetime matters as much as array lifetime |
| Exposed structs | Retain static-allocation layouts; callers treat bookkeeping as read-only; discuss opacity/ABI before 1.0 |
| Scalar/count types | Keep float and uint16_t; neither arbitrary precision nor unlimited capacity is implied |
| Result codes | Keep existing enum; detected numeric-range failure uses PG_ERR_INVALID_PATH |
| Getter behavior | pg_measure_get_length has no status and does not validate its argument; use after successful init, or with NULL/a zeroed object (length zero) |
| Low-level helpers | Bezier primitives require finite intermediates and have no status; they are not checked path operations |
| Writers | Synchronous non-owning sinks; errors can expose a partial prefix; CLOSE metadata is not retained |
| API growth | No new renderer, path format, allocator, precision option, bounds API or upstream namespace |

Return values depend on the operation. A MOVE-only path is structurally valid and
flattening can successfully emit its MOVE, but measurement returns PG_ERR_DEGENERATE.
NULL workspace is PG_ERR_INVALID_ARG; non-NULL workspace below two samples is
PG_ERR_WORKSPACE_TOO_SMALL. An empty buffer cannot be exported (PG_ERR_INVALID_PATH).
Result strings are diagnostic; use enum values for logic and check each operation's
contract. No guarantee is made about which error wins when several inputs are invalid.

[Geometry contract](geometry-contract.md) separates flatness, length and query
accuracy. [Embedded engineering](embedded-engineering.md) covers buffer and stack
budgets. Neither document expands the public API.

## Ownership and lifetime

1. The caller owns the command array and `pg_path_t` descriptor. Commands may be
   const/Flash-resident if accessible through ordinary C pointers on the target.
2. The caller owns `pg_measure_t` and the sample array supplied to init. The
   descriptor, command array, and sample array must all remain alive and unchanged
   through queries and slices. Keeping commands alive alone is insufficient.
3. Treat measure and buffer bookkeeping fields as library-managed. Public layout
   permits static allocation; it is not permission to mutate invariants.
4. A writer context must outlive its synchronous operation. A buffer-exported
   path borrows its command storage and must be consumed before that storage is
   reused. Source paths and destination/workspace storage must not overlap.
5. Position and tangent outputs must be distinct objects when both are supplied.
   There is no destroy function because the library acquires no resources.

Counts/capacities use `uint16_t`: at most 65535 commands or samples. Check external
counts before narrowing; `PG_ARRAY_SIZE` does not perform that check. At least two
sample slots are required. Required capacity depends on geometry and tolerance;
there is no universal fixed array size and no automatic allocation retry.

## Errors and partial output

| Result | Meaning |
| --- | --- |
| `PG_OK` | Operation completed; outputs are usable |
| `PG_ERR_INVALID_ARG` | Missing pointer/callback, invalid tolerance, NaN query, invalid range, or uninitialized measure |
| `PG_ERR_INVALID_PATH` | Malformed/non-finite input or detected unrepresentable geometry/length |
| `PG_ERR_WORKSPACE_TOO_SMALL` | Caller storage exhausted; operation failed rather than silently succeeding with truncation |
| `PG_ERR_DEGENERATE` | Valid input has no measurable length |

On failed measure initialization, a non-NULL measure is reset to an unusable zero
state. Its sample workspace may have been partially overwritten. On query error,
ignore output points. Callback-producing operations stop at the first error;
they cannot roll back an arbitrary sink. Discard partial output after **any**
producer error. The built-in buffer becomes poisoned on its own first callback
failure; a producer-side failure does not necessarily poison it. Successful
buffer export alone is not proof that the producer succeeded.

## Distance and contour semantics

Finite out-of-range distances and infinities clamp to the endpoints; NaN fails.
Normalized distances follow the same rule. Range endpoints are checked for
reversed order before clamping. An equal-endpoint slice produces a MOVE.
At an interior command/contour boundary, queries select the later measurable
command at t=0. Distance 0/total selects the first/last measurable point; leading
or trailing zero-length commands do not redefine those anchors.

Tangents use the analytic derivative, then a local sampled span in the same
command, then the command chord, then (1,0). At a cusp this is a deterministic
convention, not a unique mathematical tangent. `pg_vec_normalize` maps non-finite
inputs and magnitudes <= `PG_EPSILON` to (1,0). `pg_tangent_to_normal` rotates
(-y,x), preserves magnitude, and expects a finite unit tangent for a unit normal.
Its visual side depends on the coordinate system.

Flatten and slice preserve contour breaks with MOVE. They express CLOSE as a
line, losing the closed-topology flag. Slices preserve the degree of retained
curves; their endpoints inherit the distance-query approximation.

## Distance-query accuracy

The subdivision criteria control chord deviation and control-polygon excess,
not the linearity of distance with curve parameter. LUT interpolation therefore
has **no error bound derived from tolerance**. Even an exact total length does
not imply an accurate point at a requested distance.

Counterexample, covered by `nonuniform_parameter_regression`:

    M(0,0) Q(0,0) (100,0)
    total length = 100
    query at distance 50 -> current result (25,0)
    true half-length point = (50,0)

The straight curve produces only endpoint samples and interpolates t=0.5, while
B(t)=100*t*t. Lowering flatness tolerance does not fix this. The regression
records existing behavior so a future correction is deliberate; it is **not an
accuracy acceptance test**. Resolve distance inversion before promising a stable
point-at-distance contract upstream.

## Numerical range and limits

Single-precision float is the only scalar model. Low-level Bezier primitives have
no error result: finite coordinates and representable intermediate differences,
products, and sums are preconditions. They clamp t to [0,1], with NaN treated as
zero. Two non-NULL split outputs must not alias each other.

High-level operations reject detected non-finite geometry, cumulative length
overflow, or a positive leaf increment that cannot advance the float distance.
Finite input alone is not a universal numerical-range guarantee. Rescale extreme
coordinate ranges; do not compile with fast-math assumptions that suppress
finite/NaN checks.

Positive tolerances below `PG_MIN_TOLERANCE` (1e-4) are clamped. Chords <=
`PG_EPSILON` (1e-6) are skipped. At `PG_MAX_RECURSION` (default 12), a leaf is
accepted even if flatness criteria remain unmet. No status currently distinguishes
that limit from convergence. Tolerance is local, not a global length/deviation
bound. These absolute thresholds make unit selection important.

## Compatibility and versioning policy

This review keeps public symbols, enum values, parameter lists, struct layouts,
and version macros at the existing 0.2.0 baseline; it does not declare a release.
Bug fixes refine previously invalid numerical behavior. Existing `pg_*`,
`path2d/`, and CMake aliases remain for source compatibility. The prefix means
path geometry and contains no application concept. Do not reserve an LVGL
`lv_*` namespace before maintainer agreement.

The 0.x API has no frozen cross-release binary ABI. Consumers should rebuild the
library and callers together. Before 1.0: agree naming, ownership, struct
visibility, error/precision semantics, and compatibility policy; document any
breaking change and provide a migration path. No struct or command buffer is a
portable serialization format.
