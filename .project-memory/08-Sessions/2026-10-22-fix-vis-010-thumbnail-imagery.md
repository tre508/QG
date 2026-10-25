---
type: session
project: QG
created: 2026-10-22
updated: 2026-10-22
status: complete
aliases:
  - Fix-VIS-010-Thumbnail-Imagery
tags:
  - session
  - thumbnails
  - visual-fidelity
source:
  - GUIDE_STATE.md
---

# 2026-10-22 — FIX-VIS-010: video thumbnail imagery

## Objective

Make valid, probed video rows display actual frame imagery without ever blocking
Browse, and keep failed extractions explicit.

## What was wrong

`ThumbnailRole` carried a `QImage` while QML binds it to `Image.source`, which is
a `QUrl`. The conversion failed silently, so every `Ready` row painted nothing
and hid its placeholder. Full analysis: [[video-thumbnails-not-rendered]].

## Changes

| File | Change |
|------|--------|
| `src/controllers/thumbnail_controller.cpp` | `publishThumbnail()` writes the frame (cache dir, else temp fallback) and the role carries `QUrl::fromLocalFile(path)`; freedesktop cache is consulted before the extractor; `requestVisible` skips folder rows; a publish failure becomes `Failed` |
| `qml/QG/delegates/VideoCard.qml` | `objectName: "thumbImage"` for tests (no behavioural change) |
| `tests/unit/test_thumb_delivery.cpp` | **new**, 10 tests, real MP4+MKV fixtures |
| `tests/CMakeLists.txt` | registers `qg_thumb_delivery` |
| `tests/qml/tst_videocard.qml` | +3 tests that the delegate paints a URL source |
| `GUIDE_STATE.md`, `REQUIREMENTS_MATRIX.md`, `DECISIONS.md` | evidence + THUMB-001/002/003/005, THUMB-007, CARD-004 reconciliation |

## Verification

- `qg_thumb_delivery` 10/10; focused thumbnail/model set 8/8.
- Full QML suite 203 passed / 4 pre-existing failures.
- Real app under **Xvfb** with real media: cold cache → PNG written, DB `ok`,
  screenshot paints a 214x125 block that is 98.6 % of the expected colour; warm
  run identical.

## Follow-ups

- Warm-start serving of the QG PNG/DB cache is still unimplemented
  (`decideThumbnail()` / `ThumbnailRepository::lookup()` have no callers), so a
  warm start re-extracts.
- Probe-cache invalidation on source change remains uncovered (THUMB-007).
