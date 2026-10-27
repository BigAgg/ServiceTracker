cmake_minimum_required(VERSION 3.25)

project(ServiceTracker
    VERSION 2.0.3
    DESCRIPTION "Service- und Bestellverwaltung fuer Geraete"
    LANGUAGES C CXX
)

# --------------------------------------------------------------------------------------
# Options
# --------------------------------------------------------------------------------------
if(WIN32)
    set(_st_app_default ON)
else()
    set(_st_app_default OFF)
endif()

option(ST_BUILD_TESTS        "Build the unit tests"                                   ON)
option(ST_BUILD_APP          "Build the GUI application (requires cpp_app, Windows)"  ${_st_app_default})
option(ST_APP_COMPILE_CHECK  "Compile the app sources without linking (non-Windows)"   OFF)
option(ST_WITH_XLSX          "Enable Excel export via libxlsxwriter"                   ON)
option(ST_WARNINGS_AS_ERRORS "Treat compiler warnings as errors"                       OFF)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(BUILD_SHARED_LIBS OFF)

if(MSVC)
    # cpp_app and its dependencies are built against the static release runtime
    set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreaded)
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerWarnings)
include(Dependencies)

# --------------------------------------------------------------------------------------
# Targets
# --------------------------------------------------------------------------------------
if(ST_BUILD_TESTS)
    enable_testing()
endif()

add_subdirectory(core)

if(ST_BUILD_APP OR ST_APP_COMPILE_CHECK)
    add_subdirectory(app)
endif()

if(ST_BUILD_TESTS)
    add_subdirectory(tests)
endif()

# --------------------------------------------------------------------------------------
# Installer (Inno Setup)
# --------------------------------------------------------------------------------------
if(WIN32 AND ST_BUILD_APP)
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/installer/setup_template.iss"
        "${CMAKE_CURRENT_SOURCE_DIR}/installer/setup.iss"
        @ONLY
    )
endif()
