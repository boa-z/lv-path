# Design rationale: complementary to LVGL vector rendering

## Purpose

A geometry client may need path length, locations and directions along a curve,
or a curve-preserving subrange without constructing a widget or draw task. Such
operations also make sense without a display. This library keeps them in a
small C99 core over immutable caller data and explicitly bounded workspace.
Drawing remains the responsibility of a consumer.

## Existing LVGL overlap

Source comparison on 2026-09-29 used LVGL commit
`e70816243b4cfc59254628664d2544a6f8bb06ad`, rather than assuming an old release API:

- [Public vector interface](https://github.com/lvgl/lvgl/blob/e70816243b4cfc59254628664d2544a6f8bb06ad/include/lvgl/draw/lv_draw_vector.h)
- [Vector implementation](https://github.com/lvgl/lvgl/blob/e70816243b4cfc59254628664d2544a6f8bb06ad/src/draw/lv_draw_vector.c)

That interface already offers MOVE/LINE/QUAD/CUBIC/CLOSE, arc and shape helpers,
transforms, bounding queries, and vector draw submission. The implementation
creates mutable paths using `lv_malloc` and dynamic `lv_array` storage. Those are
valid rendering-oriented choices, and this project should not introduce a
competing LVGL path/rendering subsystem. The reviewed public header does not
expose an arc-distance measurement/query API. This is a scoped source comparison,
not a claim that every LVGL backend or private utility lacks related algorithms.

| Responsibility | Proposed geometry scope | LVGL integration responsibility |
| --- | --- | --- |
| Commands | Borrowed immutable geometry | Translate or adapt an agreed existing representation |
| Measurement | Approximate length and distance lookup | Consume results in application/drawing logic |
| Memory | Caller-provided command/sample storage | Choose ownership suitable for LVGL objects |
| Drawing | None | Fill, stroke, styles, clipping, transforms, draw tasks/backends |
| Additional parsing/shapes | None | Existing facilities or separate modules |

A possible integration would reuse LVGL types/iteration or provide a small
adapter. Its direction, ownership and close semantics require maintainer agreement.
No adapter is added here. In particular, a slice emitted through this library's
writer is not a lossless replacement for an LVGL closed path: closed joins versus
open caps must be handled explicitly.

## Choices and tradeoffs

- Borrowed arrays support constant data and predictable storage. They require
  explicit lifetime discipline and currently limit counts to uint16_t.
- Float keeps the data model small and uses portable C99 math. It does not
  establish performance or adequate range for every embedded target.
- Shared bounded subdivision avoids divergent flatten/measure behavior. Recursion
  still requires target stack analysis; the depth limit currently has no separate
  accuracy status.
- Generic writers make flattening/slicing independent of a graphics toolkit. They
  expose partial-output behavior and intentionally have no rendering policy.
- Existing names and data layouts remain compatible during the audit. A broad
  rename would create churn without answering upstream ownership questions.

## Deliberately deferred

No bounding-box API is added: LVGL already has path bounding, and precise versus
control-hull bounds require a use case and agreed semantics. No SVG parser,
renderer, GPU/tessellation support, allocator framework, widget, or product state
is in scope. A measurement accuracy correction is the next core issue, not an
excuse to grow the rendering surface.

## Discussion material

The [upstream proposal](upstream-proposal.md) compares integration options and
lists questions for maintainers. Its current-behavior references are the
[geometry contract](geometry-contract.md) and [embedded review](embedded-engineering.md).
It proposes no public rename, rendering backend or new library operation.
