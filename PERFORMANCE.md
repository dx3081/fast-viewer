# Fast Viewer — Performance Specification

## The most important system rule

> Background work must never make the computer feel slow.

## Workload priority order

- **Priority 0:** user input, current image
- **Priority 1:** likely next image
- **Priority 2:** previous image
- **Priority 3:** currently visible filmstrip thumbnails
- **Priority 4:** everything else

Whenever the user interacts, low-priority background work must yield.

## Idle resource rules (first-class requirement)

After the user stops interacting and the necessary nearby work is complete:

- CPU should approach approximately 0–1%
- disk I/O should approach zero
- do not continue processing the entire folder
- do not continue generating thumbnails for unseen images

## Background / minimized behavior

When minimized or backgrounded: pause or heavily reduce speculative preload work.

## Preload policy

- Do NOT treat "more preloading" as automatically better. The viewer should only prepare a small amount of nearby content.
- Example around image 100:
  - current: 100
  - likely preload: 99 and 101
  - if the user is repeatedly navigating right, optionally prioritize 102
- If the user rapidly jumps forward:
  - obsolete decode jobs must be cancelable or deprioritized
  - the newly requested image becomes highest priority
  - do not waste CPU decoding images the user has already skipped
- The system must support cancellation/generation semantics from the beginning of the performance phase.

## Cache / memory policy

- Do not cache an arbitrary fixed number of fully decoded images. Use a memory-budget-based policy.
- Normal steady-state usage: ideally under ~200 MB for typical images.
- Decoded-image soft cache budget: approximately 256 MB.
- Very large images may temporarily exceed typical usage, but memory must be released promptly.
- The viewer must not grow indefinitely as more images are viewed.
- Prefer simple eviction such as locality/LRU-style behavior:
  - current image is always protected
  - nearby images have priority
  - far/old images are evicted first

## Performance targets (engineering targets, not marketing claims)

- **Cold launch to first visible image:** required < 250 ms on a normal machine where practical; stretch < 120 ms. Do not fake this metric by showing an obviously poor-quality placeholder.
- **Navigation to already-preloaded image:** required < 50 ms; stretch 16–25 ms.
- **Uncached ordinary JPEG navigation:** approximately < 150 ms for normal 12–24 MP images, where practical.
- **Zoom and pan:** 60 FPS. Zoom/pan should primarily use rendering transforms rather than repeated disk decode operations.
- **Large directories:** a directory containing 50,000 images must not freeze the UI. Initial image display must not wait for the full directory to be processed.

## Thumbnail policy

- Main-image decode and filmstrip thumbnails are separate concerns.
- Do not decode a giant source image at full resolution solely to produce a tiny filmstrip thumbnail if a more efficient WIC path is available.
- Only load thumbnails around the current filmstrip viewport.
- Do not eagerly generate thumbnails for the entire directory.
- Do not create tens of thousands of child window controls or UI objects.
- The filmstrip should behave like a tiny virtualized renderer.

## Close behavior

- On close: terminate all worker threads and directory watchers; leave no resident process, no service, no tray process.
