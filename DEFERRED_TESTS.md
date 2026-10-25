# DEFERRED TESTS — QG Batch Testing Register

Workflow policy effective **2026-10-22**: implementation is logic-first; builds,
test runs, app launches, screenshots, viewport checks, benchmarks, and
hardware/device validation are **deferred to a testing phase the user authorizes**.
This file is the consolidated backlog (§5 of the policy). Do not duplicate a check
across rows — add it here once and reference it.

Status vocabulary: `IMPLEMENTED` · `STATICALLY REVIEWED` · `NEEDS_REVISION` ·
`BLOCKED` · `TESTING DEFERRED` · `VERIFIED` (tests actually executed).

## 1. Pending register

| Fix ID | Changes Implemented | Static Review | Deferred Tests | Risk |
|---|---|---|---|---|
| FIX-VIS-013/014 | Path Bar Density button derives icon + label from the active mode (`densityIconName`/`densityLabel`); mode-naming tooltip and accessible name; ContextStrip options gained tooltips, accessible names, active chip, and 24 px sizing inside the 28 px strip | STATICALLY REVIEWED — `qmllint` no new errors (pre-existing Repeater `Unqualified access` warnings only); no remaining `name: "density"` use; shared state path unchanged | (a) cycle Gallery/Standard/Compact and capture each — Path Bar icon must change and the strip chip must follow; (b) hover tooltip text on both surfaces; (c) `Ctrl+1/2/3` still route to `setDensityValue`; (d) revisit a folder and confirm its density persists; (e) `tst_pathbar`, `tst_contextstrip`, `tst_density`, `tst_main` re-runs | Low |
| FIX-VIS-011/012 | Card placeholder cues bound to `thumbnailState` (`placeholderKind`); exclusive centre zone (`playGlyphVisible`); amber failure scrim + compact Retry; fixed TL favourite/watched slots; 22 px TR hover chip with one chip background; `Accessible.name` on hover actions + Retry; meta line anchored to the filename; `Theme.chipScrim` token | STATICALLY REVIEWED — `qmllint` clean apart from two artifacts of the stale `build/qml/Theme.qml` copy; zone geometry checked arithmetically at the 168 px minimum card width | (a) `tst_videocard` full re-run — existing assertions on `quickActions`, `hoverPlayGlyph`, `thumbFilmstrip`, `thumbRetry` must still hold; (b) screenshot matrix: idle / hover / focus / selected / favourited / watched / partially watched / thumbnail-failed / permissions-denied; (c) at 168, 244 and 360 px card widths confirm no zone overlap (TR chip vs centre glyph vs BR chip vs TL badges); (d) confirm hover does not select and focus/selection rings are unaffected; (e) full QML suite + CTest | Medium — layering/anchoring changed in a shared delegate |

### Note on hover animation

There is **no** hover animation in `VideoCard` (the body colour switch is
instantaneous), so "verify after the hover animation completes" reduces to
asserting the settled hover layout — covered by (b)/(c). If a hover transition is
added later it must respect `Theme.motionEnabled` / `Theme.reducedMotion`.

## 2. Completed before the policy took effect (evidence exists)

These were built, run, and (for VIS-010) validated on real media; they are **not**
part of the deferred backlog. Keep them for the batch phase as regression re-runs.

| Fix ID | Changes Implemented | Static Review | Tests already executed | Risk |
|---|---|---|---|---|
| FIX-UI-003 | Sidebar icons, device classification (`VolumeKind`), mount states, section headings as model rows, capability-gated eject/unmount | Reviewed; incident (accidental overwrite) recovered byte-identically from checkpoints | `qg_mounts` 14/14, `qg_places_model` 14/14, `tst_places_sidebar` 16/16, `tst_icon`, full QML 187/4 known | Low |
| FIX-UI-004 | Grid columns from the §3.5 density target, single left gutter, centring | Reviewed | `tst_grid_geometry` 13/13 at 7 widths, neighbours green | Low |
| FIX-VIS-010 | Thumbnails delivered as `file://` URLs (was `QImage`), publish with temp fallback, cache-before-extractor, folder rows skipped | Reviewed | `qg_thumb_delivery` 10/10 (real MP4+MKV), `tst_videocard` +3, full QML 203/4 known, full CTest **47/48 twice** (172.3 s / 181.5 s; only pre-existing `qg_qml`; `qg_keyboard` passed both), Xvfb real-app run: 214×125 px block 98.6 % expected frame colour | Low |

## 3. Known defects recorded, not scheduled

| Item | Where | Note |
|---|---|---|
| Worker-pool delivery use-after-free | `src/workers/tasks.cpp:81` | Queued delivery invokes a raw `QObject *receiver` from a pool thread; ~4 % of `qg_keyboard_test` teardown runs crash in `QObject::thread()`. Needs its own ASan-backed fix. See `.project-memory/05-Debugging/Root-Cause-Analysis/worker-pool-receiver-use-after-free.md` |
| Warm start re-extracts thumbnails | `src/controllers/thumbnail_controller.cpp` | `decideThumbnail()` and `ThumbnailRepository::lookup()` have no callers, so the QG PNG+DB cache is write-only (DESIGN §18.2 "Serve" unimplemented) |
| Probe-cache invalidation on source change | probe chain | THUMB-007 only half covered (thumbnail half verified) |
| `ContentView` undeclared `focusIndex` | `qml/QG/ContentView.qml` | Legacy no-`selectionState` branches only; dead in production |
| Startup abort on a real session (`xsettings`/`antialiasing`) | host schema | Environmental: g-s-d 50 key relocation + a stale consumer. Fixed by `scripts/fix-xsettings-compat.sh` (user-level shim). **Verified 2026-10-22:** with the shim installed, `DISPLAY=:0 timeout 12 ./build/src/qg` stayed up (exit 124 = timer kill, no abort) and logged `QG starting` → OpenGL → media probing. See `.project-memory/05-Debugging/Root-Cause-Analysis/startup-glib-gio-settings-crash.md` |
| Four pre-existing `qg_qml` subtests | CardActions keyboard focus, ContentView focus/selection, Keyboard duplicate `Alt+Left`, TransportBar icon state | Baseline failures, unrelated to recent work |

## 4. Batch testing phase checklist (when authorized)

Run once, in order, and record real output only:

1. `cmake -S . -B build -G Ninja && cmake --build build` — clean build, no new warnings.
2. `ctest --test-dir build --output-on-failure` — expect 48 tests, only the four
   known `qg_qml` subtests failing; `qg_keyboard` may SEGFAULT in teardown
   (pre-existing flake — re-run directly to confirm).
3. `QT_QPA_PLATFORM=offscreen ./build/tests/qg_qml_test -input tests/qml` — expect
   203 passed / 4 known failures.
4. Focused re-runs: `qg_thumb_delivery`, `qg_thumb_wiring`, `qg_thumb_scheduler`,
   `qg_thumb_failure`, `qg_mounts`, `qg_places_model`, `tst_grid_geometry`,
   `tst_places_sidebar`, `tst_videocard`.
5. Cold/warm cache: delete the cache dir, launch, confirm PNGs appear and rows
   show imagery; relaunch and confirm no re-extraction of unchanged sources.
6. Rapid navigation + scroll: no stale thumbnails, GUI stays responsive.
7. Xvfb visual pass at 1833/1500/1150/1100/900/820/700 px (sidebar + grid), plus the
   card state matrix from FIX-VIS-011/012 (idle/hover/focus/selected/favourite/
   watched/partial/failed/denied) at 168, 244 and 360 px card widths.
8. Hardware/device validation still needs a real USB drive and a disposable
   mount for eject (currently unverified).

## 5. Blocked on authorization — FIX-UI-008 cross-state audit

The audit's deliverable is runtime captures, which the policy forbids. It is
**BLOCKED on the testing phase**, not on code. No findings were recorded because
none were observed.

Capture plan (execute in order, save each PNG with a descriptive name):

1. Fixtures in one temp folder: 4 videos with extractable thumbnails; 1 truncated
   video for the extraction-failure state; 1 `chmod 000` video for permissions;
   2 subfolders; 200+ files for scroll/virtualization.
2. Launch under Xvfb 1833x1080 with an isolated HOME/XDG root (recipe proven during
   VIS-010 validation) and `QT_QPA_PLATFORM=xcb`.
3. Captures: Browse x {Gallery, Standard, Compact} x {idle, hover, focus, selected};
   FolderCard hover + drop target; CompactRow; ManifestRow; Stage idle; Stage
   playing; Cinema; Reel open; Reel filtered; StateOverlay empty / denied / error /
   no-results; ToastHost with Undo; Properties dialog; RenameConflict dialog;
   ConfirmDelete dialog; narrow widths 1100 / 900 / 820 / 700.
4. Audit each capture against DESIGN §§5–13 and §§25.2–25.10, one row per clause with
   PASS/FAIL and the capture filename. Findings enter the register only from
   observed captures.
