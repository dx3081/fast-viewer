# Fast Viewer — UX Specification

## Launch

When launched with an image path from Windows Explorer:

1. Show the requested image as quickly as possible.
2. Default to immersive borderless viewing mode.
3. Center the image.
4. Fit images larger than the viewport.
5. Do not unnecessarily upscale small images.
6. Directory discovery must not delay first-image display.

There is no meaningful launch behavior without an image path. A future implementation may simply exit if no valid image path is supplied.

## Viewing modes

- **Immersive borderless mode (default):** borderless window filling the active monitor, simple dark background, centered image. No desktop capture, acrylic, blur, glass, wallpaper sampling, or fancy compositing.
- **Normal window mode:** regular window with standard chrome.
- Double-click and F11 both toggle between the two modes (two equivalent ways).

## Mouse behavior

| Input | Action |
| --- | --- |
| Mouse wheel over main image | Zoom in/out, centered on the mouse pointer position. Direct and immediate. Ctrl+wheel is not the primary zoom gesture. |
| Left mouse drag (image zoomed beyond viewport) | Pan the image. Not file selection, editing, or annotation. |
| Double-click on main image | Toggle immersive borderless mode <-> normal window mode. Intentionally NOT a zoom-reset gesture. |
| Right-click | Close Fast Viewer (V1). No context menu. |
| Pointer to bottom edge | Reveal the filmstrip via the bottom activation zone. |

The input/action layer must be designed so the right-click mapping (and mouse mappings generally) can be changed later without restructuring the viewer.

## Keyboard behavior (required V1)

- `Left Arrow` -> previous image
- `Right Arrow` -> next image
- `Esc` -> close Fast Viewer
- `F11` -> toggle immersive borderless mode / normal window mode
- `1` -> toggle 100% view / fit view

F11 and double-click intentionally provide two equivalent ways to toggle viewing mode.

## Zoom / pan semantics

- Zoom centered around the mouse pointer position.
- Zooming is direct and immediate; primary wheel zoom does not require Ctrl.
- Zoom/pan should primarily use rendering transforms rather than repeated disk decode operations (target 60 FPS).
- When navigating to a new image, reset to default fit behavior; do not inherit arbitrary zoom/pan state from the previous image.

## Filmstrip behavior

- The filmstrip is for navigation only. It is NOT a library UI and NOT a file-management UI.
- Hidden by default.
- Appears when the pointer reaches the bottom activation zone (~20–30 px).
- Show immediately or nearly immediately.
- After the pointer leaves, hide after approximately 500–800 ms.
- Typical visible thumbnail count ~7–11 depending on viewport width.
- The current thumbnail stays horizontally centered in the viewer, even near
  the beginning or end of the directory: where neighboring images do not
  exist, the space is simply empty filmstrip background (no fake cells).
- Only nearby thumbnails exist/load. Do not create UI elements for every image in a directory.
- Mouse wheel over the filmstrip navigates between images.
- Clicking a thumbnail navigates directly to that image.
- The filmstrip behaves like a tiny virtualized renderer.

## Lightweight status information

When the filmstrip is visible, lightweight information may also be shown:

- filename
- dimensions
- current index / total image count

This information must not be permanently visible.

## Navigation order

- Only the current image's direct parent directory participates in navigation. Do not recurse into subdirectories.
- Initial navigation order: **natural filename sort** (e.g., `IMG_1.jpg`, `IMG_2.jpg`, `IMG_10.jpg` — not lexical order that places `IMG_10.jpg` before `IMG_2.jpg`).
- When navigating to a new image, reset to default fit behavior; do not inherit arbitrary zoom/pan state from the previous image.
- Directory changes made externally by Windows Explorer may eventually be observed by the viewer, but must never block foreground interaction.

## Close behavior

- Esc and right-click close Fast Viewer and return to Explorer.
- On close: terminate all worker threads and directory watchers; leave no resident process, no service, no tray process.
