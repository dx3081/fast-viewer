Fast Viewer 1.1.0 (portable, Windows x64)
==========================================

A tiny, fast, native Windows image viewer in the spirit of the old Picasa
Photo Viewer: double-click a picture in Explorer, look, flip through the
folder, close. No library, no database, no background service, no telemetry.
One 550 KB executable, no installation.

Source code (GPL-3.0): https://github.com/dx3081/fast-viewer


Requirements
------------
- Windows 10 or Windows 11, 64-bit. No runtime or redistributable needed.
- Formats come from the Windows imaging codecs on your PC:
    JPEG, PNG, BMP, TIFF, WebP, GIF (animated) work on every Windows 10/11.
    HEIC / HEIF (iPhone photos) need "HEVC Video Extensions" from the
      Microsoft Store (about 1 USD).
    AVIF needs "AV1 Video Extension" from the Microsoft Store (free).
    Camera RAW (.dng .cr2 .cr3 .nef .arw ...) needs "Raw Image Extension"
      from the Microsoft Store (free). RAW files show the camera's embedded
      preview instantly; zooming in develops the real RAW data.
  If an extension is missing, the viewer tells you which one.


How to use
----------
1. Unzip anywhere. No installer, nothing is written outside the zip folder
   except one small registry key that remembers the window position.
2. Right-click a picture in Explorer > Open with > Choose another app >
   browse to fast_viewer.exe. Tick "Always" if you want it as the default.
3. The viewer opens full-screen (borderless). Move the mouse to the bottom
   edge to show the control bar and filmstrip.

Note: the executable is not code-signed, so Windows SmartScreen may warn
the first time. Choose "More info" > "Run anyway".


Keys
----
  Left / Right        previous / next image in the folder
  Mouse wheel         zoom in / out around the pointer
  Left drag           pan when zoomed in
  1                   toggle 100% / fit to window
  R / Shift+R         rotate view right / left (the file is never changed)
  Ctrl+C              copy the image (paste into chats or documents) and the
                      file (paste into Explorer)
  F11 / double-click  toggle full-screen / normal window
  Esc / right-click   close

Bottom bar (move the mouse to the bottom edge):
  zoom in, zoom out, 1:1, previous / next, rotate left / right, and a
  filmstrip of the neighbouring pictures; click a thumbnail to jump to it.


Privacy
-------
No network access, no analytics, no updater, no tray icon. Nothing keeps
running after you close the window.


License
-------
Copyright (C) 2026 DMZ
GNU General Public License v3.0 only. See LICENSE.
