# Standalone geometry tests and benchmark.
foreach(t bezier path flatten measure writer slice)
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

