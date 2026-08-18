# Fast Viewer

Fast Viewer is a tiny, extremely fast, native Windows image viewer. It is a
dedicated viewing layer for Windows Explorer: double-click an image, view and
navigate, close, and you are back in Explorer. No library, no database, no
cloud, no telemetry, no background services.

- Target: Windows 10 x64
- Formats: JPEG/JPG, PNG, BMP, TIFF (via Windows Imaging Component)
- Size: a single ~500 KB executable, no install required for the portable
  build, no VC redistributable needed

## Run the portable version

1. Extract `fast_viewer.exe` anywhere (a USB stick is fine).
2. Double-click an image in Explorer, or run:

   ```
   fast_viewer.exe "C:\path\to\image.jpg"
   ```

No configuration file is required; built-in defaults are sufficient.

## Core controls

Mouse:

- Wheel over the image: zoom in/out (centered on the pointer)
- Left drag (when zoomed): pan
- Double-click: toggle immersive / window mode
- Right-click: close
- Move the pointer to the bottom edge: show the filmstrip
- Wheel over the filmstrip: previous / next image
- Click a thumbnail: jump to that image

Keyboard:

- Left / Right arrow: previous / next image
- Esc: close
- F11: toggle immersive / window mode
- 1: toggle 100% / fit view

The filmstrip hides by default; it appears when the pointer reaches the bottom
activation zone and hides shortly after the pointer leaves.

## Set Fast Viewer via "Open with"

1. Right-click an image in Explorer.
2. Choose **Open with** → **Choose another app** (or "More apps").
3. Select **Fast Viewer** (check "Always use this app" if you want it as the
   default for that file type).

You can also change defaults in **Settings → Apps → Default apps** by file
type. Windows 10 requires you to confirm default-app changes; Fast Viewer
never overrides that choice on its own.

## Known limitations

- GIF and RAW are not supported.
- HEIC/HEIF/AVIF open only if a system codec is installed.
- The filmstrip draws over the bottom edge of the main image while visible.
- Very large images (above ~512 MB decoded) are rejected with an error rather
  than risking memory exhaustion.
- The executable is unsigned; Windows SmartScreen may show a warning for a
  new, unsigned application. This is expected.

## Privacy

Fast Viewer performs no telemetry, no analytics, no network access, no
automatic updates, and installs no background services or tray processes.
When the viewer is closed, nothing remains running.
