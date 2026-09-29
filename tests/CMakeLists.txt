# Standalone geometry tests and benchmark.
foreach(t bezier path flatten measure writer slice regressions reference accuracy)
    add_executable(test_${t} test_${t}.c)
    target_link_libraries(test_${t} PRIVATE lv_path::lv_path lv_path_dev_flags)
    target_include_directories(test_${t} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(NAME ${t} COMMAND test_${t})
endforeach()

add_executable(bench_measure bench_measure.c)
target_link_libraries(bench_measure PRIVATE lv_path::lv_path lv_path_dev_flags)
target_include_directories(bench_measure PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME bench COMMAND bench_measure)
set_tests_properties(bench PROPERTIES PASS_REGULAR_EXPRESSION "queries/sec")

# Every public header must compile independently as C99 (no include-order dependency).
foreach(header types path bezier flatten measure writer)
    set(source "${CMAKE_CURRENT_BINARY_DIR}/header_${header}.c")
    file(WRITE "${source}"
        "#include \"path2d/pg_${header}.h\"\nint header_${header}(void) { return PG_OK; }\n")
    add_library(header_${header} OBJECT "${source}")
    target_link_libraries(header_${header} PRIVATE lv_path::lv_path lv_path_dev_flags)
endforeach()
