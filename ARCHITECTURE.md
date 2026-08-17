# Fast Viewer — Architecture Specification

## Windows 10-first policy

- Primary target: Windows 10 x64 (22H2 / build 19045 development environment).
- Windows 11 compatibility is welcome but never required.
- No Win11-only dependencies without explicit approval.
- Fast Viewer must not require Windows 11 for DPI or display support.

## Technical direction (preferred V1 stack)

- C++ using current MSVC; use `/std:c++latest` on the installed toolchain where C++23 features are needed
- Win32
- Direct2D
- Windows Imaging Component (WIC)
- CMake
- MSVC
- Windows 10 SDK

Do not require: WinUI, Qt, Electron, Tauri, WebView, .NET UI frameworks, Python runtime, Java runtime.

DirectComposition is not part of the default plan. Only introduce it later if a measurable product requirement proves it necessary.

## Dependency constraints

- No heavy frameworks.
- No third-party image decoders in the default plan (use WIC).
- No dependency is added automatically; any dependency addition requires human approval.
- No plugin system, no extension marketplace, no theme engine.

## Simplicity rules

- The entire viewer architecture must remain understandable by one experienced developer in roughly one day.
- Avoid unnecessary patterns:
  - dependency injection container
  - plugin host
  - event bus / command bus
  - repository abstraction layers
  - provider/factory forests
  - service locator
  - elaborate MVC/MVVM architecture
  - custom widget toolkit
- No speculative "future-proof" architecture.

A possible source organization later might resemble:

```text
src/
  main.cpp
  window.cpp
  renderer.cpp
  image_loader.cpp
  directory.cpp
  navigation.cpp
  cache.cpp
  filmstrip.cpp
  input.cpp
  config.cpp
```

This list is not mandatory. The simplicity level is mandatory.

## Do not reinvent platform functionality

Use mature Windows/platform functionality where appropriate. Do not write your own:

- JPEG decoder
- PNG decoder
- WebP decoder
- filesystem watcher
- GPU renderer
- image resampling engine
- UI toolkit
- color-management engine

Prefer:

- WIC for image decoding where appropriate
- Direct2D for image rendering/transforms
- Win32 for native window/input behavior
- Windows directory notification facilities for eventual folder-change monitoring

The project's custom code should focus on:

- viewer interaction
- navigation
- scheduling
- cache policy
- cancellation
- filmstrip behavior
- performance control
- window lifecycle
- configuration mapping

## Configuration philosophy

- V1 has no settings GUI.
- Use a single small text configuration file, conceptually similar to mpv (future file name: `viewer.conf`).
- The application must have built-in defaults and must work if this file does not exist.
- Invalid values must fall back to safe defaults.
- Unknown keys must not prevent startup.
- Do not implement a large configuration framework.
- The input architecture should allow future mouse mappings without rewriting core viewer logic.

> UI remains minimal, but input actions are internally mappable.

## Rough milestone architecture

### M0 — Core Viewer (future scope; not implemented yet)

- open a supplied image path
- Win32 window
- Direct2D rendering
- WIC decoding
- fit behavior
- mouse-wheel zoom
- pan
- Left/Right navigation
- Esc close
- F11 toggle viewing mode
- double-click viewing mode toggle
- right-click close

No filmstrip in M0. M0 must be benchmarked before M1 begins.

### M1 — Performance (future scope)

- asynchronous directory discovery
- natural sorting
- bounded preload
- memory-budget cache
- cancellation
- direction-aware preload
- directory change awareness

### M2 — Picasa-style UX (future scope)

- filmstrip
- bottom activation zone
- auto-hide
- nearby thumbnail loading
- lightweight filename/dimension/index display
- polish

After M2, the project may be considered a valid 1.0 candidate.

Do not invent M3 feature expansion without human approval.

## External reference projects

These may be studied for ideas. Do not blindly copy their architecture. Do not add their dependencies automatically.

Useful reference categories:

- open-source Picasa Photo Viewer clones
- FlyPhotos
- ImageGlass Picasa-related issues
- minimal/high-performance Windows image viewers
- Microsoft WIC + Direct2D viewer examples

Reference projects are evidence and inspiration, not requirements. If studying external source code later: respect licenses, do not copy incompatible code, prefer learning behavior and architecture patterns.
