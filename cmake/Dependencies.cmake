# Third party dependencies. All of them are fetched at configure time so a fresh
# checkout builds without any manual setup (see docs/build.md).
include(FetchContent)
set(FETCHCONTENT_QUIET ON)

# --------------------------------------------------------------------------------------
# nlohmann_json – declared with the same name and tag as in cpp_app so that both
# projects share one copy (the first declaration wins).
# --------------------------------------------------------------------------------------
set(JSON_BuildTests OFF CACHE INTERNAL "")
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.12.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(json)

# --------------------------------------------------------------------------------------
# doctest (unit tests)
# --------------------------------------------------------------------------------------
if(ST_BUILD_TESTS)
    FetchContent_Declare(
        doctest
        GIT_REPOSITORY https://github.com/doctest/doctest.git
        GIT_TAG v2.4.12
        GIT_SHALLOW TRUE
    )
    set(DOCTEST_NO_INSTALL ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(doctest)
endif()

# --------------------------------------------------------------------------------------
# zlib + libxlsxwriter (Excel export). Both are compiled from source into small static
# libraries so no system packages or vcpkg are required.
# --------------------------------------------------------------------------------------
if(ST_WITH_XLSX)
    FetchContent_Declare(
        st_zlib
        GIT_REPOSITORY https://github.com/madler/zlib.git
        GIT_TAG v1.3.1
        GIT_SHALLOW TRUE
        SOURCE_SUBDIR _not_used_
    )
    FetchContent_Declare(
        st_xlsxwriter
        GIT_REPOSITORY https://github.com/jmcnamara/libxlsxwriter.git
        GIT_TAG v1.2.3
        GIT_SHALLOW TRUE
        SOURCE_SUBDIR _not_used_
    )
    FetchContent_MakeAvailable(st_zlib st_xlsxwriter)

    set(_zlib_sources adler32 compress crc32 deflate gzclose gzlib gzread gzwrite
        infback inffast inflate inftrees trees uncompr zutil)
    list(TRANSFORM _zlib_sources PREPEND "${st_zlib_SOURCE_DIR}/")
    list(TRANSFORM _zlib_sources APPEND ".c")
    add_library(st_zlib STATIC ${_zlib_sources})
    target_include_directories(st_zlib PUBLIC "${st_zlib_SOURCE_DIR}")
    if(MSVC)
        target_compile_definitions(st_zlib PRIVATE _CRT_SECURE_NO_DEPRECATE _CRT_NONSTDC_NO_DEPRECATE)
        target_compile_options(st_zlib PRIVATE /W0)
    else()
        target_compile_definitions(st_zlib PRIVATE HAVE_UNISTD_H)
        target_compile_options(st_zlib PRIVATE -w)
    endif()

    set(_lxw_root "${st_xlsxwriter_SOURCE_DIR}")
    file(GLOB _lxw_sources
        "${_lxw_root}/src/*.c"
        "${_lxw_root}/third_party/minizip/ioapi.c"
        "${_lxw_root}/third_party/minizip/zip.c"
        "${_lxw_root}/third_party/tmpfileplus/tmpfileplus.c"
        "${_lxw_root}/third_party/md5/md5.c"
    )
    if(WIN32)
        list(APPEND _lxw_sources "${_lxw_root}/third_party/minizip/iowin32.c")
    endif()
    add_library(st_xlsxwriter STATIC ${_lxw_sources})
    target_include_directories(st_xlsxwriter PUBLIC "${_lxw_root}/include")
    target_include_directories(st_xlsxwriter PRIVATE "${_lxw_root}/third_party")
    target_link_libraries(st_xlsxwriter PUBLIC st_zlib)
    set_target_properties(st_xlsxwriter st_zlib PROPERTIES POSITION_INDEPENDENT_CODE ON)
    if(MSVC)
        target_compile_definitions(st_xlsxwriter PRIVATE _CRT_SECURE_NO_WARNINGS)
        target_compile_options(st_xlsxwriter PRIVATE /W0)
    else()
        target_compile_options(st_xlsxwriter PRIVATE -w)
    endif()
endif()

# --------------------------------------------------------------------------------------
# cpp_app – GUI framework (raylib + Dear ImGui docking + NFD + updater)
# --------------------------------------------------------------------------------------
set(ST_CPP_APP_TAG "main" CACHE STRING "Git tag/branch of cpp_app to build against")

if(ST_BUILD_APP)
    set(APP_BUILD_SHARED OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        App
        GIT_REPOSITORY https://github.com/BigAgg/cpp_app.git
        GIT_TAG ${ST_CPP_APP_TAG}
    )
    FetchContent_MakeAvailable(App)
elseif(ST_APP_COMPILE_CHECK)
    # Only the headers are needed to compile-check the app sources on Linux.
    FetchContent_Declare(
        App
        GIT_REPOSITORY https://github.com/BigAgg/cpp_app.git
        GIT_TAG ${ST_CPP_APP_TAG}
        SOURCE_SUBDIR _not_used_
    )
    FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG v1.92.1-docking
        GIT_SHALLOW TRUE
        SOURCE_SUBDIR _not_used_
    )
    FetchContent_MakeAvailable(App imgui)
endif()
