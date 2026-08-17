# Fast Viewer — Task State

Current phase: M2 — Picasa-style UX

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
  (96 logical px) + padding; thumbnail cells 120 x 96 logical px, 8 px gap,
  16 px margins; visible cell count = width-driven, clamped to 5..11
- constants centralized in filmstrip.h (future viewer.conf candidates)
- current thumbnail marked with a brighter border only; dark flat background,
  no blur/glass/transparency effects

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
- Regression: M0 50/50, M0.1 27/27, M1 26/26 - all pass on the M2 build

After M2: STOP. No installer, no release, no M3. Human review decides whether
to prepare 1.0.
