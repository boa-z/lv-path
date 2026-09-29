# lv-path

Renderer-independent **C99** path geometry, without a widget, operating-system,
rendering or heap dependency.

## Scope

- M/L/Q/C/Close validation and borrowed immutable paths.
- Quadratic/cubic Bezier evaluation, derivatives and subdivision.
- Adaptive flattening and arc-length lookup tables.
- Position/tangent/normal queries by distance or normalized distance.
- Curve-preserving slices and fixed-capacity command writers.

Coordinates and units belong to the caller. Rendering lives in clients.

## Build and test

CMake 3.16+ and a C99 compiler are required. No downloads are performed.

    cmake -S . -B build
    cmake --build build
    ctest --test-dir build --output-on-failure

Use add_subdirectory(path/to/lv-path) and link lv_path::lv_path.
Non-CMake users compile the seven src/pg_*.c files, add include/ to the
header path and link the platform math library when required.
LV_PATH_BUILD_TESTS defaults off in subprojects. Development flags are private:
LV_PATH_STRICT_WARNINGS and LV_PATH_SANITIZE (host ASan/UBSan).

Public headers remain path2d/pg_*.h, with unchanged pg_* types/functions
and PG_* macros. path2d::path2d is a compatibility CMake alias.
PATH2D_VERSION_* macros retain the geometry API version.

## Memory and numerical contracts

Paths borrow caller commands, which may reside in Flash. Measurement and
writers use caller-provided storage; exhaustion returns an error instead of
truncating geometry. No malloc/free hooks or allocator are required.
Subdivision uses bounded recursion (PG_MAX_RECURSION, default 12); target
stack usage still needs measurement. Queries allocate nothing.

Single-precision float is used throughout. Arc length and distance-to-parameter
mapping are approximate; positions are evaluated on the original curve.
Tolerance is clamped by PG_MIN_TOLERANCE and work bounded by recursion depth.
This is not an exact arc-length solver. Tangents retain the existing
analytic/local-span/chord/(1,0) fallback. The positive normal is (-t.y,t.x).

Multiple contours contribute to one distance domain with zero-length jumps.
Exact contour joints resolve to the later command at t=0. Writers and
flatteners emit MOVE at contour boundaries. Read-only queries may share an
immutable prepared measure; mutation and storage lifetimes are caller-managed.

## Quick start


```c
#include "path2d/pg_path.h"
#include "path2d/pg_measure.h"

static const pg_cmd_t path_cmds[] = {
    PG_MOVE_TO(0.0f, 0.0f),
    PG_CUBIC_TO(0.0f, 100.0f, 100.0f, 100.0f, 100.0f, 0.0f),
};
static const pg_path_t path = { path_cmds, PG_ARRAY_SIZE(path_cmds) };

static pg_measure_sample_t workspace[128];
pg_measure_t m;

if (pg_measure_init(&m, &path, workspace, 128, 0.5f) != PG_OK) {
    /* PG_ERR_WORKSPACE_TOO_SMALL / PG_ERR_DEGENERATE / ... */
}

pg_point_t pos, tan;
pg_measure_get_pos_tan_normalized(&m, 0.53f, &pos, &tan);
/* tan may be NULL when only the position is needed. */
```

Extracting a sub-range keeps the original curve degree:

```c
#include "path2d/pg_writer.h"

static pg_cmd_t slice_cmds[16];
pg_path_buffer_t buffer;
pg_path_t slice;

pg_path_buffer_init(&buffer, slice_cmds, PG_ARRAY_SIZE(slice_cmds));
pg_path_writer_t writer = pg_path_buffer_writer(&buffer);

if (pg_measure_slice_normalized(&m, 0.2f, 0.8f, &writer) != PG_OK) {
    /* writer overflow: the buffer is poisoned, buffer.failure holds the code */
}
pg_path_buffer_to_path(&buffer, &slice);
```

## License

MIT licensed. Original copyright notices are preserved; see [LICENSE](LICENSE).
