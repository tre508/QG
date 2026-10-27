---
type: dashboard
project: QG
created: 2026-10-20
updated: 2026-10-22
status: active
tags:
  - dashboard
  - project-overview
source:
  - DESIGN.md
  - GUIDE_STATE.md
  - ARCHITECTURE_OVERVIEW.md
---

# QG Project Dashboard

## Project Overview

**QG** — Native Ubuntu/Linux video file explorer and player  
**Stack**: C++20, Qt 6 Quick/QML, libmpv Render API, OpenGL, SQLite, FFmpeg  
**Architecture**: 7-layer separation (qml → controllers → models → services → workers → persistence → playback)  
**Status**: MVP implementation complete; last executed gate 47/48 (98 %). New work is logic-first — runtime validation deferred per [[Agent-Workflow]] (2026-10-22 policy)

## Current State (as of 2026-10-27)

| Area | Status | Notes |
|------|--------|-------|
| Core Architecture | ✅ Complete | All 7 layers implemented, strict separation |
| Playback Stack | ✅ Complete | Single mpv handle, Render API, in-window Cinema |
| Filesystem Layer | ✅ Complete | Enumeration, operations, trash, mounts, watcher |
| Media Pipeline | ✅ Complete | Probe, thumbnails (freedesktop + FFmpeg) |
| Search | ✅ Complete | Recursive search wired end-to-end (Subfolders scope, manifest integration) |
| Persistence | ✅ Complete | SQLite WAL, identity-keyed repositories |
| UI/Presentation | ✅ Complete | Monoline icon system adopted (33 SVGs); high-contrast focus outlines wired |
| Packaging | 🟡 Partial | .deb metadata exists, unvalidated |
| Platform Verification | ⚪ Unverified | Wayland, High-DPI visual, cross-app DnD need desktop session |
| Workflow | 🔒 Logic-first | Builds/tests/launches deferred to an authorized batch phase ([[Agent-Workflow]], [[Deferred-Test-Register]]) |

## Test Results — 47/48 PASS (98 %), last executed 2026-10-22

**Command**: `TMPDIR=/tmp ctest --test-dir build --output-on-failure` (172.3 s / 181.5 s, two runs)  
**Only failure**: `qg_qml` — four pre-existing subtests (CardActions keyboard focus,
ContentView focus/selection, Keyboard duplicate `Alt+Left`, TransportBar icon state).
The intermittent `qg_keyboard` teardown SEGFAULT recorded here (worker-pool delivery
use-after-free) was fixed 2026-10-25; see [[2026-10-25-fix-worker-delivery-uaf]].
AUD-003 (file operations blocking the GUI thread) was fixed the same day; the new
`qg_file_ops_async` suite joins the regression set
([[2026-10-25-fix-aud-003-file-ops-async]]).
AUD-004 (selection/focus frozen because `SelectionState` had no notifying
properties) was fixed the same day; verified with a red-on-HEAD C++ control probe
and a rendered-pixel probe (16/16 checks over Gallery/Standard/Compact/Manifest),
full CTest 48/49 and `qg_qml` 200/8 — the same failure set as the pre-fix control
run ([[2026-10-25-fix-aud-004-selection-focus]]).
AUD-009's reflow half landed the same day (FIX-STAGE-01): `ContentView.reservedTop`
and the shell binding make the grid reflow beneath the inline Stage instead of
being covered; verified by a 6/6 harness composing the real Stage
([[2026-10-25-fix-aud-009-stage-reflow]]).
AUD-005 (the Manifest intensity was unreachable: no `Q_PROPERTY` on
`SearchController` plus two non-invokable context setters) is **implemented and
statically reviewed, not runtime-verified** (2026-10-26, FIX-SRCH-01) — static
instruments only (`g++ -fsyntax-only` + `qmllint` control pair), with the whole
§7.2 presentation path, focus ownership and §7.3 row focus in the deferred
register ([[2026-10-26-fix-aud-005-search-manifest]]).
AUD-007 (playback ended on every navigation, because the controller read the playing
identity's absence from the *displayed* model as proof of deletion) is **implemented and
statically reviewed, not runtime-verified** (2026-10-26, FIX-PC-01 / AUD-007 half): `FolderModel`
publishes `folderPath` and `PlaybackController` only stops when the displayed folder is
the session's own; a live session on another folder keeps its context and Reel. Static
instruments only (`g++ -fsyntax-only` + red controls); the navigation/deletion/Reel
checks and the `qg_reel_wiring` + `qg_playback_lifecycle` re-runs are deferred
([[2026-10-26-fix-aud-007-playback-navigation]]).
AUD-008 (the QG thumbnail PNG+DB cache was write-only, so every navigation re-decoded
every visible row and corrupt files were retried forever) is **implemented and statically
reviewed, not runtime-verified** (2026-10-26, FIX-TH-01): the persisted row is now read,
validated against the current `(mtime_ns, size)` through the §18.5 policy, and turned into
serve / terminal / extract — with the DB query on the GUI thread (the connection is
thread-affine) and the PNG read on the media pool. Static instruments only
(`g++ -fsyntax-only` + five red controls); the warm-start and terminal-marker proofs are
deferred ([[2026-10-26-fix-aud-008-thumbnail-cache]]).
AUD-006 (persisted favorite/watched/progress never applied after the first navigation) is **implemented and statically reviewed, not runtime-verified** (2026-10-26, FIX-AUD-006): `StateController` now subscribes to the canonical model's `modelReset`/`rowsInserted` and decorates new rows exactly once per identity per generation via a coalesced zero-delay `scheduleLoad()`; `loadIntoModel()` preserved for tests. Deferred checks in `DEFERRED_TESTS.md` FIX-AUD-006 ([[2026-10-26-fix-aud-006-persisted-state]]).

AUD-010 (Cinema never entered fullscreen — the whole intensity ran inside a decorated window) is **implemented and statically reviewed, not runtime-verified** (2026-10-27, FIX-STAGE-01 / AUD-010 half): `Main.qml` remembers `window.visibility`, sets `Window.FullScreen` on entry (Stage focus + both transport overlays), and restores the remembered state on exit, using the existing window throughout. Static instruments only (`qmllint` control pair: 141 vs 139, no new diagnostic class); the X11 checks and the offscreen `visibility` property assertion are deferred — `Window.FullScreen` is a no-op under `QT_QPA_PLATFORM=offscreen` ([[2026-10-27-fix-aud-010-cinema-fullscreen]]).

**Frozen by policy**: no new runs are recorded until an authorized batch testing
phase. Outstanding checks live in [[Deferred-Test-Register]]; details in
[[Test-Results]].

## Active Workstreams

1. **FIX-UI-001** — Responsive shell geometry (sidebar beside workspace) — COMPLETE
2. **FIX-DND-001..008** — Cross-app clipboard & drag-and-drop (QG ↔ Nautilus)
   - FIX-DND-001/002/007/008 COMPLETE; FIX-DND-003..006 BLOCKED (no Wayland GUI automation)
3. **Packaging** — Validate native .deb build/install

## Key Architectural Invariants (VERIFIED)

- ✅ Filesystem is authoritative — no library ingestion
- ✅ One surface, four intensities (Browse/Stage/Cinema/Manifest)
- ✅ Single ordered source (FolderModel drives Browse, Reel, PlaybackContext)
- ✅ One mpv handle + Render API surface
- ✅ In-window Cinema — layout state only, no session transfer
- ✅ GUI thread never waits — bounded pools + TaskToken
- ✅ C++ owns domain — QML presentation-only

## Quick Links

- [[00-Home/Current-State|Current State]]
- [[00-Home/Session-Handoff|Session Handoff]]
- [[01-Architecture/Architecture-Overview|Architecture Overview]]
- [[03-Decisions/Decision-Index|Decision Index]]
- [[04-Development/Active-Tasks|Active Tasks]]
- [[06-Testing/Build-Commands|Build & Test Commands]]
- [[09-References/Repository-Documents|Repository Documents]]

## Next Actions

1. Complete FIX-VISUAL-010 visual compliance audit
2. Validate native .deb packaging end-to-end
3. Implement system clipboard MIME data (`text/uri-list` + `x-special/gnome-copied-files`)
4. Address first-screenful 300ms target (355–383ms measured, marginal)
5. Platform verification (Wayland, High-DPI visual, cross-app DnD) — requires desktop session
