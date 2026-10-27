---
type: handoff
project: QG
created: 2026-10-20
updated: 2026-10-27
status: active
tags:
  - session-handoff
  - context-recovery
source:
  - GUIDE_STATE.md
  - DEFERRED_TESTS.md
  - .project-memory vault reconciliation
---

# Session Handoff

## Workflow Mode — READ FIRST

**Logic-first, deferred testing** (policy 2026-10-22, [[Agent-Workflow]]).
Implement → static review → document → record the deferred checks → report. Do
**not** build, run CTest/QML tests, launch the app, capture screenshots, or run
benchmarks unless the user authorizes a batch testing phase. Status words:
`IMPLEMENTED` · `STATICALLY REVIEWED` · `NEEDS_REVISION` · `BLOCKED` ·
`TESTING DEFERRED` · `VERIFIED` (only after real execution). Backlog:
`DEFERRED_TESTS.md` → [[Deferred-Test-Register]].

## Current Objective

**FIX-DB-01 (AUD-017) and the trash-integrity portion of FIX-FO-01 (AUD-018) were implemented and verified on 2026-10-27 after explicit authorization.** The full `build-work` CMake/Ninja build passed; persistence 9/9 includes the SQLite `PRAGMA query_only=ON` failure test, probe wiring 6/6, state controller 7/7, targeted action-feedback trash cases 6/6 (including deterministic `Trash/info` blockage and outside-root refusal), playback lifecycle 5/5 and video surface 6/6. After changing the trash-info failure fixture, `qg_action_feedback_test` was rebuilt and rerun; its focused trash run passed 6/6. Full CTest, GUI-rendered toast, and probe write-failure integration remain unverified. Local no-root Qt/SQLite staging was used; CMake warned about missing QtQuick `qmldir` files and the build emitted unrelated ignored-return warnings. AUD-019 EXDEV fallback and FIX-FO-02 progress/cancellation remain open.

**FIX-DB-01 (AUD-017) and the trash-integrity portion of FIX-FO-01 (AUD-018) were implemented and verified on 2026-10-27 after explicit authorization.** The full `build-work` CMake/Ninja build passed; persistence 9/9 includes the SQLite `PRAGMA query_only=ON` failure test, probe wiring 6/6, state controller 7/7, targeted action-feedback trash cases 6/6 (including deterministic `Trash/info` blockage and outside-root refusal), playback lifecycle 5/5 and video surface 6/6. After changing the trash-info failure fixture, `qg_action_feedback_test` was rebuilt and rerun; its focused trash run passed 6/6. Full CTest, GUI-rendered toast, and probe write-failure integration remain unverified. Local no-root Qt/SQLite staging was used; CMake warned about missing QtQuick `qmldir` files and the build emitted unrelated ignored-return warnings. AUD-019 EXDEV fallback and FIX-FO-02 progress/cancellation remain open.

**FIX-DB-01 (AUD-017) and the trash-integrity portion of FIX-FO-01 (AUD-018) were implemented and verified on 2026-10-27 after explicit authorization.** The full `build-work` CMake/Ninja build passed; persistence 9/9 includes the SQLite `PRAGMA query_only=ON` failure test, probe wiring 6/6, state controller 7/7, targeted action-feedback trash cases 6/6 (including deterministic `Trash/info` blockage and outside-root refusal), playback lifecycle 5/5 and video surface 6/6. After changing the trash-info failure fixture, `qg_action_feedback_test` was rebuilt and rerun; its focused trash run passed 6/6. Full CTest, GUI-rendered toast, and probe write-failure integration remain unverified. Local no-root Qt/SQLite staging was used; CMake warned about missing QtQuick `qmldir` files and the build emitted unrelated ignored-return warnings. AUD-019 EXDEV fallback and FIX-FO-02 progress/cancellation remain open.



**FIX-STAGE-01 (AUD-010 half) is IMPLEMENTED + STATICALLY REVIEWED as of 2026-10-27, NOT runtime-verified.** Cinema now enters real fullscreen: `qml/QG/Main.qml` carries `property int windowedVisibility: Window.Windowed` and its `onCinemaActiveChanged` handler remembers `window.visibility` (unless already `FullScreen`), sets `Window.FullScreen`, focuses the Stage and wakes the transport overlays (§4.2/§4.4), then on exit restores the remembered state — never a stale fullscreen — and focuses the content view (§4.5). The existing window is used throughout (no second window), no playback call was added (§4.2.6/§25.7 continuity holds by construction), and the Cinema layout, Stage sizing, cursor auto-hide, Reel restore and the AUD-009 `reservedTop` inset are untouched. `import QtQuick.Window` was added so the `Window.*` enum is declared rather than inherited transitively. Evidence is **static only**: `qmllint 6.10.2` on the finished file vs a pre-fix control (the change reverse-applied, siblings in place) gives 141 vs 139 diagnostics whose only additions are two `wakeControls not found on "Stage"` warnings that the pre-fix file already emits twice (the `qg` module has no `qmldir`, so local component types never resolve); no `Window`/`visibility` diagnostic appears. **Deferred:** the whole §25.7 X11 behaviour (F11 removes decorations and fills the screen, Esc/exit restores the exact geometry, double-click toggles, playback does not restart or re-buffer, pointer + overlays hide after 2 s), the offscreen `window.visibility` **property** assertion (`Window.FullScreen` is a no-op with `QT_QPA_PLATFORM=offscreen`), maximized → Cinema → exit, re-entry, and Stage focus surviving a WM round-trip — `DEFERRED_TESTS.md` FIX-STAGE-01. See [[2026-10-27-fix-aud-010-cinema-fullscreen]].

**FIX-TH-01 (AUD-008) is IMPLEMENTED + STATICALLY REVIEWED as of 2026-10-26, NOT
runtime-verified.** The thumbnail pipeline now *serves* its own cache instead of only
writing it: `consultQgCache()` (GUI thread, because `Database` is a single thread-affine
connection) reads the persisted `thumbnails` row, validates it against the current
`(mtime_ns, size)` through `decideThumbnail()` and yields Serve / Terminal / Extract; the
media pool reads the resolved PNG (never re-encoded on a served hit), a terminal failure
marker is reported without any decode, and a manual Retry still reaches the extractor.
Static evidence: `g++ -std=c++20 -fsyntax-only` exit 0 on both translation units with five
red controls (one of which proves the audit's `failed.error = QStringLiteral(…)` line does
not compile). **Deferred:** the 200-video warm-start proof (second visit must not open the
videos through FFmpeg — extractor counter or `strace`), the bounded-retry/terminal proof,
the Retry-still-works check and the touched-suite re-runs — `DEFERRED_TESTS.md`
FIX-TH-01. See [[2026-10-26-fix-aud-008-thumbnail-cache]].

**FIX-PC-01 (AUD-007 half) is IMPLEMENTED + STATICALLY REVIEWED as of 2026-10-26, NOT
runtime-verified.** Playback no longer ends when you browse away: `FolderModel` now
publishes `folderPath`/`folderPathChanged` (from `FolderSnapshot::path`, emitted inside
the model reset), and `PlaybackController::onModelStructureChanged()` only treats the
playing file as removed when the displayed model still represents the session's
containing directory (`QFileInfo(session->path()).absolutePath()`) **and** the identity
is absent. A live session on another folder also leaves the ordered context and the Reel
untouched, so the Reel keeps the playing folder's list (a deliberate deviation from the
audit patch, which rebuilt it from the browsed folder — see the session note). Static
evidence: `g++ -std=c++20 -fsyntax-only` exit 0 on both translation units with red
controls (deliberate renames → exit 1). **Deferred:** navigate away/back with the Stage
open, Reel retention + re-anchor, genuine deletion still stopping playback, and the
`qg_reel_wiring` / `qg_playback_lifecycle` re-runs — `DEFERRED_TESTS.md` FIX-NAV-01.
See [[2026-10-26-fix-aud-007-playback-navigation]].

**FIX-SRCH-01 (AUD-005) is IMPLEMENTED + STATICALLY REVIEWED as of 2026-10-26, NOT
runtime-verified.** The Manifest intensity is now *reachable*: `SearchController`
publishes `active`/`summary`/`resultCount` as notifying properties (`setActive()`
writes the member before the notification and de-duplicates; `clear()` re-notifies
`resultsChanged` so a bound summary cannot go stale after Esc), `setFolderName` /
`setSortLabel` are `Q_INVOKABLE`, and the shell reads the summary and applied query
from the properties instead of signal arguments. Evidence is **static only**:
`g++ -std=c++20 -fsyntax-only` exit 0 with a deliberately-broken control that fails
(exit 1), and `qmllint 6.10.2` diagnostic sets identical to the pre-fix worktree
(128 PathBar / 74 ContentView / 139 Main). No build, no CTest, no QML run, no launch.
See [[2026-10-26-fix-aud-005-search-manifest]] and `DEFERRED_TESTS.md` FIX-SRCH-01.

**FIX-AUD-006 (AUD-006) is IMPLEMENTED + STATICALLY REVIEWED as of 2026-10-26, NOT
runtime-verified.** `StateController::loadIntoModel()` ran once in
`CoreServices::wire()` (`src/core/composition.cpp:98`) before the first `navigate()`
populated the model, so the decoration loop never executed; later `applySnapshot`
→ `modelReset` cycles replaced rows without re-applying state. Fix: `setModel()`
subscribes to `modelReset`/`rowsInserted`, clears `m_applied` on reset, and
`scheduleLoad()` decorates new rows exactly once per identity per generation on a
coalesced zero-delay `QTimer`; `loadIntoModel()` preserved for tests. Static
evidence: root cause re-anchored at `composition.cpp:98`, guard + timer logic
reviewed against the current tree. **Deferred:** first-load/navigation/refresh
restore, restart persistence, first-paint on a 5,000-entry folder — recorded in
`DEFERRED_TESTS.md` FIX-AUD-006. See [[2026-10-26-fix-aud-006-persisted-state]].

**Three things must be settled by the batch phase before this is called done**, all
recorded in the register: (1) **focus ownership** — `ContentView.qml:867`
(`focus: contentView.manifest`) may pull active focus out of the PathBar search field
the moment results appear, which would break §7.1 incremental typing and §7.2's
first-Esc-clears contract (inferred from the focus-scope model, never observed);
(2) **§7.3 row focus** — `manifestList` has no `Keys` contract, so ↑/↓ and Enter on a
Manifest row do nothing to `selFocusIndex` (a separate finding, deliberately not
implemented here); (3) **AUD-023** — `enterManifest()` clears the selection and
`exitManifest()` restores only `contentY`, so §25.4's "restores … selection" is not
met and is now user-visible for the first time.

**FIX-SEL-01 (AUD-004) is COMPLETE as of 2026-10-25** — `SelectionState` now exposes
selection/focus as notifying properties plus a `selectionRevision`; ContentView owns
the identity lists for range/select-all and the delegate bindings read the properties;
`Main.qml` reads them through accessors. Verified by a red-on-HEAD C++ control probe,
a rendered-pixel probe (9/9) and the full suites (CTest 48/49, `qg_qml` 200/8 = the
pre-fix failure set). Regression risk to keep in mind: **no repository test covers it**
— the instruments are scratch harnesses, so promoting them into `tests/` is the
natural next step ([[2026-10-25-fix-aud-004-selection-focus]]). Note that the test
files were deliberately left untouched, so `tests/unit/test_selection_state.cpp`
still does not execute (no `QTEST_MAIN`) and still encodes the old `clear()` contract.

Also landed: **FIX-VIS-013/014** (density presentation) is IMPLEMENTED / STATICALLY
REVIEWED ([[Fix-VIS-013-014-Density]]), and **FIX-UI-008** (cross-state visual audit)
is BLOCKED by the deferred-testing policy because its deliverable is screenshots
([[Blocked-UI008-Audit]]). **AUD-009's reflow half is COMPLETE** (same day): the
inline Stage reserves `ContentView.reservedTop` and the grid reflows beneath it —
harness 6/6, GTest/QML suites unchanged ([[2026-10-25-fix-aud-009-stage-reflow]]) — **re-verified 2026-10-26** against the FIX-STAGE-01 prompt: every required line is already in the tree (`ContentView.qml:124/128/581/683/727`, `Main.qml:622-623/777-783`, reporter at `ContentView.qml:312-326`), no code change was needed, and the acceptance harness is still only in `~/.hermes/cache/scratch/aud009/` (no in-tree gate);
the remaining FIX-STAGE-01 items are Cinema fullscreen (AUD-010), Esc semantics
(AUD-011) and the entry fade (AUD-030). Next roadmap item per the audit:
**FIX-SRCH-01** (AUD-005).

> **Workspace hazard seen this session:** at 15:19:57 the whole `tests/` tree was
> moved to the freedesktop Trash by something outside the agent session, which broke
> `cmake` configure until it was restored from
> `/home/gayzer/.local/share/Trash/files/tests`. If a build suddenly reports
> *"add_subdirectory given source 'tests' which is not an existing directory"*,
> check the Trash before touching the sources.

## Last Completed Tasks

0. **FIX-STAGE-01 (AUD-010 half)** — Cinema enters real fullscreen and restores the previous window state (static only, checks deferred) — [[2026-10-27-fix-aud-010-cinema-fullscreen]]
1. **FIX-TH-01 (AUD-008)** — the thumbnail cache is served, not just written; terminal
   failures stop re-decoding (static only, checks deferred) —
   [[2026-10-26-fix-aud-008-thumbnail-cache]]
1. **FIX-PC-01 (AUD-007 half)** — playback survives navigation; the Reel keeps the playing
   folder's context (static only, checks deferred) —
   [[2026-10-26-fix-aud-007-playback-navigation]]
2. **FIX-AUD-006 (AUD-006)** — persisted favorite/watched/progress are re-applied when
   the model is populated/navigated (static only) —
   [[2026-10-26-fix-aud-006-persisted-state]]
3. **FIX-SRCH-01 (AUD-005)** — the Manifest intensity is reachable (static only,
   checks deferred) — [[2026-10-26-fix-aud-005-search-manifest]]
1. **FIX-VIS-013/014** — Density + context presentation — [[Fix-VIS-013-014-Density]]
2. **FIX-VIS-011/012** — Card placeholder cues + fixed badge zones — [[Fix-VIS-011-012-Card-Zones]]
2. **FIX-VIS-010** — Video thumbnail imagery (VIS-010) — [[Fix-VIS-010-Thumbnail-Imagery]]
2. **FIX-UI-004** — Standard grid geometry (VIS-008/009) — [[Fix-UI-004-Grid-Geometry]]
3. **FIX-UI-003** — Sidebar icons, device classification & mount states (VIS-005/006/007)
4. **FIX-UI-002** — Path Bar navigation, breadcrumbs & actions (VIS-002/003/004)
5. **FIX-UI-001** — Responsive shell geometry (VIS-001)

## Active Task

**FIX-STAGE-01 (AUD-010 half) — IMPLEMENTED / STATICALLY REVIEWED / TESTING DEFERRED.**
No task is in flight beyond it. The five preceding fixes (FIX-SRCH-01, FIX-AUD-006, FIX-PC-01,
FIX-TH-01 and AUD-009's reflow half) remain statically reviewed and unverified. The next task
comes from the user; implementation only until a testing phase is authorized.

### AUD-010 outcome (for context)

- Root cause: the Cinema layout half was implemented (`!window.cinema` bindings + Stage filling the
  region) but nothing changed the window state — no `Window.*`/`visibility` reference existed anywhere
  in `qml/`, and no file even imported `QtQuick.Window`.
- Fix: `windowedVisibility` + a rewritten `onCinemaActiveChanged` (remember → `Window.FullScreen` →
  Stage focus → wake overlays; on exit restore the remembered state → content-view focus).
- Not done: AUD-011 (Esc semantics) and AUD-028 (window geometry persistence) stay separate, per the
  work item. Wayland window-state behaviour is untested.

### FIX-VIS-010 outcome (for context)

- Root cause: `ThumbnailRole` carried a `QImage` while QML binds it to
  `Image.source` (a `QUrl`) — empty source on `Ready` rows with the placeholder
  hidden, so the well was blank. Now published as `file://` URLs. See
  [[video-thumbnails-not-rendered]].
- Also fixed: freedesktop cache consulted before the extractor; folder rows are no
  longer thumbnail subjects (they wrote bogus `failed` DB rows with `size 0`).
- FIX-UI-004 (grid) remains as recorded: 1593 px workspace → 6 × 244 px, gutters
  24.5/24.5, see [[standard-grid-seven-columns]] / DEC-044.

## Files Modified (this session)

- `qml/QG/PathBar.qml` — density icon/label derived from the active mode, tooltip + a11y
- `qml/QG/strips/ContextStrip.qml` — named density options, active chip, 24 px sizing
- `qml/QG/delegates/VideoCard.qml` — exclusive centre state cue, fixed badge slots,
  22 px TR hover chip, compact Retry, a11y names, meta line anchored to the filename
- `qml/QG/Theme.qml` — `chipScrim` token
- `src/controllers/thumbnail_controller.cpp` — URL publishing, cache-first order,
  folder-row skip
- `qml/QG/delegates/VideoCard.qml` — `objectName: "thumbImage"` (no behaviour change)
- `tests/unit/test_thumb_delivery.cpp` (new), `tests/CMakeLists.txt`,
  `tests/qml/tst_videocard.qml`
- `GUIDE_STATE.md`, `REQUIREMENTS_MATRIX.md`, `DECISIONS.md`, `DEFERRED_TESTS.md`
- Vault: [[Agent-Workflow]], [[Test-Results]], [[Deferred-Test-Register]],
  [[Project-Dashboard]], [[Decision-Index]] (DEC-045), [[Current-State]],
  [[Known-Issues]], [[video-thumbnails-not-rendered]],
  [[Fix-VIS-010-Thumbnail-Imagery]]

## Verification

**Executed before the policy took effect (2026-10-22):**
`qg_thumb_delivery` 10/10 with real MP4+MKV · full QML 203 passed / 4 known
failures · full CTest **47/48 twice** (172.3 s, 181.5 s; only `qg_qml`) · Xvfb real
app: cached frame 320x180 100 % red, painted block 214x125 px at 98.6 % red.
FIX-UI-004 earlier: `tst_grid_geometry` 13/13 at seven widths.

**Static (2026-10-22, FIX-VIS-013/014):** `qmllint` — no new errors (pre-existing
Repeater `Unqualified access` warnings only); no remaining `name: "density"` use.

**Static (2026-10-22, FIX-VIS-011/012):** `qmllint` — no new errors (two `chipScrim`
warnings are artifacts of the stale `build/qml/Theme.qml` copy); zone geometry
checked arithmetically at the 168 px minimum card width.

**Deferred:** nothing has been run since the policy was adopted. Cold/warm cache,
rapid navigation, the seven-width visual sweep and hardware eject validation are
all in [[Deferred-Test-Register]].

## Outstanding Issues

1. **✅ Worker-pool use-after-free (pre-existing) — RESOLVED 2026-10-25** —
   `submitTask` delivered via a raw `QObject *receiver` from a pool thread
   (~4 % of `qg_keyboard_test` teardown runs crashed in `QObject::thread()`).
   Delivery now goes through a process-lifetime GUI-thread hub with a `QPointer`
   guard built at submission and re-checked on the GUI thread. Pre-fix ASan
   control SEGV 3/3; post-fix 0 crashes in 200 plain + 125 ASan runs. See
   [[worker-pool-receiver-use-after-free]], [[2026-10-25-fix-worker-delivery-uaf]]
2. **✅ AUD-001 seek memory-safety/reliability — RESOLVED 2026-10-25** —
   `seek()`/`seekAbsolute()` stored a `QByteArray` temporary's `constData()` in a
   `const char *args[]` read one statement later (ASan-confirmed heap-use-after-free),
   dispatched synchronously, and had no rapid-seek consolidation. Now: named
   argument buffer + `mpv_command_async`, manager-owned seekability policy with
   `seekRejected()` wired to the transport bar, and one-command-per-window
   consolidation with the newest target winning. 15 new policy tests; 10/10 real
   media seek runs; ASan+UBSan clean. See [[2026-10-25-fix-aud-001-seek-memory-safety]]
3. **✅ AUD-003 file operations on the GUI thread — RESOLVED 2026-10-25 (async
   half)** — the mutating entry points ran their whole walk/copy/delete inside the
   `Q_INVOKABLE` call (measured **818 ms** of GUI-thread block for a 3 MB /
   400-file copy; 6.0 s for 24 MB / 3,000 files) and the result signal re-entered
   QML before the call returned. They now submit to `WorkerPools::io()` through
   `submitTask` and deliver on the GUI thread after the event loop turns. Pre-fix
   control red on 4/4 discriminating cases; `qg_file_ops_async` 8/8 and 25/25
   repeats, ASan+UBSan clean. Progress/cancellation (§8.1) and the §8.2 partial
   summary remain open under FIX-FO-02, as does the `EXDEV` fallback (FIX-FO-01).
   See [[2026-10-25-fix-aud-003-file-ops-async]]
4. **Warm start re-extracts thumbnails** — the QG PNG+DB cache is write-only
   (`decideThumbnail()` / `ThumbnailRepository::lookup()` have no callers)
5. **FIX-DND-003..006 BLOCKED** — need Wayland GUI automation or a human operator
6. **.deb packaging** unvalidated
7. **Probe-cache invalidation** on source change uncovered (THUMB-007 half)
8. **Real eject/unmount** unexecuted (FIX-UI-003 verified capability only; no
   disposable device)
9. **Pre-existing**: `ContentView` reads/assigns an undeclared `focusIndex` in the
   legacy no-`selectionState` branches (dead in production)
10. **Pre-existing, host-flaky**: `qg_watcher::modelDiffPreservesUnaffectedRows`
    and `qg_video_surface_offset::decodedFramesRenderAtItemRect` (both re-confirmed
    on pre-change binaries)
11. **Search focus ownership (AUD-005 batch check)** — `ContentView.qml:867`
    (`focus: contentView.manifest`) may take active focus from the PathBar field as
    soon as results appear, which would break §7.1 incremental typing and §7.2's
    first-Esc-clears. Inferred from the focus-scope model; never observed.
12. **§7.3 Manifest row focus missing** — `manifestList` has no `Keys` contract, so
    ↑/↓ and Enter do not move `selFocusIndex`; the ListView's own cursor moves a
    highlight no binding reads. Separate item (not AUD-005).
13. **AUD-023 / FIX-SRCH-02** — `enterManifest()` clears the selection and
    `exitManifest()` restores only `contentY`, so §25.4 "restores … selection" fails;
    now user-visible because the Manifest is reachable.
14. **FIX-NAV-01 batch checks (AUD-007)** — playback must survive navigating three
    folders away (Stage open, Reel keeping the playing folder's rows, re-anchor on
    return) while an external deletion of the playing file *still* stops it;
    `qg_reel_wiring::removalNeverLeavesAStaleIndexOrCrashes` and `qg_playback_lifecycle`
    re-runs are the smallest gate.
15. **Reel header vs playing folder (new with FIX-NAV-01)** — `ReelPanel.folderName` /
    `sortLabel` are bound to `browserController` (`Main.qml:847-852`), so while the Reel
    keeps the playing folder's rows after navigating away, its header names the browsed
    folder.
16. **Path-equality assumption (new with FIX-PC-01)** — the session folder is compared
    as a string against `folderPath`; a relative navigated path or a symlink alias of
    the session's folder compares unequal, which *skips* removal detection (never a
    false stop). Canonicalising would require filesystem access on the GUI thread.
17. **FIX-TH-01 checks (AUD-008)** — the warm-start proof (second visit to a 200-video
    folder must not open the videos through FFmpeg), the bounded-retry/terminal proof,
    the Retry-still-re-extracts check, and the touched thumbnail/card-action suite
    re-runs. Smallest gate: `qg_thumb_wiring`, `qg_thumb_delivery`,
    `qg_thumb_failure`, `qg_thumb_scheduler`, `qg_card_actions`.
18. **Thumbnail GUI-thread DB read (new with FIX-TH-01)** — one indexed `SELECT` per
    requested row is now issued on the GUI thread (matching the probe service's
    cache-first step). If the batch phase measures a stall on a large viewport, batch
    the lookup or give the pool thread its own connection (`database.hpp` allows a
    connection per thread).
19. **Thumbnail follow-ups still open** — `publishThumbnail()` encodes the PNG on the
    GUI thread for fresh extractions (FIX-TH-02), the decoded LRU is populated but never
    read (`hasDecoded()`/`decoded()` unused, and `ThumbnailScheduler::clear()` keeps it),
    and a terminal row is re-persisted (`upsertFailed`) on every visit, so its
    `fail_count` keeps growing without any decode.

## Next Recommended Action

**Either authorize the batch testing phase and run `DEFERRED_TESTS.md` §4** (the
FIX-SRCH-01 row's checks first — the search surface has never been executed — and, for the
window work, a **real X11 session**, since fullscreen cannot be proven offscreen), **or take the
next audit item**: AUD-011 (Esc in Cinema stops playback instead of returning to the Stage —
the natural companion to what just landed, the audit recommends them together), AUD-023 /
FIX-SRCH-02 (Manifest activation + selection restore), then the §7.3 row-focus contract on
`manifestList`. FIX-AUD-006's,
FIX-PC-01's and FIX-TH-01's deferred checks join the same batch — FIX-TH-01's smallest
gate is `qg_thumb_wiring` + `qg_thumb_delivery` + `qg_thumb_failure` +
`qg_thumb_scheduler` + `qg_card_actions` plus the 200-video warm-start measurement.
Do not promote any of them to VERIFIED — no build, test or launch was run for them.

## Files Modified (2026-10-26, FIX-SRCH-01)

- `src/controllers/search_controller.hpp` / `.cpp` — `active`/`summary`/`resultCount`
  properties, `setActive()`, `resultCount()`, invokable `setFolderName`/`setSortLabel`,
  `clear()` re-notifies `resultsChanged`
- `qml/QG/Main.qml` — `searchIsActive()`/`searchSummaryText()`/`searchAppliedQuery()`,
  rewritten `onActiveChanged`/`onResultsChanged`, `onSearchClearRequested`
- `qml/QG/PathBar.qml` — `clearSearchQuery()` (the ✕ now routes through it)
- `qml/QG/ContentView.qml` — `searchClearRequested()` signal, wired from `NoResults`
- `REQUIREMENTS_MATRIX.md` (INV-002, SEARCH-004/005/006/007), `DEFERRED_TESTS.md`,
  `.project-memory/{04-Development/Known-Issues,06-Testing/Deferred-Test-Register,00-Home/*}`
- `tests/` deliberately untouched; `GUIDE_STATE.md` does not exist in this tree (noted
  as a discrepancy — status recorded here and in the register instead)

## Files Modified (2026-10-26, FIX-AUD-006)

- `src/controllers/state_controller.hpp` — `scheduleLoad()` declaration, `setModel()`
  moved header→cpp, `QSet<QString> m_applied` + `bool m_loadScheduled` members
- `src/controllers/state_controller.cpp` — `#include <QTimer>`, `setModel()` signal
  wiring (`modelReset`/`rowsInserted` → `scheduleLoad()`), `scheduleLoad()` zero-delay
  coalescing, `loadIntoModel()` identity guard
- `DEFERRED_TESTS.md`, `.project-memory/06-Testing/Deferred-Test-Register.md`,
  `.project-memory/04-Development/Known-Issues.md`, `.project-memory/00-Home/*`
  — deferred-test register entries
- `tests/` deliberately untouched

## Files Modified (2026-10-26, FIX-NAV-01 / AUD-007)

- `src/models/folder_model.hpp` / `.cpp` — `folderPath` property + `folderPathChanged`
  signal + `m_folderPath`; `applySnapshot()` maintains it inside the reset; `clear()`
  drops it
- `src/controllers/playback_controller.cpp` — `#include <QFileInfo>`; folder-aware
  `playingGone`; early return that preserves the session's context/Reel while another
  folder is displayed
- `REQUIREMENTS_MATRIX.md` (KEY-001, REEL-001, PLAY-002, PLAY-004), `DEFERRED_TESTS.md`,
  `.project-memory/{04-Development/Known-Issues,06-Testing/Deferred-Test-Register,00-Home/*}`,
  `proj-audit.md` (AUD-007 status note), new session note
- `tests/` deliberately untouched; `GUIDE_STATE.md` still absent from this tree

## Files Modified (2026-10-26, FIX-TH-01 / AUD-008)

- `src/media/thumbnails.hpp` — `ThumbnailExtraction::CacheOrigin` + `fromCache()`,
  `ThumbnailRequest::manualRetry` + `cachedPngPath`, named `kMaxThumbnailFailRetries`
- `src/controllers/thumbnail_controller.hpp` — `enum class CacheDecision`,
  `consultQgCache()` (GUI thread), `terminalResult()`, `m_resolving`
- `src/controllers/thumbnail_controller.cpp` — QG-cache consultation before dispatch,
  QG PNG read in `runExtraction()` (media pool), no re-encode on served hits, no decode
  for a terminal marker, `retry()` marks `manualRetry`
- `REQUIREMENTS_MATRIX.md` (THUMB-002/006/008), `DEFERRED_TESTS.md`,
  `.project-memory/{04-Development/Known-Issues,06-Testing/Deferred-Test-Register,00-Home/*}`,
  `proj-audit.md` (AUD-008 status note), new session note
- `tests/` deliberately untouched; `GUIDE_STATE.md` still absent from this tree

## Discrepancies observed this session

- **`GUIDE_STATE.md` is absent** (not tracked, no history, and the documented project
  root `/home/treshen/Projects/QG` does not exist on this host). The policy vocabulary
  status for FIX-NAV-01 (and FIX-SRCH-01) is therefore recorded in `DEFERRED_TESTS.md`
  + the session notes rather than in that file.
- **`FolderModel::clear()` has no callers** in `src/` or `tests/` — the folderPath
  consistency change there is inert in production (kept so the model's lifecycle stays
  truthful if a caller appears).
- **`ReelPanel.folderName`/`sortLabel` are bound to `browserController`**
  (`Main.qml:847-852`) while the list now keeps the playing folder: after navigating
  away the Reel header names the browsed folder over the playing folder's rows. Newly
  visible because of FIX-NAV-01; not fixed here (AUD-007 scope).
- **`.hermes.md` has no §"Search Rules"** section, although the work item cites one;
  the governing rules used were `.hermes.md` §Implementation Discipline and
  [[Agent-Workflow]].
- **`REQUIREMENTS_MATRIX` SEARCH-004/005/006 said `PLANNED`** while
  `ManifestRow`, the Manifest list/header and the no-results overlay all exist at
  HEAD — rows reconciled in this session.

## Files Modified (2026-10-27, FIX-STAGE-01 / AUD-010)

- `qml/QG/Main.qml` — `property int windowedVisibility: Window.Windowed`; rewritten
  `onCinemaActiveChanged` (remember state → `Window.FullScreen` → Stage focus → `stage.wakeControls()`;
  restore remembered state → content-view focus); `import QtQuick.Window`
- `DEFERRED_TESTS.md`, `.project-memory/{04-Development/Known-Issues,06-Testing/Deferred-Test-Register,00-Home/{Current-State,Project-Dashboard,Session-Handoff}}.md`, `REQUIREMENTS_MATRIX.md` (new CINEMA-006), `proj-audit.md` (AUD-010 executed note + roadmap row)
- Session note: `.project-memory/08-Sessions/2026-10-27-fix-aud-010-cinema-fullscreen.md`
- `tests/` deliberately untouched; no commit (tree left for review)
