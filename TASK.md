# Fast Viewer — Task State

Current phase: M1 — Performance

## M1 status

M0.1 (Core Viewer Cleanup) is complete: commit b59fbfa "fix: clamp pan and
bound image decode memory". M1 makes navigation genuinely fast without making
the computer feel slow.

## M1 authorized scope (performance only)

1. Move expensive image decode work off the UI thread.
2. Keep user input responsive even while uncached large images load.
3. Small bounded decoded-image cache (budget-based, ~256 MB soft).
4. Conservative nearby preloading (N-1 / N+1, direction N±2 optionally).
5. Cancel/deprioritize stale work during rapid navigation (generation ids).
6. Keep idle CPU, disk activity, and memory low.
7. Keep large-directory (50,000 entries) behavior responsive.
8. Preserve the small, simple architecture; no new dependencies.

Primary principle:

> Fast Viewer must never achieve responsiveness by aggressively consuming
> background CPU, SSD bandwidth, or memory.

## M1 explicit exclusions

No filmstrip, thumbnail UI, thumbnail cache, settings GUI, viewer.conf parsing,
installer, file associations, directory watcher, image editing, right-click
menu, metadata UI, plugin system, telemetry, update checker, network code, AI,
cloud features, recursive directory browsing, custom codecs, tiled image
renderer, speculative multi-stage architecture, generic task frameworks.

## M1 threading / architecture summary

- UI thread: window messages, input, paint, Direct2D (all D2D objects are
  created and used on the UI thread only).
- Decode worker (1 thread, normal priority): user-requested decodes.
- Preload worker (1 thread, THREAD_PRIORITY_BELOW_NORMAL): speculative decodes.
- COM/WIC threading: each worker thread calls CoInitializeEx(COINIT_MULTITHREADED)
  and creates its own IWICImagingFactory; no COM objects cross threads.
- Cross-thread handoff: workers produce immutable CPU-side pixel buffers
  (32bpp premultiplied BGRA); the UI thread creates the Direct2D bitmap from
  the buffer. No D2D or WIC object is shared across threads.
- Request model: every navigation intent gets an incrementing request id; the
  decode queue is latest-wins (single pending slot, no unbounded queue); stale
  completed decodes are detected by id and never displayed.

## M1 decoded-memory policy

- Per-image safety limit preserved: 512 MB estimated decoded bytes
  (width*height*4, overflow-safe) - unchanged from M0.1.
- Decoded-image cache: soft budget 256 MB, LRU with the current image pinned.
- Preload gate: speculative decode is skipped when its estimate plus current
  cache/current-image bytes plus headroom would exceed ~448 MB, and preloads
  over 256 MB estimated are never started. Large images therefore reduce or
  eliminate speculative caching.
- The user-requested image takes priority over speculative cache.

## M1 Definition of Done

- image decode does not block the UI thread during navigation
- rapid navigation stays responsive; stale results cannot overwrite newer ones
- preload is conservative; decoded cache is budgeted and bounded
- large images reduce/disable speculative caching as needed
- no unbounded job queue
- idle CPU settles near zero; idle disk activity settles to zero; memory stabilizes
- 50,000-entry directory does not freeze the UI
- current image zoom/pan stays responsive during background decode
- minimized viewer does not continue aggressive speculative work
- all close paths cleanly stop workers
- all M0.1 behavior remains correct
- clean build with zero warnings; no third-party dependencies
- architecture remains small; tests and measurements documented
- M2 remains unauthorized

## Remaining known limitations (unchanged from M0.1 unless noted)

- No preload of distant images; only the immediate neighborhood.
- WebP only via an installed system WIC codec (none on Windows 10 22H2).
- HEIC/HEIF/AVIF/GIF/RAW out of scope; failure state.
- Corrupt files may partially decode via WIC; never crashes.
- Normal-window geometry persists via a tiny registry key.
- Timing instrumentation only when FAST_VIEWER_TIMING is set; no telemetry.

M2 is NOT authorized. Stop and await human review.
