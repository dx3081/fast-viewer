# Fast Viewer — Task State

Current phase: M0 — Core Viewer

## M0 authorized scope

- launch from a single command-line image path (clean exit if no valid path is supplied)
- immersive borderless mode (default) and normal window mode
- Direct2D + WIC rendering pipeline
- fit behavior, 100% / Fit toggle, cursor-centered wheel zoom, left-drag pan
- F11 and double-click mode toggle
- Esc / right-click / window X close
- Left/Right navigation within the same direct parent directory, natural filename sort, no recursion
- graceful failure for corrupt / unsupported / oversized images
- Windows 10 DPI-correct behavior (per-monitor aware)
- lightweight timing/debug instrumentation only (no telemetry)

## M0 explicit exclusions

No filmstrip, thumbnails, thumbnail cache, preloading architecture,
direction-aware preload, directory watcher, viewer.conf parsing, settings UI,
installer, file associations, context menu, image editing,
delete/copy/move/rename, EXIF/metadata UI, themes, plugins,
network code, telemetry, AI, cloud features.

## M0 Definition of Done

- native executable builds successfully (CMake + MSVC, `/std:c++latest`)
- valid image path launches the viewer; immersive mode works
- normal window mode, F11 and double-click toggles work
- Esc / right-click / window X close work
- Fit, 100% / Fit toggle, cursor-centered wheel zoom, pan work
- Left/Right navigation works; natural filename sort; no recursive navigation
- JPEG / PNG / BMP / TIFF tested; WebP only through an installed system codec
- corrupt image does not crash; huge-image safety; Windows 10 DPI behavior acceptable
- idle CPU and disk activity settle; no runaway memory
- project remains small and dependency-free
- remaining M0 limitations documented in the M0 report

M1 is NOT automatically authorized after M0. Stop and await human review.

## M0 known limitations (documented per M0 Definition of Done)

- Decode is synchronous on the UI thread: navigating to a large uncached image
  briefly blocks the UI (M1 moves decode off-thread with preloading).
- No preload/cache: every navigation decodes from disk (M1 scope).
- If an arrow key is pressed before directory enumeration completes, the
  navigation is applied as soon as the scan finishes (no UI freeze).
- Pan is not clamped: at Fit zoom the image can be dragged off-center.
- WebP is only usable through an installed system WIC codec; Windows 10 22H2
  has none by default, so WebP is not in the navigation list and opens with a
  failure state if no codec is present.
- HEIC/HEIF/AVIF/GIF/RAW are out of M0 scope and show the failure state.
- Huge-image safety caps decoding at 16384 px per dimension and 200 MP;
  larger images show the failure state (no tiled rendering in M0).
- Corrupt files may partially decode via WIC: shown if possible, error state
  otherwise; never crashes.
- Normal-window geometry persists through a tiny registry key
  (HKCU\Software\FastViewer), updated only while in normal window mode.
- Timing instrumentation writes a small local log only when FAST_VIEWER_TIMING
  is set; no telemetry.
