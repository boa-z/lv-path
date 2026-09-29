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

