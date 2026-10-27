---
type: session
project: QG
created: 2026-10-26
updated: 2026-10-26
status: complete
aliases:
  - Fix-AUD-006-Persisted-State
tags:
  - session
  - state
  - persistence
  - aud-006
source:
  - proj-audit.md §AUD-006 (FIX-AUD-006)
  - DESIGN.md §6.2, §21.3, §23.11–§23.12, §25.8
  - src/controllers/state_controller.{hpp,cpp}
  - src/core/composition.cpp (root-cause evidence, unchanged)
---

# 2026-10-26 — AUD-006: persisted favorite / watched / progress applied after every navigation

## Objective
Resolve AUD-006 ("Persisted favorite / watched / progress are never applied after the first navigation") — persisted state must appear on file rows whenever they enter the canonical model: initial load, folder navigation, and refresh (DESIGN.md §6.2, §21.3, §23.11–§23.12, §25.8). Per the active policy ([[Agent-Workflow]] § Development Workflow Mode) this session delivered **IMPLEMENTED + STATICALLY REVIEWED** only: no build, no CTest/QML run, no launch, no screenshot. Runtime checks are recorded in `DEFERRED_TESTS.md` FIX-AUD-006 instead of being claimed.

## Verified root cause
**CONFIRMED, observed at `src/core/composition.cpp:93-99`.** `CoreServices::wire()` constructs `StateController`, calls `setDatabase()`/`setModel()`, and then invokes `loadIntoModel()` — but the first `navigate()` (which populates the model via `applySnapshot`) happens afterwards in `App::initialize()` (`src/app.cpp:264-267`). The decoration loop therefore iterates an empty model and never executes; every subsequent folder load replaces the model wholesale (`applySnapshot` → `modelReset`) and nothing re-applies persisted state. Visible symptom: hearts, watched checks, progress bars, Favorite sorting and the Reel's Favorites/Unwatched filters stay empty for the whole session; writes persist and reappear only after restart.

## Changes
- `src/controllers/state_controller.hpp` — public `scheduleLoad()` declaration; `setModel()` changed from inline definition to declaration only (body implemented in the cpp); private `QSet<QString> m_applied` (identities decorated in the current model generation) and `bool m_loadScheduled = false` (coalescing flag); `#include <QSet>`.
- `src/controllers/state_controller.cpp` — `#include <QTimer>`; `setModel()` disconnects the previous model, clears `m_applied`, connects `QAbstractItemModel::modelReset` (clears the guard, then `scheduleLoad()`) and `rowsInserted` (`scheduleLoad()`), then schedules an initial pass; `scheduleLoad()` guards on `m_loadScheduled` and uses `QTimer::singleShot(0, this, ...)` so a batch of insertions collapses into one pass; `loadIntoModel()` now skips `entry.identity.isEmpty() || m_applied.contains(...)` and marks each decorated identity. All favorite/watched/progress repository queries and model role updates are unchanged. `loadIntoModel()` remains the immediate entry point for existing tests.
- Docs: `DEFERRED_TESTS.md`, `.project-memory/06-Testing/Deferred-Test-Register.md`, `.project-memory/04-Development/Known-Issues.md`, `.project-memory/00-Home/{Current-State,Project-Dashboard,Session-Handoff}.md`.

## Deviations from the audit patch (necessary, documented)
- `setModel()` was inline in `state_controller.hpp:24-25` and is now implemented in the cpp so the signal wiring fits; signature and call sites (`composition.cpp:96`, unit tests) are unchanged.
- The audit's private-section append names `void applyStateToRow(const qg::Entry &entry);` — that member does not exist in this tree and `loadIntoModel()` retains its inline restoration logic, so no declaration was added.
- The audit cites "AUD-024 (identity index)" as the dependency that makes row updates affordable; in this tree the identity index is described under **AUD-025** (`src/models/folder_model.cpp:302-310`, still a linear-scan `indexOfIdentity`) and is **not implemented**. Recorded as a missing dependency, not implemented here. Correctness is unaffected; the residual cost is O(n) `updateFavorite` scans per row on first paint of very large folders.

## Deferred verification (not executed — see `DEFERRED_TESTS.md` FIX-AUD-006)
- Start QG with a database containing favorites/watched/positions; the first folder shows them immediately.
- Navigate to a second decorated folder; state is restored after `modelReset`; refresh re-applies via `rowsInserted`.
- Toggle a heart/watched flag, restart, and confirm persistence (write path covered by `qg_card_actions`).
- Compare first-paint time before/after on a 5,000-entry folder.

## Integration / regression notes
- `setModel()` is called from `src/core/composition.cpp:96` and from unit tests; the new `scheduleLoad()` call inside it is inert until an event-loop turn, so guileless unit tests are unaffected, and explicit `loadIntoModel()` calls remain effective — the `m_applied` guard only makes a second same-generation call a no-op, which no existing test relies on.
- `tests/` untouched: `test_state_controller::loadIntoModelAppliesPersistedState` and `test_card_actions::favoriteAndWatchedPersistAcrossRestart` exercise the preserved immediate entry point.
