cmake_minimum_required(VERSION 3.20)

# Fast Viewer — single source of truth for the release version.
# version.h is generated from this block so the executable's version metadata
# (via src/resources/app.rc) always matches the declared release version.
set(FAST_VIEWER_VERSION_MAJOR 1)
set(FAST_VIEWER_VERSION_MINOR 0)
set(FAST_VIEWER_VERSION_PATCH 0)
set(FAST_VIEWER_VERSION_SUFFIX "rc1")
set(FAST_VIEWER_VERSION_DISPLAY "1.0.0-rc1")

project(fast_viewer
    VERSION ${FAST_VIEWER_VERSION_MAJOR}.${FAST_VIEWER_VERSION_MINOR}.${FAST_VIEWER_VERSION_PATCH}
    DESCRIPTION "Fast Viewer - tiny fast native Windows image viewer"
    LANGUAGES CXX)

# Release strategy (measured, see PERFORMANCE.md / TASK.md): static MSVC
# runtime (/MT). The resulting executable imports only inbox Windows system
# DLLs (d2d1, DWrite, ole32, SHLWAPI, KERNEL32, USER32, SHELL32, ADVAPI32) —
# no MSVCP140/VCRUNTIME140, no UCRT DLLs, no VC redistributable required.
# Must be set before add_executable so the target picks it up.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

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
    src/cache.cpp
    src/cache.h
    src/decoder.cpp
    src/decoder.h
    src/filmstrip.cpp
    src/filmstrip.h
    src/thumbnail.cpp
    src/thumbnail.h
    src/resources/app.rc
)

target_compile_options(fast_viewer PRIVATE
    /std:c++latest
    /W4
    /permissive-
    /utf-8
    /EHsc
    /Zc:__cplusplus
)

# Reproducible release build: /Brepro makes the linker derive the PE header
# timestamp from inputs instead of the wall clock, so identical sources build
# to byte-identical executables (verifiable via SHA-256).
target_link_options(fast_viewer PRIVATE /Brepro)

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

# Generate version.h from the block above; app.rc consumes it.
configure_file(src/version.h.in ${CMAKE_CURRENT_BINARY_DIR}/generated/version.h @ONLY)
target_include_directories(fast_viewer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}/generated
)
