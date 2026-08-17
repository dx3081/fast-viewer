# Fast Viewer — Task State

Current phase: M0.1 — Core Viewer Cleanup

## M0 status

M0 (Core Viewer) is complete: commit 9061d8e "feat: M0 core viewer (Win32 +
Direct2D + WIC)". M0.1 is a small safety/interaction cleanup pass on top.

## M0.1 authorized scope (exactly two goals)

1. Correct pan clamping (no more off-center dragging at Fit; no overscroll).
2. Stronger decode-memory safety (explicit decoded-bytes budget before any
   large allocation).

## Pan clamp semantics (M0.1)

Deterministic, per-axis clamping applied after drag, wheel zoom, Fit, 100%,
window resize, immersive/normal toggle, and navigation:

- If the scaled image dimension fits the viewport on an axis, the image is
  centered on that axis and pan is locked (offset resolves to 0).
- If the scaled image dimension exceeds the viewport on an axis, pan is
  limited to +/- (scaled - viewport) / 2 so every image edge/corner is
  reachable and overscroll into empty background is impossible.
- No elastic scrolling, no animation.

## Decoded-memory safety limit (M0.1)

- Estimation: `decoded_bytes = width * height * 4` (32bpp BGRA), computed with
  overflow-safe arithmetic after a 16384 px/dimension cap (Direct2D feature
  limit) that also bounds the byte math.
- Hard budget: **512 MB** of estimated decoded bytes per image.
- Rationale: the raw 4-byte/pixel estimate is the conservative reference; the
  real WIC/Direct2D path transiently holds decoder frame + format converter +
  D2D bitmap (measured roughly 2.5-4x the raw estimate in working set), so
  this budget rejects ~768 MB-raw-class full decodes outright (e.g.
  16000x12000) while accepting ordinary photos up to ~10000x8000 (~320 MB raw).
- Rejection: graceful failure state via the existing error path
  (hr=0x800700DF, ERROR_FILE_TOO_LARGE); navigation remains usable; the
  dangerous allocation is never attempted.
- The exact threshold may be revisited in M1 based on measured behavior.

## M0.1 tests performed (external scripted harness, outside the repo)

- Pan clamp: Fit lock, horizontal-only overflow, vertical-only overflow,
  both-axis overflow with all four corners reachable, no overscroll, resize
  reclamp, 100%/Fit reclamp — all pass.
- Memory safety: 6000x4000 accepted (real decode), 10000x8000 accepted per the
  guard formula (math validation; no 1 GB real decode performed), 12000x12000
  rejected by the byte guard before allocation, 20000x1000 rejected by the
  dimension cap, WIC-level header rejection graceful, navigation usable after
  rejection — all pass.
- Full M0 regression suite still passes (50/50).

## M0.1 Definition of Done

- Fit images cannot be dragged off-center
- pan clamped correctly on both axes
- pan reclamps after zoom / resize / mode change / navigation
- decoded-memory estimate uses overflow-safe arithmetic
- oversized images rejected before dangerous allocation
- ordinary modern photos remain supported
- no new dependencies, no new product features
- all previous M0 behavior still passes (scripted harness)
- clean build with zero warnings
- commit pushed to origin/main

## Remaining known limitations (unchanged from M0 unless noted)

- Decode is synchronous on the UI thread (M1 moves decode off-thread).
- No preload/cache (M1 scope).
- Arrows pressed before directory enumeration completes are applied when the
  scan finishes.
- WebP only via an installed system WIC codec (none on Windows 10 22H2).
- HEIC/HEIF/AVIF/GIF/RAW out of scope; failure state.
- Corrupt files may partially decode via WIC; never crashes.
- Normal-window geometry persists via a tiny registry key.
- Timing instrumentation only when FAST_VIEWER_TIMING is set; no telemetry.

M1 is NOT authorized. Stop and await human review.
