cmake_minimum_required(VERSION 3.20)

project(fast_viewer
    VERSION 0.1.0
    DESCRIPTION "Fast Viewer - tiny fast native Windows image viewer"
    LANGUAGES CXX)

# MSVC-only target: Windows 10 x64.
# NOTE: this toolset (MSVC 19.44) has no literal /std:c++23 switch, so C++23
# is requested through /std:c++latest (see ARCHITECTURE.md).
add_executable(fast_viewer WIN32
    src/main.cpp
    src/app.cpp
    src/app.h
    src/window.cpp
    src/window.h
    src/renderer.cpp
    src/renderer.h
    src/image_loader.cpp
    src/image_loader.h
    src/navigation.cpp
    src/navigation.h
    src/input.cpp
    src/input.h
)

target_compile_options(fast_viewer PRIVATE
    /std:c++latest
    /W4
    /permissive-
    /utf-8
    /EHsc
    /Zc:__cplusplus
)

target_compile_definitions(fast_viewer PRIVATE
    UNICODE
    _UNICODE
    _WIN32_WINNT=0x0A00
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

target_link_libraries(fast_viewer PRIVATE
    d2d1
    dwrite
    windowscodecs
    ole32
    uuid
    shlwapi
)
