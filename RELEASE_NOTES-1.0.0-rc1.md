# Fast Viewer 1.0.0-rc1 — Release Notes (DRAFT)

Version: 1.0.0-rc1
Target: Windows 10 x64 (22H2 / build 19045 development environment)
Executable: fast_viewer.exe (~500 KB, single file, static runtime)
License: not yet selected
Code signing: unsigned

## What this is

Fast Viewer is a tiny, extremely fast, native Windows image viewer. It is a
dedicated viewing layer for Windows Explorer: double-click an image, view and
navigate, close. No library, no database, no cloud, no telemetry, no
background services, no tray process, no updater.

## Supported formats

- JPEG / JPG, PNG, BMP, TIFF, WebP (via Windows Imaging Component)
- GIF and RAW: not supported
- HEIC / HEIF / AVIF: only if a system codec is installed
- WebP is supported through the Windows imaging codec available on the
  system; verified on the primary Windows 10 development environment

## Highlights

- Cold launch to first visible image: ~80–120 ms on the development machine
- Navigation to a preloaded neighbor: ~10 ms (cache-hit presentation)
- Bounded memory: 256 MB decoded-image budget, 24 MB thumbnail budget,
  512 MB per-image decode safety cap
- Picasa-style filmstrip: bottom hot-zone reveal, auto-hide, nearby
  thumbnails only, current image centered (68 px thumbnails, 80 px strip)
- EXIF orientation, Unicode paths, high-DPI / per-monitor DPI aware v2
- 50,000-image directories do not block the UI

## Packaging

- Portable: single fast_viewer.exe + README.txt (no install, no VC
  redistributable; static MSVC runtime)
- Installer: per-user Inno Setup install to
  %LocalAppData%\Programs\Fast Viewer; no administrator rights; registers
  Fast Viewer for "Open with" on .jpg/.jpeg/.png/.bmp/.tif/.tiff/.webp;
  clean uninstall; never forces a default-app change

## Notes for this RC

- Icon is a temporary placeholder; final icon pending human approval.
- Executable and installer are unsigned; Windows SmartScreen may warn on a
  new unsigned application. This is expected for a small private project.
- License not yet selected.
- Release has not been published; the human owner decides whether RC1 is
  published (e.g., as a GitHub Release).

## Verified on the development machine

- Viewer regression suites: M0 50/50, M0.1 27/27, M1 26/26, M1.1 (latency),
  M2 26/26, RC polish 21/21 — all pass on the release build.
- Portable artifact runs from a clean folder outside the repository.
- Installer: install, launch, uninstall, reinstall all clean.
- Uninstall removes files, registry entries, and leaves no process.
