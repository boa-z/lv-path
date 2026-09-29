cmake_minimum_required(VERSION 3.16)
project(lv-path VERSION 0.2.0 LANGUAGES C)

# Compile as ISO C99, including when a parent uses a newer dialect.
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(LV_PATH_TOP_LEVEL ON)
else()
    set(LV_PATH_TOP_LEVEL OFF)
endif()
option(LV_PATH_BUILD_TESTS "Build geometry host tests" ${LV_PATH_TOP_LEVEL})
option(LV_PATH_STRICT_WARNINGS "Treat geometry warnings as errors" ${LV_PATH_TOP_LEVEL})
option(LV_PATH_SANITIZE "Enable host address and undefined behavior sanitizers" OFF)

add_library(lv_path_dev_flags INTERFACE)
if(LV_PATH_STRICT_WARNINGS)
    target_compile_options(lv_path_dev_flags INTERFACE
        $<$<COMPILE_LANG_AND_ID:C,GNU,Clang,AppleClang>:-Wall;-Wextra;-Wpedantic;-Werror>
        $<$<COMPILE_LANG_AND_ID:C,MSVC>:/W4;/WX>)
else()
    target_compile_options(lv_path_dev_flags INTERFACE
        $<$<COMPILE_LANG_AND_ID:C,GNU,Clang,AppleClang>:-Wall;-Wextra;-Wpedantic>
        $<$<COMPILE_LANG_AND_ID:C,MSVC>:/W4>)
endif()
if(LV_PATH_SANITIZE)
    if(MSVC)
        message(FATAL_ERROR "LV_PATH_SANITIZE is not supported with MSVC")
    endif()
    target_compile_options(lv_path_dev_flags INTERFACE
        -fsanitize=address,undefined -fno-omit-frame-pointer -g)
    target_link_options(lv_path_dev_flags INTERFACE -fsanitize=address,undefined)
endif()

add_library(lv_path STATIC
    src/pg_path.c
    src/pg_bezier.c
    src/pg_subdiv.c
    src/pg_writer.c
    src/pg_slice.c
    src/pg_flatten.c
    src/pg_measure.c
)
add_library(lv_path::lv_path ALIAS lv_path)
# Source compatibility; both names refer to the same engine.
add_library(path2d::path2d ALIAS lv_path)
target_compile_features(lv_path PUBLIC c_std_99)
target_include_directories(lv_path PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(lv_path PRIVATE lv_path_dev_flags)
if(NOT MSVC)
    target_link_libraries(lv_path PUBLIC m)
endif()
if(LV_PATH_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
