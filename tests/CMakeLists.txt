file(GLOB_RECURSE ST_TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

add_executable(st_tests ${ST_TEST_SOURCES})
target_link_libraries(st_tests PRIVATE st::core doctest::doctest)
target_include_directories(st_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(st_tests PRIVATE ST_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")
st_set_warnings(st_tests)

include("${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake")
doctest_discover_tests(st_tests)
