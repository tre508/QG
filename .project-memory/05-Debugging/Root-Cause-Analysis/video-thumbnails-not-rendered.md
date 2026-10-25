---
type: root-cause-analysis
project: QG
created: 2026-10-22
updated: 2026-10-22
status: resolved
aliases:
  - VIS-010
tags:
  - thumbnails
  - qml
  - image
  - delivery
  - bug
source:
  - FIX-VIS-010
  - src/controllers/thumbnail_controller.cpp
---

# Root Cause: Video Thumbnails Never Rendered (VIS-010)

Related: [[Current-State]] · [[Known-Issues]] · [[Decision-Index]] ·
[[worker-pool-receiver-use-after-free]]

## Symptom

Valid, probed video rows showed no frame imagery. Cards had a blank, uniformly
tinted thumbnail well even though thumbnails were being extracted successfully
and the database recorded `state = ok`.

## Diagnosis method

Instrumented the whole chain with `tests/unit/test_thumb_delivery.cpp`
(`diagnoseDataToVisualChain`) on a real FFmpeg-generated 320x180 H.264 clip and
logged every hop: source identity/size/mtime, model role type, controller
signals, cache path existence, decodability, and what `Image.source` would
receive.

## The break

     role(after) : type=QImage  state=2   ← Ready, but the value is a QImage
     role->QUrl  : valid=0 url=           ← QImage does not convert to QUrl
     Image.source would be: ''            ← delegate has nothing to paint

`ThumbnailController::applyResult` stored `QVariant::fromValue(image)`, while
every delegate binds `Image.source: card.thumbReady ? card.thumbnail : ""`.
`Image.source` is a QUrl. Because `thumbnailState` was `Ready`, `thumbReady` was
true, so the filmstrip placeholder was hidden too — the card showed a blank well
instead of any indication that something was missing. The extraction, the cache
writing, the DB rows and the model notifications were all correct; only the
representation handed to QML was wrong.

This is why the defect was invisible to the existing tests: they asserted
`ThumbnailRole.isValid()`, which a `QImage` satisfies.

## Two further defects found while instrumenting

1. **Cache never consulted first.** `runExtraction` returned the injected
   extractor's result before trying the freedesktop cache, so a cache hit still
   paid for a decode. Now: cache → extractor → extract.
2. **Folder rows were thumbnail subjects.** `requestVisible` requested a
   thumbnail for every row, including directories. In the real app this produced
   `failed` DB rows with `source_size = 0` for ordinary folders and wasted
   media-pool slots. Now folders are skipped.

## Fix

See [[Decision-Index]] (2026-10-22): thumbnails travel as file URLs;
`publishThumbnail()` writes the PNG (cache dir, else temp fallback) and a publish
failure is reported as `Failed`.

## Verification

- `qg_thumb_delivery` 10/10 with real MP4 **and** MKV fixtures.
- `tst_videocard` +3: `Image.status === Ready`, `paintedWidth > 0`, well fills
  with `PreserveAspectCrop`.
- Real app under Xvfb (1833x1080) with a colour-coded clip (1 s per colour):
  the cached PNG is 320x180 and **100 % red** (the frame at the 10 % seek), and
  the screenshot shows a **214x125 px block that is 98.6 % red** — the painted
  pixels are the extracted source frame.
- Warm run: same thumbnail, no re-extraction of unchanged sources, new URL after
  a source change.

## Lesson

When a model role feeds a QML property, test the **consumer's type contract**, not
just `isValid()`. `Image.source` needs a URL; a variant that "is valid" can still
be unpaintable. Assert `toUrl().isLocalFile()` and that the file decodes.
