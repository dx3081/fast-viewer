# Fast Viewer — Task State

Current phase: 1.0 RC — Release Preparation

## 1.0 RC fix — filmstrip edge centering (Task A)

Human review rejected the old "natural edge alignment" behavior (first few
images left-align, last few right-align). New product rule: the current
thumbnail stays horizontally centered in the viewer at ALL directory
positions; where neighbors do not exist, the space is simply empty filmstrip
background (no fake cells, no placeholder boxes, no wrap).

- Fix: Filmstrip::Update now always places the current cell (position
  p = (visible-1)/2) on the viewport center; cells whose index falls outside
  [0, count) are skipped. visibleStart_ may be negative (logical row start);
  ScheduleThumbs iterates the logical row via VisibleWidth so edge cells still
  load. Geometry log reports the first real cell index (>= 0).
- Verified: 1/1297, 2/1297, 5/1297, 100/1297, 500/1297, 1293/1297, 1296/1297,
  1297/1297 all curcx=1280 (diff 0); rapid Right/Left keeps the selection at
  fixed screen center; empty edge space confirmed visually (left for index 1,
  right for index N); thumbnail click and wheel navigation correct at edges;
  125% DPI and normal window mode correct; M0 50/50, M2 26/26, RC 21/21 pass.
- Docs: UX.md and TASK.md updated to the permanent-center rule.

## 1.0 RC diagnosis — double-click close bug (Task B, DIAGNOSIS ONLY)

Human-observed: immersive -> left double-click -> normal -> left double-click
-> viewer closes. Expected: double-click toggles immersive/normal repeatedly
and never closes. No product code was changed for this (diagnosis only).

- Runtime evidence (synthetic + real SendInput, temporary env-gated diag log
  removed after capture):
  - Client-area left double-clicks NEVER close: immersive->normal->immersive
    toggles correctly at center, edges, fast human-speed, after F11, after
    pan; 4x single clicks never close. Right-click closes (expected).
  - The app has NO handler for WM_NCLBUTTONDBLCLK / WM_SYSCOMMAND /
    WM_NCHITTEST; all fall through to DefWindowProc, which implements the
    standard title-bar behavior. Caption (HTCAPTION) double-click ->
    SC_MAXIMIZE/SC_RESTORE (verified in diag: msg 0x00A1 wp=0x2 -> 0x0112
    0xF012 -> 0xF032; does NOT close).
  - Close is triggered ONLY by WM_SYSCOMMAND SC_CLOSE -> WM_CLOSE -> destroy
    (verified: posting SC_CLOSE and WM_CLOSE each exit the process cleanly).
    SC_CLOSE is produced by DefWindowProc when the click/double-click lands
    on the title-bar close button (HTCLOSE) of the NORMAL-mode window.
  - Root cause: in normal mode the window gains a standard title bar whose
    close button (X) is live. A left double-click that lands on that X is
    interpreted by DefWindowProc as SC_CLOSE. The bug is therefore
    cursor-position/geometry dependent (only when the second double-click
    hits the X region); client-area double-clicks are not involved.
  - Awaiting human decision on the fix (not implemented per task authority).

## 1.0 RC fix — filmstrip first-reveal centering

Human testing observed that on the first reveal after launch, the filmstrip
could briefly render in an incorrect (empty/left) layout, then shift to the
correct centered position once navigation state finished updating.

- Root cause: initialization-order race. RevealFilmstrip draws the first
  frame using whatever navigation state exists at that moment. If the reveal
  happens before the async directory scan completes, that frame is drawn with
  count=0 (no navigation result yet). OnScanComplete then set displayIndex_
  but did NOT trigger a redraw, so the visible strip kept the stale
  pre-scan layout until an unrelated later event (decode/preload/user input)
  happened to repaint it — perceived as a left-to-center jump.
- Fix (src/app.cpp OnScanComplete): when the filmstrip is already visible at
  scan completion, immediately recompute the layout with the now-final
  navigation range (DrawNow + ScheduleThumbs). The first frame that carries
  real layout now uses the final centered/edge position directly.
- Behavior when the final layout genuinely cannot yet be determined (reveal
  before scan done): the strip shows only the empty surface (no thumbnails,
  no knowingly-wrong left-aligned cells) and populates once the scan lands —
  matching the agreed preferred behavior. No animation, no fade, no visuals
  changed; cache/worker/navigation architecture untouched.
- Verified: launch at 250/500 (normal + race reveal) first cells-frame
  centered at 1280; 1/500 left-edge; 500/500 right-edge; 1920-wide centered
  at 960; 50,000-file directory race (scan ~40 ms) still first-cells-frame
  centered; repeated hide/reveal unchanged; rapid navigation unchanged;
  M2 26/26 and RC polish 21/21 pass; cold launch ~74 ms, idle CPU 0%, no log
  growth. No new dependencies.

## 1.0 RC fix — WebP first-class support

Human testing on the development machine proved that a real .webp file
already opened and displayed correctly through the existing WIC decode path.
The observed problem was that .webp was missing from directory
navigation/filtering and installer registration — not a decode problem.

- Root cause: the single runtime allowlist IsSupportedExtension()
  (src/image_loader.cpp) listed jpg/jpeg/png/bmp/tif/tiff only; directory
  scan (navigation.cpp) therefore excluded .webp. Direct launch worked
  because main.cpp does no extension filtering.
- Fix: .webp added to IsSupportedExtension() (one line). No decoder added,
  no codec installed, no dependency added, no core decoder change — the
  existing WIC path decodes WebP on this machine (verified: decode_ms logged,
  first_render ok).
- Installer: .webp registered alongside the other formats in OpenWithProgids,
  Applications\fast_viewer.exe\SupportedTypes, and
  FastViewer\Capabilities\FileAssociations; uninstall removes Fast Viewer's
  own .webp registration only.
- Docs: README.md and release notes now list WebP with wording reflecting the
  WIC-based architecture ("supported through the Windows imaging codec
  available on the system; verified on the primary Windows 10 development
  environment"). No GIF/HEIC/AVIF/RAW associations added.
- Verified: direct .webp launch, mixed-format natural sort (IMG_1.jpg ->
  IMG_2.webp -> IMG_3.png -> IMG_4.webp -> IMG_10.jpg), Left/Right through
  WebP in both directions, WebP-only directory, filmstrip WebP thumbnails +
  thumbnail click + wheel navigation across formats, corrupt .webp fails
  gracefully with navigation still usable and no thumbnail retry loop, idle
  CPU 0% / no log growth, cold launch ~98 ms.
- Release artifacts rebuilt (portable zip, installer, SHA-256 checksums).
  Version stays 1.0.0-rc1. No GitHub Release, no v1.0.0 tag.

## 1.0 RC status

RC UX POLISH is complete (commit 662a549). 1.0 RC release preparation is
complete; publication is NOT authorized automatically — the human owner
decides whether RC1 is published.

- Version: 1.0.0-rc1 (centralized in CMakeLists.txt; generated version.h;
  executable metadata; installer metadata; release docs)
- Runtime strategy: static MSVC runtime (/MT via CMAKE_MSVC_RUNTIME_LIBRARY).
  The release executable (514,560 bytes) imports only inbox Windows system
  DLLs (d2d1, DWrite, ole32, SHLWAPI, KERNEL32, USER32, SHELL32, ADVAPI32) —
  no MSVCP140/VCRUNTIME140, no UCRT DLLs, no VC redistributable required.
- Executable metadata: FileDescription/ProductName "Fast Viewer",
  FileVersion/ProductVersion 1.0.0-rc1, no invented company/copyright.
- Icon: temporary placeholder (src/resources/fast_viewer.ico, dark rounded
  square + light "F") — FINAL ICON PENDING HUMAN APPROVAL. Swapping the icon
  requires changing one line in src/resources/app.rc only.
- License: NOT YET SELECTED (no LICENSE file added).
- Code signing: UNSIGNED (no certificates fabricated). Windows SmartScreen may
  warn on a new unsigned application; expected for a small private project.
- Portable package: single fast_viewer.exe + README.txt (zip, ~247 KB).
  Verified to run from a clean folder outside the repository: JPEG/PNG/BMP/
  TIFF/EXIF all open; filmstrip, zoom/pan, mode toggles, close all work; no
  missing-DLL error; no config file required; idle CPU 0% / disk zero /
  working set ~39 MB.
- Installer: Inno Setup (release-only build tool, not part of the runtime),
  per-user install to %LocalAppData%\Programs\Fast Viewer, no admin rights.
  Registers "Open with" + capabilities for .jpg/.jpeg/.png/.bmp/.tif/.tiff
  (per-user, additive; never forces a default-app change — Windows 10
  default-app confirmation is respected). WebP/GIF/RAW/HEIC not registered.
- Install/uninstall/reinstall tested on Windows 10: clean install, clean
  uninstall (files + FastViewer registry entries removed, no leftover
  process), reinstall works, unrelated associations untouched.
- Multiple-launch behavior: each Explorer double-click opens its own
  process/window (documented; no single-instance IPC added).
- Viewer regressions on the release build: M0 50/50, M0.1 27/27, M1 26/26,
  M1.1 (preload-hit median ~9-11 ms), M2 26/26, RC polish 21/21.
- Performance on the release build: cold first image median ~78 ms,
  preload-hit navigation median ~9 ms, idle CPU 0%, idle disk zero,
  working set ~39 MB. No regression from release metadata/runtime changes.
- Release staging: C:\DSWorkspace\release-staging (gitignored; not committed):
  FastViewer-1.0.0-rc1-portable.zip, FastViewer-1.0.0-rc1-setup.exe,
  SHA256SUMS.txt, portable folder.
- GitHub Release: NOT published. Git tag: NOT created.
- Reproducible assembly: packaging/build_release.ps1 (builds, stages portable,
  zips, runs ISCC, writes SHA-256 checksums).

## RC UX polish status

M2 is complete (commit 2742af2). RC UX POLISH refined the filmstrip layout to
be smaller and less box/grid-like while preserving every interaction:

- Thumbnails shrunk from 120x96 to **92x68 logical px**; strip total height is
  now **80 logical px** (68 + 2 x 6 padding), down from ~108.
- Gap 8 -> **6 logical px**; margins 16 -> **12 logical px**; padding 6 logical px.
- Current image now **visually centers on the viewport center** always: the
  current thumbnail occupies position p = (visible-1)/2 of the row and the row
  is placed so that cell sits on the viewport center (window of 5..11 cells);
  at directory edges the current cell stays centered and the missing neighbor
  space is simply empty filmstrip background (no fake cells, no wrap).
- Box/grid look removed: non-current cells have **no border**; the current cell
  keeps a thin **1.5 px selection border** only; dark flat background + 1 px
  top separator remain.
- Info text (name / WxH / index-total) uses a smaller 12 px font and sits in
  the 16 px band directly above the strip (stripRect.top-18 .. -2).
- All interactions preserved: hot-zone reveal, 600 ms auto-hide with re-entry
  cancellation, wheel-nav over the strip, wheel-zoom over the image, thumbnail
  click navigation, pan, right-click close.
- No performance regression: same worker/queue/cache architecture untouched.

## RC polish layout values (final)

- hot zone: bottom 24 logical px
- thumbnail cells: 92 x 68 logical px
- gap: 6 logical px; margins: 12 logical px; padding: 6 logical px
- strip height: 68 + 2 x 6 = 80 logical px
- visible cells: width-driven, clamped to 5..11
- centering: current always at position (visible-1)/2 of the row, row placed
  so the current cell center = viewport center; at directory edges missing
  neighbors leave empty filmstrip background (no fake cells, no wrap)
- constants centralized in filmstrip.h

## M2 status

M1.1 (Preloaded Presentation Latency Verification) is complete: commit 10f8456
"perf: verify cached presentation latency". M2 adds the minimal Picasa
Photo Viewer-style interaction layer on top of the already-fast core viewer.

## M2 authorized scope

- hidden bottom filmstrip (overlay; main image stays dominant)
- reveal when the pointer enters the bottom activation zone (~24 px, DPI-aware)
- auto-hide ~600 ms after the pointer leaves; re-entry cancels the hide
- nearby thumbnails only, rendered as a small virtualized window around the
  current index (never one UI object per image)
- thumbnails decoded off the UI thread by one low-priority worker; bounded
  separate thumbnail cache (~24 MB)
- mouse wheel over the filmstrip navigates (wheel up = previous, wheel down =
  next); wheel over the main image still zooms
- clicking a visible thumbnail navigates through the existing async
  latest-wins request system
- current image stays near the filmstrip center where practical; natural
  alignment at directory start/end (no fake slots, no wrap)
- lightweight filename / dimensions / index-total text only while the
  filmstrip is visible
- filmstrip works in immersive and normal window modes; DPI-aware

## Filmstrip layout rules

- hot zone: bottom 24 logical px; filmstrip height = thumbnail height
  (68 logical px) + 2 x 6 padding = 80 logical px; thumbnail cells 92 x 68
  logical px, 6 px gap, 12 px margins; visible cell count = width-driven,
  clamped to 5..11
- current thumbnail marked with a thin 1.5 px selection border only;
  non-current cells have no border; dark flat background, 1 px top separator,
  no blur/glass/transparency effects
- current image stays horizontally centered in the viewer, including at
  directory edges: where neighbors do not exist, the space is empty filmstrip
  background (no fake cells, no placeholder boxes, no wrap)
- constants centralized in filmstrip.h (future viewer.conf candidates)

## M2 explicit exclusions

No right-click menu, settings GUI, file operations, image editing, rotation
UI, slideshow, favorites/ratings/tags/albums, EXIF panel, metadata editor,
folder tree, home screen, toolbar, menu bar, persistent title overlay, zoom
controls, scrollbars, plugin system, themes, updater, telemetry, network, AI,
cloud, recursive browsing, GIF/RAW support, custom codecs, installer, file
associations, viewer.conf parsing, release packaging.

## M2 Definition of Done

- filmstrip hidden by default; bottom hot-zone reveal works
- auto-hide works; re-entry cancels hide
- nearby thumbnails render; current image near center; start/end OK
- wheel over filmstrip navigates; wheel over main image still zooms
- thumbnail click navigates via the async latest-wins path
- lightweight info (filename/dimensions/index) only while visible
- immersive + normal window modes work; DPI works
- close during thumbnail work is safe
- thumbnail cache bounded; no whole-directory thumbnail generation
- hidden filmstrip does no unnecessary work; idle CPU ~0%, disk zero
- main-image navigation and cold-start performance preserved
- all M0/M0.1/M1/M1.1 regressions pass; zero warnings; no dependencies
- project remains small

After M2: STOP. No installer, no release, no M3. Human review decides whether
to prepare 1.0.

## M2 known limitations

- Thumbnails use the WIC full-decode-then-scale path (IWICBitmapScaler): WIC
  has no generic reduced-size decode, so a large image's thumbnail still decodes
  the full frame once (off-thread, one-time, cached, only for visible cells).
- The filmstrip draws over the bottom of the main image (overlay); the very
  bottom edge of a zoomed image is temporarily covered while the strip is
  visible.
- Cache-hit presentation is unchanged (~10 ms); the first time a 40-60 MP image
  is shown, its one-time D2D upload (35-50 ms) is paid on the UI thread (as in
  M1).
- If the D2D device is lost (D2DERR_RECREATE_TARGET), cached bitmaps are not
  automatically re-uploaded; the current image re-renders from the next decode
  (rare on desktop, existing M1 limitation).
- Thumbnail failures are remembered per session (bounded) so broken files are
  never retried forever.
- No thumbnail animation/transition (by design; instant appearance).

## M2 measurements (M2 completion)

- Cold first visible image: ~87-119 ms (M1.1 baseline ~112 ms; no regression)
- Filmstrip reveal (bottom-zone entry -> rendered): ~8-9 ms
- First nearby thumbnail: ~31-44 ms; nearby batch settles in ~1.8 s
- Main preload-hit navigation with filmstrip visible: ~18 ms (M1.1 ~10 ms;
  within the 50 ms target)
- Filmstrip visible settled: CPU 0%, working set +~8 MB (thumbnails ~24 MB
  budget), zero pending thumbnail jobs
- Filmstrip hidden idle (5 s and 30 s): CPU 0%, disk zero, no thumbnail work
- 500-image dir: only visible-neighborhood thumbnails requested (~11-13, never
  the whole directory); thumbnail cache bounded under the 24 MB budget
- 50,000-entry dir: filmstrip reveals at index 0 request only indices ~0-15

## M2 automated verification

- M2 suite: 26 checks (reveal, auto-hide, hide cancellation, layout at start
  and mid-directory, wheel-zoom unchanged, wheel-nav, thumbnail click, rapid
  clicks, right-click close, close during thumbnail load, hidden idle, bounded
  cache, 50k local-only, pan near bottom) - all pass
- RC polish suite: 21 checks (centering at 5/100/250/494/500 + edges, strip
  height <= 90 logical px, thumbnail height ~68 logical px, 1920-wide
  centering, rapid-nav stationary) - all pass on the RC build
- Regression: M0 50/50, M0.1 27/27, M1 26/26 - all pass on the M2 build

After M2: STOP. No installer, no release, no M3. Human review decides whether
to prepare 1.0.

## After RC UX polish

- RC polish (filmstrip layout only) committed; pushed to origin/main
- Human visual inspection: Smaller/Centered/Less box/grid-like
- Next authorized phase: NONE — awaiting human visual review

## After 1.0 RC release preparation

- Release preparation committed; pushed to origin/main
- Publication NOT authorized automatically: human owner decides whether RC1
  is published (e.g., GitHub Release)
- Next authorized action: NONE — do not publish 1.0 without human approval
