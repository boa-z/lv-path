# Generic API examples

Both examples build with LV_PATH_BUILD_EXAMPLES=ON, the standalone CMake default
(off by default as a subproject). Neither requires LVGL, a display, an OS timer,
a parser or a rendering backend.

## Checked API operations

[path_queries.c](path_queries.c) is the minimal no-stdio example: measurement,
position/tangent/normal, slicing and flattening with fixed arrays. It checks
every result before using its outputs.

## Point moving along a path

[path_motion.c](path_motion.c) simulates a point on one cubic contour for two
seconds at 60 Hz, including both endpoints (121 data rows). It emits CSV with
time, requested distance, position, tangent and normal. The curve travels from
(0,0) through controls (0,80), (120,80) to (120,0), in caller-defined units.

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --target path_motion
    ./build/path_motion > motion.csv

On Windows use build/path_motion.exe; multi-configuration generators may place
it under build/Release. CSV goes to stdout, errors to stderr. Exit zero means
the complete stream was written. Discard output on failure: it may contain a
header or prefix of rows.

The client measures once, keeps a 256-entry static sample array, and computes
requested_distance = measured_length * elapsed_time / duration. It retains no
frame history and uses no sleep or animation scheduler. A real client supplies
elapsed time and uses the queried position as object state. Tangent supplies a
forward direction; normal can orient a perpendicular offset. CSV can be inspected
numerically or plotted in an existing tool; no renderer is added here.

**This is approximate distance-driven motion, not a constant-speed guarantee.**
Flatness controls subdivision, not arc-distance inversion. Equal requested
distance increments can produce unequal traveled distances. More frames do not
fix that. The single-contour example avoids MOVE jumps and does not change the
known nonuniform-parameter counterexample. Read the [geometry contract](../docs/geometry-contract.md#point-at-distance-and-motion)
before choosing motion accuracy requirements.

The geometry retains only two command records and the caller's sample array.
Host stdio may allocate internally and is outside the core's no-mandatory-heap
contract. See [embedded engineering](../docs/embedded-engineering.md).

Example output checkpoints (GCC host run; final digits may vary):

| Simulated time (s) | Requested distance | Position (x, y) |
| ---: | ---: | --- |
| 0.000000 | 0.000000 | (0.000000, 0.000000) |
| 1.000000 | 95.147041 | (60.000000, 60.000000) |
| 2.000000 | 190.294083 | (120.000000, 0.000000) |

These sample positions illustrate the traversal, not an arc-distance error bound.

## GIF visualization

`docs/media/path-motion.gif` is generated from the compiled `path_motion` CSV by the optional host-only `tools/export_path_motion.py` script. It contains 61 frames over a 3-second looping presentation, with two seconds of simulated motion and endpoint pauses. Pillow is not a library or target dependency.
