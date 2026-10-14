# st_core – all business logic without any UI dependency.
# Everything in here is unit tested (tests/) and builds on Windows and Linux.
file(GLOB_RECURSE ST_CORE_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
file(GLOB_RECURSE ST_CORE_HEADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h")

add_library(st_core STATIC ${ST_CORE_SOURCES} ${ST_CORE_HEADERS})
add_library(st::core ALIAS st_core)

target_include_directories(st_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(st_core PUBLIC nlohmann_json::nlohmann_json)
target_compile_definitions(st_core PUBLIC ST_VERSION="${PROJECT_VERSION}")

if(ST_WITH_XLSX)
    target_link_libraries(st_core PRIVATE st_xlsxwriter)
    target_compile_definitions(st_core PUBLIC ST_WITH_XLSX=1)
endif()

if(WIN32)
    target_compile_definitions(st_core PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

st_set_warnings(st_core)
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${ST_CORE_SOURCES} ${ST_CORE_HEADERS})
