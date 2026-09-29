# Compatibility and migration notes

lv-path is a standalone C99 geometry library. Headers live in include/path2d,
and implementations live in src. No LVGL headers, widget library, display
configuration, or repository submodule is required to build the core.

The pg_* API, PG_* constants and PATH2D_VERSION_* macros remain compatible.
Use lv_path::lv_path with CMake; path2d::path2d remains a compatibility alias.

| Previous build option | Current build option |
| --- | --- |
| PATH2D_BUILD_TESTS | LV_PATH_BUILD_TESTS |
| PATH2D_STRICT_WARNINGS | LV_PATH_STRICT_WARNINGS |
| PATH2D_SANITIZE | LV_PATH_SANITIZE |

The project requires C99 rather than a compiler's default dialect. Keep headers
and library objects at the same reviewed version. No binary ABI freeze or
upstream acceptance is implied by the current pre-1.0 version.

## Upstream-readiness review (unreleased)

Public declarations, struct layouts, CMake aliases and 0.2.0 version macros remain
unchanged. Existing callers must continue to check all result codes. Detected
length overflow or float-distance stagnation now returns PG_ERR_INVALID_PATH
instead of exposing an unusable successful measure. Large-vector normalization,
contour-local fallback direction, tiny-parameter slice controls and uninitialized
normalized-slice error behavior are corrected. Numerical output may differ at
floating-point roundoff level from earlier geometry revisions.

Measure descriptor and command/workspace lifetimes are now explicit. Failed
producers require discarding sink output even if the sink itself is not poisoned.
See [API contracts](api.md) for the known distance-inversion limitation and CLOSE
metadata loss. LV_PATH_BUILD_EXAMPLES adds only a standalone usage program;
like tests, it defaults off when included by another project.

Consumers should pin a reviewed library version and rebuild library and headers
together when adopting these corrections.
