# ServiceTracker GUI (cpp_app: raylib + Dear ImGui docking).
file(GLOB_RECURSE ST_APP_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
file(GLOB_RECURSE ST_APP_HEADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h")

if(ST_BUILD_APP)
    add_executable(ServiceTracker ${ST_APP_SOURCES} ${ST_APP_HEADERS})
    target_link_libraries(ServiceTracker PRIVATE st::core APP)
    if(WIN32)
        # Desktop notifications: tray icon (shell32), taskbar overlay (ole32),
        # toasts via C++/WinRT (runtimeobject).
        # Do not add user32/gdi32 here: they are linked by default, listing them before
        # raylib makes the linker pick user32's CloseWindow over raylib's (LNK2005).
        target_link_libraries(ServiceTracker PRIVATE shell32 ole32 runtimeobject)
    endif()
    target_include_directories(ServiceTracker PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_definitions(ServiceTracker PRIVATE APP_VERSION="${PROJECT_VERSION}")
    st_set_warnings(ServiceTracker)
    app_attach_updater(ServiceTracker)
    source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src" FILES ${ST_APP_SOURCES} ${ST_APP_HEADERS})

    if(MSVC)
        set_property(TARGET ServiceTracker PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:ServiceTracker>")
        # Release builds are windowed applications (no console)
        set_target_properties(ServiceTracker PROPERTIES LINK_FLAGS_RELEASE "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
        set_property(DIRECTORY "${PROJECT_SOURCE_DIR}" PROPERTY VS_STARTUP_PROJECT ServiceTracker)
    endif()

    # Resources (font, icons, manual) and default layout next to the executable
    add_custom_command(TARGET ServiceTracker POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/res" "$<TARGET_FILE_DIR:ServiceTracker>"
        COMMENT "Copying resources..."
    )
else()
    # ST_APP_COMPILE_CHECK: compile every app source against the real headers
    # (imgui, cpp_app) without linking. Catches API errors on Linux CI.
    add_library(st_app_compile_check OBJECT ${ST_APP_SOURCES})
    target_link_libraries(st_app_compile_check PRIVATE st::core)
    target_include_directories(st_app_compile_check PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_include_directories(st_app_compile_check SYSTEM PRIVATE
        "${app_SOURCE_DIR}/include"
        "${imgui_SOURCE_DIR}"
        "${imgui_SOURCE_DIR}/backends"
    )
    target_compile_definitions(st_app_compile_check PRIVATE APP_VERSION="${PROJECT_VERSION}" ST_APP_COMPILE_CHECK=1)
    st_set_warnings(st_app_compile_check)

    # Headless UI smoke test: renders every window with Dear ImGui (no GPU) while
    # walking through the main workflows. Windows-only parts of cpp_app are stubbed.
    set(_st_smoke_sources ${ST_APP_SOURCES})
    list(FILTER _st_smoke_sources EXCLUDE REGEX "/src/main\\.cpp$")
    file(GLOB _st_cppapp_ui "${app_SOURCE_DIR}/src/imgui/*.cpp" "${app_SOURCE_DIR}/src/imgui/modern/*.cpp")
    set(_st_imgui
        "${imgui_SOURCE_DIR}/imgui.cpp"
        "${imgui_SOURCE_DIR}/imgui_draw.cpp"
        "${imgui_SOURCE_DIR}/imgui_tables.cpp"
        "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
        "${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp")
    set_source_files_properties(${_st_cppapp_ui} ${_st_imgui} PROPERTIES COMPILE_OPTIONS "-w")
    add_executable(st_app_smoke ${_st_smoke_sources} ${_st_cppapp_ui} ${_st_imgui}
        "${CMAKE_CURRENT_SOURCE_DIR}/smoke/smoke_main.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/smoke/cpp_app_stubs.cpp")
    target_link_libraries(st_app_smoke PRIVATE st::core)
    target_include_directories(st_app_smoke PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_include_directories(st_app_smoke SYSTEM PRIVATE "${app_SOURCE_DIR}/include" "${imgui_SOURCE_DIR}" "${imgui_SOURCE_DIR}/backends")
    target_compile_definitions(st_app_smoke PRIVATE APP_VERSION="${PROJECT_VERSION}" ST_APP_COMPILE_CHECK=1)
    if(ST_BUILD_TESTS)
        add_test(NAME app_smoke COMMAND st_app_smoke)
    endif()
endif()
