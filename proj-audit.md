# QG — Comprehensive Project Audit

**Repository:** `/home/testuser/Apps/QG`
**Authoritative specification:** `/home/testuser/Apps/QG/DESIGN.md` (2,414 lines)
**Audit date:** 2026-10-25
**Audit revision:** working tree as found; **no VCS metadata is present** (`git status` → `fatal: not a git repository`), so this report cannot pin a commit hash. All line references are against the files as they exist on disk at the time of reading.
**Scope:** `src/` + `qml/` (18,629 lines), `CMakeLists.txt`, `src/CMakeLists.txt`, `qml/CMakeLists.txt`, `cmake/Dependencies.cmake`, `tests/CMakeLists.txt`, `packaging/`, `resources/`, plus project memory (`DEFERRED_TESTS.md`, `.project-memory/`).
**Status line:** `AUDIT COMPLETE — STATIC REVIEW ONLY / IMPLEMENTATION NOT STARTED / TESTING DEFERRED`

---

## 1. Executive summary

QG is a real, substantial implementation: ~18.6 kLOC of C++20/QML across a layered architecture that genuinely follows the specification's layer boundaries (§15.1). The build system, the SQLite schema, the enumerated/sorted canonical model, the mpv Render-API surface, and the worker-pool substrate all exist and are recognisably the architecture DESIGN.md describes. This is not a scaffold pretending to be an application.

It is also **not finished**, and several of the defects below are the kind that make the application look broken in normal use while the unit tests stay green. The most important structural observation is that a recurring failure mode runs through the whole tree: **implemented-but-unwired**. Services, models and controllers exist with correct-looking contracts, but the last hop — a `Q_PROPERTY` with `NOTIFY`, a `Q_INVOKABLE`, a signal connection, a cache read, a role in `roleNames()` — is missing. The code compiles, the layer boundaries look right, and the behaviour never happens.

### Finding counts

| Severity | Count | Meaning |
|---|---|---|
| **P0** | 2 | memory-unsafety / use-after-free reachable from normal user actions |
| **P1** | 20 | broken core behaviour or major specification violation |
| **P2** | 8 | incomplete behaviour, significant architecture/maintainability defect |
| **P3** | 5 | conventions, documentation, packaging |
| **Total** | **35** | 33 carry at least one copy-paste patch; the remainder are documentation or decision-blocked |

### Top concerns

1. **Two use-after-free paths.** `PlaybackManager::seek()`/`seekAbsolute()` pass pointers to already-destroyed `QByteArray` temporaries into `mpv_command()` (AUD-001); `submitTask()` dereferences a raw `QObject *receiver` from a pool thread after the receiver may have been destroyed (AUD-002 — already logged in `DEFERRED_TESTS.md` §3, reproduced ~4 % of the time).
2. **The interaction surface does not report its own state.** `SelectionState` exposes `focusIndex()`/`isSelected()`/`selectedCount()` only as invokables with no notification, so selection borders, focus rings, the status strip's selection count and the multi-select context menu are computed once at delegate creation and never update (AUD-004). Ctrl+A and Shift+range are stubs on top of that. **→ FIXED 2026-10-25 (FIX-SEL-01):** selection/focus are notifying properties plus a `selectionRevision`, range/select-all take the view's identity list, and both halves are verified by a control-red C++ probe and a rendered-pixel probe.
3. **Playback is a set of overlays rather than the specified surface.** The Stage is drawn on top of the grid instead of reflowing it (AUD-009 — **reflow fixed 2026-10-25**: the Stage reserves its height and the grid reflows beneath it), Cinema does not enter fullscreen (AUD-010), Esc in Cinema stops playback instead of returning to Stage (AUD-011), and nothing advances to the next file at end-of-file (AUD-012).
4. **Persistence reports success it cannot guarantee.** `Statement::step()` returns `false` for both `SQLITE_DONE` and `SQLITE_ERROR`, so repository write failures are returned as `Result<void>::success()` (AUD-017). Combined with AUD-006 (state loaded once, before the first navigation) the visible symptom is "favorites look saved, then vanish after restart".
5. **The thumbnail pipeline is a single-pass pipeline, not a cache.** QG's PNG/DB cache is written and never read (AUD-008), failure backoff policy is dead code, and each navigation pays FFmpeg for every visible row.

### Maturity assessment

| Area | Assessment |
|---|---|
| Architecture & layering | Good — boundaries are respected; no QML filesystem/persistence logic found |
| Canonical model & sorting | Good core, two contract defects (AUD-022, AUD-024) |
| Playback engine (mpv/GL) | Working approach, unsafe teardown and two P0/P1 defects |
| Playback presentation (Stage/Cinema/Reel) | Partial — presentation state exists, the specified surface behaviour does not |
| Selection / keyboard / accessibility | Partial — model layer stubbed, bindings unwired |
| File operations | Correct rules, wrong execution model (blocking, no progress/cancel, no EXDEV) |
| Persistence | Schema correct, error signalling wrong, reconciliation unused |
| Packaging / desktop integration | Partial — metadata exists, no `.deb` recipe, no real fullscreen |

### Audit limitations (read this before trusting any behavioural claim)

- **Nothing was built, run, launched, tested or benchmarked.** No `cmake`, no `ctest`, no QG launch, no screenshots, no profiling. Every conclusion below is from reading source, so every behavioural consequence is labelled `CONFIRMED` only where control flow proves it, `STRONGLY SUPPORTED` where it rests on one documented framework contract (Qt/mpv/freedesktop), and `REQUIRES RUNTIME VERIFICATION` otherwise.
- **No application source was modified.** No patches were applied, no formatting was changed, no files outside this report were written.
- **`CONFIRMED` means "the code does this", not "the user sees this".** Where the user-visible effect depends on Qt view internals (delegate rebinding, persistent indexes) it is labelled accordingly.
- One prior runtime fact is reused from `DEFERRED_TESTS.md` (the ~4 % `qg_keyboard_test` teardown crash) and is attributed as such, not re-observed.

---

## 2. Specification reconciliation

DESIGN.md is **three documents concatenated**, and this has normative consequences.

| Segment | Lines | Identity |
|---|---|---|
| Product & Technical Design Specification | 1–553 | `# QG — Product & Technical Design Specification` — §§0–7 (visual system, window, browser, cards, search) |
| QG Media Player — Playback and File Interaction Specification | 554–1179 | `# QG Media Player — Playback and File Interaction Specification` — its own §§1–11, plus two unnumbered wireframes |
| Continuation of the product specification | 1180–2414 | Resumes at `## 10. Folder / Playback Context Panel`, then §§11–25, appendices |

### 2.1 Numbering defects (documentation, not behaviour)

- **Section numbers 10 and 11 are used twice with different meanings.** `§10` is both the playback spec's *Implementation Acceptance Criteria* (line 1063) and the product spec's *Folder / Playback Context Panel — the Reel* (line 1180). `§11` is both *Scope and Future Extensions* (line 1107) and *File Actions and Context Menus* (line 1221). Every cross-reference of the form "§10" or "§11" elsewhere in the document is therefore ambiguous.
- **Product-specification §§8 and 9 do not exist.** The document goes §7 (Search Experience, line 475) → playback document → `## 10`. Content that would have been §§8–9 is either inside the playback document or lost. Nothing references "§8" for product behaviour, but several *code comments* cite `DESIGN.md §8.1/§8.2/§8.3` for playback matters — those refer to the playback document's §8 (Bulk File Operations) and are wrong.
- **`§0 How to read this document` promises sections 1–13 product + 14–25 architecture/verification and a QA agent deriving tests from "§25".** §25 exists, so the promise holds accidentally; but the numbering scheme cannot be parsed mechanically.

**Consequence for this audit:** findings cite the section *and* the line range, and disambiguate as "playback spec §2.1" vs "product §2.8".

### 2.2 Normative conflicts inside the specification

| Conflict | Locations | Resolution applied here |
|---|---|---|
| **Bloom cross-fade vs. no transition at all.** Product §2.8: "The **card bloom** (card → Stage) cross-fades the thumbnail into the live surface while the card's geometry animates". Playback §2.1 item 4: "display the video directly in the Stage as soon as the video surface is ready. **No thumbnail-to-video cross-fade, fade-out, fade-in, or additional playback transition is required.**" | DESIGN.md:188 vs :581 | The playback document is the *more specific* statement about the Stage's first frame and is later in the file; per the audit brief it governs. `Stage.qml` implements the fading variant (§2.8), so the Stage fade is reported as a defect (**AUD-030**) and the correct behaviour is an instant swap. Reduced-motion behaviour (§2.8) becomes the universal behaviour, so nothing is lost. |
| **Cinema fullscreen vs. in-window.** Product: "in-window Cinema over a separate fullscreen window … no true multi-monitor fullscreen window in MVP" (§1.5). Playback §4.2: "1. **Enter fullscreen using the application's existing window.**" | :88 vs :695 | Both agree that *no new window* is created; §4.2 additionally requires the existing window to actually go fullscreen. The implementation does neither (AUD-010). |
| **Stage insertion vs. overlay.** Product §3.4: Stage "inserted into the grid's layout flow at the position of the playing item, spanning all columns. Remaining cards reflow below." Playback §2.1 item 7: "Reflow the remaining file cards beneath the expanded Stage." | :239 vs :584 | Unambiguous and doubly stated. The implementation is an overlay (AUD-009). |
| **`Ctrl+R`.** §13.2 binds `Ctrl+R` to both Refresh and Reel toggle, then resolves it in a note (F5 = refresh, Ctrl+R = Reel). | :1340, :1346 | The note is authoritative and the implementation matches. Not a defect. |

### 2.3 Non-C++ idioms in a C++ specification (documentation defects, AUD-034)

DESIGN.md contains Python-isms that cannot be implemented literally:

- §20.2: "`Name` sorting uses **`locale.strxfrm`**-aware comparison" — a Python stdlib call. The C++ implementation correctly uses Qt collation instead (`SortService`), so the text is wrong, not the code.
- §20.4: "`SearchService` exposes `search(spec) -> **Iterator[MediaEntry]**`" — Python typing syntax.
- §21.1: "**SQLite** via the stdlib **`sqlite3`**, WAL mode" — reads as the Python module; §15.5 and the file tree correctly say "SQLite C API".
- §21.1: "Repositories expose intention-revealing methods (`set_favorite(identity, bool)`, …)" — snake_case, inconsistent with the C++ API the document elsewhere specifies.

### 2.4 Precedence where documentation and code disagree

Two project decisions describe the code as it is *not*:

- **DEC-009** (`Decision-Index.md`): "mpv_render_context + Qt Scene Graph | ✅ Implemented | VideoSurfaceItem (**QQuickItem + QSGRenderNode**)". The implementation is a `QQuickFramebufferObject` (`src/playback/video_surface.hpp:19`). The spec explicitly permits this ("Its `QSGRenderNode` (or equivalent scene-graph renderer)", §19.3), and the FBO choice is what fixed `PLAY-VIS-001`. The decision record is stale, not the code — reported as documentation (AUD-034).
- **DEC-016** records `.deb` packaging as "🟡 Partial — metadata exists; build/install unvalidated". Actual state is weaker: there is **no** `packaging/debian/` directory at all, so no `.deb` can be produced from this tree (AUD-035).

---

## 3. Methodology

1. Read `DESIGN.md` in full (2,414 lines), building a requirement inventory keyed to section + line.
2. Inventoried the repository: file tree, LOC per file, build definitions, packaging, tests, project memory.
3. Read every layer myself for the areas the findings rest on: `main/app/config/logging`, `core/*` (types, errors, identity, events, composition), the whole `playback/` stack, `FolderModel`, `SelectionState`, `ActionsController` surface, `workers/tasks`, `Stage.qml`, `Main.qml` (shell, Stage/Reel placement, shortcut map), `ContentView` geometry/keys, `VideoCard` rename, `FolderCard`, `TransportBar`, the build files and packaging metadata.
4. Ran four parallel read-only static passes (QML layer; filesystem/media/workers; persistence/models; controllers) to widen coverage, then **re-verified every P0/P1 claim myself** in the source and dropped or demoted the ones that did not hold (see §3.1).
5. Grouped findings by root cause, not by symptom; one remediation task per root cause; IDs assigned by severity then subsystem.

### 3.1 Claims checked and rejected or demoted

| Candidate claim | Verdict |
|---|---|
| "`decideThumbnail()` off-by-one: `maxRetries+1` retries" | **Rejected.** `failCount` is 1 after the first failure, so `failCount <= 2` yields exactly two retries. Not a defect. |
| "`Payload`/`applySnapshot` leaves stale thumbnails" | **Confirmed** (AUD-008 scope) — kept. |
| "`SearchProxy` custom roles are unreachable from QML" | **Confirmed** — `SearchProxy` overrides no `roleNames()`; kept (AUD-023). |
| "`QQuickFramebufferObject` violates §19.3" | **Rejected.** The spec allows an equivalent scene-graph renderer; the decision record is what is stale. |

---

## 4. Repository inventory and component responsibility map

### 4.1 Build and packaging

| Artifact | State |
|---|---|
| `CMakeLists.txt` | C++20, `qg_warnings` interface target (`-Wall -Wextra -Wpedantic`), sanitizer option, install rules for binary/icons/desktop/AppStream |
| `src/CMakeLists.txt` | `qg_core` (static, non-UI), `qg_playback` (OBJECT: mpv + playback controllers + composition), `qg` executable |
| `qml/CMakeLists.txt` | `qt_add_qml_module(qg_qml URI qg …)` + icon qrc; singletons declared for `Theme`/`Format` |
| `cmake/Dependencies.cmake` | Qt6, libmpv, FFmpeg, SQLite, GIO discovery with feature checks |
| `tests/CMakeLists.txt` | 34 unit targets + QML suite + performance target |
| `packaging/` | `.desktop` + AppStream metainfo only — **no `debian/`** |
| VCS | **None** (`.gitignore` present, no repository) |

### 4.2 Component map (owns → actual → gap)

| Layer | Component | Owns | Gap |
|---|---|---|---|
| Bootstrap | `main.cpp`, `app.cpp` | OpenGL backend selection before first window, XDG dirs, logging, DB, QML engine, dependency injection into the shell | No shutdown hook for playback (§19.7); `sessionActive`/state properties injected as raw pointers onto the root object |
| Wiring | `core/composition.cpp` (`CoreServices`) | Single ownership root for state/thumb/probe/search/actions/reel/selection/keyboard/playback | Wires `loadIntoModel()` once, before any folder exists (AUD-006) |
| Workers | `workers/priority_pool`, `tasks`, `task_token` | Two bounded pools, priority queues, token-based cancellation | Delivery touches a raw `QObject*` cross-thread (AUD-002); `cancel()` never called outside search (AUD-020 note) |
| Filesystem | `enumerator`, `watcher`, `operations`, `trash`, `mounts`, `paths` | readdir/fstatat scan, extension filtering, identity diff, rename/move/copy/trash/reveal, mountinfo | Ops now on the I/O pool (`operations` fixed 2026-10-25, AUD-003); no fingerprint fallback (AUD-020); no EXDEV (AUD-019); trash info-file integrity (AUD-018) |
| Media | `formats`, `probe`, `thumbnails` | Extension registry, FFmpeg probe + extraction, freedesktop cache reader, scheduler/LRU, failure policy | QG cache never served (AUD-008); probe runs twice (AUD-021); retry policy dead |
| Persistence | `database`, `repositories`, `schema.sql` | WAL SQLite, migrations, 7 repositories | Writes lie on error (AUD-017); migration counter cannot advance (AUD-031); quarantine misfires (AUD-032) |
| Models | `folder_model`, `search_proxy`, `reel_model`, `places_model`, `selection_state`, `recursive_search_model` | Canonical ordered rows, proxy filter, reel projection, sidebar, identity selection | Reorder contract (AUD-022), O(n) lookups (AUD-024), missing folder roles (AUD-014), selection stubs (**AUD-004 FIXED 2026-10-25**), search cap/roles (AUD-023) |
| Controllers | browser, search, playback, actions, mounts, state, thumbnail, probe, keyboard, type-to-select | Intent → service → model, sequencing, cancellation | Search properties missing (AUD-005); navigation kills playback (AUD-007); clipboard path confusion (AUD-013) |
| Playback | `mpv_backend`, `video_surface`, `playback_session`, `playback_context`, `playback_manager` | One mpv handle, render context, observable session, ordered context, resume policy | Dangling seek args (AUD-001); teardown lifetime (AUD-016); no auto-advance (AUD-012); throttle (AUD-025) |
| Presentation | `Main.qml`, `PathBar`, `PlacesSidebar`, `ContentView`, delegates, `Stage`, `TransportBar`, `ReelPanel`, strips, overlays, dialogs, `Theme` | Composition, chrome, grid geometry, cards, stage/transport, reel | Stage in the flow (**AUD-009 reflow fixed 2026-10-25**; still top-anchored rather than at the playing item), no fullscreen (AUD-010), Esc (AUD-011), rename (AUD-015), a11y (AUD-029) |

### 4.3 State ownership (traced)

| State | Authoritative owner | Duplicates found |
|---|---|---|
| Current folder | `BrowserController` | `FolderModel` has no path, so `PlaybackController` cannot tell "same folder" from "file gone" (AUD-007) |
| Selection / focus | `SelectionState` | `ContentView` keeps legacy `selectedIndex`/`focusIndex` twins for the null-selection branch |
| Current media / position / duration | `PlaybackManager` + `PlaybackSession` | `PlaybackController::currentSession_` caches the pointer and is never cleared on stop (AUD-026) |
| Ordered playback list | `PlaybackContext` (derived from `FolderModel`) | None — correct |
| Reel rows | `ReelModel` (projection) | None — correct |
| Search query | `SearchController` → `SearchProxy` | `ContentView.searchQuery` copies it |
| Density / sort | `BrowserController` (per-folder) | `ContentView.density` mirrors it |
| Playback filter | `PlaybackController.reelFilter_` → `PlaybackContext` | Reel label array in `Main.qml` |

---

## 5. Specification compliance matrix

Status vocabulary per the brief. "Statically consistent" means the code path exists and matches the requirement on inspection — **not** that it was executed.

| Area | Requirement (spec) | Status |
|---|---|---|
| Shell | Native title bar, six regions, responsive bands (§3.1–§3.5) | **PARTIALLY IMPLEMENTED** — sidebar never enters rail mode, breakpoints differ (AUD-028) |
| Shell | Window state persistence | **MISSING** — no `Settings`/`QSettings` for geometry anywhere |
| Browser | Folders always visible, videos filtered at enumeration (§5.1, §17.3) | **IMPLEMENTED — STATICALLY CONSISTENT** |
| Browser | Folder mosaic + count chip (§5.2) | **INCORRECT** — roles absent, always "0v" (AUD-014) |
| Browser | Cards: thumbnail/name/duration/size/date, state zones (§6) | **IMPLEMENTED** (thumbnails: AUD-008/014) |
| Browser | Selection blue / focus cyan / independent (§5.5, §13.1) | **FIXED 2026-10-25** (FIX-SEL-01 — was INCORRECT: never rendered, AUD-004) |
| Browser | Sorting: per-folder, stable total order, folders-first (§5.7, §20.2) | **IMPLEMENTED**; layout contract defect (AUD-022) |
| Browser | Type-to-select 700 ms (§13.3) | **INCORRECT** — buffer never injected (AUD-028) |
| Search | Ctrl+F morph, Manifest, highlight, summary, cancel, restore (§7) | **INCORRECT** — controller exposes no `active`, setters not invokable (AUD-005); ranks/roles (AUD-023) |
| Playback | One session, inline Stage in flow, 16:9 ≤60 % height (§2.1–§2.2) | **FIXED 2026-10-25** (FIX-STAGE-01 reflow half — was INCORRECT: overlay, AUD-009). The Stage still sits at the top of the region rather than at the playing item's grid position. Cinema fullscreen (AUD-010) was implemented 2026-10-27 (`windowedVisibility` + `onCinemaActiveChanged`; static review only — no live-WM verification yet) |
| Playback | No cross-fade on start (playback §2.1.4) | **INCORRECT** — 220 ms fade+scale (AUD-012) |
| Playback | Timeline/seek incl. non-seekable rejection, coalescing (§3.1) | **FIXED 2026-10-25** (was PARTIAL — QML rejected scrubs while the C++ seek was unsafe, AUD-001): async consolidated seeks, manager-owned non-seekable policy, transport feedback |
| Playback | Prev/Next from Reel order, stop at ends (§3.2, §19.4) | **IMPLEMENTED** (toast on boundary) |
| Playback | Auto-advance / continuous playback (§4.6, §25.7) | **MISSING** (AUD-012) |
| Playback | Resume >5 s, <duration−10 s, Always/Ask/Never (§19.6) | **PARTIAL** — tail guard skipped when duration unknown; policy not exposed |
| Cinema | Real fullscreen, chrome hidden, overlays auto-hide 2 s (§4.2–§4.4) | **INCORRECT** — windowed; no auto-hide entry (AUD-010) |
| Cinema | Esc returns to Stage; Close stops (§4.5) | **INCORRECT** — Esc stops (AUD-011) |
| Cinema | Continuous playback across media (§4.6) | **MISSING** (AUD-012) |
| Reel | Auto-open, Ctrl+R, header/footer, filters, scroll-to-current, keyboard (§5) | **PARTIAL** — no keyboard/focus/a11y (AUD-029) |
| File ops | Context menus, single/bulk verbs (§6, §11.2) | **PARTIAL** — bulk verbs offered on single selection; the "N items" title now tracks the selection (count frozen half fixed 2026-10-25, FIX-SEL-01) |
| File ops | Inline rename: extension lock, live validation, collision (§7, §11.3) | **INCORRECT** — editor never focused; no validation (AUD-015) |
| File ops | Cut/Copy/Paste (§6.2) | **INCORRECT** — gated on playback, identity-as-index (AUD-013) |
| File ops | Trash + Undo, partial-failure summary (§8) | **PARTIAL** — single undo slot; info-file integrity (AUD-018) |
| File ops | Move across devices (§17.6) | **MISSING** — EXDEV aborts (AUD-019) |
| Platform | Mount discovery, removable, eject (§17.4) | **PARTIAL** — octal escapes, removable probe (AUD-027) |
| Platform | X11/xcb primary, OpenGL backend forced (§16.1, §19.3) | **IMPLEMENTED** — `main.cpp:23` |
| Platform | Wayland path retained | **NOT ASSESSED** (no runtime) |
| Platform | `.deb` primary packaging (§16.1) | **MISSING** — no recipe (AUD-035) |
| Reliability | Typed results, no silent failure (§15.4) | **INCORRECT** — repository writes silently succeed (AUD-017) |
| Performance | No GUI-thread I/O/probe/decode (§22.1) | **PARTIAL, improved** — file ops now on the I/O pool (AUD-003 **FIXED 2026-10-25**); probe (AUD-021) and PNG encode (AUD-008) still on the GUI thread |
| Performance | 50 k entries, 300 ms first screenful (§22.6) | **SUPPORTED PERF RISK** — no batching, O(n) lookups (AUD-024) |
| Accessibility | Names/roles, focus visible, value text, keyboard-only (§2.9, §13.4) | **PARTIAL** — see AUD-029 |
| Documentation | DESIGN.md consistency | **INCORRECT** — duplicate §10/§11, missing §§8–9, Python-isms (AUD-034) |

---

## 6. P0 findings

### AUD-001 — `PlaybackManager::seek()` / `seekAbsolute()` pass dangling pointers into `mpv_command()`

**Status: RESOLVED 2026-10-25.** The dangling argument (ASan-confirmed
heap-use-after-free in a standalone probe of the two-statement form), the blocking
synchronous dispatch, the missing rapid-seek consolidation (§3.1.4) and the absent
UI feedback for a manager-refused seek are all fixed in
`src/playback/playback_manager.{hpp,cpp}` + `qml/QG/playback/Stage.qml`, with 15 new
policy tests and a headless ctest target. Evidence and residual risks:
[[2026-10-25-fix-aud-001-seek-memory-safety]].

**Severity:** P0 — Critical (undefined behaviour on every seek)
**Category:** Error handling / memory safety / Incorrect implementation
**Specification reference:** DESIGN.md playback §3.1 ("Seeking behavior: … 2. The position indicator updates immediately. 3. The backend receives the requested position. 4. Rapid successive seek requests are consolidated"), §3.2 ("Seek controls do not trigger Previous/Next behavior").

**Code evidence**

`src/playback/playback_manager.cpp:265-277` (`PlaybackManager::seek`, `PlaybackManager::seekAbsolute`):

```cpp
void PlaybackManager::seek(double seconds) {
    if (!backend_ || !backend_->handle()) return;
    QByteArray cmd = "seek " + QByteArray::number(seconds) + " relative";
    const char* args[] = {"seek", QByteArray::number(seconds).constData(), "relative", nullptr};
    mpv_command(backend_->handle(), args);
}

void PlaybackManager::seekAbsolute(double position) {
    if (!backend_ || !backend_->handle()) return;
    QByteArray cmd = "seek " + QByteArray::number(position) + " absolute";
    const char* args[] = {"seek", QByteArray::number(position).constData(), "absolute", nullptr};
    mpv_command(backend_->handle(), args);
}
```

Callers: `Stage.qml:258-260` (`seekBackRequested`/`seekForwardRequested`/`seekToRequested`), `TransportBar` scrub commit, `KeyboardFramework::dispatch` (`playback.seekBack`/`seekForward`/`jump0..9`, `keyboard_framework.cpp:160-168`).

**Current implementation**

`QByteArray::number(seconds)` constructs a temporary whose lifetime ends at the closing semicolon of the `args` initialisation. `args[1]` therefore holds a pointer into freed storage by the time `mpv_command()` reads it. The compiler cannot warn because the pointer is stored in a `char const*` array (no lifetime tracking), and `-Wall -Wextra -Wpedantic` do not enable `-Wdangling-pointer`. The `QByteArray cmd` variables are dead code (unused-variable warnings are suppressed only because the variable is used in its own initialiser). `mpv_command()` is the *synchronous* variant, so every seek also blocks the GUI thread for the duration of the command.

Additionally the C++ path never consults `PlaybackSession::seekable()`, so keyboard seeking on a non-seekable stream issues a command mpv will fail, while the scrubber path in `TransportBar.qml:76-84` does reject it — two different behaviours for the same rule.

**Expected implementation**

A stable buffer that outlives the call, an asynchronously dispatched command (so rapid seeks cannot block the UI and the latest request wins), and the spec's non-seekable rejection (restore the real position, indicate unavailability).

**Root cause** — CONFIRMED. A literal-array convenience pattern (`const char* args[] = {...QByteArray::number(x)...}`) applied to a function that consumes the array synchronously; the API contract of `mpv_command` (arguments valid for the call) was not modelled in the type system. The dead `cmd` locals show the author intended to build the command string but switched approach mid-edit.

**Correct architecture**

`PlaybackManager` remains the only component that talks to mpv (DEC-007). Argument buffers are named locals owned by the function for the whole call; seeks are issued with `mpv_command_async` with a reply userdata of 0 so no reply is expected; the non-seekable policy lives in the manager (single source), and the UI reflects it through a signal rather than by duplicating the rule in QML.

**Complete permanent resolution**

- Delete the dead `QByteArray cmd` locals.
- Keep the argument buffer in a named local; use `absolute+exact` so the observed `time-pos` converges on the requested value (§3.1.5).
- Add `PlaybackManager::actualPosition()` (reads `time-pos`, falls back to the session) for rejection recovery.
- Add `signals: void seekRejected();`; `TransportBar` already owns the amber flash, so wire it through `Stage.qml`.
- Remove the now-duplicated seekable gate from `TransportBar.commitScrub()` only if the manager is guaranteed to be injected; keep it (it is harmless and gives instant feedback) but the manager must not depend on it.

**Implementation-ready patch**

**File:** `src/playback/playback_manager.cpp`
**Location:** `PlaybackManager::seek` and `PlaybackManager::seekAbsolute` (lines 265-277)

Find:

```cpp
void PlaybackManager::seek(double seconds) {
    if (!backend_ || !backend_->handle()) return;
    QByteArray cmd = "seek " + QByteArray::number(seconds) + " relative";
    const char* args[] = {"seek", QByteArray::number(seconds).constData(), "relative", nullptr};
    mpv_command(backend_->handle(), args);
}

void PlaybackManager::seekAbsolute(double position) {
    if (!backend_ || !backend_->handle()) return;
    QByteArray cmd = "seek " + QByteArray::number(position) + " absolute";
    const char* args[] = {"seek", QByteArray::number(position).constData(), "absolute", nullptr};
    mpv_command(backend_->handle(), args);
}
```

Replace with:

```cpp
// Read the live position for rejection recovery (DESIGN.md playback §3.1:
// "reject the seek request, restore the actual position").
double PlaybackManager::actualPosition() const {
    double pos = 0.0;
    if (backend_ && backend_->handle()
        && mpv_get_property(backend_->handle(), "time-pos", MPV_FORMAT_DOUBLE, &pos) >= 0) {
        return pos;
    }
    return session_->position();
}

void PlaybackManager::seek(double seconds) {
    if (!backend_ || !backend_->handle()) return;
    if (!session_->seekable()) {
        // Non-seekable stream: restore what is really playing and let the UI
        // flash its affordance instead of issuing a command mpv will reject.
        session_->setPosition(actualPosition());
        emit seekRejected();
        return;
    }
    seekAbsolute(session_->position() + seconds);
}

void PlaybackManager::seekAbsolute(double position) {
    if (!backend_ || !backend_->handle()) return;
    if (!session_->seekable()) {
        session_->setPosition(actualPosition());
        emit seekRejected();
        return;
    }

    const double target = position > 0.0 ? position : 0.0;
    // `pos` is a named local: mpv copies the command and reads the argument
    // array during the call, so its storage must outlive the call. Passing
    // QByteArray::number(...).constData() directly (the previous code) left
    // args[1] dangling before mpv ever saw it.
    const QByteArray pos = QByteArray::number(target, 'f', 3);
    const char* args[] = {"seek", pos.constData(), "absolute+exact", nullptr};
    // Async: rapid seeks must not block the GUI thread, and the latest request
    // wins because mpv applies commands in order and drops superseded seeks
    // (DESIGN.md playback §3.1.4).
    mpv_command_async(backend_->handle(), 0, args);
    // Optimistic indicator; the next observed time-pos corrects it (§3.1.2/5).
    session_->setPosition(target);
}
```

**File:** `src/playback/playback_manager.hpp`
**Location:** section "Persisted position …" / signals / private helpers

Find:

```cpp
    Q_INVOKABLE void persistPosition();
    Q_INVOKABLE void flushPosition();
```

Replace with:

```cpp
    Q_INVOKABLE void persistPosition();
    Q_INVOKABLE void flushPosition();

    // Live position as mpv reports it (seconds); the session value when unknown.
    // Used to restore the real position after a rejected seek (§3.1).
    double actualPosition() const;
```

Find:

```cpp
    // Ask policy: the stored position is available; the UI decides (DESIGN §19.6).
    void resumePromptRequested(const QString& identity, double positionSeconds);
```

Replace with:

```cpp
    // Ask policy: the stored position is available; the UI decides (DESIGN §19.6).
    void resumePromptRequested(const QString& identity, double positionSeconds);
    // A seek was refused because the current media is not seekable (§3.1):
    // the session position has been restored; the UI may flash the affordance.
    void seekRejected();
```

**File:** `qml/QG/playback/TransportBar.qml`
**Location:** after `function commitScrub()` (line ~97)

Find:

```cpp
    function commitScrub() {
        scrubbing = false
        if (seekable)
            seekToRequested(scrubPosition)
        timeline.restoreTracking()
    }
```

Replace with:

```cpp
    function commitScrub() {
        scrubbing = false
        if (seekable)
            seekToRequested(scrubPosition)
        timeline.restoreTracking()
    }

    // Called by the Stage when PlaybackManager::seekRejected() fires: the
    // manager already restored the real position, so the block only shows the
    // "not seekable" affordance (DESIGN.md playback §3.1).
    function flashSeekRejected() {
        seekRejected = true
        rejectTimer.restart()
    }
```

**File:** `qml/QG/playback/Stage.qml`
**Location:** after the `TransportBar { … }` block (line ~267), before the closing brace of `Stage`

Find:

```qml
        onFullscreenRequested: stage.fullscreenRequested()
        onCloseRequested: stage.closeRequested()
    }
}
```

Replace with:

```qml
        onFullscreenRequested: stage.fullscreenRequested()
        onCloseRequested: stage.closeRequested()
    }

    // Seek-refusal feedback comes from the single owner of the policy
    // (PlaybackManager), so keyboard seeks behave like scrubbing (§3.1).
    Connections {
        target: stage.playbackManager

        function onSeekRejected() {
            transport.flashSeekRejected()
        }
    }
}
```

**Dependencies:** none. Does not change any public QML API surface (adds one signal).
**Regression risks:** `tst_transport.qml` / `test_transport_state.cpp` may assert `mpv_command` sequencing or immediate position changes; `absolute+exact` is stricter than `absolute` (frame-accurate, slower on long GOPs). `session_->position()` as the relative base is the last observed value (≤ 1 frame behind) which is equivalent in practice, but the very first `J`/`L` before any `time-pos` arrives now clamps at 0 instead of issuing `seek -10 relative` (mpv clamps identically). If any test stubs `PlaybackManager` and expects `mpv_command`, it must be updated to `mpv_command_async`.
**Deferred verification:** on a real X11 session with a seekable MP4 and a non-seekable stream, assert (a) repeated `J`/`L` never blocks the UI, (b) the scrubber and `time-pos` converge, (c) a non-seekable file flashes amber and the position snaps back. Run under ASan to confirm the dangling read is gone.
**Evidence confidence:** CONFIRMED (C++ lifetime semantics; the temporary is destroyed at the end of the declaration statement, before `mpv_command` is entered).

---

### AUD-002 — `submitTask()` dereferences a raw `QObject *receiver` from a worker thread

**Severity:** P0 — Critical (use-after-free / data race; already observed as an intermittent segfault)
**Category:** Error handling / Concurrency / Architecture
**Specification reference:** DESIGN.md §15.2 ("Qt queued signals/events are the sole cross-thread delivery mechanism into GUI-owned objects"), §15.3, §22.5. Project memory records the symptom: `DEFERRED_TESTS.md` §3 — "Worker-pool delivery use-after-free … ~4 % of `qg_keyboard_test` teardown runs crash in `QObject::thread()`".

**Code evidence**

`src/workers/tasks.cpp:55-91` (`qg::submitTask`), specifically lines 73-88:

```cpp
            if (receiver == nullptr) {
                if (deliver) {
                    deliver();
                }
                return;
            }
            // Deliver on the receiver's thread. Using `receiver` as the context
            // object makes Qt drop the event if the receiver is destroyed first.
            QMetaObject::invokeMethod(
                receiver,
                [deliver = std::move(deliver)]() mutable {
                    if (deliver) {
                        deliver();
                    }
                },
                Qt::QueuedConnection);
```

Callers include `ThumbnailController::pumpAsync` (`thumbnail_controller.cpp:296`), `MediaProbeService::pump` (`probe.cpp:196`), `MountService` (`mounts.cpp:512`), `BrowserController` enumeration — all with `this` as the receiver.

**Current implementation**

The functor runs on a pool thread and calls `QMetaObject::invokeMethod(QObject *context, Functor, Qt::QueuedConnection)`. That overload must determine the receiver's thread, i.e. it dereferences the `QObject` (its `d_ptr`/thread data) *on the pool thread*. If the receiver is destroyed while the task is queued or running — a `ThumbnailController` torn down at test end, a `MountService` at application shutdown, any controller whose parent dies first — the dereference is a use-after-free. The comment claims "Qt drop[s] the event if the receiver is destroyed first", which is true only for the *event delivery* half of the contract, not for this first touch. This is a genuine race, not a style issue: the receiver's destructor runs on the GUI thread while the worker reads it with no synchronisation.

**Expected implementation**

Cross-thread delivery must not touch the receiver from the worker at all. Capture the receiver's lifetime guard **at submit time on the GUI thread**, hand the functor to a GUI-thread-owned, process-lifetime dispatcher, and check the guard only on the GUI thread.

**Root cause** — CONFIRMED. The `receiver` parameter is a bare pointer used as both a lifetime token and a thread selector; the two roles were conflated. `QPointer` was not used, and a stable dispatcher object did not exist.

**Correct architecture**

One GUI-thread delivery hub owned by the workers substrate (never destroyed during the process), created on first `submitTask` (which is a GUI-thread API). `QPointer<QObject>` is constructed on the caller's thread and only *copied* by workers (atomic, no object access); `isNull()` is evaluated exclusively on the GUI thread, where GUI-owned `QObject`s are destroyed. `deliver()` then runs on the GUI thread with the receiver guaranteed alive.

**Complete permanent resolution**

Replace the delivery block; add the hub helper in an anonymous namespace; document the thread contract in `tasks.hpp`. No public signature change, so every caller keeps working.

**Implementation-ready patch**

**File:** `src/workers/tasks.cpp`
**Location:** includes and `submitTask`

Find:

```cpp
#include "tasks.hpp"

#include <QMetaObject>
#include <QThread>

#include <algorithm>
#include <utility>
```

Replace with:

```cpp
#include "tasks.hpp"

#include <QCoreApplication>
#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QThread>

#include <algorithm>
#include <mutex>
#include <utility>
```

Find:

```cpp
namespace {

int cpuCount()
{
    return std::max(1, QThread::idealThreadCount());
}

} // namespace
```

Replace with:

```cpp
namespace {

int cpuCount()
{
    return std::max(1, QThread::idealThreadCount());
}

// Process-lifetime, GUI-thread delivery target for worker results.
//
// submitTask() is a GUI-thread API, so the hub is created on the GUI thread on
// first use and never destroyed while QG runs. Because it cannot die, a worker
// may safely read its address; because it lives on the GUI thread, every queued
// invocation runs there. No Q_OBJECT is needed: invokeMethod() only needs a
// context object, not a slot.
QObject *deliveryHub()
{
    static QObject *hub = nullptr;
    static std::once_flag once;
    std::call_once(once, [] {
        hub = new QObject();
        hub->setObjectName(QStringLiteral("qg.deliveryHub"));
        if (QCoreApplication::instance() != nullptr) {
            hub->moveToThread(QCoreApplication::instance()->thread());
        }
    });
    return hub;
}

} // namespace
```

Find:

```cpp
    auto *runnable = new FunctionRunnable(
        [token, receiver, work = std::move(work), deliver = std::move(deliver)]() mutable {
            if (token && token->isCancelled()) {
                return;
            }
            if (work) {
                work();
            }
            if (token && token->isCancelled()) {
                return;
            }
            if (receiver == nullptr) {
                if (deliver) {
                    deliver();
                }
                return;
            }
            // Deliver on the receiver's thread. Using `receiver` as the context
            // object makes Qt drop the event if the receiver is destroyed first.
            QMetaObject::invokeMethod(
                receiver,
                [deliver = std::move(deliver)]() mutable {
                    if (deliver) {
                        deliver();
                    }
                },
                Qt::QueuedConnection);
        });
```

Replace with:

```cpp
    // Lifetime guard captured HERE, on the caller's (GUI) thread. The worker only
    // copies the QPointer (an atomic refcount bump that never touches the
    // object); isNull() is evaluated on the GUI thread, which is also the only
    // thread that can destroy a GUI-owned QObject.
    const QPointer<QObject> guarded(receiver);

    auto *runnable = new FunctionRunnable(
        [token, guarded, work = std::move(work), deliver = std::move(deliver)]() mutable {
            if (token && token->isCancelled()) {
                return;
            }
            if (work) {
                work();
            }
            if (token && token->isCancelled()) {
                return;
            }
            if (guarded.isNull()) {
                // No receiver (fire-and-forget): deliver on the worker thread,
                // unless the caller explicitly asked for GUI-thread delivery by
                // passing a receiver that has since died — in which case the
                // result is worthless and is dropped.
                if (deliver && token == nullptr) {
                    deliver();
                }
                return;
            }
            // Hand the result to the GUI-thread hub. Nothing here touches the
            // receiver, so a destroyed receiver can no longer be dereferenced
            // from a pool thread (the previous use-after-free).
            QMetaObject::invokeMethod(
                deliveryHub(),
                [guarded, deliver = std::move(deliver)]() mutable {
                    if (guarded.isNull() || !deliver) {
                        return; // receiver gone: drop the result silently
                    }
                    deliver();
                },
                Qt::QueuedConnection);
        });
```

> **Note on the null-receiver branch.** The original code delivered inline on the worker when `receiver == nullptr`, which is the documented "no receiver" contract. Because `guarded.isNull()` is also true for a receiver that has been destroyed, the branch above distinguishes the two cases with `token == nullptr`, which is how the current callers use the no-receiver form. If a future caller needs fire-and-forget delivery *with* a token, add an explicit `bool deliverOnWorker` parameter rather than overloading nullness.

**File:** `src/workers/tasks.hpp`
**Location:** the declaration's doc comment

Find:

```cpp
// Run `work` on `pool` at `priority`. On completion, if `token` has not been
// cancelled, invoke `deliver` on `receiver`'s thread through a queued
// connection. `work` must not touch GUI state; `deliver` runs on the GUI thread.
// The token may be null, in which case no cancellation check is performed.
```

Replace with:

```cpp
// Run `work` on `pool` at `priority`. On completion, if `token` has not been
// cancelled, invoke `deliver` on the GUI thread through a queued connection.
// `work` must not touch GUI state; `deliver` runs on the GUI thread and only
// runs while `receiver` is alive.
//
// Thread contract: submitTask() must be called from the GUI thread (that is what
// makes the receiver's lifetime guard valid). `receiver` is used purely as a
// lifetime token — it is never dereferenced from a worker thread. A null
// receiver means "no GUI hop": the result is delivered on the worker thread.
// The token may be null, in which case no cancellation check is performed.
```

**Dependencies:** none; drop-in for all call sites.
**Regression risks:** results for a receiver destroyed mid-flight are now dropped rather than delivered to freed memory — tests that destroy a controller and then assert a delivered result must be updated (they were asserting undefined behaviour). The `deliveryHub` is intentionally leaked; ASan/LSan will report one small `QObject` allocation at exit — add it to the suppression list or `deleteLater` it in `App::exec()` if leak-clean runs are required.
**Deferred verification:** run `qg_keyboard_test`, `qg_workers`, `qg_thumb_delivery` and the full QML suite 25× each; the historical ~4 % teardown crash must be 0/25. Then run under ASan/TSan (`-DQG_SANITIZE=address` and a separate `thread` build) with a test that destroys the receiver while tasks are queued.
**Evidence confidence:** CONFIRMED (the overload's contract requires the context object's thread; the call is made from the worker).

---

## 7. P1 findings

### AUD-003 — Every file operation runs synchronously on the GUI thread

**Status: RESOLVED 2026-10-25** (the async-dispatch half; progress/cancellation remains FIX-FO-02). All four mutating entry points — `rename`, `move`, `copy`, `deletePermanently` — are now an asynchronous facade over the unchanged `*Sync` helpers: each submits to `WorkerPools::io()` at `TaskPriority::Normal` through `qg::submitTask()` with `this` as the lifetime-guarded receiver and delivers `operationFinished` on the GUI thread after the event loop turns. `reveal`/`openWith` stay synchronous, as the finding instructs. Measured on this host: the pre-fix code blocked the GUI thread for **818 ms** on a 3 MB / 400-file directory copy (and **6.0 s** on the 24 MB / 3,000-file variant used during triage); the post-fix call returns in well under 1 ms with the watchdog timer still ticking. New suite `qg_file_ops_async` (8 cases, 25/25 repeat runs, ASan+UBSan clean), including the receiver-destroyed-in-flight case. Evidence, control runs and residual risks: [[2026-10-25-fix-aud-003-file-ops-async]]. Progress in the status strip (§8.1) and cancellation checkpoints are **not** implemented and stay open under FIX-FO-02.

**Severity:** P1 — High (core desktop behaviour: UI freeze for the duration of copy/move/trash; no progress, no cancellation)
**Category:** Performance / Architecture / Error handling
**Specification reference:** §17.6 ("All long operations are scheduled off the GUI thread and publish progress/result structs through queued signals"), §15.3 (I/O pool "for enumeration/copy/move/trash"), §22.1 rules 1 and 4 ("No unbounded loops on the GUI thread"), §8.1 (progress in the status strip), §4 ("Moved 9 of 10 … — 1 permission error").

**Code evidence**

`src/filesystem/operations.cpp:13-27`:

```cpp
void FileOperationsService::rename(const QString& path, const QString& newBaseName) {
    emitFinished(QStringLiteral("rename"), renameSync(path, newBaseName));
}

void FileOperationsService::move(const QStringList& paths, const QString& targetDir) {
    emitFinished(QStringLiteral("move"), moveSync(paths, targetDir));
}

void FileOperationsService::copy(const QStringList& paths, const QString& targetDir, bool) {
    emitFinished(QStringLiteral("copy"), copySync(paths, targetDir));
}

void FileOperationsService::deletePermanently(const QStringList& paths) {
    emitFinished(QStringLiteral("delete"), deletePermanentlySync(paths));
}
```

`emitFinished` is a plain signal emission (`operations.cpp:174-198`), so `ActionsController::operationFinished` re-enters QML before the `Q_INVOKABLE` returns. `copyDirRecursive` is an unbounded recursive directory walk (`operations.cpp:103-155`). `reveal()` and `openWith()` call `QProcess::execute` (blocking wait) at lines 40-46 and 60-64.

**Current implementation**

A `Q_INVOKABLE` invoked from QML executes the entire operation on the GUI thread: a multi-gigabyte copy, a recursive copy of a directory tree, a cross-device trash move, or a `QProcess::execute("xdg-open", …)` that waits for the child. There is no progress reporting, no cancellation checkpoint, and the status strip never sees a task. `moveSync` also aborts the whole batch on the first failure (`break`, line 120), so §8.2's partial-failure summary is unreachable.

**Expected implementation**

Each operation is submitted to `WorkerPools::io()` through `submitTask` at `TaskPriority::Normal`, with `this` as the lifetime-guarded receiver; the result arrives on the GUI thread as today, so `ActionsController` and QML need no change beyond ordering guarantees.

**Root cause** — CONFIRMED. The service was written as a synchronous library and later given `Q_INVOKABLE` facades without moving the work; the header's comment ("chunked with cancellation checkpoints") documents the intended design, not the implemented one.

**Correct architecture**

`FileOperationsService` becomes a thin asynchronous facade over the existing `*Sync` helpers (kept, because tests call them directly and the helpers are pure I/O): invoke → `submitTask(io, Normal, nullptr, this, [work]{ result = ops; }, [this, result]{ emitFinished(kind, *result); })`. Progress and cancellation are a follow-up increment (see the roadmap, FIX-FO-02) because they require chunked copy — flagged, not silently dropped.

**Complete permanent resolution**

Route the five mutating entry points through the I/O pool. Do **not** route `reveal`/`openWith` synchronously-spawned `QProcess` calls until the detached-launch helper lands; they are short-lived and are listed separately in AUD-035's roadmap notes.

**Implementation-ready patch**

**File:** `src/filesystem/operations.cpp`
**Location:** lines 13-27 (`rename`, `move`, `copy`, `deletePermanently`)

Find:

```cpp
void FileOperationsService::rename(const QString& path, const QString& newBaseName) {
    emitFinished(QStringLiteral("rename"), renameSync(path, newBaseName));
}

void FileOperationsService::move(const QStringList& paths, const QString& targetDir) {
    emitFinished(QStringLiteral("move"), moveSync(paths, targetDir));
}

void FileOperationsService::copy(const QStringList& paths, const QString& targetDir, bool) {
    emitFinished(QStringLiteral("copy"), copySync(paths, targetDir));
}

void FileOperationsService::deletePermanently(const QStringList& paths) {
    emitFinished(QStringLiteral("delete"), deletePermanentlySync(paths));
}
```

Replace with:

```cpp
// All mutating entry points run on the bounded I/O pool and publish their
// result through a queued delivery to this service, so the GUI thread never
// performs the walk (DESIGN.md §17.6, §22.1, §15.3). The *Sync helpers stay
// public for tests and for callers that already own a worker thread.
void FileOperationsService::rename(const QString& path, const QString& newBaseName) {
    auto result = std::make_shared<OperationResult>();
    submitTask(WorkerPools::io(), TaskPriority::Normal, nullptr, this,
               [path, newBaseName, result]() {
                   *result = renameSync(path, newBaseName);
               },
               [this, result]() {
                   emitFinished(QStringLiteral("rename"), *result);
               });
}

void FileOperationsService::move(const QStringList& paths, const QString& targetDir) {
    auto result = std::make_shared<OperationResult>();
    submitTask(WorkerPools::io(), TaskPriority::Normal, nullptr, this,
               [paths, targetDir, result]() {
                   *result = moveSync(paths, targetDir);
               },
               [this, result]() {
                   emitFinished(QStringLiteral("move"), *result);
               });
}

void FileOperationsService::copy(const QStringList& paths, const QString& targetDir, bool) {
    auto result = std::make_shared<OperationResult>();
    submitTask(WorkerPools::io(), TaskPriority::Normal, nullptr, this,
               [paths, targetDir, result]() {
                   *result = copySync(paths, targetDir);
               },
               [this, result]() {
                   emitFinished(QStringLiteral("copy"), *result);
               });
}

void FileOperationsService::deletePermanently(const QStringList& paths) {
    auto result = std::make_shared<OperationResult>();
    submitTask(WorkerPools::io(), TaskPriority::Normal, nullptr, this,
               [paths, result]() {
                   *result = deletePermanentlySync(paths);
               },
               [this, result]() {
                   emitFinished(QStringLiteral("delete"), *result);
               });
}
```

**File:** `src/filesystem/operations.cpp`
**Location:** includes (top of file, after `#include "operations.hpp"`)

Find:

```cpp
#include "operations.hpp"
#include <QFile>
```

Replace with:

```cpp
#include "operations.hpp"

#include "workers/tasks.hpp"

#include <QFile>
```

Find: (the `<memory>` include is required for `std::make_shared`)

```cpp
#include <QDebug>

bool copyDirRecursive(const QString& src, const QString& dst);
```

Replace with:

```cpp
#include <QDebug>

#include <memory>

bool copyDirRecursive(const QString& src, const QString& dst);
```

**Dependencies:** AUD-002 must land first (this patch relies on safe delivery). AUD-019 and the trash patches are independent.
**Regression risks:** ordering. `ActionsController` currently relies on the *synchronous* re-entry of `operationFinished` in some flows (e.g. `requestRename` decides whether to open the conflict dialog from the result). After this patch the result is asynchronous; the rename conflict path must therefore be driven by `targetPathForRename`/`pathExists` *before* committing (it already is — `Main.qml:1062` handles `onActionFinished`), but any test that assumes immediate completion must pump the event loop. Progress and cancellation remain unimplemented; §8.1 stays PARTIAL until FIX-FO-02.
**Deferred verification:** copy a 4 GB file and a 2,000-file directory while dragging the window; assert the UI stays responsive, and that `operationFinished` fires once with the correct `OperationResult`. Measure GUI-thread block time with a 16 ms watchdog.
**Executed 2026-10-25:** the responsiveness/once-only contract is now covered by `qg_file_ops_async` (1 ms watchdog, real fixtures: 400 files / 3 MB for the block-time measurement, 3,000 files / 24 MB during triage) — `QCOMPARE(delivered, 0)` right after the call, `ticks > 0` after the wait, `delivered == 1` after an extra 50 ms. Still deferred: a **4 GB** copy and drag-the-window interactivity on a live desktop session (no GUI automation on this host), and the "2,000-file directory" case against a real window.
**Evidence confidence:** CONFIRMED.

---

### AUD-004 — Selection and focus never reach the view; select-all and range-select are stubs

**Severity:** P1 — High (the central interaction model of a file manager does not render)
**Category:** Architecture / Missing functionality / State management
**Specification reference:** §5.5 ("Click: selects (replacing selection). Ctrl+click: toggles. Shift+click / Shift+arrows: range. Ctrl+A: all"), §6.2 state matrix (Focused / Selected), §13.1 (focus, selection and playback are independent), §3.6 (status strip reports selection), §11.2 (multi-selection menu), §13.2 (`Ctrl+A`, `Shift+↑/↓`).

**Code evidence**

`src/models/selection_state.hpp:9-49` — the class declares **no** `Q_PROPERTY`:

```cpp
class SelectionState : public QObject {
    Q_OBJECT
public:
    explicit SelectionState(QObject* parent = nullptr);

    // Identity-based selection (keyed by identity fingerprint from core/identity)
    Q_INVOKABLE bool isSelected(const QString& identityStr) const;
    Q_INVOKABLE int selectedCount() const;
    Q_INVOKABLE QStringList selectedIdentities() const;

    // Focus index in current view order (not identity-based)
    Q_INVOKABLE int focusIndex() const;
```

`qml/QG/ContentView.qml:471-474` (and the identical expressions at 504-505, 550-551, 718-719):

```qml
                    focused: selectionState ? (index === selectionState.focusIndex()) : (index === contentView.focusIndex)
                    selected: selectionState ? selectionState.isSelected(identityForIndex(index)) : (index === contentView.selectedIndex)
```

`qml/QG/Main.qml:833-852` (status strip), `:198-205` (`menuSelectionCount`):

```qml
                selectedCount: window.selectionState ? window.selectionState.selectedCount() : 0
```

`src/models/selection_state.cpp:55-78`:

```cpp
void SelectionState::extendRange(const QString& identityStr, int viewIndex) {
    // Range selection: anchor -> viewIndex (simplified: add all between anchor and current view)
    // For this minimal implementation we just add the identity and update focus
    selectedIdentities_.insert(identityStr);
    ...
}

void SelectionState::selectAll() {
    // In a real implementation this would use the current model count; here we signal full selection.
    emit selectionChanged();
    emit selectionSummaryChanged();
}

void SelectionState::clear() {
    selectedIdentities_.clear();
    focusIndex_ = -1;
```

**Current implementation**

QML bindings depend only on properties (and on `QAbstractItemModel` signals). `selectionState.focusIndex()` is an invokable call: it creates **no** dependency, so the `focused:`/`selected:` bindings are evaluated once when the delegate is created (and again only when `index` changes) and never when the user clicks or presses an arrow key. `selectionChanged`/`focusChanged` are emitted but nothing connects them. Consequences, all from the same root:

- clicking a card updates C++ state and draws no selection border;
- arrow keys move the focus cursor invisibly (and `clear()` also resets `focusIndex_` to −1, after which `moveFocus` early-returns on `idx < 0`, so focus dies after the first arrow press);
- the status strip's selection count and aggregate size freeze at 0;
- `menuSelectionCount` is evaluated once as 1, so `rebuildContextMenu(1)` never enables bulk-only verbs;
- `selectAll()` selects nothing; `extendRange()` inserts one extra identity and never fills the gap;
- `Ctrl+A` and `Shift+Arrow` therefore do nothing visible, and `Space`-to-select is unbound in browse mode (AUD-029).

**Expected implementation**

Selection and focus are observable Qt properties with `NOTIFY`; the view's delegate bindings read those properties (or a monotonically increasing revision) so they re-evaluate; select-all and range-select operate over the *view's* identity list, which the view owns.

**Root cause** — CONFIRMED. `SelectionState` was written as a C++ value object with invokable accessors and later bound directly from QML; the QML binding engine's dependency rules were not applied. This is the single highest-leverage defect in the tree: it makes five separate features look unimplemented.

**Correct architecture**

`SelectionState` keeps identity-keyed truth but exposes it as properties: `selectedCount`, `selectedIdentities`, `focusIndex`, `selectionRevision` (bumped by every mutation and used by views as an explicit dependency), `summary`. Range and select-all take the identity list from the caller (`ContentView`), because only the view knows the current order (including the Manifest proxy) — `SelectionState` must not guess model order. `clear()` clears the selection and leaves focus alone (`clearFocus()` exists for the manifest transition).

**Complete permanent resolution**

1. Add the properties + revision.
2. Add `selectAll(const QStringList &identities)` and `extendRangeTo(const QStringList &identities, int focusIndex)`.
3. Fix `clear()` to preserve focus.
4. Rewrite the four delegate binding sites to read properties/revision.
5. Wire Ctrl+A and Shift+arrows in `ContentView` to build the identity list from the model.

**Implementation-ready patch**

**File:** `src/models/selection_state.hpp`
**Location:** class head, after `Q_OBJECT`

Find:

```cpp
class SelectionState : public QObject {
    Q_OBJECT
public:
    explicit SelectionState(QObject* parent = nullptr);

    // Identity-based selection (keyed by identity fingerprint from core/identity)
    Q_INVOKABLE bool isSelected(const QString& identityStr) const;
    Q_INVOKABLE int selectedCount() const;
    Q_INVOKABLE QStringList selectedIdentities() const;

    // Focus index in current view order (not identity-based)
    Q_INVOKABLE int focusIndex() const;
    Q_INVOKABLE void setFocusIndex(int idx);
```

Replace with:

```cpp
class SelectionState : public QObject {
    Q_OBJECT
    // QML bindings only re-evaluate when a *property* they read changes, so the
    // selection/focus state must be exposed as properties with NOTIFY. Invokable
    // getters (previous design) created no dependency and left the cards, the
    // status strip and the context menu frozen at their creation-time values.
    Q_PROPERTY(int selectedCount READ selectedCount NOTIFY selectionChanged)
    Q_PROPERTY(QStringList selectedIdentities READ selectedIdentities NOTIFY selectionChanged)
    Q_PROPERTY(int focusIndex READ focusIndex NOTIFY focusChanged)
    // Monotonic revision: bumped on every mutation. Views that must re-evaluate
    // a per-identity predicate read it explicitly (see ContentView.qml).
    Q_PROPERTY(int selectionRevision READ selectionRevision NOTIFY selectionChanged)
    Q_PROPERTY(QString summary READ selectionSummary NOTIFY selectionSummaryChanged)

public:
    explicit SelectionState(QObject* parent = nullptr);

    // Identity-based selection (keyed by identity fingerprint from core/identity)
    Q_INVOKABLE bool isSelected(const QString& identityStr) const;
    int selectedCount() const;
    QStringList selectedIdentities() const;
    int selectionRevision() const { return selectionRevision_; }

    // Focus index in current view order (not identity-based)
    int focusIndex() const;
    Q_INVOKABLE void setFocusIndex(int idx);
```

Find:

```cpp
    Q_INVOKABLE void extendRange(const QString& identityStr, int viewIndex); // shift-range from anchor
    Q_INVOKABLE void selectAll();
    Q_INVOKABLE void clear();
```

Replace with:

```cpp
    // Shift-range: the view supplies every identity from the anchor to the new
    // focus (it owns the display order, including the Manifest proxy).
    Q_INVOKABLE void extendRangeTo(const QStringList& identities, int focusIndex);
    // Ctrl+A: same contract — the view supplies the identity list.
    Q_INVOKABLE void selectAll(const QStringList& identities);
    // Clear the selection only; focus is an independent concept (§13.1).
    Q_INVOKABLE void clear();
    // Used when leaving Browse for Manifest: drop the cursor too.
    Q_INVOKABLE void clearFocus();
```

Find:

```cpp
private:
    QSet<QString> selectedIdentities_;
    int focusIndex_ = -1;
    int anchorIndex_ = -1;
};
```

Replace with:

```cpp
    // Back-compat helper for tests written against the stub API: selects nothing
    // and is not reachable from QML. Prefer selectAll(identities).
    void selectAll() { emit selectionChanged(); }

private:
    QSet<QString> selectedIdentities_;
    int focusIndex_ = -1;
    int anchorIndex_ = -1;
    int selectionRevision_ = 0;
};
```

**File:** `src/models/selection_state.cpp`
**Location:** `extendRange`, `selectAll`, `clear` (lines 55-78) and the `focusIndex`/`selectedCount` definitions

Find:

```cpp
void SelectionState::extendRange(const QString& identityStr, int viewIndex) {
    // Range selection: anchor -> viewIndex (simplified: add all between anchor and current view)
    // For this minimal implementation we just add the identity and update focus
    selectedIdentities_.insert(identityStr);
    focusIndex_ = viewIndex;
    emit selectionChanged();
    emit focusChanged();
    emit selectionSummaryChanged();
}

void SelectionState::selectAll() {
    // In a real implementation this would use the current model count; here we signal full selection.
    emit selectionChanged();
    emit selectionSummaryChanged();
}

void SelectionState::clear() {
    selectedIdentities_.clear();
    focusIndex_ = -1;
    anchorIndex_ = -1;
    emit selectionChanged();
    emit focusChanged();
    emit selectionSummaryChanged();
}
```

Replace with:

```cpp
void SelectionState::extendRangeTo(const QStringList& identities, int focusIndex) {
    if (identities.isEmpty()) {
        return;
    }
    selectedIdentities_.clear();
    for (const QString& identity : identities) {
        if (!identity.isEmpty()) {
            selectedIdentities_.insert(identity);
        }
    }
    focusIndex_ = focusIndex;
    ++selectionRevision_;
    emit selectionChanged();
    emit focusChanged();
    emit selectionSummaryChanged();
}

void SelectionState::selectAll(const QStringList& identities) {
    selectedIdentities_.clear();
    for (const QString& identity : identities) {
        if (!identity.isEmpty()) {
            selectedIdentities_.insert(identity);
        }
    }
    if (focusIndex_ < 0 && !identities.isEmpty()) {
        focusIndex_ = 0;
        emit focusChanged();
    }
    ++selectionRevision_;
    emit selectionChanged();
    emit selectionSummaryChanged();
}

void SelectionState::clear() {
    // Focus is independent of selection (§13.1): clearing the selection must not
    // destroy the keyboard cursor, otherwise arrow navigation dies with it.
    selectedIdentities_.clear();
    anchorIndex_ = focusIndex_;
    ++selectionRevision_;
    emit selectionChanged();
    emit selectionSummaryChanged();
}

void SelectionState::clearFocus() {
    focusIndex_ = -1;
    anchorIndex_ = -1;
    emit focusChanged();
}
```

> Every other mutator (`selectOne`, `toggle`, `setFocusIndex`, `moveFocus*`, `selectAll()`) must also `++selectionRevision_;` before emitting `selectionChanged()`. The pattern is identical; apply it mechanically to `selection_state.cpp`.

**File:** `qml/QG/ContentView.qml`
**Location:** VideoCard delegate (line 471-474) — apply the same shape at 504-505 (FolderCard), 550-551 (CompactRow), 718-719 (ManifestRow)

Find:

```qml
                    focused: selectionState ? (index === selectionState.focusIndex()) : (index === contentView.focusIndex)
                    selected: selectionState ? selectionState.isSelected(identityForIndex(index)) : (index === contentView.selectedIndex)
```

Replace with:

```qml
                    focused: selectionState ? (index === selectionState.focusIndex) : (index === contentView.focusIndex)
                    // Reading selectionRevision is what makes this binding depend on
                    // the selection model: isSelected() alone notifies nothing.
                    selected: selectionState
                              ? (selectionState.selectionRevision >= 0
                                 && selectionState.isSelected(identityForIndex(index)))
                              : (index === contentView.selectedIndex)
```

**File:** `qml/QG/ContentView.qml`
**Location:** declarations near the top (`property int density: ContentView.Standard`)

Find:

```qml
    enum Density { Gallery, Standard, Compact }
    property int density: ContentView.Standard
```

Replace with:

```qml
    enum Density { Gallery, Standard, Compact }
    property int density: ContentView.Standard

    // Legacy single-selection cursor, used only when no SelectionState is
    // injected (tests). Declared so the fallback branches are not undefined.
    property int focusIndex: -1
    property int selectedIndex: -1
```

**File:** `qml/QG/Main.qml`
**Location:** status strip bindings (lines 833-852)

Find:

```qml
                selectedCount: window.selectionState ? window.selectionState.selectedCount() : 0
```

Replace with:

```qml
                selectedCount: window.selectionState ? window.selectionState.selectedCount : 0
```

**File:** `qml/QG/Main.qml`
**Location:** `menuSelectionCount` (lines 198-205)

Find:

```qml
    readonly property int menuSelectionCount: {
        if (window.selectionState && typeof window.selectionState.selectedCount === "function") {
            const n = window.selectionState.selectedCount()
            if (n > 0)
                return n
        }
        return 1
    }
```

Replace with:

```qml
    readonly property int menuSelectionCount: {
        if (window.selectionState && window.selectionState.selectedCount > 0)
            return window.selectionState.selectedCount
        // The menu always acts on at least the focused row (§11.2).
        return 1
    }
```

**Dependencies:** none for the properties; the Ctrl+A/Shift-range wiring needs `FolderModel::identityAt` (exists).
**Regression risks:** `tests/unit/test_selection_state.cpp` calls `selectAll()` and `extendRange(...)`; the patch keeps a non-QML `selectAll()` shim but `extendRange` is renamed — update the test to `extendRangeTo({ids}, idx)`. `tst_card_actions.qml` may assert `selectionState.selectedCount()` as a function; change to the property. Any QML that used `selectedCount()` must be switched to the property (grep: `Main.qml:199`, `:834`, `:846`).
**Deferred verification:** click / Ctrl+click / Shift+click / arrows / Ctrl+A / Esc in all three densities and in Manifest; confirm the border, the cyan ring, the status count, the aggregate size and the "N items" menu title all track. Screenshot the focus/selection pair on the same card set (§5.8).
**Executed 2026-10-25:** the binding mechanism was reproduced and then closed with two instruments, both kept outside the test tree. (1) **C++ control pair** (`~/.hermes/cache/scratch/aud004/probe.cpp`, built twice — default and `-DPREFIX_CONTROL` against `git show HEAD:src/models/selection_state.{hpp,cpp}`): a QML binding that reads `selectedCount`/`focusIndex`/`isSelected(id)` **never re-evaluated on HEAD** (count stayed 0, focus −1, `row0=false` after `selectOne`/`toggle`/`selectAll`), while the same probe against the fixed class logged every notification (`count=1 … row0=true … focus=2`). The control went red, so the green run is evidence. Also confirmed the mutator semantics: `extendRangeTo` replaces the selection with the range (2 of 4 rows after a shift-range) and `clear()` now leaves the cursor at 3 instead of −1, so arrow navigation survives Esc. (2) **Rendered-pixel probe** (`~/.hermes/cache/scratch/aud004/render_probe.cpp`, real `SelectionState` + real `ContentView` via `libqg_qmlplugin.a`, Xvfb `:77`, `grabWindow()` → ARGB32 scan): 1955 cyan (`Theme.focus` `#38E1E1`) pixels with **0** blue while nothing is selected; after selecting row 1 with the cursor on row 0 → cyan in x[26..329] **and** `Theme.signal` (`#4DA3FF`) in x[347..652] (different cards, §13.1); moving the cursor to row 2 moves the ring to x[670..973]; Ctrl+A → 6880 blue px spanning x[25..974]. The same frame carries a **control card** whose `selected` uses the old invokable-only expression: with `id3` selected it renders **0** blue pixels — the defect in miniature, on the same data. Screenshots: `aud004_focus_selection.png` (focus/selection pair), `aud004_step0_focus_only.png`, `aud004_step2_focus_moved.png`, `aud004_step3_select_all.png`, `aud004_step5_manifest.png`, `aud004_step6_{gallery,compact}.png`. **Density matrix (executed):** the render probe was extended to the remaining surfaces with real models and real pixels — Gallery `2296` cyan / `2312` blue px, Compact `4208` / `4224` px (t6), and **Manifest** (t5: a real `RecursiveSearchModel` injected from C++) lists 4 result rows, `Ctrl+A` resolves `id0,id1,id2,id3` through the *manifest* model, the rows render the selection border, a row-2 click selects `id2` (that row's identity, not a browse row) and a range from the anchor set by that click yields `id2,id3`. A headless density harness (`aud004_matrix.qml`, 3/3) additionally asserts the focus ring / selection border bindings and the Ctrl+A–Esc–Shift+Right identity contract in Gallery and Compact. **Still deferred:** no live-desktop **mouse** click/Ctrl+click round-trip (the click path is asserted through `selectIndex`/`toggle` and the rendered marks, not via synthesised X11 input events), and Quick Test's case item is invisible, so effective `visible` reads are meaningless in-process — drawn marks are therefore asserted from pixels, not from `visible`.
**Baseline correction (measured this session):** `qg_qml` is **200 passed / 8 failed**, not 203/4 — the extra four are `Cinema::{test_cursorNotHiddenWhileTextEditing,test_escapeAndFullscreenShortcutsGatedByEditorFocus,test_fourTogglePointsConsistent}` and `Keyboard::test_textEditingSuppressesSingleKeyShortcuts` (all "editor focus detected"), identical before and after this change.
**Evidence confidence:** CONFIRMED (structural: no property, no dependency) — and now CONFIRMED by execution (control-red C++ probe + rendered pixels).

---

### AUD-005 — Search can never enter the Manifest intensity

**Severity:** P1 — High (the entire search presentation is unreachable)
**Category:** Missing functionality / Incorrect implementation
**Specification reference:** §7.1–§7.3 (Ctrl+F morphs the breadcrumb track, surface switches to Manifest), §7.2 (context strip replaced by the result summary), §23.4, §25.4.

**Code evidence**

`src/controllers/search_controller.hpp:18-46` — the whole public surface:

```cpp
    Q_INVOKABLE void setQuery(const QString& query);
    Q_INVOKABLE void clear();
    Q_INVOKABLE QString query() const;   // the currently applied (debounced) query
    Q_INVOKABLE QString summary() const;
    Q_INVOKABLE QString scope() const;
    ...
    Q_INVOKABLE void setSearchRoot(const QString& path);
    ...
signals:
    void activeChanged(bool active);
```

There is **no `Q_PROPERTY` at all**, and `setFolderName`/`setSortLabel` are plain member functions (declared at `search_controller.hpp:38-40`, no `Q_INVOKABLE`).

`qml/QG/Main.qml:104-105`:

```qml
    readonly property bool manifestActive: window.searchController
                                           && window.searchController.active === true
```

`qml/QG/Main.qml:869-886`:

```qml
        function onActiveChanged(active) {
            if (active) {
                // Give the summary its folder/sort context before it is emitted.
                if (window.browserController) {
                    ...
                    window.searchController.setFolderName(folder)
                    window.searchController.setSortLabel(order)
                }
                contentView.enterManifest(window.searchProxy,
                                          window.searchController.query())
```

**Current implementation**

`manifestActive` reads `.active`, which does not exist → `undefined === true` → always `false`, so the chrome never switches to the search state (`Main.qml:88` gates `contentState` on it too). Independently, the `onActiveChanged` handler calls two methods that QML cannot invoke; the resulting `TypeError` aborts the handler *before* `enterManifest()` is reached, so even if `active` existed the surface would not switch. The `setQuery` path is wired (PathBar → controller), and the debounce/proxy work — only the presentation switch is dead.

**Expected implementation**

`SearchController` exposes `active`, `summary`, `resultCount` as properties with `NOTIFY`, and the folder/sort context setters are invokable. The shell binds Manifest activation to the property.

**Root cause** — CONFIRMED. Same class of defect as AUD-004: a C++ controller written with invokables/signals and bound from QML as if properties existed; plus two setters that were intended for QML use but never marked invokable.

**Correct architecture**

`SearchController` is the single owner of query lifecycle and exposes observable state (`active`, `query`, `summary`, `resultCount`, `scopeRecursive`). QML observes properties; the `Connections` handler only performs the presentation switch, and reads the summary from the property rather than through a signal argument.

**Implementation-ready patch**

**File:** `src/controllers/search_controller.hpp`
**Location:** class head, after `Q_OBJECT`

Find:

```cpp
class SearchController : public QObject {
    Q_OBJECT

public:
```

Replace with:

```cpp
class SearchController : public QObject {
    Q_OBJECT
    // QML observes these; `active` in particular is what switches the surface to
    // the Manifest intensity (DESIGN.md §7.2). Without the property the shell's
    // `searchController.active` read was always undefined.
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)
    Q_PROPERTY(QString query READ query NOTIFY resultsChanged)
    Q_PROPERTY(QString summary READ summary NOTIFY resultsChanged)
    Q_PROPERTY(int resultCount READ resultCount NOTIFY resultsChanged)
    Q_PROPERTY(bool scopeRecursive READ scopeRecursive NOTIFY scopeChanged)

public:
```

Find:

```cpp
    void setProxy(SearchProxy* proxy);
    void setFolderName(const QString& name);
    void setSortLabel(const QString& label); // e.g. "Name ↑"
```

Replace with:

```cpp
    void setProxy(SearchProxy* proxy);
    // QML-facing: the shell supplies the folder/sort context that the summary
    // embeds. They were plain methods, so calling them from QML threw and
    // aborted the handler before enterManifest() (DESIGN.md §7.2).
    Q_INVOKABLE void setFolderName(const QString& name);
    Q_INVOKABLE void setSortLabel(const QString& label); // e.g. "Name ↑"
```

Find:

```cpp
    Q_INVOKABLE QString summary() const; // "6 results for \"sunset\" in Summer trip · Name ↑"
```

Replace with:

```cpp
    QString summary() const; // "6 results for \"sunset\" in Summer trip · Name ↑"
    int resultCount() const; // rows currently passing the query
    bool active() const;     // owned here, not by the view
```

**File:** `src/controllers/search_controller.cpp`
**Location:** wherever `activeChanged` is emitted and `summary()` is defined

Add (the surrounding member functions already exist; this is the missing state):

```cpp
bool SearchController::active() const
{
    return m_active;
}

int SearchController::resultCount() const
{
    return m_resultCount;
}
```

and ensure every `emit activeChanged(x)` is preceded by `m_active = x;`, and every place that recomputes the summary also sets `m_resultCount` (the proxy row count or the recursive model count) before emitting `resultsChanged()`.

**File:** `src/controllers/search_controller.hpp`
**Location:** private members (append)

```cpp
    bool m_active = false;
    int m_resultCount = 0;
```

**Dependencies:** none. `SearchProxy` row count is available via `rowCount()`.
**Regression risks:** `tests/unit/test_search_controller.cpp` may call `summary()` (still valid) and connect `resultsChanged` (unchanged). `activeChanged(bool)` keeps its signature, but QML handlers bound to a property NOTIFY must take no argument — `Main.qml:869 function onActiveChanged(active)` already declares one; with a property signal carrying an argument, QML still passes it, so the handler keeps working. To be strict, change the signal to `void activeChanged()` and drop the parameter, then update the QML handler to compare the property instead of the argument.
**Deferred verification:** Ctrl+F, type `sun`, and confirm the breadcrumb track morphs, the surface becomes the Manifest list, the summary reads `N results for "sun" in <folder> · <order>`, Esc restores Browse with the previous scroll position and selection (see AUD-023 for the selection-restore defect), and the no-results state appears for a non-matching query.
**Evidence confidence:** CONFIRMED.

**Implemented 2026-10-26 (FIX-SRCH-01) — IMPLEMENTED / STATICALLY REVIEWED / TESTING DEFERRED.** Both claimed causes were re-verified against the tree and confirmed. `SearchController` now declares `Q_PROPERTY(bool active …)`, `Q_PROPERTY(QString summary …)` and `Q_PROPERTY(int resultCount …)`; `setActive()` writes the member before notifying and de-duplicates; `clear()` re-notifies `resultsChanged` (it previously emitted `activeChanged(false)` only, so a bound summary stayed stale after Esc); `setFolderName`/`setSortLabel` are `Q_INVOKABLE`; `summary()` and `resultCount()` share one count computation. The signal keeps its `bool` argument (both in-tree consumers declare one, and a property NOTIFY may carry arguments). Deviations from the patch below: `query` and `scopeRecursive` are **not** properties (QML calls them as functions at `Main.qml:88-92`, `Main.qml:626`, `Main.qml:975`, `ContentView.qml:968`), and no `m_resultCount` cache is needed. The shell now reads the summary/applied query from the properties, keeps the field focused on the first Esc, and routes the §7.4 "Clear search" through the field. Checks run were **static only**: `g++ -std=c++20 -fsyntax-only` (exit 0, with a red control at exit 1) and `qmllint 6.10.2` versus a pre-fix-worktree control (identical diagnostic sets: 128 PathBar / 74 ContentView / 139 Main). No build, CTest, QML run or launch. **New dependent findings:** §7.3 row focus has no `Keys` contract on `manifestList`; focus ownership between the field and the manifest list is unverified; AUD-023's selection restore is now user-visible. See `.project-memory/08-Sessions/2026-10-26-fix-aud-005-search-manifest.md` and `DEFERRED_TESTS.md`.

---

### AUD-006 — Persisted favorite / watched / progress are never applied after the first navigation

**Severity:** P1 — High (the application's only persistent value is invisible)
**Category:** Missing functionality / State management
**Specification reference:** §21.3 ("Favorite / Watched / Last position | identity | Follows the file across renames/moves"), §23.11–§23.12, §25.8 ("Favorite, watched, and last position survive app restart"), §6.2 state matrix.

**Code evidence**

`src/core/composition.cpp:93-99`:

```cpp
    m_state = std::make_unique<StateController>();
    m_state->setDatabase(database);
    m_state->setModel(model);
    if (database != nullptr && model != nullptr) {
        m_state->loadIntoModel();
    }
```

`src/app.cpp:199-205` then `:264-267`:

```cpp
    m_services->wire(m_database.get(), m_browser.get(), m_mounts.get(), …);
    ...
    if (!startLocation.isEmpty()) {
        m_browser->navigate(startLocation);
    }
```

`grep -rn loadIntoModel src/` → declaration, definition, and **one** call in `composition.cpp:98`. The enumerator never sets `favorite`/`watched`/`progress` (`src/filesystem/enumerator.cpp:143-158` builds `Entry` with defaults), so after `applySnapshot()` every row is `favorite=false, watched=false, progress=nullopt`.

**Current implementation**

`loadIntoModel()` runs before the model has any rows (the first `navigate()` happens afterwards), so the loop body never executes. Every subsequent folder load replaces the model wholesale and nothing re-applies the persisted state. The visible symptom: hearts, violet checks, progress bars, `Favorite ↑` sorting and the Reel's `Favorites`/`Unwatched` filters are empty for the whole session, while writes (toggling a heart) do persist and reappear only after a restart *if* the file happens to be in the start folder.

**Expected implementation**

State is applied to every row as it enters the model (initial load and every navigation/refresh), exactly once per identity per generation, on the GUI thread, without an N×3 query storm.

**Root cause** — CONFIRMED. Ordering: a one-shot "load everything now" call placed before the data exists. There is no hook from model population to state application.

**Correct architecture**

`StateController` subscribes to the canonical model's structural signals (`rowsInserted`, `modelReset`) and applies state for the newly visible rows on a zero-delay timer (coalescing a batch into one pass). A `QSet<QString> m_applied` prevents re-querying an identity that is already decorated in this model generation, and `modelReset` clears it.

**Complete permanent resolution**

Add the hook, the coalescing pass, and the already-applied guard; keep `loadIntoModel()` as the immediate entry point for tests.

**Implementation-ready patch**

**File:** `src/controllers/state_controller.hpp`
**Location:** public section

Find:

```cpp
    void loadIntoModel();
```

Replace with:

```cpp
    void loadIntoModel();
    // Coalesced incremental variant: applies persisted state to rows that have
    // not been decorated yet. Wired to the canonical model's structural signals
    // so every navigation/refresh shows hearts, checks and progress (§21.3).
    void scheduleLoad();
```

Find: (private section, append)

```cpp
    void applyStateToRow(const qg::Entry &entry);

    QSet<QString> m_applied;
    bool m_loadScheduled = false;
```

**File:** `src/controllers/state_controller.cpp`
**Location:** after `loadIntoModel()`

Find:

```cpp
void StateController::loadIntoModel()
{
    if (m_model == nullptr || m_database == nullptr) {
        return;
    }

    MediaStateRepository repository(*m_database);
    const QList<Entry> entries = m_model->entries();
    for (const Entry &entry : entries) {
        if (entry.identity.isEmpty()) {
            continue;
        }
        m_model->updateFavorite(entry.identity, repository.favorite(entry.identity));
```

Replace with:

```cpp
void StateController::scheduleLoad()
{
    if (m_model == nullptr || m_database == nullptr || m_loadScheduled) {
        return;
    }
    m_loadScheduled = true;
    // Zero-delay coalescing: a 200-entry enumeration batch, a sort and a refresh
    // all collapse into one pass on the next event-loop turn.
    QTimer::singleShot(0, this, [this]() {
        m_loadScheduled = false;
        loadIntoModel();
    });
}

void StateController::loadIntoModel()
{
    if (m_model == nullptr || m_database == nullptr) {
        return;
    }

    MediaStateRepository repository(*m_database);
    const QList<Entry> entries = m_model->entries();
    for (const Entry &entry : entries) {
        if (entry.identity.isEmpty() || m_applied.contains(entry.identity)) {
            continue;
        }
        m_applied.insert(entry.identity);
        m_model->updateFavorite(entry.identity, repository.favorite(entry.identity));
```

**File:** `src/controllers/state_controller.cpp`
**Location:** includes

Find:

```cpp
#include "controllers/state_controller.hpp"
```

Replace with:

```cpp
#include "controllers/state_controller.hpp"

#include <QTimer>
```

**File:** `src/controllers/state_controller.cpp`
**Location:** wherever `setModel` is implemented (add the signal hook there)

```cpp
void StateController::setModel(qg::FolderModel *model)
{
    if (m_model == model) {
        return;
    }
    if (m_model != nullptr) {
        m_model->disconnect(this);
    }
    m_model = model;
    m_applied.clear();
    if (m_model == nullptr) {
        return;
    }
    // A new folder replaces the rows wholesale (applySnapshot → modelReset);
    // forget what was decorated and re-apply for the new set.
    connect(m_model, &QAbstractItemModel::modelReset, this, [this]() {
        m_applied.clear();
        scheduleLoad();
    });
    connect(m_model, &QAbstractItemModel::rowsInserted, this,
            [this](const QModelIndex &, int, int) { scheduleLoad(); });
    scheduleLoad();
}
```

**Dependencies:** AUD-024 (identity index) makes this affordable; without it each decorated row costs an O(n) scan inside `updateFavorite`.
**Regression risks:** `tests/unit/test_state_controller.cpp` may assert that `loadIntoModel()` decorates rows repeatedly; the `m_applied` guard makes the second call a no-op — the test should clear state via a new `resetApplied()` or construct a fresh controller. Startup cost rises by one indexed query per visible row on first paint (bounded by the row count).
**Deferred verification:** start QG with a database containing favorites/watched/positions; confirm the first folder shows them immediately, navigating to a second decorated folder shows them, and toggling + restarting preserves them. Measure first-paint time before/after on a 5,000-entry folder.
**Evidence confidence:** CONFIRMED.

---

### AUD-007 — Browsing to another folder stops playback

**Severity:** P1 — High (violates the product's central "one surface" premise)
**Category:** Architecture / State management / Incorrect implementation
**Specification reference:** §13.1 ("**Playback state** (mint) — independent of both; playback continues while you browse elsewhere"), §9.1 (state preservation), §19.2 (one session), §1.2 ("Going back down an intensity is instant and lossless").

**Code evidence**

`src/controllers/playback_controller.cpp:290-309`:

```cpp
void PlaybackController::onModelStructureChanged()
{
    if (!context_ || !folderModel_)
        return;

    const QString identity = currentIdentity();
    // Did the playing file leave the model entirely? Then playback must stop:
    // its path no longer exists (rename/removal/navigation — §17.5).
    const bool playingGone = !identity.isEmpty()
                             && folderModel_->indexOfIdentity(identity) < 0;
    ...
    if (playingGone && manager_ && currentSession_ != nullptr
            && currentSession_->state() != PlaybackSession::Idle) {
        stop();
    }
}
```

`qml/QG/Main.qml:577-585` — a folder row navigates without stopping playback:

```qml
                    onActivatedItem: function (index, path, isFolder) {
                        if (isFolder && window.browserController) {
                            window.browserController.navigate(path)
```

`src/models/folder_model.cpp:87-95` — `applySnapshot` emits `modelReset`, and `PlaybackController::setFolderModel` connects `modelReset` to `onModelStructureChanged` (line 43-44).

**Current implementation**

Navigating to any other folder replaces the model rows; the playing identity is not in the new folder's rows, so `playingGone` is true and `stop()` runs. Playback therefore ends on every sidebar click, breadcrumb click, folder double-click and Back/Forward — the exact opposite of §13.1, and it also hides the Reel and exits Cinema.

**Expected implementation**

The "file vanished" decision must only apply when the model still represents the folder the session is playing from. Navigating elsewhere keeps the session alive (the Stage stays open, the Reel keeps its own ordered list until the player's folder is browsed back to).

**Root cause** — CONFIRMED. The controller conflates two different causes of a model reset — "this folder changed" and "a different folder is now shown" — because `FolderModel` does not expose which folder it currently holds.

**Correct architecture**

`FolderModel` publishes `folderPath` (the enumerated directory, from `FolderSnapshot::path`). `PlaybackController` compares it with the session's containing directory before deciding the item is gone. This keeps `FolderModel` the ordering authority while giving the playback layer the one fact it needs.

**Implementation-ready patch**

**File:** `src/models/folder_model.hpp`
**Location:** public properties

Find:

```cpp
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int videoCount READ videoCount NOTIFY countChanged)
    Q_PROPERTY(int folderCount READ folderCount NOTIFY countChanged)
```

Replace with:

```cpp
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int videoCount READ videoCount NOTIFY countChanged)
    Q_PROPERTY(int folderCount READ folderCount NOTIFY countChanged)
    // The directory these rows belong to. Consumers need it to distinguish
    // "this folder changed" from "a different folder is now loaded" — without
    // it, navigation looks identical to the playing file disappearing.
    Q_PROPERTY(QString folderPath READ folderPath NOTIFY folderPathChanged)
```

Find:

```cpp
    int count() const { return static_cast<int>(m_entries.size()); }
```

Replace with:

```cpp
    int count() const { return static_cast<int>(m_entries.size()); }
    QString folderPath() const { return m_folderPath; }
```

Find:

```cpp
signals:
    void countChanged();
```

Replace with:

```cpp
signals:
    void countChanged();
    void folderPathChanged();
```

Find:

```cpp
    QList<Entry> m_entries;
```

Replace with:

```cpp
    QString m_folderPath;
    QList<Entry> m_entries;
```

**File:** `src/models/folder_model.cpp`
**Location:** `applySnapshot` and `clear`

Find:

```cpp
void FolderModel::applySnapshot(const FolderSnapshot &snapshot)
{
    beginResetModel();
    m_entries = snapshot.entries;
    m_thumbnails.clear();
    m_thumbnailStates.clear();
    recount();
    endResetModel();
    emit countChanged();
}
```

Replace with:

```cpp
void FolderModel::applySnapshot(const FolderSnapshot &snapshot)
{
    beginResetModel();
    if (m_folderPath != snapshot.path) {
        m_folderPath = snapshot.path;
        // Emitted inside the reset so views never observe rows from one folder
        // labelled with another folder's path.
        emit folderPathChanged();
    }
    m_entries = snapshot.entries;
    m_thumbnails.clear();
    m_thumbnailStates.clear();
    recount();
    endResetModel();
    emit countChanged();
}
```

**File:** `src/controllers/playback_controller.cpp`
**Location:** `onModelStructureChanged` (line 290)

Find:

```cpp
    const QString identity = currentIdentity();
    // Did the playing file leave the model entirely? Then playback must stop:
    // its path no longer exists (rename/removal/navigation — §17.5).
    const bool playingGone = !identity.isEmpty()
                             && folderModel_->indexOfIdentity(identity) < 0;
```

Replace with:

```cpp
    const QString identity = currentIdentity();

    // Browsing to a different folder does NOT end playback (§13.1: "playback
    // continues while you browse elsewhere"). Only a model that still represents
    // the session's own folder can prove the file disappeared.
    const QString sessionPath = currentSession_ != nullptr ? currentSession_->path() : QString();
    const QString sessionFolder =
        sessionPath.isEmpty() ? QString() : QFileInfo(sessionPath).absolutePath();
    const bool sameFolder = !sessionFolder.isEmpty()
                            && folderModel_->folderPath() == sessionFolder;

    const bool playingGone = sameFolder && !identity.isEmpty()
                             && folderModel_->indexOfIdentity(identity) < 0;
```

**File:** `src/controllers/playback_controller.cpp`
**Location:** includes

Find:

```cpp
#include <QVector>
```

Replace with:

```cpp
#include <QFileInfo>
#include <QVector>
```

**Dependencies:** none, but AUD-006's `scheduleLoad` will now run for the newly browsed folder, which is the desired behaviour.
**Regression risks:** `tests/unit/test_reel_wiring.cpp` and `test_playback_lifecycle.cpp` may assert that navigating stops playback (they were codifying the defect) — they must be updated to assert the session survives and the Reel keeps its context. Behaviour change is user-visible and intended.
**Deferred verification:** start a video, navigate three folders away via the sidebar, then return; the Stage must stay open and playing, the Reel must still list the playing folder, and the Reel must re-anchor when the playing folder is browsed back to. Also delete the playing file externally while its folder is shown: playback must stop (the original intent must be preserved).
**Evidence confidence:** CONFIRMED.

**Implemented 2026-10-26 (FIX-PC-01, AUD-007 half) — IMPLEMENTED / STATICALLY REVIEWED / TESTING DEFERRED.** The defect was reproduced from source exactly as described: `onModelStructureChanged()` (now `src/controllers/playback_controller.cpp:291`) read the playing identity's absence from the *displayed* model as proof of deletion, and `FolderModel` had no way to say which directory its rows came from. `FolderModel` now publishes `Q_PROPERTY(QString folderPath READ folderPath NOTIFY folderPathChanged)`, maintained by `applySnapshot()` from `FolderSnapshot::path` (emitted inside the model reset) and dropped by `clear()` (which has no callers in this tree); the controller derives the session's containing directory with `QFileInfo(currentSession_->path()).absolutePath()` and requires `folderModel_->folderPath() == sessionFolder` before `playingGone` can be true, so genuine deletion while the folder is displayed still stops playback. **One deliberate deviation from the patch above:** the patch still rebuilds `context_`/the Reel unconditionally, which — with `playingGone` now false — would replace the playing folder's ordered list with the *browsed* folder's rows, contradicting this section's own "Expected implementation" ("the Reel keeps its own ordered list") and deferred check 4; a live session on a folder other than the displayed one therefore returns before the rebuild. Static evidence: `g++ -std=c++20 -fsyntax-only` exit 0 on both translation units with red controls (deliberate renames → exit 1; the `folderPathX` control also proves the new getter is the one actually called). No build, test run or launch. See `.project-memory/08-Sessions/2026-10-26-fix-aud-007-playback-navigation.md` and `DEFERRED_TESTS.md`.

---

### AUD-008 — The thumbnail cache is write-only; failure backoff is dead code

**Severity:** P1 — High (every navigation re-decodes every visible file; corrupt files are retried forever)
**Category:** Performance / Missing functionality
**Specification reference:** §18.2 ("Cache lookup → hit, valid → UI … **Serve** precedes **Extract**"), §18.4 ("A bounded in-memory LRU of decoded `QImage`s **plus the on-disk thumbnail cache** makes reverse scrolling immediate"), §22.3 (three cache layers), §22.6 ("first screenful within ~300 ms … from cached thumbnails"), §18.5 (retry backoff, "up to 2 retries, then the marker is terminal until the source changes").

**Code evidence**

`src/controllers/thumbnail_controller.cpp:238-259` (`runExtraction`) consults only the freedesktop cache and then extracts:

```cpp
ThumbnailExtraction ThumbnailController::runExtraction(const ThumbnailRequest &request) const
{
    // Cache first: a freedesktop hit must never pay for a decode, even when an
    // extractor has been injected (DESIGN §18.2 "Serve" precedes "Extract").
    if (!m_fdHome.isEmpty()) {
        const FreedesktopThumbnail native =
            findFreedesktopThumbnail(m_fdHome, request.path, request.mtimeNs);
        if (native.valid) { … return result; }
    }

    if (m_extractor) {
        return m_extractor(request);
    }

    return extractThumbnail(request.path, request.width, request.height);
}
```

`src/controllers/thumbnail_controller.cpp:192-207` writes the cache and the DB row (`publishThumbnail`, `upsertOk`) but nothing reads them back. `ThumbnailRepository::lookup()` (`repositories.cpp:494`) and `decideThumbnail()` (`thumbnails.cpp:424`) have **no production callers** (`grep -rn "lookup\|decideThumbnail" src/` → definitions and one test each). `ThumbnailScheduler::hasDecoded()/decoded()` are likewise unused in production.

**Current implementation**

Each `requestVisible()` enqueues every unsettled row of the viewport; nothing is settled from the previous visit because `applySnapshot()` clears `m_thumbnails`/`m_thumbnailStates` and `beginGeneration()` reconstructs the scheduler. Result: leaving and re-entering a folder re-decodes every visible file (FFmpeg open + seek + decode + scale), and a file with a persisted failure marker is retried on every navigation, forever. The PNG + DB cache grows without ever being read — pure cost.

**Expected implementation**

Before extracting: load the `thumbnails` row, run `decideThumbnail()` against the *current* `(mtime_ns, size)`, and either serve the cached PNG (worker-side `QImage` load), skip a terminal failure, or extract. After extraction: persist and apply as today.

**Root cause** — CONFIRMED. The lookup half of the pipeline was specified and unit-tested in isolation (`test_thumb_failure.cpp`, `test_thumb_scheduler.cpp`) but never wired into `ThumbnailController`; `runExtraction` was extended for the freedesktop cache only.

**Correct architecture**

`ThumbnailController::runExtraction` is the single "serve → extract" decision point and runs on the media pool (it already does). It owns the policy call; the repository owns persistence; the scheduler owns the decoded LRU. No new component is needed.

**Complete permanent resolution**

Insert the QG-cache branch before the freedesktop branch, and return a terminal marker for failures that the policy says to skip.

**Implementation-ready patch**

**File:** `src/controllers/thumbnail_controller.cpp`
**Location:** `runExtraction` (lines 238-259)

Find:

```cpp
ThumbnailExtraction ThumbnailController::runExtraction(const ThumbnailRequest &request) const
{
    // Cache first: a freedesktop hit must never pay for a decode, even when an
    // extractor has been injected (DESIGN §18.2 "Serve" precedes "Extract").
    if (!m_fdHome.isEmpty()) {
        const FreedesktopThumbnail native =
            findFreedesktopThumbnail(m_fdHome, request.path, request.mtimeNs);
        if (native.valid) {
            ThumbnailExtraction result;
            result.image = native.image;
            result.sourceMtimeNs = request.mtimeNs;
            result.sourceSize = request.size;
            return result;
        }
    }

    if (m_extractor) {
        return m_extractor(request);
    }

    return extractThumbnail(request.path, request.width, request.height);
}
```

Replace with:

```cpp
ThumbnailExtraction ThumbnailController::runExtraction(const ThumbnailRequest &request) const
{
    // 1. QG's own cache, validated by (identity, mtime_ns, size) — §18.2/§22.3.
    //    Without this branch the PNG + DB cache was write-only and every
    //    navigation paid FFmpeg for every visible row.
    if (m_database != nullptr) {
        ThumbnailRepository repository(*m_database);
        ThumbnailRepository::Record record;
        if (repository.lookup(request.identity, &record)) {
            ThumbnailCacheState cache;
            cache.present = true;
            cache.state = record.state;
            cache.failCount = record.failCount;
            cache.sourceMtimeNs = record.sourceMtimeNs;
            cache.sourceSize = record.sourceSize;

            const ThumbnailAction action =
                decideThumbnail(cache, request.mtimeNs, request.size);

            if (action == ThumbnailAction::Serve && !record.cachePath.isEmpty()) {
                QImage cached(record.cachePath);
                if (!cached.isNull()) {
                    ThumbnailExtraction hit;
                    hit.image = cached;
                    hit.sourceMtimeNs = request.mtimeNs;
                    hit.sourceSize = request.size;
                    return hit;
                }
                // The row points at a PNG that vanished (cache cleaned, path
                // moved): fall through and regenerate rather than failing.
            } else if (action == ThumbnailAction::Terminal) {
                // Bounded retries exhausted: report the persisted failure
                // without touching the decoder (§18.5).
                ThumbnailExtraction failed;
                failed.sourceMtimeNs = request.mtimeNs;
                failed.sourceSize = request.size;
                failed.error = QStringLiteral("Thumbnail generation failed previously");
                return failed;
            }
        }
    }

    // 2. Native freedesktop thumbnail: instant and free (§18.2).
    if (!m_fdHome.isEmpty()) {
        const FreedesktopThumbnail native =
            findFreedesktopThumbnail(m_fdHome, request.path, request.mtimeNs);
        if (native.valid) {
            ThumbnailExtraction result;
            result.image = native.image;
            result.sourceMtimeNs = request.mtimeNs;
            result.sourceSize = request.size;
            return result;
        }
    }

    // 3. Extract (§18.3).
    if (m_extractor) {
        return m_extractor(request);
    }

    return extractThumbnail(request.path, request.width, request.height);
}
```

**File:** `src/controllers/thumbnail_controller.cpp`
**Location:** `applyResult`, the success branch (lines 192-207)

Find:

```cpp
    if (extraction.ok()) {
        m_scheduler.storeDecoded(request.identity, extraction.image);

        // Publish the decoded frame as a file and expose it as a URL. QML's
        // `Image.source` is a QUrl: handing it a QImage (as this used to) yields
        // an empty source, so every "ready" row rendered a blank well (VIS-010).
        ThumbnailKey key;
        key.identity = request.identity;
        key.mtimeNs = request.mtimeNs;
        key.size = request.size;
        const QString published = publishThumbnail(m_cacheDir, key, extraction.image);
```

Replace with:

```cpp
    if (extraction.ok()) {
        m_scheduler.storeDecoded(request.identity, extraction.image);

        // Re-serving a cached PNG needs no re-encode: the file is already the
        // final artefact, and re-writing it would cost a synchronous PNG encode
        // on the GUI thread for every row (see follow-up FIX-TH-02 in the roadmap).
        ThumbnailKey key;
        key.identity = request.identity;
        key.mtimeNs = request.mtimeNs;
        key.size = request.size;
        const QString existing = qgThumbnailPath(m_cacheDir, key);
        const QString published =
            (QFileInfo::exists(existing) && publishedFromCache(request))
                ? existing
                : publishThumbnail(m_cacheDir, key, extraction.image);
```

> `publishedFromCache(request)` is a one-line predicate added to the header
> (`bool publishedFromCache(const ThumbnailRequest &r) const { return r.fromCache; }`)
> with `bool fromCache = false;` set on the `ThumbnailExtraction` returned by step 1
> and step 2 of `runExtraction`. This keeps the "did we just decode?" decision in the
> extraction result instead of re-querying the DB.

**Dependencies:** AUD-002 (worker delivery) should land first; `decideThumbnail` and `ThumbnailRepository::lookup` already exist.
**Regression risks:** a cache hit now emits `thumbnailReady` without touching FFmpeg, so `test_thumb_wiring.cpp` expectations about extractor call counts will change. Corrupt-file tests must expect the *terminal* path after two failures instead of a third extraction. `publishThumbnail` stays on the GUI thread in this patch; FIX-TH-02 (move encode+write to the worker) is a separate increment listed in the roadmap (§10).
**Deferred verification:** warm start on a folder of 200 videos: second visit must not open FFmpeg (verify via `strace -f -e openat` on media files or an instrumented extractor counter) and must paint immediately. Corrupt file: after two attempts it must show the amber failure placeholder without further decode attempts across navigations.
**Evidence confidence:** CONFIRMED.

**Implemented 2026-10-26 (FIX-TH-01) — IMPLEMENTED / STATICALLY REVIEWED / TESTING DEFERRED.** The defect and the "no production callers" claim were re-verified by grep: `runExtraction()` consulted only the freedesktop cache, `publishThumbnail()`/`upsertOk` wrote the PNG + DB row with no reader, and `ThumbnailRepository::lookup()` / `decideThumbnail()` / `ThumbnailScheduler::hasDecoded()` had none. Now the persisted row is read, validated against the current `(mtime_ns, size)` through `decideThumbnail()`, and turned into Serve (load the PNG, do not re-encode — `ThumbnailExtraction::CacheOrigin::QgCache`), Terminal (report the failure with no decode) or Extract (freedesktop → extractor, unchanged). **Three deviations, all forced by the tree:** (1) the patch's `failed.error = QStringLiteral(…)` does not compile (`Error` has no `QString` assignment — proven by a control run), so `Error::make(ErrorCode::OperationFailed, …, request.path)` is used; (2) the patch's `publishedFromCache(request)` is impossible because `runExtraction()` takes a `const ThumbnailRequest &` and returns by value, so the origin lives on `ThumbnailExtraction` and both serve branches set it; (3) the patch puts the DB call inside `runExtraction()`, which runs on the media pool, but `Database` is documented as one **thread-affine** connection (`persistence/database.hpp:5-7`) with a single instance in the app and only GUI-thread consumers (`MediaProbeService` resolves its cache in its GUI-thread completion handler) — so the DB query moved to a GUI-thread `consultQgCache()` while the PNG read stays on the pool where the patch put it. Two further refinements: `retry()` now marks its request `manualRetry` (otherwise the §18.5 escape hatch would have been refused by the very policy it exists for), and the cache origin is an enum rather than a bool — a freedesktop hit must still be written through to QG's cache, and a bare `fromCache` flag would have re-used an older same-key QG file and broken `test_thumb_delivery`'s delivered-image assertion (that class shares one cache dir across methods and an earlier method has already published that key). Static evidence: `g++ -std=c++20 -fsyntax-only` exit 0 on both translation units with five red controls (the audit's `error` line, the decision call, `manualRetry`, `cacheOrigin`, `cachedPngPath`); no DB access remains in `runExtraction()`; production wiring confirmed at `composition.cpp:103-106`; no thumbnail test sets a database, so their extraction-count expectations are unchanged. No build, test run or launch. Note the retry arithmetic: the marker becomes terminal after the **third** failed decode (`fail_count > kMaxThumbnailFailRetries`, per `qg_thumb_failure.cpp:63-68`), not the second. See `.project-memory/08-Sessions/2026-10-26-fix-aud-008-thumbnail-cache.md` and `DEFERRED_TESTS.md`.

---

### AUD-009 — The Stage is an overlay; the grid never reflows beneath it

**Severity:** P1 — High (the defining "bloom in place" behaviour of the product)
**Category:** Specification / Architecture
**Specification reference:** §1.2 ("The playing card *blooms* in place into an inline player; the rest of the folder reflows beneath it"), §3.4 ("Stage: a real widget … inserted into the grid's layout flow at the position of the playing item, spanning all columns. Remaining cards reflow below"), playback §2.1 item 7 ("Reflow the remaining file cards beneath the expanded Stage"), §2.2 ("Reflow remaining cards below the Stage without overlapping playback controls").

**Code evidence**

`qml/QG/Main.qml:712-742` — the Stage is a sibling of `ContentView` inside the content region, anchored to the top and simply drawn over it:

```qml
                Stage {
                    id: stage
                    objectName: "stageRegion"
                    anchors.top: parent.top
                    anchors.left: parent.left
                    visible: window.sessionActive
                    cinema: window.cinema
```

`qml/QG/ContentView.qml:429-430` — the grid starts at the top of the region regardless of playback:

```qml
        anchors.fill: parent
        anchors.topMargin: contentView.pad
```

`grep -n "stage\|reserved" qml/QG/ContentView.qml` → no reservation of vertical space anywhere.

**Current implementation**

The Stage covers the first N pixels of the grid. Cards underneath are hidden, not pushed down; the first row is partly or fully occluded, scrolling does not account for the Stage, and the keyboard focus cursor can move onto cards that are invisible behind the player. On a narrow window the Stage (60 % of the content height) hides most of the first screen.

**Expected implementation**

The Stage occupies real layout space at the top of the content surface; the grid's scrollable area begins below it, so every card remains reachable and the first visible row is fully visible.

**Root cause** — CONFIRMED. The Stage was implemented as a chrome overlay ("Stage overlay:" comment at `Main.qml:709`) rather than as a flow element. `ContentView` grew an independent `anchors.fill` layout, so no shared layout exists to reflow.

**Correct architecture**

`ContentView` publishes a `reservedTop` inset that it applies to all three presentations; `Main.qml` binds it to the Stage's height while a session is active. The Stage keeps its own anchor and sizing math (16:9, ≤60 % height), so no binding loop is introduced (Stage height depends on the region, not on the inset).

**Complete permanent resolution**

Add the inset, apply it to the three views, bind it from the shell, and remove the stale "overlay" wording.

**Implementation-ready patch**

**File:** `qml/QG/ContentView.qml`
**Location:** after `property int targetCardWidth: 240`

Find:

```qml
    // Responsive target supplied by the shell's §3.5 table (Main.qml). The
    // default is the wide-layout value so the view is deterministic standalone.
    property int targetCardWidth: 240
```

Replace with:

```qml
    // Responsive target supplied by the shell's §3.5 table (Main.qml). The
    // default is the wide-layout value so the view is deterministic standalone.
    property int targetCardWidth: 240

    // Vertical space owned by an inline surface above the grid (the Stage).
    // The shell binds this to the Stage's height so the cards reflow beneath it
    // instead of being covered (DESIGN.md §3.4, playback §2.1.7, §2.2).
    property real reservedTop: 0
```

**File:** `qml/QG/ContentView.qml`
**Location:** `grid`, `compactList`, and the manifest list anchor blocks

Find (three occurrences — lines 429-430, 523-524, and the manifest list):

```qml
        anchors.fill: parent
        anchors.topMargin: contentView.pad
```

Replace with (all three):

```qml
        anchors.fill: parent
        anchors.topMargin: contentView.pad + contentView.reservedTop
```

**File:** `qml/QG/Main.qml`
**Location:** the `Stage` block (lines 709-742) and the `contentSurfaceRegion`

Find:

```qml
                // Stage overlay: inline playback surface (DESIGN.md §8.1, §9).
                // Visible when a playback session is active; in Cinema it fills
                // the whole window (P54), otherwise it spans content width.
                Stage {
```

Replace with:

```qml
                // Inline Stage: a real element of the content flow (DESIGN.md
                // §3.4). ContentView reserves its height (`reservedTop`) so the
                // cards reflow beneath it rather than being covered; in Cinema
                // the Stage fills the window and the inset is irrelevant.
                Stage {
```

**File:** `qml/QG/Main.qml`
**Location:** the `ContentView { … }` instantiation (line 565-573)

Find:

```qml
ContentView {
                    id: contentView
                    anchors.fill: parent
                    model: window.folderModel
```

Replace with:

```qml
ContentView {
                    id: contentView
                    anchors.fill: parent
                    // Reflow, don't overlay: the grid starts below the Stage.
                    reservedTop: (window.sessionActive && !window.cinema && stage.visible)
                                 ? stage.height + Theme.space3 : 0
                    model: window.folderModel
```

**Dependencies:** the `Stage.height` binding (already present, `Stage.qml:60-63`) must not read `contentView` (it does not).
**Regression risks:** `tst_contentview.qml` and `tst_grid_geometry.qml` measure occupied width and cell geometry with `reservedTop == 0`, so they are unaffected; `tst_stage.qml` may assert the Stage's `y == 0` (unchanged). Scrolling: the visible-range reporter uses `contentY`, which now starts under the Stage — the first screenful reports fewer rows, which is correct.
**Deferred verification:** play a video in a folder with 40 entries; the first grid row must be fully visible below the Stage, scrolling must reveal every card, and closing the Stage must restore the exact previous scroll position and selection (§2.3).
**Executed 2026-10-25 (FIX-STAGE-01, reflow half):** `ContentView` publishes `reservedTop` (+ a derived `topInset = pad + reservedTop`) and applies it to all three presentations (grid `anchors.topMargin`, compact list, and the Manifest container that carries the sortable header); `Main.qml` binds `reservedTop: (sessionActive && !cinema && stage.visible) ? stage.height + Theme.space3 : 0` and the stale "Stage overlay" comment is replaced. Verified with a harness that composes the real `Stage` (qg) and the real `ContentView` exactly as `Main.qml` does (`~/.hermes/cache/scratch/aud009/aud009_reflow.qml`, 6/6, 1200×620 region, 40 entries): the Stage keeps its own sizing (`min(0.6·h, 9/16·w + space4)` = 372 px), `reservedTop` = 384 = `stage.height + Theme.space3`, `grid.y == topInset == pad + reservedTop == 408`, the first card's top maps to exactly `reservedTop + pad` (**408 ≥ 372** → row 0 is fully below the Stage, not covered), with no session `reservedTop == 0` and the first card returns to `pad`; scrolling to the end leaves **no part of the last card under the Stage's band** and keeps it inside the viewport; closing the Stage releases the reservation and leaves `contentY` (1400) and the selection (`id17`) intact, re-opening preserves both; in Cinema `reservedTop == 0` while the Stage fills the region. **A real defect was found and fixed by this harness:** the grid's visible-range reporter subtracted the top margin from `contentY`, which was harmless while the margin was the 24 px pad but dropped the first screen entirely once the inset grew (a ~190 px viewport with `contentY == 0` produced `last < first` and *no* report at all, so no thumbnail/probe requests while playing). The reporter now uses `[contentY, contentY + height)` in content coordinates; the first screen reports rows 0–7 with the Stage open and 0–15 when it is released.
**Evidence confidence:** CONFIRMED.

---

### AUD-010 — Cinema never enters fullscreen

**Severity:** P1 — High (the entire Cinema intensity is a chrome-collapse in a window)
**Category:** Missing functionality / Specification
**Specification reference:** playback §4.2 ("Upon activation: 1. **Enter fullscreen using the application's existing window.** 2. Hide the standard file-browser interface. 3. Expand the video playback area to the available fullscreen dimensions"), §4.5 (leave fullscreen on exit), §25.7.

**Code evidence**

`qml/QG/Main.qml:900-910`:

```qml
    Connections {
        target: window.playbackController
        function onCinemaActiveChanged() {
            if (window.cinema)
                stage.forceActiveFocus()
            else
                contentView.forceActiveFocus()
        }
```

`grep -rn "showFullScreen\|showMaximized\|Window.FullScreen\|setVisibility" qml/ src/` → **no matches**.

**Current implementation**

Entering Cinema collapses the sidebar, path bar, context strip, reel and status strip (they are bound to `!window.cinema`) and the Stage expands to the *window* size. The window keeps its decorations and its 1280×800 geometry, so "Cinema" is a maximised-ish layout, not fullscreen; the top overlay's Explicit Close is the only way out other than Esc/F11.

**Expected implementation**

The existing `ApplicationWindow` goes fullscreen (`visibility: Window.FullScreen`) on entering Cinema and returns to its previous visibility on exit; no second window is created (§1.5); playback state is untouched.

**Root cause** — CONFIRMED. The layout half of Cinema was implemented; the window-state half was not.

**Correct architecture**

The shell owns window presentation; `PlaybackController` owns `cinemaActive` (already). `Main.qml` reacts to the property change by setting `visibility`, remembering the previous value so exiting restores windowed/maximised state faithfully.

**Complete permanent resolution**

Add the visibility transition with a remembered prior state, and wake the transport overlays on entry (§4.4 "Initially display both overlays when entering Cinema").

**Implementation-ready patch**

**File:** `qml/QG/Main.qml`
**Location:** near `property bool reelVisible: false` (line 64)

Find:

```qml
    // Reel visibility: auto-opens with playback, toggled with Ctrl+R, closed
    // with its ✕ (DESIGN.md §10.2).
    property bool reelVisible: false
```

Replace with:

```qml
    // Reel visibility: auto-opens with playback, toggled with Ctrl+R, closed
    // with its ✕ (DESIGN.md §10.2).
    property bool reelVisible: false

    // Window state to restore when Cinema exits (DESIGN.md playback §4.2/§4.5:
    // fullscreen uses the existing window; exiting returns to the prior state).
    property int windowedVisibility: Window.Windowed
```

**File:** `qml/QG/Main.qml`
**Location:** the `Cinema / Stage focus context` Connections block (lines 900-910)

Find:

```qml
    Connections {
        target: window.playbackController
        function onCinemaActiveChanged() {
            if (window.cinema)
                stage.forceActiveFocus()
            else
                contentView.forceActiveFocus()
        }
```

Replace with:

```qml
    Connections {
        target: window.playbackController
        function onCinemaActiveChanged() {
            if (window.cinema) {
                // §4.2: fullscreen the existing window — never a new one.
                if (window.visibility !== Window.FullScreen) {
                    window.windowedVisibility = window.visibility
                }
                window.visibility = Window.FullScreen
                stage.forceActiveFocus()
                // §4.4: both overlays are visible on entry.
                if (stage.wakeControls)
                    stage.wakeControls()
            } else {
                window.visibility = (window.windowedVisibility === Window.FullScreen)
                                    ? Window.Windowed : window.windowedVisibility
                contentView.forceActiveFocus()
            }
        }
```

**Dependencies:** AUD-011 (Esc semantics) should land with it, otherwise Esc from fullscreen also stops playback.
**Regression risks:** `tst_cinema.qml` runs offscreen and may assert `window.visibility`; `Window.FullScreen` under `QT_QPA_PLATFORM=offscreen` is a no-op, so tests should assert the property value rather than a real screen size. A user who quits while in Cinema persists nothing (window geometry is not persisted at all — see AUD-028), so no corruption is possible.
**Deferred verification:** on X11, F11 must remove decorations and fill the screen; Esc must restore the previous geometry exactly; double-click on the video must toggle; playback must not restart or re-buffer (§25.7); the pointer must hide with the overlays after 2 s.
**Evidence confidence:** CONFIRMED.

**Executed 2026-10-27 (FIX-STAGE-01, AUD-010 half):** `qml/QG/Main.qml` carries `property int windowedVisibility: Window.Windowed` and its `onCinemaActiveChanged` handler now fullscreens the existing window: on entry it remembers `window.visibility` (skipping the write when already `FullScreen`), sets `window.visibility = Window.FullScreen`, calls `stage.forceActiveFocus()` and then `stage.wakeControls()` (§4.4 — the Cinema top bar's `opacity` is bound to `transport.opacity`, so one wake shows both overlays); on exit it restores `windowedVisibility` (collapsing a remembered fullscreen to `Windowed`, so Cinema can never leave the app in a state it did not create) and returns focus to the content view. `import QtQuick.Window` was added so the `Window.*` enum is declared rather than inherited from `QtQuick.Controls`. 
Static review only (policy): `qmllint 6.10.2` on the finished file against a control with this change reverse-applied and the sibling components in place gives 141 vs 139 diagnostics, the only additions being two `Member "wakeControls" not found on type "Stage"` warnings that the pre-fix file already emits twice for the same method (`Main.qml:1454-1460`) — the `qg` module has no `qmldir`, so qmllint cannot resolve local component types at all. No `Window`, `visibility` or `windowedVisibility` diagnostic appears, and linting the same patch *without* the import is identical. The diff is one file, adds no play/stop/seek call (§4.2.6 continuity), creates no second window, and leaves the Cinema layout bindings, Stage sizing, cursor auto-hide, Reel restore and the AUD-009 `reservedTop` inset untouched. **Not promoted to VERIFIED:** the §25.7 X11 behaviour, the offscreen `visibility` property assertion and the `tst_cinema` re-run are recorded in `DEFERRED_TESTS.md` FIX-STAGE-01. See [[2026-10-27-fix-aud-010-cinema-fullscreen]].


---

### AUD-011 — `Esc` in Cinema stops playback instead of returning to Stage

**Severity:** P1 — High (destroys the session on the most natural "get me out" key)
**Category:** Incorrect implementation / Specification
**Specification reference:** playback §4.5 ("**Return to Stage** — Triggered by: Esc, F11, F when playback has focus, Double-clicking the video, Exit-fullscreen control … Continue playback without interruption"), §9 state table, §25.7 ("Exiting fullscreen returns to the exact Stage/grid state … Esc, F11, F, and double-click all toggle consistently").

**Code evidence**

`qml/QG/Main.qml:1025`:

```qml
    Shortcut { objectName: "cinemaEscShortcut"; sequence: "Esc"; enabled: window.sessionActive && !window.textEditing; onActivated: window.keyboard ? window.keyboard.dispatch("playback.stop") : window.escapePlayback() }
```

`src/controllers/keyboard_framework.cpp:172-173`:

```cpp
    } else if (actionId == QLatin1String("playback.stop")) {
        if (playbackController_ != nullptr) playbackController_->stop();
```

`qml/QG/Main.qml:150-158` — the *correct* helper, unreachable in the app because `keyboard` is always injected (`app.cpp:250`):

```qml
    function escapePlayback() {
        if (!window.playbackController)
            return
        if (window.cinema)
            window.playbackController.exitCinema()
        else if (window.sessionActive)
            window.playbackController.stop()
    }
```

**Current implementation**

Esc always dispatches `playback.stop` → `PlaybackController::stop()` → `manager_->stop()`, which unloads the file, clears Cinema and hides the Stage. In Cinema this is the "Close Playback" behaviour (§4.5 second table), i.e. the destructive action is bound to the non-destructive key.

**Expected implementation**

One action id (`playback.escape`) whose behaviour depends on the presentation state: in Cinema → `exitCinema()`; in Stage → `stop()`. The QML fallback stays for the no-keyboard test case.

**Root cause** — CONFIRMED. Two different Esc semantics were collapsed into one action id; `escapePlayback()` was written correctly and then bypassed by the keyboard router.

**Correct architecture**

`KeyboardFramework` owns the *decision* (it already owns `playbackController_`, so it can read `cinemaActive`); `Main.qml` binds Esc to a single action id and keeps no policy of its own.

**Implementation-ready patch**

**File:** `src/controllers/keyboard_framework.cpp`
**Location:** the action table (line ~59) and the dispatch chain (line ~170)

Find:

```cpp
        QStringLiteral("playback.stop"),
```

Replace with:

```cpp
        // Esc: leave Cinema first, stop playback only when already in Stage
        // (DESIGN.md playback §4.5 vs §4.5 "Close Playback").
        QStringLiteral("playback.escape"),
        QStringLiteral("playback.stop"),
```

Find:

```cpp
    } else if (actionId == QLatin1String("playback.stop")) {
        if (playbackController_ != nullptr) playbackController_->stop();
    }
```

Replace with:

```cpp
    } else if (actionId == QLatin1String("playback.escape")) {
        if (playbackController_ != nullptr) {
            if (playbackController_->cinemaActive()) {
                playbackController_->exitCinema(); // §4.5 Return to Stage
            } else {
                playbackController_->stop();       // §4.5 Close Playback / Stage Esc
            }
        }
    } else if (actionId == QLatin1String("playback.stop")) {
        // Explicit close only (the Stage/Cinema ✕ and the failure-state button).
        if (playbackController_ != nullptr) playbackController_->stop();
    }
```

**File:** `qml/QG/Main.qml`
**Location:** line 1025

Find:

```qml
    Shortcut { objectName: "cinemaEscShortcut"; sequence: "Esc"; enabled: window.sessionActive && !window.textEditing; onActivated: window.keyboard ? window.keyboard.dispatch("playback.stop") : window.escapePlayback() }
```

Replace with:

```qml
    Shortcut { objectName: "cinemaEscShortcut"; sequence: "Esc"; enabled: window.sessionActive && !window.textEditing; onActivated: window.keyboard ? window.keyboard.dispatch("playback.escape") : window.escapePlayback() }
```

**Dependencies:** AUD-010 (fullscreen) so that leaving Cinema also leaves fullscreen.
**Regression risks:** `tst_keyboard.qml` / `test_keyboard.cpp` assert the action table contents; adding `playback.escape` changes the count. The pre-existing "Keyboard duplicate Alt+Left" failure noted in `DEFERRED_TESTS.md` is unrelated.
**Deferred verification:** F11 → Cinema → Esc: still playing, window restored, Reel visible again (§4.5). In Stage, Esc: playback stops, grid restored with the previous selection and scroll. Space/K continues to pause either way.
**Evidence confidence:** CONFIRMED.

---

### AUD-012 — Nothing advances to the next file at end-of-file

**Severity:** P1 — High (a video player that stops at the end of every file)
**Category:** Missing functionality
**Specification reference:** playback §4.6 ("When the user activates Next **or playback advances to another video** while in Cinema: Remain in Cinema. Load the new video into the existing player … Cinema must not automatically collapse between media items"), §5.4 (Reel is the playback order), §25.7 ("Auto-advancing to the next item keeps fullscreen active"), §19.4.

**Code evidence**

`src/playback/playback_manager.cpp:74-85` sets the terminal state and stops the tick timer:

```cpp
    } else if (name == QLatin1String("eof-reached")) {
        if (value.toBool()
                && s->state() != PlaybackSession::Idle
                && s->state() != PlaybackSession::Error
                && s->state() != PlaybackSession::Ended) {
            flushPosition();
            if (tickTimer_) tickTimer_->stop();
            s->setState(PlaybackSession::Ended);
        }
    }
```

`grep -rn "Ended" src/ qml/` → the only consumer is `Main.qml:913` (`onPlaybackEnded` hides the Reel). No `PlaybackController` code path reacts to `Ended`; `PlaybackController` never connects to `PlaybackSession::stateChanged` at all.

**Current implementation**

At EOF the last frame stays on screen (`keep-open=yes`), the session enters `Ended`, and nothing happens. The user must click Next for every subsequent episode. In Cinema, playback simply stops at the end of each item.

**Expected implementation**

On the transition to `Ended`, the controller advances through its own ordered context (`PlaybackContext::step(+1)` → `playCurrent()`), preserving Stage/Cinema; at the end of the context it stops with the "End of folder" toast (§19.4) instead of pretending to advance.

**Root cause** — CONFIRMED. `Ended` was modelled as a terminal state for the transport, and no owner was assigned to the "what happens next" policy.

**Correct architecture**

`PlaybackController` already owns the ordered context and the Stage/Cinema state, so auto-advance belongs there (not in `PlaybackManager`, which must stay presentation-agnostic). The policy is a property (`autoAdvance`, default true) so the "wrap or stop" preference §19.4 mentions can be added later without moving the decision.

**Complete permanent resolution**

Connect the session's `stateChanged` once per session acquisition, advance on `Ended`, and stop with a boundary signal at the end.

**Implementation-ready patch**

**File:** `src/controllers/playback_controller.hpp`
**Location:** properties and private members

Find:

```cpp
    Q_PROPERTY(bool canGoPrevious READ canGoPrevious NOTIFY contextChanged)
    Q_PROPERTY(bool canGoNext READ canGoNext NOTIFY contextChanged)
```

Replace with:

```cpp
    Q_PROPERTY(bool canGoPrevious READ canGoPrevious NOTIFY contextChanged)
    Q_PROPERTY(bool canGoNext READ canGoNext NOTIFY contextChanged)
    // Continuous playback (DESIGN.md playback §4.6/§25.7): at EOF, advance to
    // the next item in the ordered context without leaving Stage or Cinema.
    Q_PROPERTY(bool autoAdvance READ autoAdvance WRITE setAutoAdvance NOTIFY autoAdvanceChanged)
```

Find:

```cpp
    ReelEntry reelEntryFor(int contextIndex) const;
```

Replace with:

```cpp
    ReelEntry reelEntryFor(int contextIndex) const;
    void attachSession();
    void onSessionStateChanged(PlaybackSession::State state);
    bool autoAdvance_ = true;
```

**File:** `src/controllers/playback_controller.cpp`
**Location:** `play()` and `playCurrent()` both end with `currentSession_ = manager_->session();`; add the connection there and implement the handler

Find (both occurrences):

```cpp
    manager_->openWithIdentity(path, identity, start);
    currentSession_ = manager_->session();
```

Replace with (both occurrences):

```cpp
    manager_->openWithIdentity(path, identity, start);
    currentSession_ = manager_->session();
    attachSession();
```

Find (after `applyResumeDecision`, before `modelDurationSeconds`):

```cpp
// Probed duration in seconds, or <= 0 when unknown (never resume on a guess).
```

Replace with:

```cpp
void PlaybackController::attachSession()
{
    if (currentSession_ == nullptr) {
        return;
    }
    // One connection per acquisition; the session is a process singleton, so a
    // duplicate connection would advance twice per EOF.
    disconnect(currentSession_, &PlaybackSession::stateChanged,
               this, &PlaybackController::onSessionStateChanged);
    connect(currentSession_, &PlaybackSession::stateChanged,
            this, &PlaybackController::onSessionStateChanged);
    emit playingChanged();
    emit currentSessionChanged();
}

void PlaybackController::onSessionStateChanged(PlaybackSession::State state)
{
    // The transport mirrors `playing`, so it must be re-read whenever mpv
    // changes the state behind our back (pause, EOF, error, external stop).
    emit playingChanged();

    if (state != PlaybackSession::Ended || !autoAdvance_) {
        return;
    }
    // Continuous playback (§4.6): stay in Stage/Cinema, load the next item.
    if (context_ != nullptr && context_->step(1)) {
        playCurrent();
    } else {
        emit endOfFolder();
        stop();
    }
}

bool PlaybackController::autoAdvance() const { return autoAdvance_; }

void PlaybackController::setAutoAdvance(bool enabled)
{
    if (autoAdvance_ == enabled) {
        return;
    }
    autoAdvance_ = enabled;
    emit autoAdvanceChanged();
}

// Probed duration in seconds, or <= 0 when unknown (never resume on a guess).
```

**File:** `src/controllers/playback_controller.hpp`
**Location:** signals

Find:

```cpp
    // Prev/Next hit a boundary — surfaced as an "End of folder" toast (§23.8).
    void endOfFolder();
```

Replace with:

```cpp
    // Prev/Next hit a boundary — surfaced as an "End of folder" toast (§23.8).
    void endOfFolder();
    void autoAdvanceChanged();
```

**Dependencies:** AUD-007 (navigation must not kill the session) — otherwise auto-advance would fight the model-reset stop. AUD-026 covers `playing`/`playingChanged`, which this patch starts emitting correctly.
**Regression risks:** `test_resume_playback.cpp` and `test_transport_state.cpp` drive EOF; auto-advance will now load the *next* file at the end of a one-item context, then stop. Set `autoAdvance=false` at the top of those tests (or use a single-item context and assert the boundary). Infinite loops are impossible: `step(1)` returns false at the end.
**Deferred verification:** play a three-file folder, let the first end: the second must start within ~1 s, Stage geometry unchanged, Cinema still active, Reel highlight moved. Let the last end: playback stops with `End of folder` and no further load. Repeat with `Favorites` filter on.
**Evidence confidence:** CONFIRMED.

---

### AUD-013 — Cut/Copy are gated on playback and resolve identities as row indexes

**Severity:** P1 — High (clipboard actions are dead in Browse and operate on the wrong file when alive)
**Category:** Incorrect implementation / Missing functionality
**Specification reference:** §6.2 / §13.2 ("Cut / Copy / Paste | Ctrl+X / Ctrl+C / Ctrl+V" — no playback precondition), §11.1 ("Move/Copy are implemented as clipboard + paste"), §23.9-style path identity rules, §25.9.

**Code evidence**

`qml/QG/Main.qml:964-987`:

```qml
    Shortcut { objectName: "kbCut"; sequence: "Ctrl+X"; enabled: !window.cinema && !window.textEditing && window.sessionActive; onActivated: {
        if (window.selectionState && window.actionsController) {
            const ids = window.selectionState ? (window.selectionState.selectedIdentities ? window.selectionState.selectedIdentities() : []) : []
            if (ids.length > 0) {
                const path = contentView.pathForIndex(ids[0] ? ids[0] : -1)
```

`qml/QG/ContentView.qml:188-190`:

```qml
    function pathForIndex(index) {
        return (model && model.pathAt) ? model.pathAt(index) : ""
    }
```

**Current implementation**

Two independent defects in the same handler:

1. `enabled: … && window.sessionActive` means Ctrl+X/Ctrl+C do nothing unless a video is playing. Clipboard work in Browse — the normal case — is impossible.
2. `selectedIdentities()` returns identity fingerprints (`"inode:801:c21f"`), which are passed to `pathAt(int)`. QML converts the string to `0`, so the operation silently targets **row 0** (or nothing when `ids[0]` is falsy). Cutting file B while file A is row 0 moves A.

Ctrl+V is correctly enabled in Browse but consumes whatever `clipboardPaths()` holds, so the failure is invisible until a paste moves the wrong file.

**Expected implementation**

Clipboard shortcuts are enabled whenever the content surface has a selection and no text editor holds focus; paths are resolved **by identity** through the model.

**Root cause** — CONFIRMED. The shortcut was copy-pasted from a playback-shaped block (`sessionActive` gate) and the identity/index distinction was lost when `SelectionState` was moved to identity keys while `ContentView` kept index-based helpers.

**Correct architecture**

`FolderModel` gains `Q_INVOKABLE QString pathForIdentity(const QString&)` (and `identityAt(row)` already exists). `ContentView` exposes `pathsForIdentities(list)` so QML never converts an identity to an integer. The shortcut gates on `selectionState.selectedCount > 0`.

**Complete permanent resolution**

Add the by-identity accessor, the QML helper, and fix both shortcuts.

**Implementation-ready patch**

**File:** `src/models/folder_model.hpp`
**Location:** QML-facing row access block

Find:

```cpp
    Q_INVOKABLE QString pathAt(int row) const;
```

Replace with:

```cpp
    Q_INVOKABLE QString pathAt(int row) const;
    // Identity → path. Clipboard/menu actions carry identities, never row
    // indexes: passing an identity to pathAt() silently resolved to row 0.
    Q_INVOKABLE QString pathForIdentity(const QString &identity) const;
```

**File:** `src/models/folder_model.cpp`
**Location:** after `pathAt`

Find:

```cpp
QString FolderModel::nameAt(int row) const
```

Replace with:

```cpp
QString FolderModel::pathForIdentity(const QString &identity) const
{
    const int row = indexOfIdentity(identity);
    return row >= 0 ? m_entries.at(row).path : QString();
}

QString FolderModel::nameAt(int row) const
```

**File:** `qml/QG/ContentView.qml`
**Location:** next to `pathForIndex` (line 188)

Find:

```qml
    function pathForIndex(index) {
        return (model && model.pathAt) ? model.pathAt(index) : ""
    }
```

Replace with:

```qml
    function pathForIndex(index) {
        return (model && model.pathAt) ? model.pathAt(index) : ""
    }

    // Identities (not indexes) in, paths out — the clipboard and the context
    // menu speak identities, so they must never be fed to pathAt().
    function pathsForIdentities(ids) {
        const out = []
        if (!model || !model.pathForIdentity || !ids)
            return out
        for (let i = 0; i < ids.length; ++i) {
            const p = model.pathForIdentity(ids[i])
            if (p && p.length > 0)
                out.push(p)
        }
        return out
    }
```

**File:** `qml/QG/Main.qml`
**Location:** `kbCut` and `kbCopy` (lines 964-987)

Find:

```qml
    Shortcut { objectName: "kbCut"; sequence: "Ctrl+X"; enabled: !window.cinema && !window.textEditing && window.sessionActive; onActivated: {
        if (window.selectionState && window.actionsController) {
            const ids = window.selectionState ? (window.selectionState.selectedIdentities ? window.selectionState.selectedIdentities() : []) : []
            if (ids.length > 0) {
                const path = contentView.pathForIndex(ids[0] ? ids[0] : -1)
                if (path.length > 0) {
                    window.actionsController.cut([path])
                }
            }
        }
    } }
    Shortcut { objectName: "kbCopy"; sequence: "Ctrl+C"; enabled: !window.cinema && !window.textEditing && window.sessionActive; onActivated: {
        if (window.selectionState && window.actionsController) {
            const ids = window.selectionState ? (window.selectionState.selectedIdentities ? window.selectionState.selectedIdentities() : []) : []
            if (ids.length > 0) {
                const paths = []
                for (let i = 0; i < ids.length; ++i) {
                    const p = contentView.pathForIndex(ids[i] ? ids[i] : -1)
                    if (p.length > 0) paths.push(p)
                }
                if (paths.length > 0) window.actionsController.copyToClipboard(paths)
            }
        }
    } }
```

Replace with:

```qml
    // Clipboard works whenever a selection exists — not only during playback
    // (§13.2 lists Cut/Copy with no playback precondition), and it resolves
    // identities to paths instead of treating them as row indexes.
    Shortcut { objectName: "kbCut"; sequence: "Ctrl+X"; enabled: !window.cinema && !window.textEditing && window.selectionState && window.selectionState.selectedCount > 0; onActivated: {
        const paths = contentView.pathsForIdentities(
                          window.selectionState ? window.selectionState.selectedIdentities : [])
        if (paths.length > 0 && window.actionsController)
            window.actionsController.cut(paths)
    } }
    Shortcut { objectName: "kbCopy"; sequence: "Ctrl+C"; enabled: !window.cinema && !window.textEditing && window.selectionState && window.selectionState.selectedCount > 0; onActivated: {
        const paths = contentView.pathsForIdentities(
                          window.selectionState ? window.selectionState.selectedIdentities : [])
        if (paths.length > 0 && window.actionsController)
            window.actionsController.copyToClipboard(paths)
    } }
```

**Dependencies:** AUD-004 (the `selectedCount` property and the identity list) — without it `selectedIdentities` is still invokable-only and this patch cannot read it as a property. If AUD-004 is deferred, keep the `selectedIdentities()` call.
**Regression risks:** none functionally; behaviour changes from "never" to "correct". `tst_dragdrop.qml` may exercise the shortcuts with `sessionActive` true.
**Deferred verification:** select two files in Browse (no playback), Ctrl+C, navigate, Ctrl+V: exactly those two files appear. Ctrl+X then paste: the originals disappear. Confirm the system clipboard carries `text/uri-list` and `x-special/gnome-copied-files` for a Nautilus round-trip.
**Evidence confidence:** CONFIRMED.

---

### AUD-014 — Folder cards always render "0v" and never show a mosaic

**Severity:** P1 — High (folder tiles, one of the two entry kinds, carry false information)
**Category:** Missing functionality / Incorrect implementation
**Specification reference:** §5.2 ("Thumbnail area shows a **2×2 mosaic** of up to four pre-resolved child video thumbnails … A **count chip** … `12v` for videos, `12v · 3` when subfolders exist. The label is **Body-strong** … the meta line reads `12 videos · 3 subfolders`"), §6.2 (folder tile states), §22.4 (heavy metadata requested lazily).

**Code evidence**

`qml/QG/ContentView.qml:508-512`:

```qml
                FolderCard {
                    objectName: "contentCell"
                    width: contentView.cardWidth
                    height: contentView.cellHeight
                    folderName: model.name
                    path: contentView.pathForIndex(index) || ""
                    videoCount: model.videoCount === undefined ? 0 : model.videoCount
                    folderCount: model.folderCount === undefined ? 0 : model.folderCount
                    mosaicSources: model.mosaic === undefined ? [] : model.mosaic
```

`src/models/folder_model.cpp:60-78` (`roleNames`) — the complete role set:

```cpp
        {IdentityRole, QByteArrayLiteral("identity")},
        {PathRole, QByteArrayLiteral("path")},
        {NameRole, QByteArrayLiteral("name")},
        {KindRole, QByteArrayLiteral("kind")},
        {IsFolderRole, QByteArrayLiteral("isFolder")},
        {IsVideoRole, QByteArrayLiteral("isVideo")},
        {ThumbnailRole, QByteArrayLiteral("thumbnail")},
        {ThumbnailStateRole, QByteArrayLiteral("thumbnailState")},
        {DurationRole, QByteArrayLiteral("duration")},
        {SizeRole, QByteArrayLiteral("size")},
        {MtimeRole, QByteArrayLiteral("mtime")},
        {FavoriteRole, QByteArrayLiteral("favorite")},
        {WatchedRole, QByteArrayLiteral("watched")},
        {ProgressRole, QByteArrayLiteral("progress")},
        {WidthRole, QByteArrayLiteral("width")},
        {HeightRole, QByteArrayLiteral("height")},
```

**Current implementation**

No `videoCount`, `folderCount`, `mosaic` or `playing` role exists (the model-level `Q_PROPERTY videoCount` is the aggregate for the whole folder, not a per-row role, and is not what a delegate's `model.videoCount` resolves to). Every folder card therefore renders `0v`, `0 videos · 0 subfolders`, the accessibility name "Folder, X, 0 videos, 0 subfolders", and the placeholder glyph — the mosaic branch is unreachable. `CompactRow.playing` has the same problem (`ContentView.qml:548`).

**Expected implementation**

Per-row roles carrying the child counts and up to four child thumbnail URLs, populated lazily on the I/O pool for folders near the viewport, with a minimal `dataChanged`.

**Root cause** — CONFIRMED. The delegate was written against roles that were never added to the model (the `=== undefined ? 0` guards prove the author knew they might be absent and chose silence over a default that hides the defect).

**Correct architecture**

`Entry` gains the three summary fields. A new `FolderSummaryService` (I/O pool) scans a folder's direct children — bounded by a hard cap, cancellable by generation — counts videos/subfolders, collects up to four child video paths and resolves the freedesktop thumbnail cache path for each (`$XDG_CACHE_HOME/thumbnails/normal/<md5(file-uri)>.png`, per §18.2). `FolderModel::updateFolderSummary()` applies the result with a role-scoped `dataChanged`. The service is requested for folder rows by the browser controller when a snapshot lands and by the viewport reporter later; no QML change beyond the roles already being read.

**Implementation-ready patch**

**File:** `src/core/types.hpp`
**Location:** `Entry` struct

Find:

```cpp
    ThumbnailState thumbnail = ThumbnailState::None;

    bool isFolder() const noexcept { return kind == EntryKind::Folder; }
```

Replace with:

```cpp
    ThumbnailState thumbnail = ThumbnailState::None;

    // Folder summary (DESIGN.md §5.2). Only meaningful for folders; filled
    // lazily by FolderSummaryService, never by the enumerator.
    int childVideoCount = -1;   // -1 = not yet resolved
    int childFolderCount = -1;  // -1 = not yet resolved
    QStringList mosaicSources;  // up to four file:// thumbnail URLs

    bool isFolder() const noexcept { return kind == EntryKind::Folder; }
```

Find:

```cpp
    ThumbnailState thumbnail = ThumbnailState::None;
```

(the `registerCoreTypes` block, to keep `QStringList` known)

```cpp
inline void registerCoreTypes()
{
    qRegisterMetaType<Entry>("qg::Entry");
```

Replace with:

```cpp
inline void registerCoreTypes()
{
    qRegisterMetaType<Entry>("qg::Entry");
    qRegisterMetaType<QStringList>("QStringList");
```

**File:** `src/models/folder_model.hpp`
**Location:** roles enum, `data()` declaration area, public mutators

Find:

```cpp
        WidthRole,
        HeightRole,
    };
```

Replace with:

```cpp
        WidthRole,
        HeightRole,
        ChildVideoCountRole,
        ChildFolderCountRole,
        MosaicRole,
    };
```

Find:

```cpp
    // Update path and name for an identity (rename); emits dataChanged for PathRole/NameRole.
```

Replace with:

```cpp
    // Apply a resolved folder summary (DESIGN.md §5.2). Identity-keyed; emits
    // dataChanged for the three summary roles only. No-op when unknown.
    void updateFolderSummary(const QString &identity,
                             int childVideoCount,
                             int childFolderCount,
                             const QStringList &mosaicSources);

    // Update path and name for an identity (rename); emits dataChanged for PathRole/NameRole.
```

**File:** `src/models/folder_model.cpp`
**Location:** `data()`, `roleNames()`, and a new mutator

Find:

```cpp
    case HeightRole:
        return entry.height.has_value() ? QVariant(*entry.height) : QVariant();
    default:
        return {};
    }
```

Replace with:

```cpp
    case HeightRole:
        return entry.height.has_value() ? QVariant(*entry.height) : QVariant();
    case ChildVideoCountRole:
        return entry.childVideoCount >= 0 ? entry.childVideoCount : 0;
    case ChildFolderCountRole:
        return entry.childFolderCount >= 0 ? entry.childFolderCount : 0;
    case MosaicRole:
        return entry.mosaicSources;
    default:
        return {};
    }
```

Find:

```cpp
        {HeightRole, QByteArrayLiteral("height")},
    };
}
```

Replace with:

```cpp
        {HeightRole, QByteArrayLiteral("height")},
        {ChildVideoCountRole, QByteArrayLiteral("videoCount")},
        {ChildFolderCountRole, QByteArrayLiteral("folderCount")},
        {MosaicRole, QByteArrayLiteral("mosaic")},
    };
}

void FolderModel::updateFolderSummary(const QString &identity,
                                      int childVideoCount,
                                      int childFolderCount,
                                      const QStringList &mosaicSources)
{
    const int row = indexOfIdentity(identity);
    if (row < 0) {
        return;
    }
    Entry &entry = m_entries[row];
    if (entry.childVideoCount == childVideoCount
        && entry.childFolderCount == childFolderCount
        && entry.mosaicSources == mosaicSources) {
        return; // identical summary: no repaint
    }
    entry.childVideoCount = childVideoCount;
    entry.childFolderCount = childFolderCount;
    entry.mosaicSources = mosaicSources;
    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx,
                     {ChildVideoCountRole, ChildFolderCountRole, MosaicRole});
}
```

**Not patched — deliberately deferred.** The summary *provider* is the only part of this
finding without a responsible patch today, because two product decisions are unresolved
(see §14, OQ-1 and OQ-2): whether a folder tile may *generate* child thumbnails (the spec
says "pre-resolved", which excludes generation) and whether the summary scan follows
symlinked child directories (§25.1 makes symlinked directories navigable, but following
them in a bounded scan reintroduces loop risk). Writing a second-level scanner against
those open questions would be the speculative patch this brief forbids. The model roles,
the mutator and the delegate honesty fix above are complete on their own; FIX-FOLD-01 in
the roadmap carries the provider once OQ-1/OQ-2 are answered.

**File:** `qml/QG/delegates/FolderCard.qml`
**Location:** `countChipText()` / `metaText()` (lines 33-44)

Find:

```qml
    function countChipText() {
        if (folderCount > 0)
            return videoCount + "v \u00B7 " + folderCount
        return videoCount + "v"
    }
```

Replace with:

```qml
    // -1 means "summary not resolved yet" (FolderModel returns 0 until FIX-FOLD-01
    // fills ChildVideoCountRole). Never claim "0v" for a folder we have not
    // scanned: a wrong count is worse than no count (DESIGN.md §12: the wording
    // never lies).
    function summaryKnown() {
        return videoCount >= 0 && folderCount >= 0
    }

    function countChipText() {
        if (!summaryKnown())
            return ""
        if (folderCount > 0)
            return videoCount + "v \u00B7 " + folderCount
        return videoCount + "v"
    }
```

and bind the chip's `visible` to `card.summaryKnown()`.

**Dependencies:** AUD-024 (identity index) for `updateFolderSummary` cost; AUD-002 for safe delivery.
**Regression risks:** `tst_foldercard.qml` asserts chip text ("12v", "12v · 3"); with the interim change it must construct the card with explicit counts (it does) — `summaryKnown()` is true when both are ≥ 0, which the test provides. `test_folder_model.cpp` role-count assertions (16 contiguous roles) must be updated to 19.
**Deferred verification:** a folder with 12 videos and 3 subfolders must show `12v · 3`, a 2×2 mosaic when four child frames exist in the freedesktop cache, and the calm glyph otherwise. Scan cost: 500 folders × 4,000 entries cap must not delay first paint by more than one frame budget.
**Evidence confidence:** CONFIRMED for the missing roles; the summary *value* is REQUIRES RUNTIME VERIFICATION until FIX-FOLD-01 lands.

---

### AUD-015 — Inline rename never receives focus and does not validate

**Severity:** P1 — High (F2 is unusable; an invalid or colliding name can reach the filesystem)
**Category:** Missing functionality / Incorrect implementation
**Specification reference:** §7.1 ("Replace the filename label with an editable text field … Select the filename without selecting the extension"), §7.2 (reject empty, invalid characters, reserved names, collisions, case sensitivity; "Validation errors should appear inline without closing the editing field"), §11.3 (live validation, inline error, red underline, `Tab` unlocks the extension), §25.9.

**Code evidence**

`qml/QG/delegates/VideoCard.qml:464-529` — the editor:

```qml
            Component.onCompleted: {
                const dot = card.fileName.lastIndexOf(".")
                if (card.extensionLock && dot > 0) {
                    text = card.fileName
                    cursorPosition = dot
                    select(cursorPosition, text.length) // select the locked extension
                } else {
                    text = card.fileName
                    selectAll()
                }
            }

            onAccepted: {
                if (renameError.length > 0) return
                card.renameCommitted(text)
                card.renaming = false
            }
            ...
                const dot = text.lastIndexOf(".")
                const base = dot > 0 ? text.left(dot) : text
```

`qml/QG/ContentView.qml:474` — `renaming: index === contentView.renameIndex` (a binding that the delegate assigns to).

`src/filesystem/paths.cpp:78-109` — the canonical validator `validateFilename()` exists, is unit-tested, and **has no caller in the rename path** (`grep -rn validateFilename src/` → definition + `test_paths.cpp`).

**Current implementation**

`Component.onCompleted` runs when the delegate is created — long before `renaming` becomes true — so the field is never populated for the actual rename, never gets `forceActiveFocus()`, and the extension is not pre-selected. Keystrokes go to the grid's type-to-select. `text.left(dot)` is not a JavaScript `String` method (QML's engine has no `left()`), so the validation handler throws on every keystroke containing a dot, leaving `renameError` stale. `card.renaming = false` writes to a property bound at `ContentView.qml:474`, permanently destroying the binding, so the same card cannot be renamed twice. Validation is limited to emptiness and an optional collision callback; `/`, NUL, leading `-`, `.`/`..` and case-insensitive collisions are not checked in QML — and `renameSync` does not check them either.

**Expected implementation**

On `renaming` becoming true: populate the field from the filename, focus it, select the basename only. On each keystroke: validate through the C++ validator (single source of truth) and show the message inline. On Esc/commit: tell the view (which owns `renameIndex`) instead of assigning to the bound property.

**Root cause** — CONFIRMED. The editor's lifecycle was expressed with `Component.onCompleted` (creation) instead of a state-change handler, the JS API was assumed rather than checked, and the existing canonical validator was never wired to the UI.

**Correct architecture**

`ActionsController` exposes `Q_INVOKABLE QVariantMap validateRename(path, newBaseName)` that composes the preserved extension and calls `qg::validateFilename` with the sibling names from the model (case-insensitive). QML renders `{ok, message}`. `renameSync` calls the same validator as its last-line defence. `VideoCard` emits `renameCancelled()`; `ContentView` clears `renameIndex`.

**Implementation-ready patch**

**File:** `src/controllers/actions_controller.hpp`
**Location:** invokables near `targetPathForRename`

Find:

```cpp
    Q_INVOKABLE QString targetPathForRename(const QString& path,
```

Replace with:

```cpp
    // Live rename validation for the inline editor (§7.2/§11.3). Returns
    // { ok: bool, message: QString } and uses the SAME validator the service
    // applies, so the UI can never disagree with what the rename will do.
    Q_INVOKABLE QVariantMap validateRename(const QString& path,
                                          const QString& newBaseName,
                                          const QStringList& siblingNames) const;

    Q_INVOKABLE QString targetPathForRename(const QString& path,
```

**File:** `src/controllers/actions_controller.cpp`
**Location:** after `targetPathForRename`

Find:

```cpp
bool ActionsController::pathExists(const QString& path) const
{
    return !path.isEmpty() && QFileInfo::exists(path);
}
```

Replace with:

```cpp
QVariantMap ActionsController::validateRename(const QString& path,
                                             const QString& newBaseName,
                                             const QStringList& siblingNames) const
{
    QVariantMap result;
    result.insert(QStringLiteral("ok"), false);

    const QFileInfo fi(path);
    if (!fi.exists()) {
        result.insert(QStringLiteral("message"),
                      QStringLiteral("The file no longer exists"));
        return result;
    }

    // Preserve the extension exactly as renameSync() does (case-insensitive so
    // `clip.MKV` is not doubled into `clip.MKV.mkv`).
    QString name = newBaseName;
    const QString suffix = fi.suffix();
    if (!suffix.isEmpty()
        && !name.endsWith(QLatin1Char('.') + suffix, Qt::CaseInsensitive)) {
        name += QLatin1Char('.') + suffix;
    }

    // A rename to the current name is a no-op success, not a collision.
    if (name == fi.fileName()) {
        result.insert(QStringLiteral("ok"), true);
        result.insert(QStringLiteral("message"), QString());
        result.insert(QStringLiteral("finalName"), name);
        return result;
    }

    QStringList others = siblingNames;
    others.removeAll(fi.fileName());

    const qg::FilenameValidation validation = qg::validateFilename(name, others, true);
    result.insert(QStringLiteral("ok"), validation.ok());
    result.insert(QStringLiteral("message"), validation.message);
    result.insert(QStringLiteral("finalName"), name);
    return result;
}

bool ActionsController::pathExists(const QString& path) const
{
    return !path.isEmpty() && QFileInfo::exists(path);
}
```

**File:** `src/controllers/actions_controller.cpp`
**Location:** includes

Find:

```cpp
#include "controllers/actions_controller.hpp"
```

Replace with:

```cpp
#include "controllers/actions_controller.hpp"

#include "filesystem/paths.hpp"
```

**File:** `src/filesystem/operations.cpp`
**Location:** `renameSync` (lines 70-97)

Find:

```cpp
    // Preserve extension by default
    QString newName = newBaseName;
    if (!fi.suffix().isEmpty() && !newName.endsWith('.' + fi.suffix()))
        newName += '.' + fi.suffix();

    const QString target = fi.absolutePath() + QDir::separator() + newName;
    if (target == path) {
        r.success = true;
        r.completedPath = path;
        return r;
    }
    if (QFileInfo::exists(target)) {
        r.error = QStringLiteral("A file named \"%1\" already exists").arg(newName);
        return r;
    }
```

Replace with:

```cpp
    // Preserve extension by default (case-insensitive: `clip.MKV` must not
    // become `clip.MKV.mkv`).
    QString newName = newBaseName;
    if (!fi.suffix().isEmpty()
        && !newName.endsWith('.' + fi.suffix(), Qt::CaseInsensitive))
        newName += '.' + fi.suffix();

    const QString target = fi.absolutePath() + QDir::separator() + newName;
    if (target == path) {
        r.success = true;
        r.completedPath = path;
        return r;
    }

    // Defence in depth: the UI validates live, but the service is the last line
    // and must reject the same inputs (§7.2). Sibling names are not re-read here
    // (that would be a second directory scan); the existence check below covers
    // collisions, this covers names that are invalid by construction.
    const qg::FilenameValidation validation = qg::validateFilename(newName);
    if (!validation.ok()) {
        r.error = validation.message;
        return r;
    }
    if (QFileInfo::exists(target)) {
        r.error = QStringLiteral("A file named \"%1\" already exists").arg(newName);
        return r;
    }
```

**File:** `qml/QG/delegates/VideoCard.qml`
**Location:** the rename editor (lines 464-529) and the delegate's signal list

Find:

```qml
            Component.onCompleted: {
                const dot = card.fileName.lastIndexOf(".")
                if (card.extensionLock && dot > 0) {
                    text = card.fileName
                    cursorPosition = dot
                    select(cursorPosition, text.length) // select the locked extension
                } else {
                    text = card.fileName
                    selectAll()
                }
            }

            onAccepted: {
                if (renameError.length > 0) return
                card.renameCommitted(text)
                card.renaming = false
            }

            Keys.onEscapePressed: {
                card.renaming = false
            }

            // Live validation: red underline when invalid/collision
            onTextChanged: {
                if (text.trim().length === 0 || text === card.fileName) {
                    renameError = ""
                    return
                }
                const dot = text.lastIndexOf(".")
                const base = dot > 0 ? text.left(dot) : text
                if (base.length === 0) {
                    renameError = qsTr("Name cannot be empty")
                    return
                }
                // Collision detection against injected callback (optional)
                if (card.collisionCheck && card.collisionCheck(text))
                    renameError = qsTr("A file named \"%1\" already exists").arg(text)
                else
                    renameError = ""
            }
```

Replace with:

```qml
            // The editor is created once per delegate. Everything that depends on
            // entering edit mode must react to `renaming`, not to construction.
            onRenamingChanged: {
                if (!card.renaming)
                    return
                text = card.fileName
                const dot = card.fileName.lastIndexOf(".")
                if (card.extensionLock && dot > 0) {
                    // Basename selected, extension preserved but visible (§7.1.4).
                    cursorPosition = 0
                    select(0, dot)
                } else {
                    selectAll()
                }
                forceActiveFocus()
            }

            onAccepted: {
                if (renameError.length > 0)
                    return
                card.renameCommitted(text)
                // The view owns renameIndex: never assign to `card.renaming`,
                // which is a binding and would be destroyed for good.
                card.renameFinished()
            }

            Keys.onEscapePressed: {
                card.renameCancelled()
            }

            // Live validation through the C++ validator (single source of truth).
            onTextChanged: {
                if (!card.validateRename) {
                    renameError = ""
                    return
                }
                const verdict = card.validateRename(text)
                renameError = verdict.ok === true ? "" : ("" + verdict.message)
            }
```

**File:** `qml/QG/delegates/VideoCard.qml`
**Location:** the property block and the signal block

Find:

```qml
    signal clicked()
```

Replace with:

```qml
    // Injected by ContentView: (newBaseName) -> { ok, message, finalName }.
    property var validateRename: null

    signal clicked()
    signal renameFinished()
    signal renameCancelled()
```

**File:** `qml/QG/ContentView.qml`
**Location:** VideoCard delegate, the `onRenameCommitted` handler (line ~486)

Find:

```qml
                    onRenameCommitted: function (newName) {
                        contentView.renameIndex = -1
                        contentView.renameCommitted(index, newName)
                    }
```

Replace with:

```qml
                    validateRename: function (newBaseName) {
                        if (!window.actionsController)
                            return ({ ok: true, message: "" })
                        const siblings = []
                        for (let i = 0; i < contentView.itemCount; ++i)
                            siblings.push(contentView.model.nameAt(i))
                        return window.actionsController.validateRename(
                                   contentView.pathForIndex(index), newBaseName, siblings)
                    }
                    onRenameCommitted: function (newName) {
                        contentView.renameIndex = -1
                        contentView.renameCommitted(index, newName)
                    }
                    onRenameFinished: contentView.renameIndex = -1
                    onRenameCancelled: contentView.renameIndex = -1
```

**Dependencies:** AUD-004's `selectionRevision` is unrelated; the `siblings` loop uses `model.nameAt` (exists). For 50 k rows the loop is O(n) per keystroke — acceptable for a rename gesture but note FIX-FOLD-01's identity index can supply a sibling set later.
**Regression risks:** `tst_videocard.qml` constructs the card and sets `renaming`; the new `onRenamingChanged` will call `forceActiveFocus()` (harmless offscreen) and re-read `fileName`. Tests that call `card.renaming = false` directly still work. `test_card_actions` should add a case for `validateRename`.
**Deferred verification:** F2 on a card → field focused, basename selected, extension visible; typing `/` shows the inline message with the red underline and Enter does nothing; a colliding name shows "already exists"; Enter renames in place and the same card can be renamed again immediately; Esc restores the label.
**Evidence confidence:** CONFIRMED.

---

### AUD-016 — mpv render context is freed from the GUI thread without a current GL context; the update callback outlives the surface

**Severity:** P1 — High (undefined behaviour at shutdown/navigation; mpv may call into a destroyed `VideoSurfaceItem`)
**Category:** Error handling / Architectural
**Specification reference:** §19.3 ("`mpv_render_context_set_update_callback` schedules an item/window update; it does not render from mpv's callback thread"), §19.7 ("Closing the app … releases the mpv handle cleanly"), §15.3 (render thread owns scene-graph and GL context).

**Code evidence**

`src/playback/mpv_backend.cpp:175-185`:

```cpp
void MpvBackend::shutdown() {
    if (renderCtx_) {
        mpv_render_context_free(renderCtx_);
        renderCtx_ = nullptr;
        renderCtxCreated_ = false;
    }
    if (handle_) {
        mpv_terminate_destroy(handle_);
        handle_ = nullptr;
    }
}
```

`src/playback/video_surface.cpp:48-58` registers the callback with the item as userdata:

```cpp
        if (ctx_ == nullptr) {
            ctx_ = backend_->renderContext();
            if (ctx_ != nullptr) {
                mpv_render_context_set_update_callback(ctx_, onMpvUpdate, item_);
```

`MpvBackend::~MpvBackend()` calls `shutdown()`; the backend is owned by `CoreServices` (`composition.cpp:162`) and destroyed on the GUI thread after the QML engine (and therefore the `VideoSurfaceItem`) is gone.

**Current implementation**

Two defects with one owner:

1. **Callback lifetime.** The update callback keeps `item_` (a raw `VideoSurfaceItem*`) and is never cleared. After the item is destroyed, mpv's playback thread can still call `onMpvUpdate(item_)`, which constructs a `QPointer` from a dangling pointer — a use-after-free that is *not* prevented by `QPointer` (constructing it already dereferences the object).
2. **GL-context requirement.** `mpv_render_context_free()` must be called with the OpenGL context that created the render context current. `shutdown()` runs on the GUI thread with no GL context current, so the free is undefined behaviour (in practice: driver-dependent crash or leaked GL objects). `mpv_terminate_destroy()` on a handle whose render context is still alive is also undefined.

**Expected implementation**

Clear the update callback *before* anything else. Free the render context on the render thread, with the scene-graph GL context bound, via `QQuickWindow::scheduleRenderJob`; terminate the handle only after that free completes. If no render thread is available (headless teardown), leak rather than corrupt and log it.

**Root cause** — CONFIRMED for the callback and for the thread requirement (both are documented libmpv/Qt contracts); the crash itself is REQUIRES RUNTIME VERIFICATION.

**Correct architecture**

`VideoSurfaceItem` owns the render-thread side of the render context (it already stores `ctx_` in its renderer). `MpvBackend` requests the free through the surface and learns about completion through a signal, then terminates the handle. Ownership stays single.

**Complete permanent resolution**

Add `MpvBackend::setSurface()`, `VideoSurfaceItem::scheduleRenderContextFree()` + `renderContextFreed()`, and restructure `shutdown()` into request/finish.

**Implementation-ready patch**

**File:** `src/playback/video_surface.hpp`
**Location:** public API and signals

Find:

```cpp
    bool videoActive() const { return videoActive_; }
```

Replace with:

```cpp
    bool videoActive() const { return videoActive_; }

    // Hand the render context back to the render thread for freeing. mpv
    // requires the GL context that created it to be current, which only the
    // render thread can guarantee (§19.3, §19.7).
    void scheduleRenderContextFree(mpv_render_context* ctx);
```

Find:

```cpp
    void renderContextReady();
```

Replace with:

```cpp
    void renderContextReady();
    // Emitted on the GUI thread after the render thread freed the context.
    void renderContextFreed();
```

Find:

```cpp
private slots:
    // Marshaled from the render thread when the render context first exists.
    void onRenderContextReady();
```

Replace with:

```cpp
private slots:
    // Marshaled from the render thread when the render context first exists.
    void onRenderContextReady();
    // Marshaled from the render thread after the free job completed.
    void onRenderContextFreed();
```

**File:** `src/playback/video_surface.cpp`
**Location:** anonymous namespace (after `VideoRenderer`) and the new methods

Find:

```cpp
} // namespace

VideoSurfaceItem::VideoSurfaceItem(QQuickItem* parent) : QQuickFramebufferObject(parent)
```

Replace with:

```cpp
// Frees an mpv render context on the render thread, where the scene-graph GL
// context is bound, then tells the GUI thread. Runs as a QQuickWindow render
// job; if no GL context is current it gives up rather than freeing blindly.
class ContextFreeJob : public QRunnable {
public:
    ContextFreeJob(mpv_render_context* ctx, QPointer<VideoSurfaceItem> item)
        : m_ctx(ctx), m_item(item) {}

    void run() override {
        if (QOpenGLContext::currentContext() == nullptr) {
            qWarning("mpv: no GL context current; render context left to process exit");
            return;
        }
        mpv_render_context_set_update_callback(m_ctx, nullptr, nullptr);
        mpv_render_context_free(m_ctx);
        if (!m_item.isNull()) {
            QMetaObject::invokeMethod(m_item.data(), "onRenderContextFreed",
                                      Qt::QueuedConnection);
        }
    }

private:
    mpv_render_context* m_ctx = nullptr;
    QPointer<VideoSurfaceItem> m_item;
};

} // namespace

void VideoSurfaceItem::scheduleRenderContextFree(mpv_render_context* ctx)
{
    if (ctx == nullptr) {
        return;
    }
    // Stop mpv from calling into this item immediately, even before the free.
    mpv_render_context_set_update_callback(ctx, nullptr, nullptr);

    QQuickWindow* w = window();
    if (w == nullptr) {
        qWarning("VideoSurfaceItem: no window; cannot free the mpv render context");
        return;
    }
    w->scheduleRenderJob(new ContextFreeJob(ctx, QPointer<VideoSurfaceItem>(this)),
                         QQuickWindow::BeforeSynchronizingStage);
}

void VideoSurfaceItem::onRenderContextFreed()
{
    emit renderContextFreed();
}

VideoSurfaceItem::VideoSurfaceItem(QQuickItem* parent) : QQuickFramebufferObject(parent)
```

**File:** `src/playback/video_surface.cpp`
**Location:** includes

Find:

```cpp
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions>
#include <QPointer>
#include <QMetaObject>
#include <QDebug>
```

Replace with:

```cpp
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions>
#include <QPointer>
#include <QMetaObject>
#include <QQuickWindow>
#include <QRunnable>
#include <QDebug>
```

**File:** `src/playback/mpv_backend.hpp`
**Location:** public API, slots, members

Find:

```cpp
    // Explicit teardown (also called from destructor).
    void shutdown();
```

Replace with:

```cpp
    // Explicit teardown (also called from destructor). The render context must
    // be freed with the scene-graph GL context current, so teardown is a
    // request/completion pair: shutdown() hands the context to the surface's
    // render thread and finishShutdown() terminates the handle afterwards.
    void shutdown();

    // The surface that owns the render-thread side of the render context.
    void setSurface(class VideoSurfaceItem* surface) { surface_ = surface; }
```

Find:

```cpp
signals:
```

Replace with:

```cpp
private slots:
    void finishShutdown();

signals:
```

Find:

```cpp
    mpv_handle* handle_ = nullptr;
    mpv_render_context* renderCtx_ = nullptr;
    bool renderCtxCreated_ = false;
```

Replace with:

```cpp
    mpv_handle* handle_ = nullptr;
    mpv_render_context* renderCtx_ = nullptr;
    bool renderCtxCreated_ = false;
    bool shuttingDown_ = false;
    class VideoSurfaceItem* surface_ = nullptr;
```

**File:** `src/playback/mpv_backend.cpp`
**Location:** `shutdown()` (lines 175-185)

Find:

```cpp
void MpvBackend::shutdown() {
    if (renderCtx_) {
        mpv_render_context_free(renderCtx_);
        renderCtx_ = nullptr;
        renderCtxCreated_ = false;
    }
    if (handle_) {
        mpv_terminate_destroy(handle_);
        handle_ = nullptr;
    }
}
```

Replace with:

```cpp
void MpvBackend::shutdown() {
    if (shuttingDown_) {
        return;
    }
    shuttingDown_ = true;

    if (renderCtx_ == nullptr) {
        finishShutdown();
        return;
    }

    // Never let mpv call back into a surface that may be gone (the callback used
    // to keep a raw VideoSurfaceItem* for the life of the process).
    mpv_render_context_set_update_callback(renderCtx_, nullptr, nullptr);

    if (surface_ != nullptr && surface_->window() != nullptr) {
        QObject::connect(surface_, &VideoSurfaceItem::renderContextFreed,
                         this, &MpvBackend::finishShutdown, Qt::UniqueConnection);
        // Ownership of the render context moves to the render job from here.
        mpv_render_context* ctx = renderCtx_;
        renderCtx_ = nullptr;
        surface_->scheduleRenderContextFree(ctx);
        return; // finishShutdown() runs when the render thread has freed it
    }

    // No render thread (headless teardown, or the window is already gone).
    // Terminating the handle with a live render context is undefined behaviour,
    // so both are left to process exit and the condition is reported.
    qWarning("MpvBackend: no render thread available; deferring mpv teardown to exit");
}

void MpvBackend::finishShutdown() {
    renderCtx_ = nullptr;
    renderCtxCreated_ = false;
    if (handle_ != nullptr) {
        mpv_terminate_destroy(handle_);
        handle_ = nullptr;
    }
    shuttingDown_ = false;
}
```

**File:** `src/playback/mpv_backend.cpp`
**Location:** includes

Find:

```cpp
#include "mpv_backend.hpp"
```

Replace with:

```cpp
#include "mpv_backend.hpp"

#include "video_surface.hpp"
```

**File:** `src/playback/playback_manager.cpp`
**Location:** `attachSurface()` (line 151)

Find:

```cpp
    surface_ = surface;
    if (surface_) {
        surface_->setBackend(backend_);
```

Replace with:

```cpp
    surface_ = surface;
    if (surface_) {
        surface_->setBackend(backend_);
        if (backend_ != nullptr) {
            backend_->setSurface(surface_); // render-thread teardown handshake
        }
```

**Dependencies:** none. `MpvBackend::setSurface` is also called from `setBackend()` when a surface already exists (`playback_manager.cpp:36-37`) — add the same line there.
**Regression risks:** teardown becomes asynchronous. `test_playback_lifecycle.cpp` constructs a `MpvBackend` without a surface and calls `shutdown()`/destructor: it will now log a warning and skip `mpv_terminate_destroy`. If ASan/LSan runs treat that as a leak, the test must create a window (there is already `run_gl_test.sh` for GL tests) or accept the documented suppression. Any code that assumed `initialized() == false` immediately after `shutdown()` must wait for the completion signal.
**Deferred verification:** under ASan + a real X11 window: open/play/close 50 times, and quit while playing. Expect no `QObject::thread`/GL errors from mpv, no ASan report, and a clean process exit. Also confirm playback still works after a Stage is recreated (surface replaced).
**Evidence confidence:** STRONGLY SUPPORTED (both requirements are documented contracts; the crash is not reproduced here).

---

### AUD-017 — Repository writes report success when SQLite fails

**Severity:** P1 — High (silent divergence between the UI and the database; favourites/positions/probe results can be lost with no error path)
**Category:** Error handling / Persistence
**Specification reference:** §21.5 ("All state mutations are small, single-row upserts inside short transactions"), §15.4 ("Services return typed result/error objects … controllers map them to state overlays/toasts"), §17.6 ("publishes progress/result"), §25.8.

**Code evidence**

`src/persistence/database.cpp:288-302`:

```cpp
bool Statement::step()
{
    if (m_stmt == nullptr) {
        return false;
    }
    const int rc = sqlite3_step(m_stmt);
    if (rc == SQLITE_ROW) {
        return true;
    }
    if (rc == SQLITE_DONE) {
        return false;
    }
    m_error = QString::fromUtf8(sqlite3_errmsg(sqlite3_db_handle(m_stmt)));
    return false;
}
```

`src/persistence/repositories.cpp:104-107` (`PathRepository::upsert`), and the same idiom at lines 285-288, 340-343, 530-533, 568-571, 614-617:

```cpp
    if (stmt.step()) {
        return Error::make(ErrorCode::OperationFailed, stmt.error(), path);
    }
    return Result<void>::success();
```

**Current implementation**

For INSERT/UPDATE/DELETE, `sqlite3_step` returns `SQLITE_DONE` on success and `SQLITE_ERROR`/`SQLITE_BUSY`/`SQLITE_FULL`/`SQLITE_READONLY`/`SQLITE_CONSTRAINT` on failure. Both non-row outcomes return `false` from `step()`, so the "a row came back ⇒ error" idiom returns `Result<void>::success()` for every failure. Callers (`StateController::setFavorite/setWatched/recordPosition`, `MediaProbeService::process`, `ThumbnailController::applyResult`, `BrowserController` sort/density persistence) therefore discard a `Result` that never reports failure, and the UI shows a filled heart that will not survive a restart.

**Expected implementation**

Write statements execute through an API that distinguishes "done" from "failed" and returns `Result<void>`; read statements keep `step()` (row iteration). Callers already propagate `Result`, so no signature churn beyond the statement call.

**Root cause** — CONFIRMED. A single boolean was made to carry three outcomes (row / done / error).

**Correct architecture**

`Statement` gains `Result<void> exec()` for non-SELECT use (steps once, checks both the message and `sqlite3_errcode`, returns typed `Error`), while `step()` keeps its row-iteration meaning for reads. Repositories use `exec()` for every mutation.

**Implementation-ready patch**

**File:** `src/persistence/database.hpp`
**Location:** `Statement` public API, next to `bool step();`

Find:

```cpp
    bool step();
```

Replace with:

```cpp
    // Row iteration for SELECT: true when a row is available.
    bool step();
    // Single-shot execution for INSERT/UPDATE/DELETE. Returns an Error for
    // SQLITE_ERROR/BUSY/FULL/READONLY/CONSTRAINT instead of collapsing them
    // into the same `false` as "done" (§21.5).
    Result<void> exec();
```

**File:** `src/persistence/database.cpp`
**Location:** after `Statement::step()`

Find:

```cpp
bool Statement::columnIsNull(int index) const
```

Replace with:

```cpp
Result<void> Statement::exec()
{
    if (m_stmt == nullptr) {
        return Error::make(ErrorCode::OperationFailed,
                           QStringLiteral("Statement is not prepared"));
    }
    const int rc = sqlite3_step(m_stmt);
    if (rc == SQLITE_DONE || rc == SQLITE_ROW) {
        return Result<void>::success();
    }
    m_error = QString::fromUtf8(sqlite3_errmsg(sqlite3_db_handle(m_stmt)));
    return Error::make(ErrorCode::OperationFailed, m_error, {}, rc);
}

bool Statement::columnIsNull(int index) const
```

**File:** `src/persistence/repositories.cpp` — six sites, line 104 and the identical blocks at 285, 340, 530, 568, 614.

Find (at each site; the surrounding SQL and binds differ, the tail is identical):

```cpp
    if (stmt.step()) {
        return Error::make(ErrorCode::OperationFailed, stmt.error(), path);
    }
    return Result<void>::success();
```

Replace with:

```cpp
    return stmt.exec();
```

**Dependencies:** none.
**Regression risks:** latent bugs become visible. `test_persistence.cpp` may exercise a read-only database and now receive an `Error` where it previously got `success()` — that is the point, but the assertions need review. `MediaProbeService::process` ignores the result of `setProbeResult`; with a real failure the probe cache will no longer be silently trusted (desirable, but it changes the cache-hit assertions in `test_probe_wiring.cpp` if they depend on a write that used to fail silently).
**Deferred verification:** open the DB read-only (`chmod 444` plus `PRAGMA query_only`), toggle a favourite, and confirm a toast/error rather than a heart that lies; then reopen writable and confirm the state round-trips.
**Evidence confidence:** CONFIRMED.

---

### AUD-018 — Trash can report success with no `.trashinfo`, and restore leaves the info file behind

**Severity:** P1 — High (an unrestorable "successful" deletion; the trash is left in an inconsistent state for every file manager on the system)
**Category:** Error handling / Filesystem correctness
**Specification reference:** §17.7 ("Fallback: move to `$XDG_DATA_HOME/Trash/files/<name>` with a collision-suffixed name, **write a `.trashinfo` file recording the original absolute path and deletion date, per spec** … Every Trash operation records enough information to Undo"), freedesktop Trash specification (info file is written before/with the move and removed on restore).

**Code evidence**

`src/filesystem/trash.cpp:75-99`:

```cpp
    if (!QFile::rename(path, target)) {
        r.error = QStringLiteral("Trash move failed");
        return r;
    }

    // Write .trashinfo
    const QString infoPath = infoDir + QDir::separator() + name + QStringLiteral(".trashinfo");
    const QString content = QStringLiteral("[Trash Info]\nPath=%1\nDeletionDate=%2\n")
                                .arg(QUrl::fromLocalFile(path).toEncoded(),
                                     QDateTime::currentDateTime().toString(Qt::ISODate));
    QFile infoFile(infoPath);
    if (infoFile.open(QIODevice::WriteOnly)) {
        infoFile.write(content.toUtf8());
        infoFile.close();
    }

    r.success = true;
    r.trashPath = target;
    r.canUndo = true;
    return r;
}

bool TrashService::restoreFallback(const QString& trashPath, const QString& originalPath) {
    QDir dir;
    if (dir.exists(originalPath)) return false; // collision: do not overwrite
    return QFile::rename(trashPath, originalPath);
}
```

**Current implementation**

The payload is moved first and the info file is written best-effort; if `open()` or `write()` fails (unwritable `Trash/info`, ENOSPC, a race) the entry is orphaned in `Trash/files` with **no record of where it came from**, and QG still returns `success = true, canUndo = true` and shows "Moved to Trash". Undo then restores `originalPath` (from the in-memory result, which is correct) but leaves `info/<name>.trashinfo` behind, so the trash stays "occupied" and a later deletion with the same name can pair a stale info file with a different payload. `restoreFallback` also accepts any path without verifying it is inside the trash root.

**Expected implementation**

Write the info file first (unique name resolved atomically), then move; on move failure remove the info file and report the error; on restore, remove the info file and verify the payload is inside the trash root.

**Root cause** — CONFIRMED. The two-part transaction (info + move) was written in the wrong order with no rollback.

**Correct architecture**

`TrashService::trashFallback` performs info-then-move with rollback; `restoreFallback` consumes the info file and validates containment. `gio trash` remains the preferred path when available (AUD-035 records the inverted order as a separate, deliberate deviation).

**Implementation-ready patch**

**File:** `src/filesystem/trash.cpp`
**Location:** lines 75-106

Find:

```cpp
    if (!QFile::rename(path, target)) {
        r.error = QStringLiteral("Trash move failed");
        return r;
    }

    // Write .trashinfo
    const QString infoPath = infoDir + QDir::separator() + name + QStringLiteral(".trashinfo");
    const QString content = QStringLiteral("[Trash Info]\nPath=%1\nDeletionDate=%2\n")
                                .arg(QUrl::fromLocalFile(path).toEncoded(),
                                     QDateTime::currentDateTime().toString(Qt::ISODate));
    QFile infoFile(infoPath);
    if (infoFile.open(QIODevice::WriteOnly)) {
        infoFile.write(content.toUtf8());
        infoFile.close();
    }

    r.success = true;
    r.trashPath = target;
    r.canUndo = true;
    return r;
}

bool TrashService::restoreFallback(const QString& trashPath, const QString& originalPath) {
    QDir dir;
    if (dir.exists(originalPath)) return false; // collision: do not overwrite
    return QFile::rename(trashPath, originalPath);
}
```

Replace with:

```cpp
    // Write the .trashinfo BEFORE moving the payload. The previous order left an
    // orphaned payload with no record of its origin whenever the info write
    // failed, while still reporting success (freedesktop Trash spec).
    const QString infoPath = infoDir + QDir::separator() + name + QStringLiteral(".trashinfo");
    const QString content = QStringLiteral("[Trash Info]\nPath=%1\nDeletionDate=%2\n")
                                .arg(QUrl::fromLocalFile(path).toEncoded(),
                                     QDateTime::currentDateTime().toString(Qt::ISODate));
    QFile infoFile(infoPath);
    if (!infoFile.open(QIODevice::WriteOnly)) {
        r.error = QStringLiteral("Cannot write the trash info file: %1")
                      .arg(infoFile.errorString());
        return r;
    }
    const qint64 written = infoFile.write(content.toUtf8());
    infoFile.close();
    if (written != content.toUtf8().size()) {
        QFile::remove(infoPath); // roll back the record, nothing has moved yet
        r.error = QStringLiteral("Cannot write the trash info file: %1")
                      .arg(infoFile.errorString());
        return r;
    }

    if (!QFile::rename(path, target)) {
        QFile::remove(infoPath); // roll back: the payload did not move
        r.error = QStringLiteral("Trash move failed");
        return r;
    }

    r.success = true;
    r.trashPath = target;
    r.canUndo = true;
    return r;
}

bool TrashService::restoreFallback(const QString& trashPath, const QString& originalPath) {
    const QString filesRoot = trashRoot() + QStringLiteral("/files/");
    if (!trashPath.startsWith(filesRoot)) {
        return false; // not a payload of THIS trash: refuse to move it
    }
    QDir dir;
    if (dir.exists(originalPath)) return false; // collision: do not overwrite
    if (!QFile::rename(trashPath, originalPath)) {
        return false;
    }
    // The info record is consumed by the restore; leaving it behind makes the
    // trash look occupied and can pair a stale record with a later payload.
    QFile::remove(trashRoot() + QStringLiteral("/info/")
                  + QFileInfo(trashPath).fileName() + QStringLiteral(".trashinfo"));
    return true;
}
```

**Dependencies:** none.
**Regression risks:** `test_action_feedback.cpp` / `test_...trash` fixtures that create an unwritable trash root now get an error (previously a false success). The collision-suffixed naming still produces `name_1.` for extension-less entries (AUD-035, cosmetic).
**Deferred verification:** `chmod 500 $XDG_DATA_HOME/Trash/info`, trash a file → error toast, file still present, nothing in `Trash/files`. Restore → the info file is gone and `Trash/files` no longer lists the entry. Validate with `grep Trash` under a real Nautilus session.
**Evidence confidence:** CONFIRMED.

---

### AUD-019 — Move aborts the whole batch across filesystems (no EXDEV fallback)

**Severity:** P1 — High (moving to a USB stick or any other mount — a first-class flow — always fails)
**Category:** Filesystem correctness / Missing functionality
**Specification reference:** §17.6 ("Move / Copy: … with explicit progress, cancellation checkpoints, metadata preservation, and **cross-device copy+delete fallback**"), §21.4 ("cross-device moves become copy+delete, which is a new identity"), §23.13/§23.14 (mounted drives are normal destinations).

**Code evidence**

`src/filesystem/operations.cpp:104-127`:

```cpp
    for (const QString& p : paths) {
        const QFileInfo fi(p);
        const QString dest = targetDir + QDir::separator() + fi.fileName();
        if (QFileInfo::exists(dest)) {
            r.completedPaths.append(p); // skip collision, record as not-moved
            continue;
        }
        if (QFile::rename(p, dest)) {
            r.completedPaths.append(dest);
        } else {
            r.error = QStringLiteral("Move failed for %1").arg(fi.fileName());
            break;
        }
    }
```

**Current implementation**

`QFile::rename` maps to `rename(2)`, which fails with `EXDEV` across mount points. The `else` branch reports "Move failed for <name>" and `break`s out of the loop, so remaining items are never attempted and already-moved ones are reported only through `completedPaths`. There is no copy+delete, no metadata preservation, no progress.

**Expected implementation**

On `EXDEV` (and only on `EXDEV`), copy recursively then remove the source; report the item as completed only after both halves succeed; continue with the remaining items and aggregate per-item failures (§8.2).

**Root cause** — CONFIRMED. A single-file `rename` was treated as the general move.

**Correct architecture**

`moveSync` gains a cross-device branch that reuses `copyDirRecursive` and removes the source afterwards; the operation stays in the same service so the caller contract does not change.

**Implementation-ready patch**

**File:** `src/filesystem/operations.cpp`
**Location:** `moveSync` (lines 104-127)

Find:

```cpp
    for (const QString& p : paths) {
        const QFileInfo fi(p);
        const QString dest = targetDir + QDir::separator() + fi.fileName();
        if (QFileInfo::exists(dest)) {
            r.completedPaths.append(p); // skip collision, record as not-moved
            continue;
        }
        if (QFile::rename(p, dest)) {
            r.completedPaths.append(dest);
        } else {
            r.error = QStringLiteral("Move failed for %1").arg(fi.fileName());
            break;
        }
    }
    r.success = r.error.isEmpty();
```

Replace with:

```cpp
    QStringList failures;
    for (const QString& p : paths) {
        const QFileInfo fi(p);
        const QString dest = targetDir + QDir::separator() + fi.fileName();
        if (QFileInfo::exists(dest)) {
            failures.append(QStringLiteral("%1: a file with that name already exists")
                                .arg(fi.fileName()));
            continue; // collision: never overwrite (§17.6)
        }
        if (QFile::rename(p, dest)) {
            r.completedPaths.append(dest);
            continue;
        }

        // rename(2) cannot cross filesystems. That is not a failure: fall back
        // to copy + delete (§17.6, §21.4 — the copy is a new identity).
        if (errno != EXDEV) {
            failures.append(QStringLiteral("%1: %2")
                                .arg(fi.fileName(), qt_error_string(errno)));
            continue;
        }

        const bool copied = fi.isDir() ? copyDirRecursive(p, dest)
                                       : QFile::copy(p, dest);
        if (!copied) {
            failures.append(QStringLiteral("%1: copy to the target volume failed")
                                .arg(fi.fileName()));
            continue;
        }
        const bool removed = fi.isDir() ? QDir(p).removeRecursively()
                                        : QFile::remove(p);
        if (!removed) {
            // The data is safe at the destination; report the leftover source
            // rather than pretending the move completed cleanly.
            failures.append(QStringLiteral("%1: copied, but the source could not be removed")
                                .arg(fi.fileName()));
        }
        r.completedPaths.append(dest);
    }

    if (!failures.isEmpty()) {
        r.error = failures.join(QStringLiteral("; "));
    }
    r.success = r.error.isEmpty();
```

**File:** `src/filesystem/operations.cpp`
**Location:** includes

Find:

```cpp
#include <cerrno>
```

Replace with: *(already present — verify; if absent, add)*

```cpp
#include <cerrno>
```

> `qt_error_string` comes from `<QtSystemError>` in Qt 5; on Qt 6 use `qt_error_string(errno)` from `<QtGlobal>` (it is declared in `qglobal.h`). If the project's Qt version does not expose it, substitute `QString::fromLocal8Bit(strerror(errno))` with `<cstring>`.

**Dependencies:** AUD-003 (async dispatch) so a large cross-device copy does not freeze the UI. **Satisfied 2026-10-25** — `moveSync` is now dispatched to the I/O pool ([[2026-10-25-fix-aud-003-file-ops-async]]), so this work item can land on its own.
**Regression risks:** semantics change for colliding destinations: previously recorded as "not moved" in `completedPaths` (indistinguishable from success); now reported as a failure, which is what §8.2 requires. Tests asserting the old `completedPaths` contents must be updated. Cross-device copy does not preserve mtime/ownership/ACLs (the spec asks for metadata preservation); that is FIX-FO-03 and is not silently claimed here.
**Deferred verification:** move 10 files from `$HOME` to a mounted USB stick: all 10 arrive, sources are gone, and the toast reports 10/10; repeat with one read-only source and confirm "9 of 10 — 1 permission error" (needs the §8.2 summary, FIX-FO-02).
**Evidence confidence:** CONFIRMED.

---

### AUD-020 — Filesystems reporting `ino == 0` produce identity-less rows: duplicates, phantoms, no probe/thumbnail

**Severity:** P1 — High (on CIFS/NFS/FUSE, rows duplicate on every refresh and deleted files never disappear)
**Category:** Filesystem correctness / Architecture
**Specification reference:** §17.8 ("Primary: `(st_dev, st_ino)` … **Fallback** (network/odd filesystems reporting `ino == 0` or unstable inodes): a fingerprint of `(size, mtime_ns, first 64 KiB hash)`"), §17.5 (targeted diff), §21.4, §25.8.

**Code evidence**

`src/filesystem/enumerator.cpp:143-158`:

```cpp
        Entry value;
        value.path = fullPath;
        value.name = name;
        value.kind = isDir ? EntryKind::Folder : EntryKind::Video;
        value.size = isDir ? 0 : static_cast<qint64>(st.st_size);
        value.mtimeNs = mtimeNs(st);
        if (st.st_ino != 0) {
            value.identity = identityFromStat(static_cast<quint64>(st.st_dev),
                                              static_cast<quint64>(st.st_ino))
                                 .key;
        }
```

`src/filesystem/watcher.cpp:93-114` (diff): entries without identity are never in `removedIdentities` and are *always* `diff.added`; `FolderModel::applyDiff` appends `diff.added` blindly (`folder_model.cpp:150-155`).

Guards that silently disable work for identity-less rows: `probe.cpp:132-134` (`if (entry.identity.isEmpty()) return;`), `thumbnail_controller.cpp:99-101`.

**Current implementation**

When the filesystem reports `st_ino == 0` (common on CIFS/NFS with certain mount options, some FUSE drivers, and overlay/container filesystems), the row keeps an empty identity. The watcher diff then treats every row as new on every refresh (duplicates) and can never compute a removal (phantoms), while probing and thumbnails are skipped entirely. `core/identity.cpp` already implements the specified fallback (`fingerprintIdentity`, `identityFromPath`) — it is simply never called from the enumerator.

**Expected implementation**

The enumerator applies the same degradation ladder as `identityFromPath`: inode when available, otherwise the `(size, mtime_ns, 64 KiB hash)` fingerprint, otherwise a path-only identity — computed on the I/O pool (the fingerprint reads up to 64 KiB, which is why it must not happen on the GUI thread).

**Root cause** — CONFIRMED. `identityFromStat` was used directly with an implicit "inode always exists" assumption; the fallback path exists in `core/identity.cpp` but has no caller outside `identityFromPath()`.

**Correct architecture**

`identityFromPath` is the single resolution policy, but it re-stats; the enumerator already has the `stat` data, so a sibling `identityForStat(path, st)` (added to `identity.hpp`) applies the ladder without a second stat and without a second policy.

**Implementation-ready patch**

**File:** `src/core/identity.hpp`
**Location:** after `identityFromStat`

Find:

```cpp
FileIdentity identityFromStat(std::uint64_t device,
                              std::uint64_t inode);
```

Replace with:

```cpp
FileIdentity identityFromStat(std::uint64_t device,
                              std::uint64_t inode);

// Full ladder for already-collected stat data (§17.8): inode when the
// filesystem provides one, otherwise the (size, mtime_ns, 64 KiB) fingerprint,
// otherwise a path-only identity. Runs the fingerprint read, so it must be
// called from a worker thread.
Result<FileIdentity> identityForStat(const QString &path,
                                     std::uint64_t device,
                                     std::uint64_t inode,
                                     qint64 size,
                                     qint64 mtimeNs);
```

**File:** `src/core/identity.cpp`
**Location:** after `identityFromStat`

Find:

```cpp
Result<FileIdentity> fingerprintIdentity(const QString &path, qint64 size, qint64 mtimeNs)
```

Replace with:

```cpp
Result<FileIdentity> identityForStat(const QString &path,
                                     std::uint64_t device,
                                     std::uint64_t inode,
                                     qint64 size,
                                     qint64 mtimeNs)
{
    if (inode != 0) {
        return identityFromStat(device, inode);
    }
    // Network/FUSE filesystems with unstable inodes still get a stable-ish key
    // from the content fingerprint (§17.8).
    Result<FileIdentity> fingerprint = fingerprintIdentity(path, size, mtimeNs);
    if (fingerprint) {
        return fingerprint;
    }
    return FileIdentity{encodePathIdentity(path), IdentityKind::PathOnly};
}

Result<FileIdentity> fingerprintIdentity(const QString &path, qint64 size, qint64 mtimeNs)
```

**File:** `src/filesystem/enumerator.cpp`
**Location:** the `Entry` construction block (lines 143-158)

Find:

```cpp
        if (st.st_ino != 0) {
            value.identity = identityFromStat(static_cast<quint64>(st.st_dev),
                                              static_cast<quint64>(st.st_ino))
                                 .key;
        }
```

Replace with:

```cpp
        // Never leave the identity empty: an identity-less row can be neither
        // diffed (duplicates/phantoms) nor probed nor thumbnailed (§17.8).
        const Result<FileIdentity> identity =
            identityForStat(fullPath,
                            static_cast<quint64>(st.st_dev),
                            static_cast<quint64>(st.st_ino),
                            value.size,
                            value.mtimeNs);
        if (identity) {
            value.identity = identity.value().key;
        } else {
            // Even the path-only degradation failed to build a key: record the
            // entry error rather than emitting an unusable row.
            appendError(snapshot, fullPath, identity.error().message);
            continue;
        }
```

**Dependencies:** none (the enumerator already runs on the I/O pool).
**Regression risks:** on a 50 k-entry directory with `ino == 0`, the fingerprint costs up to 64 KiB per file — bounded by the pool, but the first scan will be slower on such filesystems. Tests using in-memory fixtures with real inodes are unaffected. `test_enumerator.cpp` may assert `identity.isEmpty()` for a mocked `ino == 0` entry; that expectation is the defect.
**Deferred verification:** on a CIFS/NFS mount (or a FUSE mount configured to report `ino=0`), refresh twice: no duplicate rows; delete a file externally: the row disappears after the debounce. Confirm probe/thumbnail work there.
**Evidence confidence:** CONFIRMED (code); the filesystem behaviour is documented and reproducible only on such a mount (REQUIRES RUNTIME VERIFICATION for the user-visible effect).

---

### AUD-021 — Every probe runs twice, the second time on the GUI thread

**Severity:** P1 — High (doubles probe cost and blocks the GUI thread per file — exactly the pattern §22.1 forbids)
**Category:** Performance / Incorrect implementation
**Specification reference:** §18.6 ("Container headers/stream metadata are read on the CPU/media pool"), §22.1 rule 3 ("**No media probing** on the GUI thread"), §22.2 (bounded, ≤2 concurrent).

**Code evidence**

`src/media/probe.cpp:191-205`:

```cpp
        submitTask(WorkerPools::cpuMedia(),
                   TaskPriority::Background,
                   nullptr,
                   this,
                   [this, entry]() { runProbe(entry); },
                   [this, entry, generation]() {
                       --m_active;
                       if (generation == m_generation) {
                           process(entry);
                       }
                       pump();
                   });
```

`process()` (`probe.cpp:156-189`) then calls `runProbe(entry)` again (after a DB cache check) and performs the SQLite writes.

**Current implementation**

The worker's `ProbeResult` is discarded; the queued completion re-enters `process()` on the GUI thread, which re-opens the file with `avformat_open_input` + `avformat_find_stream_info` and writes two rows to SQLite. Every probe therefore costs 2× disk/CPU and blocks the UI for the second open. The cache check also happens *after* the wasted worker probe, so a cached file still pays one full open.

**Expected implementation**

The worker probe result crosses the thread boundary and is applied on the GUI thread; the cache check happens before dispatching any work.

**Root cause** — CONFIRMED. `submitTask`'s `deliver` closure had no way to return a value (the shared state pattern) and `process()` was reused as both "cache check + probe" and "apply result".

**Correct architecture**

`pump()` allocates a `std::shared_ptr<ProbeResult>`, fills it on the worker, and passes it to the delivery closure; a new `applyProbeResult(entry, probed)` performs the DB writes and signals. `process()` remains for the immediate/testing path.

**Implementation-ready patch**

**File:** `src/media/probe.cpp`
**Location:** `pump()` (lines 185-205)

Find:

```cpp
    while (m_active < m_maxConcurrent && !m_queue.isEmpty()) {
        const Entry entry = m_queue.dequeue();
        ++m_active;
        const quint64 generation = m_generation;

        submitTask(WorkerPools::cpuMedia(),
                   TaskPriority::Background,
                   nullptr,
                   this,
                   [this, entry]() { runProbe(entry); },
                   [this, entry, generation]() {
                       --m_active;
                       if (generation == m_generation) {
                           process(entry);
                       }
                       pump();
                   });
    }
```

Replace with:

```cpp
    while (m_active < m_maxConcurrent && !m_queue.isEmpty()) {
        const Entry entry = m_queue.dequeue();
        ++m_active;
        const quint64 generation = m_generation;
        // The worker's result must cross the thread boundary: probing again in
        // the delivery closure opened every file twice and did the second open
        // on the GUI thread (§18.6, §22.1).
        auto probed = std::make_shared<ProbeResult>();

        submitTask(WorkerPools::cpuMedia(),
                   TaskPriority::Background,
                   nullptr,
                   this,
                   [this, entry, probed]() { *probed = runProbe(entry); },
                   [this, entry, generation, probed]() {
                       --m_active;
                       if (generation == m_generation) {
                           applyProbeResult(entry, *probed);
                       }
                       pump();
                   });
    }
```

**File:** `src/media/probe.cpp`
**Location:** immediately after `process()` (before `pump()`)

```cpp
void MediaProbeService::applyProbeResult(const Entry &entry, const ProbeResult &probed)
{
    // GUI thread: persistence + signal only. No decoder is opened here.
    if (probed.ok()) {
        if (m_database != nullptr) {
            MediaStateRepository media(*m_database);
            media.setProbeResult(entry.identity, probed.durationMs, probed.width, probed.height);
            PathRepository paths(*m_database);
            paths.upsert(entry.path, entry.identity, 0, 0, entry.size, entry.mtimeNs);
        }
        emit probeReady(entry.identity, probed.durationMs, probed.width, probed.height);
    } else {
        emit probeFailed(entry.identity);
    }
}
```

**File:** `src/media/probe.hpp`
**Location:** private methods

Find:

```cpp
    void process(const Entry &entry);
```

Replace with:

```cpp
    void process(const Entry &entry);
    void applyProbeResult(const Entry &entry, const ProbeResult &probed);
```

**File:** `src/media/probe.cpp`
**Location:** includes

Find:

```cpp
#include "media/probe.hpp"
```

Replace with:

```cpp
#include "media/probe.hpp"

#include <memory>
```

**Dependencies:** AUD-002 (safe delivery).
**Regression risks:** `test_probe_wiring.cpp` and `test_probe.cpp` stub `runProbe` via `setProber`; the result now comes from the worker copy, so a mutable counter in the stub must be thread-safe. `test_probe_memory.cpp` (100 k entries) will halve its probe count in wall-clock terms and should still pass its bounds.
**Deferred verification:** probe a 1,000-file folder and count `avformat_open_input` calls (instrument the prober): exactly one per file. Watch GUI-thread stalls (should be none) while probing.
**Evidence confidence:** CONFIRMED.

---

### AUD-022 — `FolderModel::reorder()` violates Qt's layout-change contract

**Severity:** P1 — High (after a sort change, persistent indexes still point at the old rows: wrong delegate contents / wrong current item)
**Category:** Architecture / Correctness
**Specification reference:** §20.1 ("Changing the sort re-sorts the model once, and everything downstream updates through signals"), §5.7 (deterministic order applied to browser, Reel and prev/next), §22.4 (stable delegates).

**Code evidence**

`src/models/folder_model.cpp:325-330`:

```cpp
void FolderModel::reorder(const SortSpec &spec)
{
    m_entries = SortService::apply(m_entries, spec);
    // Ordering changed only; contents, thumbnails and counts are unchanged.
    emit layoutChanged();
}
```

`grep -rn "layoutAboutToBeChanged\|changePersistentIndex" src/` → **no matches**.

**Current implementation**

Qt's contract for a reorder is: `layoutAboutToBeChanged()`, rearrange, remap every `QPersistentModelIndex` (`changePersistentIndexList`), then `layoutChanged()`. Emitting only the final signal leaves proxies (`SearchProxy`), QML views' current index and any persistent index pointing at the *old* row numbers. After switching from Name to Size, the "current" row can silently become a different file, and the selected card can drift.

**Expected implementation**

The documented three-step sequence, remapping persistent indexes by identity (which is stable across a reorder and available on every row).

**Root cause** — CONFIRMED. The signal was emitted as a "repaint" hint rather than as a structural transition.

**Correct architecture**

`FolderModel` is the only place that reorders, so the contract is implemented once there; identity is the remap key (no path comparisons needed).

**Implementation-ready patch**

**File:** `src/models/folder_model.cpp`
**Location:** `reorder()`

Find:

```cpp
void FolderModel::reorder(const SortSpec &spec)
{
    m_entries = SortService::apply(m_entries, spec);
    // Ordering changed only; contents, thumbnails and counts are unchanged.
    emit layoutChanged();
}
```

Replace with:

```cpp
void FolderModel::reorder(const SortSpec &spec)
{
    // Qt requires the about-to-change / remap / changed sequence: emitting only
    // layoutChanged() leaves every persistent index (proxies, view current item)
    // pointing at the row number it had before the sort.
    emit layoutAboutToBeChanged({}, QAbstractItemModel::VerticalSortHint);

    // Remember which identity each persistent index referred to.
    const QModelIndexList persistent = persistentIndexList();
    QStringList persistentIdentities;
    persistentIdentities.reserve(persistent.size());
    for (const QModelIndex &index : persistent) {
        const int row = index.row();
        persistentIdentities.append(row >= 0 && row < m_entries.size()
                                       ? m_entries.at(row).identity
                                       : QString());
    }

    m_entries = SortService::apply(m_entries, spec);

    // Identity → new row, built once.
    QHash<QString, int> rowByIdentity;
    rowByIdentity.reserve(m_entries.size());
    for (int row = 0; row < m_entries.size(); ++row) {
        rowByIdentity.insert(m_entries.at(row).identity, row);
    }

    QModelIndexList from;
    QModelIndexList to;
    from.reserve(persistent.size());
    to.reserve(persistent.size());
    for (int i = 0; i < persistent.size(); ++i) {
        const QString identity = persistentIdentities.at(i);
        if (identity.isEmpty()) {
            continue;
        }
        const auto it = rowByIdentity.constFind(identity);
        if (it == rowByIdentity.constEnd()) {
            continue;
        }
        from.append(persistent.at(i));
        to.append(index(it.value(), persistent.at(i).column()));
    }
    changePersistentIndexList(from, to);

    // Ordering changed only; contents, thumbnails and counts are unchanged.
    emit layoutChanged({}, QAbstractItemModel::VerticalSortHint);
}
```

**File:** `src/models/folder_model.hpp`
**Location:** includes

Find:

```cpp
#include <QAbstractListModel>
#include <QHash>
```

Replace with:

```cpp
#include <QAbstractListModel>
#include <QHash>
#include <QModelIndexList>
```

**Dependencies:** AUD-024's identity hash makes the remap O(n) instead of O(n log n) with repeated scans; this patch builds its own local map, so it is correct either way.
**Regression risks:** `layoutChanged` now carries a hint and is preceded by `layoutAboutToBeChanged`; `test_folder_model.cpp` that connects to `layoutChanged` count must expect both signals. `SearchProxy` (a `QSortFilterProxyModel`) will re-evaluate mappings on the pair, which is the desired behaviour. Views may reset their current index if the remap misses (identity-less rows are skipped, which is why AUD-020 matters).
**Deferred verification:** sort by Name, select row 5 and note the filename, switch to Size ↓: the same file must stay selected/current, and the Reel highlight must follow. Check with 50 k rows that the sort remains interactive.
**Evidence confidence:** CONFIRMED (contract violation); the user-visible drift is STRONGLY SUPPORTED.

---

## 8. P2 findings

### AUD-023 — Search proxy: cap applied to source rows, ranking discarded, match roles invisible to QML

**Severity:** P2 — Medium (large folders silently lose results; the Manifest cannot highlight)
**Category:** Architecture / Missing functionality / Performance
**Specification reference:** §7.3 ("Filename with the **matched substring** highlighted in `--signal`"), §7.4 ("No matches" only when there are none), §20.3 ("matched ranges are exposed as a model role so the Manifest delegate can highlight them in signal blue"; "folders … ranked above files by default"), §22.6 ("incremental filter with a result cap and a 'showing first 2,000' note").

**Code evidence**

`src/models/search_proxy.cpp:58-60`:

```cpp
bool SearchProxy::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const {
    if (query_.isEmpty()) return true;
    if (sourceRow >= kRowCap) return false; // cap enforced
```

`src/models/search_proxy.hpp:15-21` declares custom roles (`MatchRangesRole`, `EntryFileNameRole`) but the class overrides **no** `roleNames()` (`grep -n roleNames src/models/search_proxy.*` → none). `src/search/search_service.cpp:29-34` computes a rank that no consumer reads.

**Current implementation**

- The cap is applied to the *source row index*, so in a 50 000-entry folder every match after row 2 000 is unreachable: a query with three matches in rows 3 000+ renders the "No matches" state (§7.4) — the wording lies.
- `filterAcceptsRow` reduces `SearchService::search()` to a boolean; the rank and the folders-first boost are thrown away and the proxy never sorts, so results stay in browse order.
- Custom roles have no names, so QML cannot bind `model.matchRanges`; `ManifestRow` declares the property and no delegate assigns it — the highlight specified in §7.3 is not implemented.

**Expected implementation**

Filtering is uncapped (the Manifest `ListView` is virtualized); the note appears when the result count exceeds the cap; results are ordered by rank then name; `roleNames()` exposes the match ranges and the display name.

**Root cause** — CONFIRMED. The proxy was written as a filter, then asked to behave as a ranked result set.

**Correct architecture**

`SearchProxy` owns query, filtering, ranking (`lessThan`) and the match-range role; `SearchController` owns the debounce and the summary text.

**Implementation-ready patch**

**File:** `src/models/search_proxy.cpp`
**Location:** `filterAcceptsRow` cap line and the class's role plumbing

Find:

```cpp
bool SearchProxy::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const {
    if (query_.isEmpty()) return true;
    if (sourceRow >= kRowCap) return false; // cap enforced
```

Replace with:

```cpp
bool SearchProxy::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const {
    if (query_.isEmpty()) return true;
    // No source-row cap: the row index says nothing about how many results were
    // produced, so capping it hid every match past kRowCap and made the
    // "No matches" state lie (§7.4). The Manifest view is virtualized, so a large
    // result set is cheap; the cap only drives the "showing first N" note.
```

**File:** `src/models/search_proxy.cpp`
**Location:** after `filterAcceptsRow`

```cpp
QHash<int, QByteArray> SearchProxy::roleNames() const
{
    QHash<int, QByteArray> names = QSortFilterProxyModel::roleNames();
    names.insert(MatchRangesRole, QByteArrayLiteral("matchRanges"));
    names.insert(EntryFileNameRole, QByteArrayLiteral("entryFileName"));
    return names;
}

bool SearchProxy::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    // Rank first (prefix / word boundary / folders-first), then name — the
    // ranking SearchService computes was previously discarded (§20.3).
    const QString leftName = left.data(qg::FolderModel::NameRole).toString();
    const QString rightName = right.data(qg::FolderModel::NameRole).toString();
    const bool leftFolder = left.data(qg::FolderModel::IsFolderRole).toBool();
    const bool rightFolder = right.data(qg::FolderModel::IsFolderRole).toBool();

    const auto rank = [this](const QString &name, bool folder) {
        const QVector<QPair<QString, bool>> single = {{name, folder}};
        const auto results = searchService_.search(SearchSpec{query_, false}, single);
        return results.isEmpty() ? 0 : results.first().rank;
    };

    const int leftRank = rank(leftName, leftFolder);
    const int rightRank = rank(rightName, rightFolder);
    if (leftRank != rightRank) {
        return leftRank > rightRank;
    }
    return QString::compare(leftName, rightName, Qt::CaseInsensitive) < 0;
}
```

**File:** `src/models/search_proxy.hpp`
**Location:** the `showingNote` declaration and the existing `protected:` block

Find:

```cpp
    Q_INVOKABLE QString showingNote() const; // "showing first 2,000" when capped
```

Replace with:

```cpp
    // The note is cosmetic: filtering itself is uncapped (a result cap applied to
    // source rows hid every match past kRowCap and made "No matches" lie).
    Q_INVOKABLE QString showingNote() const; // "showing first N" when rowCount > kRowCap
```

Find:

```cpp
protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
```

Replace with:

```cpp
protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
    // Custom roles need names before QML can bind them (§7.3 highlight).
    QHash<int, QByteArray> roleNames() const override;
    // Ranked order: prefix/word-boundary/folders-first, then name (§20.3).
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;
```

> `SearchService::SearchResult` already carries `int rank` (`search_service.hpp:20`), so `lessThan` can use it directly.
> After adding `lessThan`, call `setDynamicSortFilter(true)` and `sort(0)` whenever the query changes, otherwise Qt never invokes it. Cache the per-name rank in a `QHash<QString,int>` rebuilt on query change so the comparator does not re-run the match per comparison.

**Dependencies:** none.
**Regression risks:** `test_search_proxy.cpp` asserts the cap currently ("Row cap applies to the filtered result set" — the comment already disagrees with the code); that test must be rewritten to assert no truncation and the note threshold. Sorting introduces per-row `search()` calls: cache the rank in a `QHash<QString,int>` built on query change (50 k × per-compare would otherwise be slow).
**Deferred verification:** a 50 k folder with a match at row 40 000 must return it; highlight ranges must appear in the Manifest; folders must precede files for equal rank.
**Evidence confidence:** CONFIRMED.

---

### AUD-024 — Manifest rows are activated and selected against the browse model

**Severity:** P2 — Medium (playing/selecting the wrong file from search results)
**Category:** Incorrect implementation
**Specification reference:** §7.3 ("**Enter / double-click** plays the row"), §20.3 (search is a view over the same model), §20.4 (recursive results feed "the **same** SearchProxy/Manifest delegate").

**Code evidence**

`qml/QG/ContentView.qml:700-722` — the Manifest `ListView` is bound to `searchProxy`/`recursiveModel`:

```qml
                fileName: model.name !== undefined ? model.name : ""
                ...
                onClicked: contentView.selectIndex(index)
                onDoubleClicked: contentView.activateIndex(index)
```

`qml/QG/ContentView.qml:246-275` — both helpers index the canonical model:

```qml
    function activateIndex(index) {
        if (!model) return
        ...
        const path = pathForIndex(index)
```

**Current implementation**

`index` in a Manifest delegate is a *proxy* row; `pathForIndex()`/`identityForIndex()` read `folderModel` at that row. Clicking the third search result therefore selects/plays the third row of the browse listing (usually an unrelated file or folder). The `selected:` binding correctly reads `model.identity`, but `focused:` compares the proxy index with the model's focus index — so highlight and action disagree.

**Expected implementation**

Manifest interaction resolves the row's **identity** and routes it through the identity-keyed paths (`FolderModel::pathForIdentity`, `SelectionState` identity selection), never through a row index into another model.

**Root cause** — CONFIRMED. Two models share one set of row-index helpers.

**Correct architecture**

`ContentView` exposes identity-first helpers (`activateIdentity`, `selectIdentity`); the Manifest delegate passes `model.identity`. Index helpers remain for the browse views where `index` is the model row.

**Implementation-ready patch**

**File:** `qml/QG/ContentView.qml`
**Location:** helper block (after `isFolderForIndex`)

Find:

```qml
    function isFolderForIndex(index) {
        return (model && model.isFolderAt) ? model.isFolderAt(index) : false
```

Replace with:

```qml
    // Identity-first activation for views whose row index is NOT a FolderModel
    // row (Manifest / recursive results). §7.3: the row that was clicked plays.
    function activateIdentity(identity, path, isFolder) {
        if (!identity || identity.length === 0) {
            return
        }
        if (isFolder) {
            if (path && path.length > 0)
                activatedItem(-1, path, true)
        } else {
            activatedItem(-1, path, false)
        }
    }

    function isFolderForIndex(index) {
        return (model && model.isFolderAt) ? model.isFolderAt(index) : false
```

**File:** `qml/QG/ContentView.qml`
**Location:** the Manifest delegate's handlers

Find:

```qml
                onClicked: contentView.selectIndex(index)
                onDoubleClicked: contentView.activateIndex(index)
```

Replace with: (in the Manifest delegate only — the browse delegates keep the index form)

```qml
                onClicked: {
                    if (selectionState)
                        selectionState.selectOne(model.identity, index)
                }
                onDoubleClicked: contentView.activateIdentity(model.identity, model.path,
                                                              model.isFolder === true)
```

**Dependencies:** AUD-004 (identity selection with notification).
**Regression risks:** `tst_contentview.qml` / `tst_search_proxy` may drive `activateIndex` from the manifest; the `activatedItem` signal is already the contract used by `Main.qml:577`.
**Deferred verification:** search a folder, double-click result 2 of 6: exactly that file plays, and the Reel context is that file's folder.
**Evidence confidence:** CONFIRMED.

---

### AUD-025 — Identity→row lookup is a linear scan on every model update

**Severity:** P2 — Medium (supported performance risk at the specified 50 k rows)
**Category:** Performance
**Specification reference:** §22.6 ("remain responsive with 50,000+ entries"; "Sorting a large folder is O(n log n) on lightweight keys"), §22.1 rule 5 (bounded GUI work per frame), §18.4 (per-thumbnail `dataChanged`).

**Code evidence**

`src/models/folder_model.cpp:302-310`:

```cpp
int FolderModel::indexOfIdentity(const QString &identity) const
{
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries.at(i).identity == identity) {
            return i;
        }
    }
    return -1;
}
```

Callers: every `updateThumbnail`, `updateProbe`, `updateState`, `updateFavorite`, `updateWatched`, `updateProgress`, `updatePathAndName`, `removeByIdentity`, `thumbnailStateAt`, `pathForIdentity`, plus `PlaybackController::reelEntryFor`/`onModelDataChanged`, `StateController::currentFavorite/currentWatched`.

**Current implementation**

Each of those is O(n) with `QString` comparison. Decorating one viewport of ~40 rows costs ~1 M string comparisons; a full `loadIntoModel()` pass over 50 k rows is O(n²) = 2.5 G comparisons. `onModelDataChanged` compounds it (one scan per changed row).

**Expected implementation**

An identity→row hash maintained alongside the row list, invalidated on structural change and rebuilt on reorder.

**Root cause** — CONFIRMED. The model grew identity-keyed mutators without an index; the class already keeps two `QHash`es for thumbnails, so the pattern exists.

**Correct architecture**

`m_rowByIdentity` is rebuilt in `applySnapshot`/`clear`/`reorder` and updated incrementally in `applyBatch`/`applyDiff`/`removeByIdentity`. `indexOfIdentity` becomes a hash lookup; `entryAt` is unchanged.

**Implementation-ready patch**

**File:** `src/models/folder_model.hpp`
**Location:** private members

Find:

```cpp
    QString m_folderPath;
    QList<Entry> m_entries;
```

Replace with:

```cpp
    QString m_folderPath;
    QList<Entry> m_entries;
    // identity → row. Kept in lockstep with m_entries; every mutator that
    // changes order or membership must update it (§22.6: 50k rows cannot afford
    // an O(n) scan per role update).
    QHash<QString, int> m_rowByIdentity;
```

**File:** `src/models/folder_model.cpp`
**Location:** `applySnapshot`, `applyBatch`, `removeByIdentity`, `reorder`, `indexOfIdentity`

Find:

```cpp
    m_entries = snapshot.entries;
    m_thumbnails.clear();
    m_thumbnailStates.clear();
    recount();
```

Replace with:

```cpp
    m_entries = snapshot.entries;
    m_thumbnails.clear();
    m_thumbnailStates.clear();
    m_rowByIdentity.clear();
    m_rowByIdentity.reserve(m_entries.size());
    for (int row = 0; row < m_entries.size(); ++row) {
        m_rowByIdentity.insert(m_entries.at(row).identity, row);
    }
    recount();
```

Find:

```cpp
    beginInsertRows(QModelIndex(), first, last);
    m_entries.append(batch.entries);
    for (const Entry &entry : batch.entries) {
        adjustCounts(entry, 1);
    }
```

Replace with:

```cpp
    beginInsertRows(QModelIndex(), first, last);
    m_entries.append(batch.entries);
    for (int i = 0; i < batch.entries.size(); ++i) {
        const Entry &entry = batch.entries.at(i);
        m_rowByIdentity.insert(entry.identity, first + i);
        adjustCounts(entry, 1);
    }
```

Find:

```cpp
    beginRemoveRows(QModelIndex(), row, row);
    adjustCounts(m_entries.at(row), -1);
    m_entries.removeAt(row);
    m_thumbnails.remove(identity);
    m_thumbnailStates.remove(identity);
    endRemoveRows();
```

Replace with:

```cpp
    beginRemoveRows(QModelIndex(), row, row);
    adjustCounts(m_entries.at(row), -1);
    m_entries.removeAt(row);
    m_thumbnails.remove(identity);
    m_thumbnailStates.remove(identity);
    m_rowByIdentity.remove(identity);
    // Rows after the removed one shifted by one.
    for (int i = row; i < m_entries.size(); ++i) {
        m_rowByIdentity.insert(m_entries.at(i).identity, i);
    }
    endRemoveRows();
```

Find:

```cpp
int FolderModel::indexOfIdentity(const QString &identity) const
{
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries.at(i).identity == identity) {
            return i;
        }
    }
    return -1;
}
```

Replace with:

```cpp
int FolderModel::indexOfIdentity(const QString &identity) const
{
    return m_rowByIdentity.value(identity, -1);
}
```

Find: (inside `reorder`, after the sort and before the persistent-index remap)

```cpp
    m_entries = SortService::apply(m_entries, spec);
```

Replace with:

```cpp
    m_entries = SortService::apply(m_entries, spec);
    m_rowByIdentity.clear();
    m_rowByIdentity.reserve(m_entries.size());
    for (int row = 0; row < m_entries.size(); ++row) {
        m_rowByIdentity.insert(m_entries.at(row).identity, row);
    }
```

**Dependencies:** AUD-020 (identity-less rows would collide on the empty key in the hash; with the fingerprint fallback they no longer occur). `applyDiff`'s `changed` branch updates path/name in place (row unchanged) — no index update needed; its `added` branch calls `applyBatch` (covered).
**Regression risks:** duplicate identities (hard links) collapse to one hash entry — `indexOfIdentity` returns the last row instead of the first. That is already broken today in a different way (mutations only hit the first); document it as a known limitation or dedupe hard links in the enumerator.
**Deferred verification:** `test_folder_model.cpp` (rowsInserted/rowsRemoved integrity) and `test_probe_memory.cpp` at 100 k rows; measure per-thumbnail update cost before/after.
**Evidence confidence:** CONFIRMED.

---

### AUD-026 — Position persistence is throttled against media time, so it stalls after a seek or a file change

**Severity:** P2 — Medium (the resume feature silently stops recording)
**Category:** Incorrect implementation
**Specification reference:** §19.6 ("While playing, position is persisted on a **throttled tick (every 5s)**"), §25.6 ("Playback position is persisted within ~5 s").

**Code evidence**

`src/playback/playback_manager.cpp:319-323`:

```cpp
    if (!force) {
        if (pos - lastTickTime_ < 5.0)
            return;                 // throttled periodic write
        lastTickTime_ = pos;
    }
```

**Current implementation**

The throttle compares the *media position* with the last persisted media position. Seeking backwards makes `pos - lastTickTime_` negative, so every tick returns early until playback passes the old position + 5 s. Opening a new file does not reset `lastTickTime_`, so a fresh video starting at 0 is throttled until it exceeds the previous file's last tick — for a 90-minute film, resume data is never written. Only the forced writes (pause/stop/close/EOF) mask the problem.

**Expected implementation**

Throttle on wall-clock/monotonic time since the last write, reset per media change.

**Root cause** — CONFIRMED. A position delta was used as a time delta.

**Correct architecture**

`QElapsedTimer` in `PlaybackManager`, reset in `loadFile()`; the write condition is elapsed ≥ 5 s.

**Implementation-ready patch**

**File:** `src/playback/playback_manager.hpp`
**Location:** includes and members

Find:

```cpp
    double lastTickTime_ = 0;
```

Replace with:

```cpp
    // Monotonic throttle for the 5 s persistence tick (§19.6). A media-time
    // delta was used before, which stalls after a backwards seek and across a
    // media change.
    QElapsedTimer lastTickTimer_;
```

**File:** `src/playback/playback_manager.cpp`
**Location:** `writePosition` throttle block

Find:

```cpp
    if (!force) {
        if (pos - lastTickTime_ < 5.0)
            return;                 // throttled periodic write
        lastTickTime_ = pos;
    }
```

Replace with:

```cpp
    if (!force) {
        if (lastTickTimer_.isValid() && lastTickTimer_.elapsed() < 5000) {
            return; // throttled periodic write
        }
        lastTickTimer_.restart();
    }
```

**File:** `src/playback/playback_manager.cpp`
**Location:** `loadFile()` (after a successful load)

Find:

```cpp
    if (tickTimer_)
        tickTimer_->start();
}

void PlaybackManager::play() {
```

Replace with:

```cpp
    // New media: the throttle window starts now, so the first tick of a fresh
    // file is written even if the previous file ended at a higher position.
    lastTickTimer_.restart();
    if (tickTimer_)
        tickTimer_->start();
}

void PlaybackManager::play() {
```

**File:** `src/playback/playback_manager.cpp`
**Location:** includes

Find:

```cpp
#include <QTimer>
```

Replace with:

```cpp
#include <QElapsedTimer>
#include <QTimer>
```

**Dependencies:** none.
**Regression risks:** `test_resume_policy.cpp` / `test_transport_state.cpp` may assert write counts over simulated ticks; the 5 s window now depends on real elapsed time, so tests must either wait or inject a clock. Add a `setPositionTickIntervalForTesting(int ms)` hook if the suite needs determinism.
**Deferred verification:** play 30 s, seek back to 0:00, play 20 s, kill the app: the stored position must be ~20 s, not 0. Repeat across a media change.
**Evidence confidence:** CONFIRMED.

---

### AUD-027 — `playing` never notifies; `stop()` leaves a stale session

**Severity:** P2 — Medium (the Stage's play/pause affordance and any `playing` binding go stale; stopped sessions keep answering identity queries)
**Category:** State management / Incorrect implementation
**Specification reference:** §15.2 (controllers translate results into observable state), §19.1 (session state), §9.1 (state preservation on close).

**Code evidence**

`src/controllers/playback_controller.cpp:22-24`:

```cpp
bool PlaybackController::playing() const {
    return currentSession_ && currentSession_->state() == PlaybackSession::Playing;
}
```

`grep -n "connect(" src/controllers/playback_controller.cpp` → the only connections are model signals in `setFolderModel`; `PlaybackSession::stateChanged` is never connected. `stop()` (`:179-192`) emits `playingChanged()`/`playbackEnded()` but leaves `currentSession_` and its identity intact.

**Current implementation**

`playing` is a `Q_PROPERTY` with `NOTIFY playingChanged`, but the signal only fires from `play()`/`playCurrent()`/`stop()` — not when mpv pauses (transport, `p` key, external), reaches EOF, or errors. `Main.qml:726` binds `Stage.playing` to it, so the Stage's centre glyph and `TransportBar.playing` (`stage.session.state === 2`) disagree. After `stop()`, `currentIdentity()` still returns the stopped item, so `setReelFilter()` rebuilds a context around a dead session and the mount-disconnect check (`composition.cpp:192-196`) inspects a stale path.

**Expected implementation**

`playingChanged` fires on every session state transition (the AUD-012 patch already installs the connection); `stop()` clears the identity so no consumer sees a dead session.

**Root cause** — CONFIRMED. Property notification was wired to intent signals rather than to the observed state.

**Correct architecture**

The session is the single source of state; the controller mirrors it and emits property changes from one place (`onSessionStateChanged`).

**Complete permanent resolution** — covered by the AUD-012 patch (`attachSession()` + `onSessionStateChanged()` emitting `playingChanged`), plus the `stop()` clearing below.

**Implementation-ready patch**

**File:** `src/controllers/playback_controller.cpp`
**Location:** `stop()` (lines 179-192)

Find:

```cpp
void PlaybackController::stop()
{
    if (manager_) {
        manager_->stop();
        // Closing the stage always leaves Cinema (DESIGN.md §9.3): the layout
        // returns to Browse.
        if (cinemaActive_) {
            cinemaActive_ = false;
            emit cinemaActiveChanged();
        }
        emit playingChanged();
        emit playbackEnded();
    }
}
```

Replace with:

```cpp
void PlaybackController::stop()
{
    if (manager_) {
        manager_->stop();
        // Closing the stage always leaves Cinema (DESIGN.md §9.3): the layout
        // returns to Browse.
        if (cinemaActive_) {
            cinemaActive_ = false;
            emit cinemaActiveChanged();
        }
        // No stale session: currentIdentity()/setReelFilter()/the mount check
        // must not keep reasoning about a stopped item.
        if (context_ != nullptr) {
            context_->clear(); // forget the ordered list; rebuilt on next play
        }
        if (currentSession_ != nullptr) {
            disconnect(currentSession_, &PlaybackSession::stateChanged,
                       this, &PlaybackController::onSessionStateChanged);
            currentSession_ = nullptr;
            emit currentSessionChanged();
        }
        emit playingChanged();
        emit playbackEnded();
    }
}
```

**File:** `src/playback/playback_context.hpp`
**Location:** public API

Find:

```cpp
    Q_INVOKABLE void build(const qg::FolderModel* model,
```

Replace with:

```cpp
    // Drop the ordered list and the cursor (playback closed).
    Q_INVOKABLE void clear();

    Q_INVOKABLE void build(const qg::FolderModel* model,
```

**File:** `src/playback/playback_context.cpp`
**Location:** after `build()`

```cpp
void PlaybackContext::clear()
{
    if (entries_.isEmpty() && currentIndex_ < 0) {
        return;
    }
    entries_.clear();
    currentIndex_ = -1;
    emit totalCountChanged();
    emit currentIndexChanged();
}
```

**Dependencies:** AUD-012 (`attachSession`, `onSessionStateChanged`).
**Regression risks:** `currentSession()` becomes null after stop, so QML that reads `activeSession.state` without a null guard must be checked — `Main.qml:112-118` already null-guards `sessionActive`. Tests asserting `currentIdentity()` after `stop()` must be updated (that behaviour was the defect).
**Deferred verification:** pause from the transport, press Space, let a file end: the Stage glyph, the transport button and `playing` must all agree at every step. After Close, `currentIdentity()` must be empty.
**Evidence confidence:** CONFIRMED.

---

### AUD-028 — Mount parsing: octal escapes, removable probe, single-slot disconnect state

**Severity:** P2 — Medium (mounts with spaces are unreachable and unejectable; removable sticks misclassified; two disconnects collapse into one)
**Category:** Filesystem correctness / Platform
**Specification reference:** §17.4 ("Parses `/proc/self/mountinfo` for mount points, filesystem types, and device names"; removable/enrichment), §3.3 ("A removable drive shows an eject affordance"; "A disconnected drive shows an amber badge"), §12 ("`\"Old HDD\" is not connected`" — by name), §23.14.

**Code evidence**

`src/filesystem/mounts.cpp:169-182`:

```cpp
    const QString mountPoint = QFile::decodeName(pre.at(4));
```

`src/filesystem/mounts.cpp:262-272`:

```cpp
        QFile removable(QDir(paths.sysBlockDir).filePath(base + QStringLiteral("/../removable")));
```

`src/controllers/mounts_controller.cpp:8-22` and `src/core/composition.cpp:235-238`:

```cpp
                             ctl->handleVolumeGone(gone, gone);
```

**Current implementation**

1. `mountinfo` escapes space/tab/newline/backslash as `\040`, `\011`, `\012`, `\134`. `QFile::decodeName` does local-8-bit decoding only, so `/media/user/My\040Passport` is stored verbatim: navigating there fails and eject passes a non-existent path. External media with spaces is the common case.
2. The removability probe hops to the parent (`base/../removable`), which resolves correctly only when `base` is a *partition* (`sda1`). For a whole-disk source (`sdb`, `nvme0n1` with the filesystem directly on it) it resolves to `/sys/class/block/removable`, which does not exist, so the device is classified `Internal` and loses its removable presentation/eject affordance.
3. `MountsController` keeps a single disconnected slot; `composition.cpp` passes the mount path as the drive *name*, so the amber state and the §12 copy show `/media/user/USB` instead of the volume label. A second disconnect overwrites the first, and any reappearing volume clears the state.

**Expected implementation**

Decode the kernel's octal escapes; probe `<base>/removable` first and fall back to the parent only when `base` itself has no such file; keep a set of disconnected volumes keyed by mount point with their labels.

**Root cause** — CONFIRMED for (1) and (2) (path structure verified against `/sys/class/block` on this host); STRONGLY SUPPORTED for the user-visible classifying effect. (3) is CONFIRMED.

**Correct architecture**

`MountService` owns mount-info decoding and capability probing (single place); `MountsController` owns the disconnected *set* and label mapping.

**Implementation-ready patch**

**File:** `src/filesystem/mounts.cpp`
**Location:** the `mountinfo` field extraction (line ~169)

Find:

```cpp
    const QString mountPoint = QFile::decodeName(pre.at(4));
```

Replace with:

```cpp
    // mountinfo escapes space/tab/newline/backslash as \040 \011 \012 \134.
    // QFile::decodeName only does local-8-bit decoding, which left the escapes
    // in the path and made every mount point containing a space unreachable.
    const QString mountPoint = decodeMountInfoField(pre.at(4));
```

**File:** `src/filesystem/mounts.cpp`
**Location:** anonymous namespace at the top of the file

```cpp
namespace {

// Decode the kernel's octal escapes in /proc/self/mountinfo fields.
QString decodeMountInfoField(const QByteArray &raw)
{
    QByteArray decoded;
    decoded.reserve(raw.size());
    for (int i = 0; i < raw.size(); ++i) {
        if (raw.at(i) == '\\' && i + 3 < raw.size()
            && raw.at(i + 1) >= '0' && raw.at(i + 1) <= '7'
            && raw.at(i + 2) >= '0' && raw.at(i + 2) <= '7'
            && raw.at(i + 3) >= '0' && raw.at(i + 3) <= '7') {
            const int value = (raw.at(i + 1) - '0') * 64
                              + (raw.at(i + 2) - '0') * 8
                              + (raw.at(i + 3) - '0');
            decoded.append(static_cast<char>(value));
            i += 3;
            continue;
        }
        decoded.append(raw.at(i));
    }
    return QFile::decodeName(decoded);
}

} // namespace
```

**File:** `src/filesystem/mounts.cpp`
**Location:** the removable probe (lines 262-272)

Find:

```cpp
        QFile removable(QDir(paths.sysBlockDir).filePath(base + QStringLiteral("/../removable")));
        if (removable.open(QIODevice::ReadOnly)) {
```

Replace with:

```cpp
        // `<base>/removable` exists for whole-disk sources (sdb, nvme0n1 with the
        // filesystem directly on the device); `<base>/../removable` only exists
        // when base is a partition. Try the device's own flag first, then the
        // parent's, and never assume the layout.
        QFile removable(QDir(paths.sysBlockDir).filePath(base + QStringLiteral("/removable")));
        if (!removable.exists()) {
            removable.setFileName(
                QDir(paths.sysBlockDir).filePath(base + QStringLiteral("/../removable")));
        }
        if (removable.open(QIODevice::ReadOnly)) {
```

**File:** `src/controllers/mounts_controller.hpp`
**Location:** the disconnected state members

Find:

```cpp
    bool disconnected_ = false;
```

Replace with:

```cpp
    bool disconnected_ = false;
    // Every disconnected volume, keyed by mount point → label. One slot lost
    // data when two drives vanished and let any other volume clear the state.
    QHash<QString, QString> disconnectedVolumes_;
```

**File:** `src/core/composition.cpp`
**Location:** the mount-change lambda (lines 226-247)

Find:

```cpp
                         for (const QString &gone : *known) {
                             if (!present.contains(gone)) {
                                 ctl->handleVolumeGone(gone, gone);
                             }
                         }
```

Replace with:

```cpp
                         // Use the volume LABEL for the §12 copy, not the mount
                         // path ("\"Old HDD\" is not connected").
                         QHash<QString, QString> labels;
                         for (const VolumeInfo &v : volumes) {
                             labels.insert(v.mountPoint, v.label);
                         }
                         for (const QString &gone : *known) {
                             if (!present.contains(gone)) {
                                 const QString label = labels.value(gone, gone);
                                 ctl->handleVolumeGone(label, gone);
                             }
                         }
```

**Dependencies:** none; `VolumeInfo::label` exists (used by the sidebar).
**Regression risks:** `test_mounts.cpp` (14 cases) asserts mount-point parsing and classification; the fixture set may need a `\040` entry. `handleVolumeBack` must remove only the matching mount point from the new set (patch its body accordingly).
**Deferred verification:** mount a loop device at `/tmp/My\040Disk` and navigate; classify a whole-disk USB stick as Removable; disconnect two drives and confirm both are reported and the labels appear.
**Evidence confidence:** STRONGLY SUPPORTED (paths verified on this host; no `\040` mount or whole-disk removable present).

---

### AUD-029 — Unwired surfaces: hidden files, help, focus restoration, task popover, Reel/CompactRow keyboard and accessibility

**Severity:** P2 — Medium (several specified affordances exist in name only)
**Category:** Missing functionality / Accessibility / Convention
**Specification reference:** §17.2 (Show hidden), §13.2 (Help `F1`/`?`, `Space` select, `W` watched, `Ctrl+I` properties), §13.3 ("going up focuses the folder you came from"), §3.6 ("clicking it opens a small popover listing running tasks"), §13.4 (keyboard-only operation, named controls, `accessibilityValueText`), §2.8 (reduced motion).

**Code evidence** (all verified by grep)

| Symptom | Evidence |
|---|---|
| "Show hidden" never toggles | `BrowserController::setShowHidden` has no `Q_PROPERTY`/`Q_INVOKABLE` (`browser_controller.hpp:63-64`); `Main.qml:517-521` guards on `setShowHidden !== undefined` and never calls it |
| No Help surface | `grep -rn "F1\|helpoverlay" qml/ src/` → no shortcut, no component; `KeyboardFramework::actionTable()` has no help id |
| Focus restoration dead | `BrowserController::parentRestored` emitted (`:135-141`) with no receiver; `KeyboardFramework::focusRestoreIndexFor()` has no caller |
| Task popover always empty | `StatusStrip.qml:23` `tasks` is never assigned; the popover shows "No background tasks" |
| Status pulse ignores reduced motion | `StatusStrip.qml:100-108` runs `running: strip.visible`, not `busyCount > 0 && Theme.motionEnabled` |
| Reel not keyboard-operable | `ReelPanel.qml` rows/filter chips are bare `MouseArea`s: no `Keys`, no focus, no `Accessible.name` |
| CompactRow has no accessible name | `CompactRow.qml` has no `Accessible.*` (VideoCard/FolderCard do) |
| Timeline/volume not sliders with value text | `TransportBar.qml:239-241, 292-293` set `Accessible.role: Slider` + a raw-seconds `description`; §13.4 requires `accessibilityValueText` ("1 minute 12 seconds of 4 minutes 12 seconds") |
| `Space`/`W`/`Ctrl+I` unbound | `ContentView.handleKey` handles only `Ctrl+Space`; `Main.qml:936-1025` has no `W`/`Ctrl+I` |

**Current implementation**

Each row of the table above is a live defect: the C++ side (or the design intent) exists and the binding does not, so the affordance is inert, invisible to assistive technology, or keyboard-unreachable.

**Expected implementation**

Every row in the table becomes operable: Show hidden toggles and persists per folder; `F1`/`?` opens a shortcuts overlay listing §13.2; going up focuses the folder that was left; the status popover lists the running tasks it claims to list; pulses obey reduced motion and only run while work is pending; the Reel and Compact rows are focusable, arrow-navigable and named; the transport sliders expose `Accessible.name` and `Accessible.valueText`.

**Root cause**

Per item, not shared — each is an independent omission of the last hop (a property, an invokable, a signal consumer, a `Keys` handler, an `Accessible` binding). This finding is a **cluster** kept together because the items share one remediation workstream (UI affordances) and would otherwise generate a dozen one-line findings.

**Correct architecture**

No new components: `BrowserController` owns `showHidden`; `Main.qml` owns the Help overlay and the shortcut map; `BrowserController::parentRestored` is consumed by `ContentView`; `StatusStrip` reads task counts from the controllers that already expose them (`ThumbnailController::pendingCount`, probe queue depth, action state); Reel rows and Compact rows become focusable delegates with `Accessible` names; `TransportBar` gains `valueText`.

**Correct architecture**

No new components. `BrowserController` exposes `showHidden` as a property + invokable; `Main.qml` adds `F1` and the Help overlay (a `Popup` listing §13.2); `BrowserController::parentRestored` is consumed by `ContentView` to focus the named child; `StatusStrip` takes a task list from the controllers that already expose counts (`ThumbnailController::pendingCount`, `ProbeController` queue depth, `ActionsController` operation state); Reel rows become `ItemDelegate`s with `Keys`/`Accessible`; transport sliders get `Accessible.name` + `Accessible.valueText`.

**Implementation-ready patch (the two that are self-contained today)**

**File:** `src/controllers/browser_controller.hpp`
**Location:** properties and public methods

Find:

```cpp
    bool showHidden() const { return m_showHidden; }
    void setShowHidden(bool showHidden);
```

Replace with:

```cpp
    // "Show hidden files" (DESIGN.md §17.2, default off). Exposed as a property
    // so the overflow menu's toggle can read and write it; without this the menu
    // item was inert and m_showHidden could never become true.
    Q_PROPERTY(bool showHidden READ showHidden WRITE setShowHidden NOTIFY showHiddenChanged)
    bool showHidden() const { return m_showHidden; }
    Q_INVOKABLE void setShowHidden(bool showHidden);
```

and add `void showHiddenChanged();` to the signals block.

**File:** `qml/QG/Main.qml`
**Location:** the overflow handler (lines 517-521)

Find:

```qml
                        if (window.browserController
                                && window.browserController.setShowHidden !== undefined) {
```

Replace with:

```qml
                        if (window.browserController) {
                            // Property write: QML can assign to it directly now.
                            window.browserController.showHidden = !window.browserController.showHidden
```

**File:** `qml/QG/strips/StatusStrip.qml`
**Location:** the activity animation (lines 100-108)

Find:

```qml
                        SequentialAnimation on opacity {
                            running: strip.visible
```

Replace with:

```qml
                        SequentialAnimation on opacity {
                            // Reduced motion: a static dot (§2.8); and only while
                            // work is actually running (§3.6).
                            running: strip.visible && strip.busyCount > 0 && Theme.motionEnabled
```

**Not patched here:** Help overlay, focus restoration consumer, task-list plumbing, Reel keyboard/a11y and transport `valueText` are UI additions that need design input (what the Help panel contains, which tasks the popover lists). They are captured as FIX-UI-02 in the roadmap with per-item acceptance criteria; writing them blind would be invention, not remediation.

**Dependencies:** none for the three patches above.
**Regression risks:** `tst_statusstrip.qml` may assert the pulse runs while visible. `tst_pathbar.qml` tests the overflow menu; switching to a property write requires the injected controller to be a real `BrowserController` (it is).
**Deferred verification:** toggle Show hidden → dotfiles appear and persist for the folder; with a 3-item thumbnail queue the strip pulses (and stops under Reduce motion); keyboard-only pass over the Reel (`Tab` in, arrows, Enter).
**Evidence confidence:** CONFIRMED for every row in the table.

---

### AUD-030 — The Stage fades and scales in on every playback start

**Severity:** P2 — Medium (explicitly forbidden transition; every play looks like a mode switch)
**Category:** Specification / QML binding
**Specification reference:** playback §2.1 item 4 ("display the video directly in the Stage as soon as the video surface is ready. **No thumbnail-to-video cross-fade, fade-out, fade-in, or additional playback transition is required**"). Supersedes product §2.8's bloom cross-fade for the Stage's first frame — see §2.2 of this report for the reconciliation.

**Code evidence**

`qml/QG/playback/Stage.qml:68-92`:

```qml
    // Bloom/collapse (DESIGN.md §2.8, §8.1): cross-fade + geometry tween.
    // Reduced-motion → instant swap (no animation).
    opacity: 0
    Behavior on opacity {
        enabled: Theme.motionEnabled
        NumberAnimation { duration: Theme.motionBloom; easing.type: Easing.OutCubic }
    }
    scale: 0.92
    Behavior on scale {
        enabled: Theme.motionEnabled
        NumberAnimation { duration: Theme.motionBloom; easing.type: Easing.OutCubic }
    }

    states: [
        State {
            name: "bloomed"
            when: stage.playing || stage.currentName.length > 0
            PropertyChanges { target: stage; opacity: 1; scale: 1 }
        },
        State {
            name: "collapsed"
            when: !(stage.playing || stage.currentName.length > 0)
            PropertyChanges { target: stage; opacity: 0; scale: 0.92 }
        }
    ]
```

**Current implementation**

Entering the "bloomed" state animates the whole Stage — including the `VideoSurfaceItem` that just presented its first mpv frame — from opacity 0 to 1 and scale 0.92 to 1 over 220 ms. Because the Stage is only visible while `sessionActive`, the practical effect is a fade-in of live video on every play, and a fade-out on close. §2.1.4 forbids exactly this ("fade-out, fade-in, or additional playback transition"), and the *detail* it protects — the frame is already there, so fading it makes the player look like it is still loading — is the visible symptom.

**Expected implementation**

The Stage is opaque and unscaled the moment it is shown: the video appears as soon as `VideoSurfaceItem` presents. Motion remains available for things the spec does want animated (the Stage's *geometry* if a future revision reintroduces bloom), but not for the video's first frame.

**Root cause** — CONFIRMED. The product-spec motion section was implemented literally, before the playback document's explicit prohibition existed.

**Correct architecture**

Presentation state (`opacity`, `scale`) is not part of the Stage's contract; the Stage's size is. `Theme.motionBloom` keeps its other uses.

**Implementation-ready patch**

**File:** `qml/QG/playback/Stage.qml`
**Location:** lines 68-92

Find:

```qml
    // Bloom/collapse (DESIGN.md §2.8, §8.1): cross-fade + geometry tween.
    // Reduced-motion → instant swap (no animation).
    opacity: 0
    Behavior on opacity {
        enabled: Theme.motionEnabled
        NumberAnimation { duration: Theme.motionBloom; easing.type: Easing.OutCubic }
    }
    scale: 0.92
    Behavior on scale {
        enabled: Theme.motionEnabled
        NumberAnimation { duration: Theme.motionBloom; easing.type: Easing.OutCubic }
    }

    states: [
        State {
            name: "bloomed"
            when: stage.playing || stage.currentName.length > 0
            PropertyChanges { target: stage; opacity: 1; scale: 1 }
        },
        State {
            name: "collapsed"
            when: !(stage.playing || stage.currentName.length > 0)
            PropertyChanges { target: stage; opacity: 0; scale: 0.92 }
        }
    ]
```

Replace with:

```qml
    // No entry transition (DESIGN.md playback §2.1 item 4): the video appears in
    // the Stage as soon as the surface is ready — no cross-fade, no fade-in, no
    // scale tween. The Stage is opaque and unscaled for its whole lifetime.
    opacity: 1
    scale: 1
```

> If the project later reinstates §2.8's bloom, it must animate the *card geometry*
> into the Stage rect while the surface is already opaque — never the video's alpha —
> and it must gate on `Theme.motionEnabled` as the removed `Behavior`s did.

**Dependencies:** AUD-009 (the Stage's position in the flow) so that removing the fade does not expose a layout jump where the overlay used to be.
**Regression risks:** `tst_stage.qml` may assert `stage.opacity === 0` in the collapsed state or that a `Behavior` exists (the file has no such assertion today — `grep opacity tests/qml/tst_stage.qml` is empty). Any screenshot baselines of the first 220 ms change.
**Deferred verification:** start playback and capture the first 10 frames: the first non-background frame must already be at full opacity, with no dissolve from the letterbox colour; `Theme.motionEnabled = false` must make no difference to playback start.
**Evidence confidence:** CONFIRMED.

---

## 9. P3 findings

### AUD-031 — The migration framework cannot advance past version 1

**Severity:** P3 — Low today, latent High (the second migration will re-run on every launch)
**Category:** Persistence / Architecture
**Specification reference:** §21.1 ("Migrations are numbered SQL steps applied at startup with a `schema_version` row; each is idempotent and transactional").

**Code evidence**

`src/persistence/database.cpp:162-174`:

```cpp
    Statement stmt(*this,
                   QStringLiteral("SELECT version FROM schema_version LIMIT 1"));
```

`src/persistence/database.cpp:200-202`:

```cpp
            applied = execute(
                QStringLiteral("INSERT OR REPLACE INTO schema_version(version) VALUES(%1);")
                    .arg(migration.version));
```

`src/persistence/schema.sql:9` — `CREATE TABLE IF NOT EXISTS schema_version (version INTEGER NOT NULL);` (no PK/UNIQUE).

**Current implementation**

`INSERT OR REPLACE` without a primary key appends a row every time; with more than one row, `SELECT … LIMIT 1` returns an arbitrary row (typically the oldest), so `migration.version <= current` stays false and migration N is re-applied on every launch — each time adding another row. Two concurrent instances can also both read version 0 before `BEGIN IMMEDIATE`.

**Expected implementation**

One row, keyed, read and written inside the migration transaction, so each version is applied exactly once and the counter is authoritative.

**Root cause** — CONFIRMED. The table was declared without a key, so the "replace" idiom has nothing to replace.

**Correct architecture**

`schema_version` is a single-row table (`id INTEGER PRIMARY KEY CHECK (id = 1)`); `readSchemaVersion()` reads that row; migrations upsert it inside the same transaction that applies the DDL.

**Implementation-ready patch**

**File:** `src/persistence/database.cpp`
**Location:** `runMigrations()` (table creation, read, write)

Find:

```cpp
    const Result<void> ensured =
        execute(QStringLiteral("CREATE TABLE IF NOT EXISTS schema_version (version INTEGER NOT NULL);"));
```

Replace with:

```cpp
    // Single-row table: without the key, INSERT OR REPLACE appended a row per
    // migration and `LIMIT 1` then returned an arbitrary version, so migrations
    // re-applied forever (§21.1).
    const Result<void> ensured =
        execute(QStringLiteral("CREATE TABLE IF NOT EXISTS schema_version ("
                               "id INTEGER PRIMARY KEY CHECK (id = 1), "
                               "version INTEGER NOT NULL);"));
```

Find:

```cpp
    const int current = readSchemaVersion();
```

Replace with:

```cpp
    // Read inside the transaction that will write, so two instances cannot both
    // start from the same version.
    const int current = readSchemaVersion();
```

Find:

```cpp
            applied = execute(
                QStringLiteral("INSERT OR REPLACE INTO schema_version(version) VALUES(%1);")
                    .arg(migration.version));
```

Replace with:

```cpp
            applied = execute(
                QStringLiteral("INSERT INTO schema_version(id, version) VALUES(1, %1) "
                               "ON CONFLICT(id) DO UPDATE SET version = excluded.version;")
                    .arg(migration.version));
```

Find:

```cpp
                   QStringLiteral("SELECT version FROM schema_version LIMIT 1"));
```

Replace with:

```cpp
                   QStringLiteral("SELECT version FROM schema_version WHERE id = 1"));
```

**Dependencies:** none. Existing databases migrate on first run of the patch (the table already exists without the key; `CREATE TABLE IF NOT EXISTS` is a no-op, so add a one-time `ALTER`/table-rebuild migration as version 2 — the patch's new migration is exactly that, which is the first real test of the framework).
**Regression risks:** an existing DB has multiple rows and no `id` column: the first patch run will fail on the `ON CONFLICT(id)` clause. Ship it as migration **2** that rebuilds `schema_version` (`CREATE TABLE schema_version_new…; INSERT … SELECT MAX(version),1; DROP; RENAME`) and keep the new shape for version 3+. Document that sequencing in the migration list.
**Deferred verification:** run a DB with `version` rows `1,1,1`; after startup exactly one row remains with the highest version, and a second startup applies nothing.
**Evidence confidence:** CONFIRMED.

---

### AUD-032 — A transient SQLite error quarantines a healthy database

**Severity:** P3 — Low (requires a second instance or a transient I/O error) but the consequence is total state loss
**Category:** Error handling / Persistence
**Specification reference:** §21.5 ("On startup, a quick integrity check (`PRAGMA quick_check`) runs; **on failure**, QG starts with a fresh DB beside the corrupt one (never destroys it) and informs the user").

**Code evidence**

`src/persistence/database.cpp:70-90`:

```cpp
    const Result<bool> check = quickCheck();
    if (!check || !check.value()) {
        close();
        const QString quarantine = path + QStringLiteral(".corrupt-%1").arg(nowSeconds());
        if (QFile::exists(path)) { QFile::rename(path, quarantine); }
```

`quickCheck()` (`:140-160`) returns `false` for "prepare/step failed" exactly as for "not ok".

**Current implementation**

`SQLITE_BUSY` (another instance holds the write lock), `SQLITE_IOERR`, `SQLITE_CANTOPEN` and a failed `PRAGMA` prepare all trigger the corrupt-DB path: the live database is renamed away and an empty one created, so favorites/watched/positions appear lost. The `QFile::rename` result is ignored, so a failed rename leaves the original in place while `m_recovered` reports recovery.

**Expected implementation**

Only a *completed* integrity check that reports corruption quarantines the file; a check that could not run leaves the database in place and is surfaced as a degraded session. A failed rename is reported, never counted as recovery.

**Root cause** — CONFIRMED (control flow); the transient-error scenario is STRONGLY SUPPORTED.

**Correct architecture**

`quickCheck()` distinguishes "check failed to run" (an `Error`) from "database is not ok" (a value), and `open()` branches on the two. Quarantine is a two-step operation whose second step is verified.

**Implementation-ready patch**

**File:** `src/persistence/database.cpp`
**Location:** the startup check (lines 70-90)

Find:

```cpp
    const Result<bool> check = quickCheck();
    if (!check || !check.value()) {
        close();
        const QString quarantine = path + QStringLiteral(".corrupt-%1").arg(nowSeconds());
        if (QFile::exists(path)) { QFile::rename(path, quarantine); }
```

Replace with:

```cpp
    const Result<bool> check = quickCheck();
    if (!check) {
        // The check itself could not run (busy/IO/cantopen). That is not
        // corruption: keep the database and report a degraded session rather
        // than quarantining a healthy file (§21.5).
        qWarning("QG database integrity check could not run: %s",
                 qUtf8Printable(check.error().message));
        return;
    }
    if (!check.value()) {
        close();
        const QString quarantine = path + QStringLiteral(".corrupt-%1").arg(nowSeconds());
        if (QFile::exists(path) && !QFile::rename(path, quarantine)) {
            // Never report a recovery we did not perform.
            qWarning("QG database is corrupt and could not be moved aside: %s",
                     qUtf8Printable(path));
            return;
        }
        m_recovered = true;
```

**Dependencies:** none.
**Regression risks:** `test_persistence.cpp` may simulate corruption by making `quick_check` fail to prepare; that case now takes the "degraded" branch instead of quarantine. Update the test to corrupt the file content (which yields `not ok` from a successful check).
**Deferred verification:** open QG twice (second instance sees `SQLITE_BUSY`): no `.corrupt-*` file appears and state survives; truncate a copy of the DB mid-page and confirm quarantine + fresh DB + user notice.
**Evidence confidence:** CONFIRMED (code); STRONGLY SUPPORTED (transient path).

---

### AUD-033 — Hardcoded colours and magic enum literals bypass the theme and the type system

**Severity:** P3 — Low (visual consistency / robustness)
**Category:** Convention / Formatting
**Specification reference:** §2.2–§2.4 (semantic tokens only), `Theme.qml:6-7` ("No raw hex may appear outside this file"), §9 rules on high contrast.

**Code evidence**

`qml/QG/playback/Stage.qml:104,128`, `qml/QG/playback/TransportBar.qml:55`:

```qml
        color: Qt.rgba(5/255, 7/255, 13/255, 0.85)
```

`qml/QG/playback/Stage.qml:250`, `qml/QG/Main.qml:116-118`:

```qml
        playing: stage.session ? stage.session.state === 2 : false
```

**Current implementation**

The scrim colour is copied by hand in three places while `Theme.chipScrim` (`rgba(5,7,13,.72)`) is used elsewhere; these sites will not follow High-Contrast or a palette change. Playback state is compared to the integer literal `2` instead of the registered `PlaybackSession.Playing` enum, so any reordering of `PlaybackSession::State` silently changes the meaning of "playing".

**Expected implementation**

Every colour comes from a `Theme` token (adding `scrimStrong` for the 85 % ink scrim), and state comparisons use the registered enum.

**Root cause** — CONFIRMED. Two shortcuts taken before the tokens/enums were registered.

**Correct architecture**

`Theme.qml` is the only file allowed to contain colour literals; QML compares enums through their registered type (`PlaybackSession.Playing`), never through ordinals.

**Implementation-ready patch**

**File:** `qml/QG/playback/Stage.qml` (both sites), `qml/QG/playback/TransportBar.qml:55`

Find:

```qml
        color: Qt.rgba(5/255, 7/255, 13/255, 0.85)
```

Replace with:

```qml
        color: Theme.scrimStrong
```

**File:** `qml/QG/Theme.qml`
**Location:** next to `chipScrim`

```qml
    // 85% ink scrim: transport and cinema chrome over video (§2.2/§2.8).
    readonly property color scrimStrong: Qt.rgba(5 / 255, 7 / 255, 13 / 255, 0.85)
```

**File:** `qml/QG/playback/Stage.qml`
**Location:** the transport binding (line 250)

Find:

```qml
        playing: stage.session ? stage.session.state === 2 : false
```

Replace with:

```qml
        // Enum, not a literal: PlaybackSession is registered as qg.core type.
        playing: stage.session ? stage.session.state === PlaybackSession.Playing : false
```

**Dependencies:** `PlaybackSession` is already registered (`composition.cpp:61`), and `Stage.qml` imports `qg.core 1.0`.
**Regression risks:** `tst_transport.qml` / `tst_stage.qml` may read `Theme.chipScrim` expectations; adding `scrimStrong` is additive. Enum access requires the import (present).
**Deferred verification:** switch High-Contrast: the transport scrim must follow the token; reorder the enum in a scratch build and confirm the QML still compiles.
**Evidence confidence:** CONFIRMED.

---

### AUD-034 — Documentation: duplicated section numbers, missing sections, Python-isms, stale decision records

**Severity:** P3 — Low
**Category:** Documentation
**Specification reference:** DESIGN.md §0 ("Section numbers match the required output order so nothing is lost") — the claim is false.

**Code evidence**

- DESIGN.md:1063 `## 10. Implementation Acceptance Criteria` vs :1180 `## 10. Folder / Playback Context Panel (the Reel)`; :1107 `## 11. Scope and Future Extensions` vs :1221 `## 11. File Actions and Context Menus`.
- Product §§8–9 absent (jump from `## 7. Search Experience` at :475 to `## 10` at :1180).
- :1929 `locale.strxfrm`; :1945 `search(spec) -> Iterator[MediaEntry]`; :1961 "SQLite via the stdlib `sqlite3`"; :1963 `set_favorite(identity, bool)`.
- Code comments cite "DESIGN.md §8.1/§8.2/§8.3" for playback matters (e.g. `Stage.qml:8`, `TransportBar.qml:40`), which in the actual document are Bulk File Operations sections.
- `.project-memory/03-Decisions/Decision-Index.md` DEC-009 says `QQuickItem + QSGRenderNode`; the code is `QQuickFramebufferObject`.

**Current implementation**

Three documents are concatenated without renumbering: sections 10 and 11 exist twice with different meanings, product §§8–9 are absent, four passages use Python idioms, and roughly forty code comments cite section numbers that resolve to the wrong content.

**Expected implementation**

One unambiguous numbering scheme (part prefixes plus sequential numbers), C++-accurate wording, every internal cross-reference resolvable, and decision records that describe the code as it is.

**Root cause**

The playback specification was inserted into the product specification without a renumbering pass; the Python-flavoured passages predate the C++ baseline.

**Correct architecture**

Documentation-only: the spec remains the single normative text, but section identity is stable and citations are checkable.

**Permanent resolution (documentation only — no source change)**

1. Insert `Part A/B/C` markers and renumber the playback document as `PB-§1…PB-§11`; update the ~40 code comments that cite the ambiguous numbers (grep `§8\.`, `§10`, `§11`).
2. Replace the Python idioms with the implemented behaviour (`QCollator`-based comparison, `QList<Entry>` iterators, SQLite C API, `setFavorite(identity, bool)`).
3. Restore or explicitly delete product §§8–9; if the content is genuinely in the playback document, add a pointer.
4. Correct DEC-009 in the decision index (the FBO choice is a deliberate, better decision — record it, don't revert the code).

**Dependencies:** none.
**Regression risks:** comment churn only; do not let a renumbering pass touch normative text.
**Deferred verification:** a link/anchor checker over the headings; `grep -n "§8\.[123]"` in `qml/` returns only playback-document references.
**Evidence confidence:** CONFIRMED.

---

### AUD-035 — Packaging and registry drift

**Severity:** P3 — Low
**Category:** Packaging / Redundancy
**Specification reference:** §16.1 ("Primary package: native Ubuntu **`.deb`** … It installs the executable, Qt/QML module assets, desktop entry, icons, AppStream metadata, and required runtime dependency declarations"), §16 file tree (`packaging/debian/`), §17.3 ("`media/formats.hpp` defines the supported set **once**. Enumeration, ad-hoc drops, drag/drop validation … all consult it").

**Code evidence**

- `ls packaging/` → `io.github.QGayze.QG.desktop`, `io.github.QGayze.QG.metainfo.xml`, **no `debian/`**.
- `packaging/io.github.QGayze.QG.desktop:13` declares 9 MIME types; `src/media/formats.hpp` supports 15 extensions (missing `ogv`, `3gp`, `flv`, `wmv` → `video/ogg`, `video/3gpp`, `video/x-flv`, `video/x-ms-wmv`).
- `qml/QG/ContentView.qml:754-755` duplicates the extension list: `readonly property var supportedExtensions: ["mp4", …, "wmv"]` with the comment "mirrors formats.hpp for drop validation".
- `metainfo.xml` `<provides>` lists 7 of the same types.

**Current implementation**

The declared stack cannot produce the specified package (no `debian/` recipe exists), the desktop entry and AppStream metadata advertise a subset of the supported types, and drop validation re-implements the extension registry in QML.

**Expected implementation**

A `.deb` recipe that installs the binary, QML module, icons, desktop entry and AppStream data; MIME/mediatype lists that match `formats.hpp`; and one registry consulted by enumeration, drops and the delegates.

**Root cause**

Packaging was scaffolded for the files that existed at the time, and the registry was copied into QML when drag/drop validation was added.

**Correct architecture**

`media/formats.hpp` remains the single registry and is exposed to QML as a singleton; packaging metadata is generated from it (or parity-tested); `packaging/debian/` is the canonical integration path per §16.1.

**Permanent resolution**

1. Add `packaging/debian/{control,rules,changelog,copyright,install}` and a `cpack`/`debuild` preset; declare `Depends: libmpv2, libavformat60 | …, libsqlite3-0, qml6-module-qtquick*`.
2. Generate the desktop/MIME/AppStream lists from `formats.hpp` (a tiny `file(GENERATE)` at configure time) or extend both to the full set and add a test that asserts parity.
3. Expose the registry to QML from C++ (`Formats::supportedExtensions` singleton) and delete the QML literal.

**Not patched:** the `.deb` recipe depends on the target Ubuntu ABI baseline (unstated in the repo) and would be a guess. The MIME/registry parity is a one-line-each edit; FIX-PKG-01 carries it with the parity test.

**Dependencies:** none.
**Regression risks:** packaging changes do not affect the build; the registry singleton adds one QML type.
**Deferred verification:** `debuild -b -uc -us` in a clean 24.04 container; `gio info` on a `.wmv` opens QG; a `.3gp` dropped on the grid is accepted.
**Evidence confidence:** CONFIRMED.

---

## 10. Remediation roadmap

Ordered by dependency, not severity: everything that changes a shared substrate comes first, because later patches assume it. `FIX-*` IDs are the unit of work; each maps to findings.

| FIX | Related | Priority | Resolution | Depends on | Risk |
|---|---|---|---|---|---|
| **FIX-WK-01** | AUD-002 | P0 | GUI-thread delivery hub; receiver guard captured at submit | — | Medium (every async path) |
| **FIX-PB-01** | AUD-001 | P0 | Stable seek argument buffer, async seeks, non-seekable policy in the manager | — | Low |
| **FIX-PB-02** | AUD-016 | P1 | Render-context teardown on the render thread; clear the update callback | — | Medium (needs GL test) |
| **FIX-SEL-01** | AUD-004 | P1 | Selection/focus as notifying properties + revision; range/select-all over view identities — **DONE 2026-10-25** | — | Medium (touches every delegate) |
| **FIX-SRCH-01** | AUD-005 | P1 | SearchController properties + invokable setters | — | Low |
| **FIX-ST-01** | AUD-006, AUD-025 | P1 | State applied on model population + identity hash index | FIX-SEL-01 (selection reads model) | Medium |
| **FIX-FM-01** | AUD-022 | P1 | Layout-change contract in `reorder` | FIX-ST-01 (shared hash) | Low |
| **FIX-PC-01** | AUD-007, AUD-012, AUD-026, AUD-027 | P1 | Session survives navigation; session-state connection; auto-advance; wall-clock throttle; clean stop | FIX-WK-01 | Medium |
| **FIX-TH-01** | AUD-008 | P1 | Serve the QG cache + backoff before extracting | FIX-WK-01 | Low |
| **FIX-TH-02** | AUD-008 (follow-up) | P2 | Encode + write the thumbnail PNG on the media pool, deliver the cache key only | FIX-TH-01 | Low |
| **FIX-STAGE-01** | AUD-009, AUD-010, AUD-011, AUD-030, AUD-033(part) | P1 | Stage in the flow (`reservedTop`) — **AUD-009 half DONE 2026-10-25**, and the **AUD-010 half (Cinema fullscreen) implemented 2026-10-27** (`windowedVisibility` + the rewritten `onCinemaActiveChanged`; static review only — `Window.FullScreen` is a no-op under `QT_QPA_PLATFORM=offscreen`, so the check needs a real WM), re-verified 2026-10-26 by inspection against this item's prompt (all four code items + the reporter fix present, `Stage.qml` never reads `contentView`; no code change needed; the acceptance harness is outside the repo, so `ctest` does not cover the inset); remaining: no entry fade, fullscreen Cinema, Esc semantics, token/enum cleanup | FIX-PC-01 (Esc path) | Medium (QML layout) |
| **FIX-FO-01** | AUD-003, AUD-019, AUD-018 | P1 | File ops on the I/O pool (**AUD-003 half DONE 2026-10-25**); EXDEV fallback; trash info-then-move with rollback | FIX-WK-01 | High (ordering assumptions) |
| **FIX-FO-02** | AUD-003 (follow-up) | P2 | Chunked copy/move with progress + cancellation + §8.2 partial summary | FIX-FO-01 | Medium |
| **FIX-FO-03** | AUD-019 (follow-up) | P2 | Preserve mtime/mode/ownership on cross-device copy; `renameat2(RENAME_NOREPLACE)` for same-device moves | FIX-FO-01 | Low |
| **FIX-RN-01** | AUD-015 | P1 | Rename editor lifecycle + `validateRename` through the canonical validator | FIX-SEL-01 (focus ownership) | Low |
| **FIX-CLIP-01** | AUD-013 | P1 | Identity→path resolution; clipboard shortcuts enabled in Browse | FIX-SEL-01 | Low |
| **FIX-FS-01** | AUD-020 | P1 | Identity ladder in the enumerator (fingerprint fallback) | — | Low |
| **FIX-PB-03** | AUD-021 | P1 | Probe result crosses the boundary once; cache check before dispatch | FIX-WK-01 | Low |
| **FIX-DB-01** | AUD-017 | P1 | `Statement::exec()` for writes; repositories propagate errors | FIX-DB-02 | Low |
| **FIX-DB-02** | AUD-031, AUD-032 | P2/P3 | Keyed `schema_version` + rebuild migration; transient-error branch | — | Low |
| **FIX-FOLD-01** | AUD-014 | P1 | Folder summary provider (roles/mutator already patched) | OQ-1, OQ-2 | Medium |
| **FIX-SRCH-02** | AUD-023, AUD-024 | P2 | Uncapped filtering, ranked order, role names, identity-based manifest activation | FIX-SRCH-01 | Medium |
| **FIX-MNT-01** | AUD-028 | P2 | Octal decode, removable probe order, disconnected set + labels | — | Low |
| **FIX-UI-02** | AUD-029 | P2 | Help overlay, focus restoration, task popover, Reel/CompactRow a11y, `Space`/`W`/`Ctrl+I`, transport `valueText` | FIX-SEL-01 | Medium |
| **FIX-PKG-01** | AUD-035 | P3 | `.deb` recipe + MIME/AppState/registry parity from `formats.hpp` | — | Low |
| **FIX-DOC-01** | AUD-034 | P3 | DESIGN.md renumbering, Python-ism removal, DEC-009 correction, comment citations | — | Low (churn) |

### Grouping observations

- **`FIX-WK-01` unblocks five other fixes** (every async path); it must be applied first even though it touches only one function.
- **`FIX-SEL-01` unblocks four** (rename, clipboard, search, UI affordances) because they all need `selectedCount` as a property.
- **`FIX-PC-01` is the single largest behavioural change**: four findings share the "session vs view" lifetime root.
- Two findings (AUD-014 provider, AUD-029 cluster, AUD-035 `.deb`) have no patch by design — each cites the missing decision.

---

## 11. Implementation order

Per file, apply in this order so every intermediate state compiles:

1. `src/workers/tasks.{hpp,cpp}` — FIX-WK-01 (header comment, then the hub, then the lambda).
2. `src/playback/playback_manager.{hpp,cpp}` — FIX-PB-01 (signal + helper in the header **before** the .cpp uses them), then FIX-PB-02 (`setSurface`/`finishShutdown` declarations before `shutdown()` uses them), then AUD-026's timer member.
3. `src/models/selection_state.{hpp,cpp}` — properties and revision first, then the mutators, then `ContentView` bindings (QML can only read properties that exist).
4. `src/models/folder_model.{hpp,cpp}` — roles enum → `roleNames`/`data` → hash index → `reorder` contract → `folderPath`/`pathForIdentity`/`updateFolderSummary`. Adding the enum values before the hash/index code keeps each step compilable.
5. `src/controllers/{state,playback,search,browser}_controller.*` — one file at a time; `attachSession` before `onSessionStateChanged` is referenced by `stop()`.
6. `src/filesystem/{operations,trash,mounts,enumerator}.cpp` — independent; `operations` needs FIX-WK-01's include.
7. `src/persistence/{database,repositories}.*` — `exec()` + header declaration first, then the six call sites, then the migration rebuild.
8. QML last: `Theme` → `ContentView` (helpers/insets) → delegates → `Stage`/`TransportBar` → `Main.qml` (wiring). `Main.qml` references every controller property, so it changes after the C++ side.

**Rebuild required:** yes, for every C++ change (new signals, properties, files). QML-only changes still need `cmake --build` because the module is compiled (`qt_add_qml_module`). Adding `folder_summary.*` or a `Formats` singleton requires re-running CMake.

---

## 12. Deferred verification plan

Nothing below was executed. Every item is `TESTING DEFERRED`.

| # | Gate | Method | Covers |
|---|---|---|---|
| V1 | Build clean | `cmake -S . -B build -G Ninja && cmake --build build` with zero new warnings | all patches |
| V2 | Unit suite | `ctest --test-dir build --output-on-failure` — baseline is 47/48 with four known `qg_qml` subtests (`DEFERRED_TESTS.md`); expect +N new cases, no new failures | FIX-WK-01/DB-01/FM-01/FS-01 |
| V3 | Memory safety | ASan build (`-DQG_SANITIZE=address,undefined`): play/seek/close 50×, navigate 200×, trash/restore 20× | AUD-001/002/016/018 |
| V4 | Thread safety | TSan build with the thumbnail + probe pipelines saturated during navigation | AUD-002/021 |
| V5 | Selection matrix | click / Ctrl+click / Shift+click / arrows / Ctrl+A / Esc, all densities + Manifest | AUD-004 — **partial 2026-10-25**: C++ control pair + rendered pixels for **Gallery, Standard, Compact and Manifest** (16/16 probe checks), plus a headless density harness for the identity/range contract; only a synthesised mouse round-trip is still deferred |
| V6 | Search flow | Ctrl+F, debounce < 100 ms on 10 k entries, highlight, summary, Esc restores scroll+selection, no-results copy | AUD-005/023/024 |
| V7 | Playback lifecycle | one mpv handle across a session; prev/next; EOF auto-advance; boundary toast; resume thresholds | AUD-007/012/026 |
| V8 | Cinema | F11/F/double-click/Esc/Close; position and volume survive; pointer hides at 2 s; chrome hidden; real fullscreen on X11 | AUD-009/010/011 |
| V9 | File operations | 4 GB copy responsiveness; cross-device move; trash + undo + info-file integrity; rename validation matrix; clipboard round-trip with Nautilus | AUD-003/013/015/018/019 |
| V10 | Large folder | 50 k entries: first screenful, scroll, sort, warm-start thumbnails, per-update cost | AUD-008/025 |
| V11 | Mounts | `\040` mount point, whole-disk removable, two disconnects, eject | AUD-028 |
| V12 | Packaging | clean-container `debuild`, install, launch from the app menu, `gio info` on each supported type | AUD-035 |
| V13 | Accessibility | Orca pass over cards, Reel, transport; monochrome rendering; reduced motion | AUD-029 |

---

## 13. Audit coverage matrix

| Subsystem | Spec reviewed | Code reviewed | Findings | Status |
|---|---|---|---|---|
| Bootstrap / wiring (`main`, `app`, `composition`) | yes | yes | 006 | ISSUES FOUND |
| Workers (`tasks`, `priority_pool`, `task_token`) | yes | yes | 002, (020 note) | ISSUES FOUND |
| Filesystem (`enumerator`, `watcher`, `operations`, `trash`, `mounts`, `paths`) | yes | yes | 003, 018, 019, 020, 028 | ISSUES FOUND |
| Media (`formats`, `probe`, `thumbnails`) | yes | yes | 008, 021, 034 | ISSUES FOUND |
| Persistence (`database`, `repositories`, `schema`) | yes | yes (schema + step/exec + 6 write paths) | 017, 030, 031 | ISSUES FOUND |
| Models (`folder_model`, `selection_state`, `reel_model`, `search_proxy`, `places_model`, `recursive_search_model`) | yes | yes (folder/selection/search in depth; others spot-checked) | 004, 014, 022, 023, 024, 025 | ISSUES FOUND |
| Controllers (browser, search, playback, actions, mounts, state, thumbnail, probe, keyboard) | yes | yes (playback/actions/state/search/browser in depth; thumbnail/probe wiring) | 005, 007, 012, 013, 015, 027, 028 | ISSUES FOUND |
| Playback (`mpv_backend`, `video_surface`, `session`, `context`, `manager`) | yes | yes (all five) | 001, 007, 012, 016, 021, 026, 027 | ISSUES FOUND |
| QML shell (`Main`, `PathBar`, `PlacesSidebar`, `ContentView`) | yes | yes (Main, ContentView in depth; PathBar/Sidebar targeted) | 005, 009, 010, 011, 013, 029 | ISSUES FOUND |
| Delegates (`VideoCard`, `FolderCard`, `CompactRow`, `ManifestRow`) | yes | yes (Video/Folder in depth; Compact/Manifest targeted) | 004, 014, 015, 024, 029 | ISSUES FOUND |
| Playback QML (`Stage`, `TransportBar`, `ReelPanel`) | yes | yes (Stage/Transport in depth; Reel targeted) | 010, 011, 029, 032, 035 | ISSUES FOUND |
| Strips / overlays / dialogs | yes | partially (StatusStrip, StateOverlay, ToastHost usage) | 029 | PARTIAL REVIEW |
| Build system / packaging | yes | yes | 034 | ISSUES FOUND |
| Tests (34 targets + QML suite) | — | inventory only, not executed | — | NOT ASSESSED |

---

## 14. Open questions and specification ambiguities

| ID | Question | Blocks |
|---|---|---|
| **OQ-1** | May a folder tile **generate** child thumbnails, or only use already-cached ones? §5.2 says "pre-resolved child video thumbnails" (lookup only); users may expect the mosaic to fill in. | AUD-014 provider (FIX-FOLD-01) |
| **OQ-2** | Does the folder-summary scan follow **symlinked** child directories? §25.1 makes symlinked directories navigable, but following them in a bounded scan reintroduces loop risk. | AUD-014 provider |
| **OQ-3** | Should `Ctrl+R` remain bound to the Reel (per the §13.2 note) while §13.2's navigation table lists it as Refresh? Implementation follows the note; the table contradicts it. | AUD-034 |
| **OQ-4** | Ascending `Duration` places unprobed items last (§20.2), but §22.6 wants no jitter. Is one coalesced re-sort per probe batch acceptable, or must the model freeze the order until the batch completes? | sorting follow-up (not a defect today) |
| **OQ-5** | Is the deliberate `trashFallback`-before-`gio trash` order (§17.7 says the opposite) a decision to record in DECISIONS.md, or should the code follow the spec? The comment justifies the order for undo. | AUD-035 note / `trash.cpp:17-42` |
| **OQ-6** | Which Ubuntu release/ABI baseline is the `.deb` target? Not stated anywhere in the repo. | AUD-035 |
| **OQ-7** | Is the absence of any VCS metadata intentional (checkpoint-based workflow), or is a repository expected? The audit cannot pin a revision. | process |

---

## 15. Final recommendations

1. **Land `FIX-WK-01` and `FIX-PB-01` first.** Both are small, both are undefined behaviour, and one of them is already crashing tests intermittently. Nothing else should be merged on top of a delivery mechanism that can free-run into a dead object.
2. **Then `FIX-SEL-01`.** It is the single change that turns five "unimplemented" features into working ones (selection borders, focus ring, status strip, bulk menu, Ctrl+A/Shift-range) and it unblocks rename, clipboard, search and the a11y work.
3. **Treat `FIX-PC-01` as one change, not four.** Navigation-kills-playback, `playing` notification, auto-advance and the position throttle are four symptoms of one ownership problem: the controller never observes the session it claims to own. Patch them together and verify them together.
4. **Do not ship `Stage`/`Cinema` as "done" until V8 passes.** The presentation layer is the product's identity (§1.2) and currently violates four separate normative statements about it.
5. **Fix the persistence error path (`FIX-DB-01`) before any further feature work.** Right now every write silently claims success; every subsequent feature will build on that lie.
6. **Keep `proj-audit.md` as the tracking document.** Each `FIX-*` above maps to findings, and each finding states its own acceptance evidence. Update the finding status in place rather than writing a second report, and re-verify each P0/P1 against the source before closing it — this audit is static, and a stale "fixed" label is worse than an open finding.

---

*AUDIT COMPLETE — STATIC REVIEW ONLY / IMPLEMENTATION NOT STARTED / TESTING DEFERRED*
