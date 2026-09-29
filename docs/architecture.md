# Architecture

## Dependency direction

    Application or optional graphics adapter
        +--> LVGL: objects, styles, drawing, display backends
        +--> lv-path: generic geometry
                 +--> C99 headers and platform math library

There is no reverse dependency from lv-path to LVGL or its consumers. Consumers
link `lv_path::lv_path` and choose their own dependency packaging.
A consumer without LVGL can link exactly the same library. No renderer, display,
OS configuration, generated product header, or submodule is needed to build it.

## Modules

| Module | Responsibility |
| --- | --- |
| `pg_types.h` | Points, command records, borrowed paths, result codes, limits |
| `pg_path.c` | Structural validation and private command-span traversal |
| `pg_bezier.c` | Quadratic/cubic evaluation, derivatives, splitting |
| `pg_subdiv.c` | Shared bounded adaptive traversal, private geometry helpers |
| `pg_flatten.c` | MOVE/LINE output through caller callbacks |
| `pg_measure.c` | Caller-owned lookup table, length and position/direction queries |
| `pg_slice.c` | Curve-preserving subranges in the same approximate distance domain |
| `pg_writer.c` | Fixed-capacity sink, sticky callback errors, borrowed output view |

`src/pg_internal.h` is private. Consumers include only `include/path2d/pg_*.h`.
`examples/` demonstrates API use; `tests/` and the benchmark are host tools and
are not linked into the library.

## Data flow and costs

Validation walks the immutable commands. Flattening and measurement share the
subdivider, so they agree on accepted leaves. Each accepted measurable leaf adds
its chord length and endpoint parameter to the distance table. Queries binary
search this table and interpolate a command parameter, then evaluate the
original Bezier curve. Slices locate their endpoints similarly and use De
Casteljau restriction to preserve degree.

For C commands, N stored samples, and maximum recursion depth D:

- Validation is O(C). Traversal can visit O(C * 2^D) leaves in the worst case.
- Measurement stores O(N) caller-supplied samples; recursion takes O(D) frames.
- Queries have O(log N) lookup plus O(C) command-start recovery; they are not
  constant-time or purely logarithmic.
- Slicing may revisit command prefixes and can cost O(C^2) plus sink work.

No hidden cache, mutable global geometry state, or allocator is present. Immutable
prepared measures can be shared by readers. Callers synchronize initialization,
workspace reuse, and writer mutation. Callbacks run synchronously and must not
modify their operation's source storage.

## Intentional boundaries

Coordinates have no pixel, screen, hardware, or product meaning. No color, style,
label, percentage state, invalidation, or draw task lives in the core. An adapter
can consume geometry in another repository. CLOSE is represented in input, but
writer output uses explicit closing lines and does not retain closed-topology
metadata; this matters if a renderer distinguishes closed joins from open caps.
