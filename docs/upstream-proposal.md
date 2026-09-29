# Discussion draft: reusable path measurement for LVGL

Status: local exploratory draft, not an upstream PR or accepted LVGL design.
Prepared 2026-09-29 for the standalone lv-path geometry library.
The documentation/example follow-up preserves core algorithms and public
declarations. No message has been posted to the community.

## Problem statement

A client may need a position and orientation along a curve without drawing it:
a point following a trajectory, a label aligned with a path, or an offline
geometry check. Filling or stroking a path does not itself define length,
distance-indexed point/tangent queries, or curve-preserving subranges. These
calculations also make sense without a widget or display.

Would a small reusable geometry/measurement facility be useful in LVGL, and how
should it reuse existing path data and math? This draft presents a standalone
prototype with explicit limitations, not a finished bounded-accuracy distance API.

## Existing scope and example

lv-path contains borrowed M/L/Q/C/Close commands, Bezier evaluation/splitting,
adaptive flattening, approximate length/LUT queries, tangent/normal helpers and
curve-preserving slices. The C99 core uses caller-owned buffers, bounded recursive
subdivision and C99 math. It has no direct allocator or LVGL calls and no mandatory
heap. Generic pg_* names and path2d headers remain for compatibility; they are
not a proposed LVGL namespace.

The [point-motion example](../examples/README.md#point-moving-along-a-path) measures
one curve once and emits 121 time/position/direction rows for a two-second
simulated trajectory. Timing and output are client decisions. Equal requested
distance is not guaranteed constant physical speed. No drawing backend is used.

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build
    ctest --test-dir build --output-on-failure
    ./build/path_motion > motion.csv

Use the .exe suffix on Windows. See [readiness](upstream-readiness.md),
[geometry guarantees](geometry-contract.md), and [embedded requirements](embedded-engineering.md).
Host evidence is distinct from board acceptance or a new remote CI result.

## Relationship to LVGL vector rendering

The comparison is pinned to LVGL commit
e70816243b4cfc59254628664d2544a6f8bb06ad; these files were rechecked on 2026-09-29:

- [Public vector API](https://github.com/lvgl/lvgl/blob/e70816243b4cfc59254628664d2544a6f8bb06ad/include/lvgl/draw/lv_draw_vector.h)
- [Vector implementation](https://github.com/lvgl/lvgl/blob/e70816243b4cfc59254628664d2544a6f8bb06ad/src/draw/lv_draw_vector.c)

The API already offers commands, shape/arc helpers, transformations, bounding and
vector draw submission. Mutable paths use LVGL allocation/array facilities. The
reviewed public header does not expose arc-distance measurement queries. This
scoped comparison does not claim that every private helper, backend or later
revision lacks related algorithms. Recheck the selected LVGL base before a patch.

Geometry measurement would return results for callers to use, while LVGL retains
drawing, fills/strokes, clipping, styles, transforms and backend selection. The
prototype's separate command array is not a reason to add a second public LVGL
path representation. Existing bounds/shape facilities should not be duplicated.

## Possible integration approaches

These are alternatives for discussion; no adapter or new API is implemented here.

| Approach | Benefit | Decisions / costs |
| --- | --- | --- |
| Standalone core with optional external LVGL adapter | Independent testing and caller-owned measurement buffers | Bounded command translation storage; lifetime, mutation and CLOSE handling; avoid private array layout dependencies |
| Selected algorithms in an internal LVGL geometry module | Reuse LVGL types/traversal, minimize duplicate public formats | Module ownership, optional configuration, tests, math policy and scratch storage; audit overlap first |
| Measurement operations on the existing LVGL path interface | Familiar public entry point for LVGL clients | Accuracy, cache invalidation on mutation, ownership and memory cost; no-heap measurement does not mean LVGL path objects allocate nothing |

Agree representation and guarantees first. Do not rename the standalone API,
freeze an adapter, add a generalized iterator framework, or expose private
structures before that decision. A narrow algorithm contribution may be preferable
to importing the complete repository.

## Known limitations accompanying this draft

- **Distance inversion has no accuracy bound.** M(0,0) Q(0,0) (100,0) has length
  100, but distance 50 returns x=25 instead of x=50. Linear t interpolation causes
  this; lowering flatness tolerance does not fix it. Slicing inherits it. The
  regression records the defect, not acceptance of that accuracy.
- **Tolerance is local.** Chord deviation/control-polygon excess guide subdivision.
  At the depth cap, leaves can be accepted without meeting them. No cap-status or
  global geometric/length error guarantee exists.
- **Direction and closure need agreement.** Corners/cusps use documented fallback
  conventions; MOVE can cause position jumps. Writer output loses CLOSE metadata,
  which matters to a renderer's closed joins versus open caps.
- **Target evidence is incomplete.** Caller capacity and recursive depth are
  bounded, but MCU stack bytes, worst-case time, code size, float/libm cost and
  extreme-range behavior remain unvalidated.
- **API policy is pre-1.0.** Public structs, float, uint16_t counts, ownership,
  errors, namespace and ABI policy need review. MIT licensing and original copyright notices
  are retained; provenance still needs normal contribution review.

Passing host tests must not obscure these limits. They do not prevent asking
whether the direction is useful, but block presenting the current implementation
as adoption-ready or accurate to the flattening tolerance.

## Questions for maintainers

1. Is measurement useful as internal math, a vector-path extension, or an optional
   external component?
2. Which existing representation/traversal should be reused? Should measurement
   remain usable without a rendering backend?
3. What absolute/relative accuracy, resource bounds and failure behavior are needed
   when workspace or subdivision limits are reached?
4. Is caller-provided workspace compatible with LVGL conventions? How should
   mutation, invalidation and closed-contour semantics work?
5. What smallest testable algorithm set and target evidence would help a first
   contribution?

## Next milestone and non-goals

Agree a distance-inversion error criterion and bounded-resource contract; fix the
straight nonuniform-parameter counterexample; test independent references over
lines, curves, loops, cusps, contours and coordinate scales. Then gather identified
target stack/time/workspace evidence and prepare a minimal patch in the preferred
representation. This need not add a new public operation.

No SVG parser, renderer, widget, GPU/tessellator, shape collection, product state,
or public rename is proposed. This draft asks for architectural feedback; it does
not propose replacing LVGL vector rendering or promise upstream acceptance.
