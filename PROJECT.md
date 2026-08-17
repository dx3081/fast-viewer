# Fast Viewer — Project Specification

## Project identity

- **Project name:** Fast Viewer
- **Core definition:** Fast Viewer is a tiny, extremely fast, native Windows image viewer inspired specifically by the interaction model and simplicity of Google Picasa Photo Viewer.
  - It is NOT a Picasa library clone.
  - Windows Explorer manages files.
  - Fast Viewer only displays images.

## Primary target platform

- Windows 10 x64
- Windows 10 22H2 / build 19045 is the primary development environment
- Windows 11 compatibility is welcome, but Windows 11 must never be required
- Do not introduce Win11-only dependencies without explicit approval

## Primary priorities (in order)

1. Responsiveness
2. Low CPU / memory / disk usage
3. Small executable and simple architecture
4. Picasa-like image-viewing interaction
5. Visual polish
6. Everything else

If visual polish or optional functionality conflicts with speed, resource use, or architectural simplicity, remove the visual polish or optional functionality.

## Product scope

Fast Viewer is a dedicated viewing layer for Windows Explorer.

Expected user flow:

```
Explorer -> double-click image -> Fast Viewer -> view/navigate -> close -> back to Explorer
```

- There is no home screen.
- There is no internal file browser.
- There is no photo library.
- There is no import workflow.
- There is no persistent media database.

Philosophy: closer to `mpv` than to a modern photo-management suite:

- narrow purpose
- strong defaults
- minimal UI
- text configuration
- low overhead
- no unnecessary abstraction

## Explicit non-goals

The following features are OUT OF SCOPE unless the human owner explicitly changes the specification:

- photo library
- database
- persistent photo indexing
- folder browser
- recursive folder browsing
- AI
- machine learning
- face recognition
- semantic search
- cloud features
- accounts
- login
- sync
- telemetry
- analytics
- background services
- updater daemon
- system tray residency
- image editing
- crop
- exposure controls
- filters
- delete file
- copy file
- move file
- rename file
- file-management UI
- metadata editor
- favorites
- ratings
- albums
- tags
- slideshow system
- print workflow
- plugin system
- extension marketplace
- theme engine
- settings GUI
- right-click context menu
- custom image codecs
- custom UI framework
- speculative "future-proof" architecture

> Windows Explorer owns files. Fast Viewer owns pixels.

## Supported formats

Primary V1 formats:

- JPEG / JPG
- PNG
- BMP
- WebP
- TIFF

- GIF is not required.
- RAW formats are explicitly out of scope.
- HEIC / HEIF / AVIF: allow them if the installed Windows/WIC codec can decode them; do not bundle large third-party codecs solely to support them; absence of a system codec is not a project failure.
- Prefer capability-based WIC decoding rather than maintaining a large hardcoded format subsystem.

## Correct presentation requirements

The viewer must correctly handle:

- EXIF orientation
- Unicode paths and filenames
- very large images without catastrophic allocation
- corrupt images without crashing
- common ICC / sRGB presentation where practical through mature Windows facilities
- high-DPI displays
- per-monitor DPI behavior
- multi-monitor environments

Fast Viewer must not require Windows 11 for DPI or display support.

## Verified development environment

- Windows 10 Home 22H2 x64 (build 19045)
- MSVC 19.44 / VS Build Tools 2022 17.14
- Windows SDK 10.0.19041.0
- Direct2D dev support verified
- WIC dev support verified
- CMake 3.31.6
- Ninja 1.12.1
- Git
- VS Code with Microsoft C/C++ and CMake Tools
