# lv-path

A renderer-independent **C99 path geometry library** with caller-owned storage.
No LVGL headers, widgets, operating system, allocator hooks, or runtime heap
allocation are required by the core.

**Status:** useful standalone geometry, ready for an exploratory design discussion,
but not ready for an upstream merge or a stable 1.0 contract. In particular,
distance-to-parameter interpolation has no accuracy bound; see the explicit
counterexample in [API contracts](docs/api.md#distance-query-accuracy).

## Scope

- Immutable, borrowed M/L/Q/C/Close command sequences and validation.
- Quadratic/cubic Bezier evaluation, derivatives, and De Casteljau splitting.
- Adaptive flattening and approximate arc-length lookup tables.
- Approximate position/unit tangent queries by absolute or normalized distance;
  normal rotation from a tangent.
- Curve-preserving slices and fixed-capacity command writers.

No widget, rendering engine, SVG parser, GPU interface, tessellator, or
application-specific state is included. Bounding boxes are intentionally deferred
pending agreement about overlap with LVGL's existing path facilities.

## Build and use

CMake 3.16+ and a C99 compiler are sufficient. The build downloads nothing.

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build
    ctest --test-dir build --output-on-failure

Link the CMake target `lv_path::lv_path` after `add_subdirectory(path/to/lv-path)`.
Without CMake, compile the seven `src/pg_*.c` files, add `include/` to the include
path, and link the platform math library when needed. The implementation requires
C99 math functions including `hypotf` and `fmaxf`; it is not a math-library-free
freestanding implementation.

`LV_PATH_BUILD_TESTS` and `LV_PATH_BUILD_EXAMPLES` default on for a standalone
build and off as a subproject. `LV_PATH_STRICT_WARNINGS` enables private strict
warnings; `LV_PATH_SANITIZE` enables host ASan/UBSan. Do not use fast-math options
that discard NaN/Inf semantics. C++ consumers can include the public headers,
but the implementation is C99.

Start with the complete, compiled [API example](examples/path_queries.c). It
checks every result and demonstrates measurement, position/tangent/normal,
slicing, and flattening using fixed arrays. Run `build/path_queries` (append
`.exe` on Windows); success returns zero without console output.

## Contracts and design

Paths borrow command arrays, which may be read-only. Measures additionally borrow
the path descriptor and sample workspace. Keep all of them alive and unchanged
until the last query. All output storage belongs to the caller. A failed measure
initialization clears the object; workspace contents are unspecified. A failed
writer operation may leave partial output, which must be discarded.

Geometry uses single-precision float and caller-defined coordinates. Recursion is
bounded by `PG_MAX_RECURSION` (default 12); this bounds depth, not a proven target
stack size. Tolerance is a per-leaf subdivision criterion, not a global geometric
or distance-query error guarantee. No mandatory heap is used, but target stack,
libm cost, and worst-case execution time still require measurement.

- [Architecture and dependencies](docs/architecture.md)
- [API and memory contracts](docs/api.md)
- [Design rationale and LVGL vector overlap](docs/design-rationale.md)
- [Upstream readiness and next milestone](docs/upstream-readiness.md)
- [Tests and host benchmark](docs/benchmarks.md)
- [Compatibility and migration notes](docs/migration.md)

Public names remain `pg_*` / `PG_*` (path geometry), with headers under `path2d/`.
The compatibility target `path2d::path2d` and `PATH2D_VERSION_*` macros remain.
This review preserves signatures and layouts; upstream naming and a stable binary
ABI have not been agreed. See the versioning policy in the API document.

## License

MIT licensed. Original copyright notices are preserved; see [LICENSE](LICENSE).
