# Fast Viewer 1.1.0-beta1 — Release Notes (DRAFT)

Version: 1.1.0-beta1 (branch `a2-overhaul`; not published)
Target: Windows 10 x64
Executable: fast_viewer.exe (~543 KB, single file, static runtime, inbox DLLs only)

## Faster and steadier

- Screen-size decoding: photos are decoded to fit the monitor (JPEG reduces
  natively), full resolution only when zooming in. Smaller cache entries keep
  the preload neighborhood ready, and memory stays at ~70-100 MB while browsing.
- Preloading continues image after image (+1, +2 ahead, -1 behind).
- Holding an arrow key scrolls through the folder instead of freezing.
- Portrait (EXIF-rotated) photos open in ~0.3 s instead of 2-3.5 s.
- No waiting for the display refresh when presenting a frame.
- The first image starts decoding while the window is being created.
- Filmstrip thumbnails use the photo's embedded preview when present.

## New

- Animated GIF playback.
- Formats follow the installed Windows codecs: HEIC/HEIF, AVIF and camera RAW
  open when their Microsoft Store extensions are installed; otherwise the
  viewer names the missing extension. RAW shows the camera preview instantly.
- R / Shift+R rotate the view (the file is never changed).
- Ctrl+C copies the image and the file.
- Picasa-style bottom bar: zoom in / zoom out / 1:1, rotate left / right,
  previous / next, caption `name (W x H)  n / total`.
- App icon in the taskbar; DPI-scaled text; Chinese UI text on Chinese Windows.
- Installer: optional GIF / HEIC / AVIF / RAW "Open with" registration.

## Not verified in this beta

- HEIC / AVIF / RAW with real files (no samples on the development machine).
- Installer install/uninstall run (the package builds; not installed).
