---
type: session
project: QG
created: 2026-10-26
updated: 2026-10-26
status: complete
aliases:
  - Fix-AUD-008-Thumbnail-Cache
tags:
  - session
  - thumbnails
  - cache
  - sqlite
  - aud-008
source:
  - proj-audit.md §AUD-008 (FIX-TH-01)
  - DESIGN.md §18.2, §18.4, §18.5, §22.1, §22.3, §22.6
  - src/controllers/thumbnail_controller.{hpp,cpp}
  - src/media/thumbnails.{hpp,cpp}
  - src/persistence/{database.hpp,repositories.cpp}
  - src/media/probe.cpp (cache-first precedent)
  - tests/unit/{test_thumb_wiring,test_thumb_delivery,test_thumb_failure,test_card_actions}.cpp
  - g++ -fsyntax-only control pair (2026-10-26, static only)
---

# 2026-10-26 — AUD-008: the thumbnail cache is served, not just written

## Objective

Resolve AUD-008 ("The thumbnail cache is write-only; failure backoff is dead code")
in the checked-out tree, then deliver **IMPLEMENTED + STATICALLY REVIEWED** only: no
build, no CTest/Qt Quick Test run, no launch, no screenshot, no test created or
modified. Every check below is static; the runtime checks are recorded in
`DEFERRED_TESTS.md`.

## Verified root cause

**CONFIRMED, observed — the audit's citations reproduce, and the "no production
callers" claim was re-checked by grep.**

1. `ThumbnailController::runExtraction()` (HEAD `thumbnail_controller.cpp:238-259`)
   consulted **only** the freedesktop cache and then extracted; nothing read QG's own
   `thumbnails` row or PNG.
2. The write half exists and is reached on every success:
   `publishThumbnail()` → `ThumbnailRepository::upsertOk(identity, mtimeNs, size, path)`
   (`thumbnail_controller.cpp:192-227`); the failure half writes via `upsertFailed`.
3. `ThumbnailRepository::lookup()` (`repositories.cpp:494`) and `decideThumbnail()`
   (`thumbnails.cpp:424`) existed with **no production caller**; `hasDecoded()`/`decoded()`
   (`thumbnails.cpp:415-420`) likewise. Confirmed by grep over `src/` + `tests/`
   (tests only).
4. Why every visit paid for a decode: a new folder replaces the rows
   (`FolderModel::applySnapshot()` → `m_thumbnails.clear()` + `m_thumbnailStates.clear()`),
   so `requestVisible()` sees `ThumbnailState::None` for every visible row again
   (`thumbnail_controller.cpp:109-111` skips only *settled* rows) and enqueues them all.
   `ThumbnailScheduler::clear()` (called from `beginGeneration()`) drops the pending
   queue and viewport — note it does **not** drop the decoded LRU, but nothing reads that
   LRU, so it cannot help.
5. §18.5's terminal marker was therefore never consulted: a corrupt file was re-extracted
   on *every* navigation, forever, while its DB `fail_count` grew.

Specification: §18.2 ("Cache lookup → hit, valid → UI … **Serve** precedes **Extract**"),
§18.4, §22.3, §22.6, §18.5 ("up to 2 retries, then the marker is terminal until the
source changes").

## Changes

| File | Change |
|------|--------|
| `src/media/thumbnails.hpp` | `ThumbnailExtraction::CacheOrigin { Decoded, QgCache, FreedesktopCache }` + `fromCache()`; `ThumbnailRequest::manualRetry` and `ThumbnailRequest::cachedPngPath`; named `kMaxThumbnailFailRetries = 2`, used as `decideThumbnail()`'s default so the controller's call site and the policy cannot drift |
| `src/controllers/thumbnail_controller.hpp` | private `enum class CacheDecision { Extract, Serve, Terminal }`; `consultQgCache(request, QString *cachePath)` (documented **GUI thread only**); `terminalResult(request)`; `bool m_resolving` |
| `src/controllers/thumbnail_controller.cpp` | `consultQgCache()` runs the §18.5 policy against the persisted row and decides Serve/Terminal/Extract; `runExtraction()` (media pool) now reads the resolved QG PNG first, then the freedesktop cache, then the extractor, and **never touches the database**; `pumpAsync()` consults the cache on the GUI thread before dispatching and completes Serve/Terminal requests without pool work (loop, not recursion); `processAllForTesting()` follows the same order; `applyResult()` re-uses the existing PNG instead of re-encoding when the pixels came from that very file (`cacheOrigin == QgCache`); `retry()` marks its request `manualRetry` |

### Deviations from the supplied patch (three, all required)

**(1) `failed.error = QStringLiteral(…)` does not compile.** `Error` is
`{ ErrorCode code; QString message; QString path; int systemError; }` (`core/errors.hpp`)
with no assignment from `QString`. Control run (deliberate paste of the audit's line):
`error: no match for 'operator=' (operand types are 'qg::Error' and 'QString')`. Replaced
with the tree's idiom `Error::make(ErrorCode::OperationFailed, …, request.path)`.

**(2) The database call cannot live in `runExtraction()`.** It runs on
`WorkerPools::cpuMedia()` (`pumpAsync`), and `Database` is documented as **one
thread-affine connection** — `persistence/database.hpp:5-7` "open a connection per
thread, never share a connection across threads" — with exactly one instance in the app
(`App::m_database`, `app.hpp:52`, handed to the probe service, state controller and
thumbnail controller). Every existing consumer touches it on the GUI thread; the closest
analogue, `MediaProbeService::pump()` (`media/probe.cpp:191-209`), runs only
`runProbe()` on the pool and does its cache lookup + writes in the **GUI-thread
completion handler** (`process()`). The patch as written would have been the first
cross-thread use of that handle.

Resolution keeps the audit's shape and both invariants: the **DB query** moved to
`consultQgCache()` on the GUI thread (called from `pumpAsync()` before dispatch and from
`processAllForTesting()`), and the **PNG read** stayed where the audit put it — inside
`runExtraction()` on the media pool, fed by `ThumbnailRequest::cachedPngPath`. So
§22.1/INV-004 ("no file I/O on the GUI thread") is not made worse, and §18.2's order (QG
cache → freedesktop → extract) is preserved.

**(3) `publishedFromCache(request)` / `request.fromCache` is impossible.** `runExtraction`
takes `const ThumbnailRequest &` and returns the result by value, so the producer cannot
set a field on the request. The origin lives on the result
(`ThumbnailExtraction::CacheOrigin`) — the audit's stated intent ("the decision in the
extraction result instead of re-querying the DB"), with no extra DB lookup. Both serve
paths set it (the task's "handle both return paths consistently").

**(4) One bool is not enough to decide the write-through, so the origin is an enum.** The
audit's note says to set a `fromCache` bool on the results of *both* cache branches and to
re-use the existing QG file whenever it exists. Checked against
`tests/unit/test_thumb_delivery.cpp`: `m_cacheDir` and `m_fdHome` are created once in
`initTestCase()` and shared by all of that class's test methods, and
`newlyExtractedThumbnailIsVisibleImagery` (declared **before** the fd test) extracts the
same `m_mp4` through FFmpeg into the same cache dir — so by the time
`freedesktopCacheHitSkipsExtraction` runs, a QG PNG for that exact key (identity + mtime +
size) exists on disk. With a bare bool, the fd hit would re-use that earlier 320×180 file
and the test's final assertion — `QCOMPARE(served.size(), QSize(64, 36))`, "the seeded
image, not a decode" — would **fail**. A freedesktop hit is also a *different* source, so
QG's cache should still be written through with what was served. Hence `CacheOrigin`:
both serve paths set it, and `applyResult()` re-uses the file only for `QgCache`, where the
file *is* the artefact that was read. The AUD-008 goal is unaffected: a warm revisit always
takes the QG-serve branch (the row we just wrote says `ok`), so no re-encode happens
there. Reverting to a single bool is a one-line change if the reviewer prefers it and
accepts the expectation change in that test.

**(3b) Manual retry had to keep working.** `decideThumbnail()` with the default
`manualRetry=false` returns `Terminal` for an exhausted marker, so the audit's call would
have made the card's Retry control a permanent no-op for exactly the rows it exists for,
contradicting §18.5 ("terminal until source changes **or manual retry**"), the class's own
comment (`thumbnail_controller.hpp:51-55`) and `qg_card_actions::thumbnailRetryReExtracts`.
`ThumbnailRequest::manualRetry` is set by `retry()` and passed to the policy, so Retry →
`Retry` → falls through to the extractor. (`qg_card_actions` sets no database, so its
extraction counters are unaffected either way — the flag protects the production path.)

### Behaviour table (derived from the final source — reasoned, not executed)

| Persisted row | Source stamp | Policy | Behaviour |
|---|---|---|---|
| none | — | `Generate` | dispatch decode → publish + `upsertOk` (unchanged) |
| `state=ok` | unchanged | `Serve` | GUI: resolve `cache_path`; pool: `QImage` read → `cacheOrigin=QgCache` → `applyResult` re-uses the file it just read (no re-encode), row → Ready, `thumbnailReady` |
| `state=ok` | mtime/size changed | `Generate` | decode + overwrite cache (stale imagery impossible) |
| `state=ok`, `cache_path` empty or PNG unreadable | unchanged | `Serve` | falls through to freedesktop → extractor → publish through (repairs the row) |
| no row (or DB absent), freedesktop hit | — | — | `cacheOrigin=FreedesktopCache` → the served image is written through to QG's cache as before (this is the path `qg_thumb_delivery` pins) |
| `state=failed`, `fail_count` ≤ 2 | unchanged | `Retry` | decode retried (bounded), `upsertFailed` increments |
| `state=failed`, `fail_count` > 2 | unchanged | `Terminal` | **no decode**: `applyResult` marks the row Failed (amber placeholder) + `thumbnailFailed`, renewed per visit; no pool work queued |
| `state=failed`, any count | changed | `Retry` | decode retried (§18.5 "until the source changes") |
| `state=failed`, manual retry | unchanged | `Retry` (manualRetry) | decode retried — the user's escape hatch |

Note on the retry arithmetic: `1 + kMaxThumbnailFailRetries` = **three** failed decodes
before the marker is terminal (confirmed by `qg_thumb_failure.cpp:63-68`), so the audit's
"after two failed attempts" is phrased one step short of the shipped policy. The deferred
check below states it as the policy defines it.

## Static verification (executed this session)

| Check | Instrument | Result |
|---|---|---|
| Both translation units compile | `g++ -std=c++20 -fsyntax-only` (`src/media/thumbnails.cpp`, `src/controllers/thumbnail_controller.cpp`) with the private Qt 6.10.2 prefix | **exit 0** |
| …control: the audit's `error` assignment | same command on a copy carrying it verbatim | **exit 1** — `no match for 'operator=' (qg::Error, QString)` |
| …control: the new decision call | copy with `consultQgCache` → `consultQgCacheX` | **exit 1** — not declared (so both call sites are type-checked) |
| …control: retry flag | copy with `request.manualRetry` → `manualRetryX` | **exit 1** |
| …control: cache-origin flag | copy with `extraction.cacheOrigin` → `cacheOriginX` | **exit 1** |
| …control: served-path field | copy with `request.cachedPngPath` → `cachedPngPathX` | **exit 1** |
| No DB access on the pool | grep inside `runExtraction()` | **none** (only the freedesktop reader + extractor) |
| Production wiring | `src/core/composition.cpp:103-106` sets model, **database**, `qgCacheDir`, freedesktop home | the new branch is live in production, and `m_cacheDir` is the same root `publishThumbnail()` writes to, so `qgThumbnailPath(m_cacheDir, key)` reproduces the stored `cache_path` |
| Existing-test compatibility (read, not run) | no thumbnail test calls `setDatabase()` (`test_thumb_wiring`, `test_thumb_failure`, `test_thumb_scheduler`, and `test_thumb_delivery` — its `m_cacheDir`/`m_fdHome` are set, never a database; the three `setDatabase` calls in `test_card_actions` are on `StateController`) | the new branch is inert in all of them, so extraction counts and delivery are unchanged; the fd-hit test's delivered-image assertion survives precisely because the write-through reuse is keyed on the QG origin (deviation 4), not on `fromCache` |
| `tests/` untouched | `git diff --stat -- tests/` | only the pre-existing AUD-001/003/004/009 edits from earlier sessions |

## Deferred verification (recorded in `DEFERRED_TESTS.md`, not executed)

1. Open a folder of ≈200 videos and let generation finish. 2. Navigate away and return.
3. Confirm the cached thumbnails display without opening the videos through FFmpeg.
4. Prove it with an instrumented extractor counter or `strace -f -e openat` on media
files. 5. Confirm the second visit paints promptly. 6. Use a corrupt file to verify the
bounded retry policy. 7. Confirm that once the marker is terminal
(`fail_count > kMaxThumbnailFailRetries`, i.e. after the third failed decode) the amber
placeholder appears with **no** further decode across subsequent navigations. 8. Confirm a
source modification invalidates the previous failure marker and permits regeneration.
9. Review the affected wiring/failure/scheduler tests for outdated extraction-count
expectations (`qg_thumb_wiring`, `qg_thumb_failure`, `qg_thumb_scheduler`, `qg_card_actions`).
Added: 10. confirm the card's Retry control still re-extracts a terminal row (manualRetry).

## Dependencies / interactions

- **AUD-002 (FIX-WK-01) is satisfied** — the route map marks it as this item's
  dependency and the fix is in the tree (`src/workers/tasks.cpp`: process-lifetime
  GUI-thread delivery hub + `QPointer` guard built at submission), registered
  "Executed before the policy" with `qg_workers` 17/17 and an ASan control (3/3 pre-fix
  SEGV). No AUD-002 work was performed here.
- AUD-003/AUD-004/AUD-006/AUD-007 work in the same working tree is preserved untouched;
  `FolderModel::applySnapshot`/`indexOfIdentity` and `state_controller` are unaffected by
  this change (the metadata restore path emits `dataChanged` only, which does not touch
  this pipeline).
- `FIX-TH-02` (encode + write the PNG on the media pool) remains the follow-up: on an
  extraction (non-served) path `publishThumbnail()` still encodes synchronously on the
  GUI thread.

## Remaining risks / not proven here

- **Nothing was built or run.** Every behavioural statement above is read from source.
- **One indexed SELECT per requested row now happens on the GUI thread** (the cache
  consultation). The probe service already does two (`pathInfo`, `probeResult`) in its
  GUI-thread completion handler and WAL mode is on, so this matches existing practice; if
  the batch phase measures a stall on a 200-row viewport, the follow-up is a batched
  lookup (or a second connection dedicated to the pool thread, which the DB contract
  explicitly allows).
- **The in-memory LRU is still write-only**: `storeDecoded()` is called (also for served
  hits) but `hasDecoded()`/`decoded()` remain unconsumed, and `ThumbnailScheduler::clear()`
  keeps the LRU across navigation — so a RAM-served fast path is available but unwired
  (THUMB-008). Out of this item's scope; recorded in the register.
- **A terminal row is re-marked on every visit** (`upsertFailed` → `fail_count + 1`): no
  decode is dispatched, but the DB write and the `thumbnailFailed` emission repeat. The
  audit's patch leaves the failure branch untouched; a "terminal, do not re-persist" path
  is a small follow-up.
- **Served hits still call `upsertOk()`** (idempotent rewrite of the same row) — kept
  from the patch, and the obvious cheap win if the batch phase shows write pressure.
- The `folderPath`-style path identity question does not arise here: the cache key is
  `(identity, mtime_ns, size)` and validation compares the persisted stamp with the
  request's, so a moved/renamed source misses the key rather than serving stale pixels.
