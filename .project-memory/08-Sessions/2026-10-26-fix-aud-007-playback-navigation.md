---
type: session
project: QG
created: 2026-10-26
updated: 2026-10-26
status: complete
aliases:
  - Fix-AUD-007-Playback-Navigation
tags:
  - session
  - playback
  - navigation
  - folder-model
  - aud-007
source:
  - proj-audit.md §AUD-007 (FIX-NAV-01)
  - DESIGN.md §1.2, §9.1, §10.2, §13.1, §19.2, §19.4
  - src/models/folder_model.{hpp,cpp}
  - src/controllers/playback_controller.{hpp,cpp}
  - src/controllers/browser_controller.cpp
  - src/playback/playback_context.{hpp,cpp}
  - tests/unit/test_reel_wiring.cpp, tests/unit/test_playback_lifecycle.cpp
  - g++ -fsyntax-only control pair (2026-10-26, static only)
---

# 2026-10-26 — AUD-007: browsing to another folder no longer stops playback

> **Fix-ID note:** the remediation roadmap keys this finding as **FIX-PC-01** ("Session
> survives navigation", which also covers AUD-012/026/027). The prompt for this work item
> named no ID, so the records below were first written under a placeholder `FIX-NAV-01`;
> the register labels now read `FIX-PC-01 (AUD-007 half)` and this note is the AUD-007
> half of that item.

## Objective

Resolve AUD-007 ("Browsing to another folder stops playback") in the checked-out
tree, then deliver **IMPLEMENTED + STATICALLY REVIEWED** only: no build, no CTest or
Qt Quick Test run, no launch, no screenshot, and no test file created or modified.
Every check below is static; the runtime checks are recorded in `DEFERRED_TESTS.md`.

## Verified root cause

**CONFIRMED, observed — the audit's citation reproduces exactly.**
`src/controllers/playback_controller.cpp` (`onModelStructureChanged`, HEAD line 290)
read:

```cpp
const QString identity = currentIdentity();
// Did the playing file leave the model entirely? Then playback must stop:
// its path no longer exists (rename/removal/navigation — §17.5).
const bool playingGone = !identity.isEmpty()
                         && folderModel_->indexOfIdentity(identity) < 0;
```

The comment is the defect: it lists *navigation* as evidence that the path no longer
exists. `FolderModel` had no way to say which directory its rows came from, so a
navigation reset and the playing file being deleted were indistinguishable.

**Mechanism, each link observed in source:**

1. `Main.qml` activation → `browserController.navigate(path)` (folder rows navigate;
   videos play) — `Main.qml` `onActivatedItem`.
2. `BrowserController::navigateTo()` cleans the path and calls `startEnumeration(true)`
   (`src/controllers/browser_controller.cpp:165-204`); the worker's completion applies
   **one** `m_model->applySnapshot(*result)` (line 193) — the model is never emptied
   in between, so navigation produces exactly one reset with the new folder's rows.
3. `FolderModel::applySnapshot()` (HEAD `src/models/folder_model.cpp:86-95`; now 86-97) replaced
   `m_entries` wholesale and emitted **no** signal identifying the directory.
4. `PlaybackController::setFolderModel()` connects `modelReset`, `layoutChanged`,
   `rowsRemoved`, `rowsInserted` → `onModelStructureChanged()`
   (`src/controllers/playback_controller.cpp:32-51`).
5. `playingGone` was therefore true for every navigation (the playing identity is not
   in the new folder's rows) → `context_->build(model, "")` + `refreshReel()` **and**
   `stop()` → the session ends, the Stage closes, Cinema exits (§13.1 violated).

**Specification:** §13.1 "**Playback state** (mint) — independent of both; playback
continues while you browse elsewhere"; §9.1 keeps "Current folder" + "Ordered playback
items" in the preserved playback context; §10.2 "The list is **the folder's ordered
video list**"; §19.4 prev/next are `PlaybackContext` steps; §19.2 one session;
§1.2 "Going back down an intensity is instant and lossless".

## Changes

| File | Change |
|------|--------|
| `src/models/folder_model.hpp` | `Q_PROPERTY(QString folderPath READ folderPath NOTIFY folderPathChanged)`, `QString folderPath() const`, `void folderPathChanged()`, private `QString m_folderPath` |
| `src/models/folder_model.cpp` | `applySnapshot()` assigns `snapshot.path` when it changes and emits `folderPathChanged()` **inside** the reset (no observer can see one folder's rows under another folder's path); `clear()` now also drops the path (inside its reset) so a cleared model claims no directory. All other snapshot/thumbnail/count/reset behaviour untouched |
| `src/controllers/playback_controller.cpp` | `#include <QFileInfo>`; `onModelStructureChanged()` derives the session's containing directory (`QFileInfo(currentSession_->path()).absolutePath()`) and requires `folderModel_->folderPath() == sessionFolder` before `playingGone` can be true; **plus** an early return that leaves the ordered context, the Reel and the session untouched when a live session is playing from a folder other than the one displayed |

### Deviation from the supplied patch (one, deliberate)

The audit's replacement computes `playingGone` folder-aware but still calls

```cpp
context_->build(folderModel_, playingGone ? QString() : identity, filterFromInt(reelFilter_));
refreshReel();
```

unconditionally. With `playingGone == false` on navigation, that **rebuilds the ordered
context and the Reel from the browsed folder** ( `PlaybackContext::build()` clears
`entries_` and re-populates from the model it is given —
`src/playback/playback_context.cpp`). The result is a Reel listing the *browsed*
folder with `currentIndex() == -1`, which contradicts:

- the audit's own **Expected implementation**: "Navigating elsewhere keeps the session
  alive (the Stage stays open, **the Reel keeps its own ordered list** until the
  player's folder is browsed back to)";
- the audit's **Deferred verification** item 4: "Confirm the Reel retains the playing
  folder's contents";
- this work item's Goal: "without closing the Stage, interrupting Cinema, or **losing
  the Reel's playing-folder context**";
- DESIGN §9.1 ("Current folder" + "Ordered playback items" belong to the preserved
  playback context) and §10.2/§19.4 (the Reel *is* the playback context).

So the guard was added:

```cpp
    if (hasActiveSession() && !sameFolder) {
        return;   // the session's context is not the displayed folder's business
    }
```

It is scoped to a **live** session, so the ordinary behaviour (Reel follows the browsed
folder when nothing is playing or the session is Idle) is unchanged — which is what
`qg_reel_wiring`'s "full navigation-style reset" assertions rely on.

### Decision table (derived from the final source — reasoned, not executed)

`identity = currentIdentity()`, `sessionFolder = QFileInfo(session->path()).absolutePath()`,
`sameFolder = sessionFolder non-empty && folderPath == sessionFolder`,
`hasActiveSession() = currentSession_ && state != Idle`.

| Situation | Before | After |
|---|---|---|
| Playing `/A/f.mp4`, navigate to `/B` | `stop()`, Reel replaced by `/B` | early return: session, Stage, Cinema, context and Reel all survive |
| Playing `/A/f.mp4`, navigate back to `/A` | `stop()` | `sameFolder` → rebuild; identity is present in the fresh rows → Reel re-anchors |
| `f.mp4` deleted while `/A` is displayed (watcher diff) | `stop()` ✓ | `sameFolder` → `playingGone` → `stop()` ✓ **preserved** |
| `f.mp4` deleted while `/B` is displayed | `stop()` (wrong — nothing in `/B` proves it) | early return, no stop |
| Sort change / watcher diff in `/B` while playing from `/A` | `stop()` (wrong) | early return, no stop |
| Nothing playing, browse anywhere | context rebuilt from the model | unchanged (`hasActiveSession()` false) |
| Session Idle (after `stop()`, `currentSession_` non-null), snapshot applied | rebuild, no stop | unchanged (`hasActiveSession()` false) |
| `FolderModel::clear()` | `stop()` | path dropped → `sameFolder` false → no stop (and `clear()` has **no callers** in `src/` or `tests/`) |
| Playing `/A/f.mp4`, deleted while `/B` shown, then browse back to `/A` | — | rebuild with the identity absent → `stop()` on the return pass (correct: the file is gone) |

## Static verification (executed this session)

| Check | Instrument | Result |
|---|---|---|
| `folder_model.cpp` compiles | `g++ -std=c++20 -fsyntax-only` (private Qt 6.10.2 prefix) | **exit 0** |
| `playback_controller.cpp` compiles | same | **exit 0** |
| …control (does the instrument go red?) | same command on copies with one deliberate rename each (`m_folderPath`→`m_folderPathX`; `folderPath()`→`folderPathX()`) | **both exit 1** with `error: 'm_folderPathX' was not declared` / `error: 'class qg::FolderModel' has no member named 'folderPathX'` — the check is live, **and** the second control proves the controller really resolves the new getter |
| `FolderModel::clear()` callers | `grep` over `src/` + `tests/` | **none** — dead code; the folderPath consistency change is inert in production |
| Existing-test compatibility | read `tests/unit/test_reel_wiring.cpp` fixture (`fill()` sets `snap.path = "/videos"`, entries `/videos/<name>`) against the new predicate | `removalNeverLeavesAStaleIndexOrCrashes`: session path `/videos/b.mp4` → `sessionFolder == folderPath == "/videos"` → `playingGone` still true → `stop()` still fires, and the later "navigation-style reset" block runs with an Idle session (no early return) → its assertions are unchanged in outcome |
| Property/signal integrity | inspection (no `moc` run) | `folderPath` READ accessor defined and `folderPathChanged` declared; `Q_PROPERTY` names match |
| `tests/` untouched | `git diff --stat -- tests/` | only the pre-existing AUD-001/003/004/009 edits from earlier sessions; no file of this work item |

## Deferred verification (recorded in `DEFERRED_TESTS.md`, not executed)

1. Start video playback. 2. Navigate three folders away through the sidebar.
3. Confirm playback continues and the Stage remains open. 4. Confirm the Reel retains
the playing folder's contents. 5. Return to the playing folder and confirm the Reel
re-anchors correctly. 6. Delete the playing file externally while its containing folder
is displayed and confirm playback stops. 7. Review the affected playback lifecycle and
Reel wiring tests for outdated navigation-stop expectations.
Added: 8. delete/rename the playing file while another folder is displayed, then browse
back — playback must stop on the return pass; 9. full CTest + full QML suite.

## Dependencies / interactions

- **AUD-006 landed in this same working tree** (`src/controllers/state_controller.{hpp,cpp}`,
  session note `2026-10-26-fix-aud-006-persisted-state.md`) — a separate session's work,
  preserved untouched. Interaction checked in source: the metadata restore path emits
  only `dataChanged` (roles Favorite/Watched/Progress —
  `src/models/folder_model.cpp:251-285`), which reaches
  `PlaybackController::onModelDataChanged()`, **not** the structure handler, and that
  handler only touches Reel rows whose identity is already in the context
  (`ctxRow >= 0`). While another folder is displayed none of its identities are in the
  playing context, so the AUD-006 pass is a no-op for playback. It cannot stop the
  session and cannot replace the Reel.
- The two `modelReset` consumers (StateController's re-decoration, PlaybackController's
  structure handler) are independent connections; their relative order does not affect
  the stop decision, which reads only the row set.

## Remaining risks / not proven here

- **Nothing was built or run.** The behaviour above is read from the source; the runtime
  checks (1)–(9) are deferred.
- **Path comparison is string equality.** `folderPath` is the browser's
  `QDir::cleanPath()`ed path (trailing separators and `.`/`..` normalised lexically —
  `browser_controller.cpp:74`), and `Entry::path` is `QDir(dir).filePath(name)`, so
  `QFileInfo(entry.path).absolutePath()` matches it for absolute inputs. A *relative*
  navigated path, or a symlink alias of the session's folder, would compare unequal →
  removal detection is skipped while the session plays (a **safe** direction: never a
  false stop). Canonicalising would need filesystem access on the GUI thread.
- **The Reel header still shows the browsed folder.** `ReelPanel.folderName`/`sortLabel`
  are bound to `browserController` (`Main.qml:847-852`) while the list now keeps the
  playing folder — a UI inconsistency created by preserving the context. Out of
  AUD-007's scope; recorded for the reviewer.
- While browsing another folder, Prev/Next and `positionLabel` continue to describe the
  **playing** folder's list (intended, §19.4), which is a behaviour change from "the
  list follows the browsed folder".
- `PlaybackContext::build()` is still called with the displayed model whenever
  `sameFolder` is true; if the session's folder is displayed but the model was replaced
  by an enumeration that no longer contains a *renamed* playing file, the existing
  identity-keyed semantics apply unchanged (`PlaybackSession::identity()` is never
  derived from the path, §17.5).