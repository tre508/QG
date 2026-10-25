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

## Current State (as of 2026-10-21)

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
`qg_keyboard` passes but can SEGFAULT in teardown (~4 %, [[worker-pool-receiver-use-after-free]]).

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
