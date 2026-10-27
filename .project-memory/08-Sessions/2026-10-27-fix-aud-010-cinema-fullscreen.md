---
type: session
project: QG
created: 2026-10-27
updated: 2026-10-27
status: complete
aliases:
  - Fix-AUD-010-Cinema-Fullscreen
tags:
  - session
  - cinema
  - fullscreen
  - window-state
  - aud-010
source:
  - proj-audit.md §AUD-010 (FIX-STAGE-01, AUD-010 half)
  - DESIGN.md playback §4.2, §4.4, §4.5, §25.7
  - qml/QG/Main.qml
  - qml/QG/playback/Stage.qml, qml/QG/playback/TransportBar.qml
  - tests/qml/tst_cinema.qml
  - qmllint 6.10.2 control pair (static only, no build/run)
---

# 2026-10-27 — AUD-010: Cinema enters real fullscreen

## Objective

Implement the missing window-state half of Cinema (playback §4.2/§4.5/§25.7): entering
Cinema must fullscreen the **existing** `ApplicationWindow`, and exiting must restore the
previous window state. Layout state, Stage geometry, focus behaviour and playback state
must be preserved.

**Workflow:** logic-first with deferred testing — the standing policy is in force (a
testing phase was offered earlier in this session and declined), so this is
**IMPLEMENTED + STATICALLY REVIEWED / TESTING DEFERRED**. Nothing was built, run,
launched or screenshotted, and no test was modified.

## Verified root cause

**CONFIRMED, observed.** The Cinema *layout* half exists (the sidebar, path bar, context
strip, Reel and status strip are all bound to `!window.cinema`, and the Stage grows to the
window) but nothing ever changed the window state: `grep -rn "showFullScreen\|showMaximized\|Window.FullScreen\|setVisibility" qml/ src/`
matched nothing, and no QML file even imported `QtQuick.Window`.

Pre-fix handler (`qml/QG/Main.qml:983-990`, now `:991-1008`; the audit's "900-910" anchor was stale):

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

So "Cinema" was a chrome-collapse inside a decorated window — §4.2.1 ("Enter fullscreen
using the application's existing window") and §4.5 ("Leave fullscreen") were unmet, and
the Top overlay's close button was the only way out besides Esc/F11.

## Changes

One file, three hunks — `qml/QG/Main.qml`, the audit's patch applied as written:

| Hunk | Content |
|---|---|
| import (`:6`) | `import QtQuick.Window` added after `QtQuick.Layouts`, with a comment naming §4.2/§4.5 (the `Window.*` enum lives in that module) |
| state | `property int windowedVisibility: Window.Windowed` next to `property bool reelVisible` (`:120` / `:121-124` after the import hunk) |
| handler (`:992-1008`) | `onCinemaActiveChanged` now: on entry remember `window.visibility` (unless already `FullScreen`) → `window.visibility = Window.FullScreen` → `stage.forceActiveFocus()` → `stage.wakeControls()`; on exit `window.visibility = (windowedVisibility === Window.FullScreen) ? Window.Windowed : windowedVisibility` → `contentView.forceActiveFocus()` |

The Reel/`playbackStarted`/`playbackEnded`/`endOfFolder` handlers in the same `Connections`
block are untouched, as instructed.

**One addition beyond the literal patch:** the `import QtQuick.Window` line. Evidence it is
not strictly required (qmllint resolves `Window.FullScreen`/`Window.Windowed` with or
without it in Qt 6.10 — the diagnostic sets are identical), and the reason to keep it
anyway: the module that owns the `Window` type is named explicitly instead of relying on a
transitive import from `QtQuick.Controls`. If the reviewer prefers the literal minimal
diff, deleting the three comment lines plus the import is the only change needed. No other
deviation: the property comment and the two handler comments were aligned to the audit's
exact wording.

### Transition semantics (derived from the final source — reasoned, not executed)

| Situation | Result |
|---|---|
| Windowed/Maximized window, Cinema entered | state remembered, window goes `FullScreen` (decorations gone, screen filled) |
| Already `FullScreen` (e.g. WM fullscreen before Cinema) | the guard skips remembering, so `windowedVisibility` keeps its previous value and exiting returns to windowed/maximized rather than a stale fullscreen — that is what the ternary in the exit branch is for |
| Cinema exited (Esc/F11/F/double-click/close) | previous state restored exactly (windowed or maximized) |
| `visibility` reads `Window.AutomaticVisibility` (never explicitly set) | remembered as-is; restoring it re-applies the platform default, i.e. windowed — unchanged behaviour |
| Entered twice in a row | exit sets Windowed/Maximized → re-entry remembers that value ✓ |
| Reel | unchanged: `visible: !window.cinema && reelModel !== null && window.reelVisible` (`Main.qml:847`) already gives §4.5's "restore the Reel's prior visibility" |
| Playback | untouched — the diff adds no play/stop/seek call, so §25.7's "no reload, no rebuffer" holds by construction |

## Static verification (executed this session)

| Check | Instrument | Result |
|---|---|---|
| QML parses and type-checks | `qmllint 6.10.2` on the finished `Main.qml` vs a **pre-fix control** (this change reverse-applied, siblings in place so local types resolve as well as they can) | 139 → 141 diagnostics; the only substantive additions are **2 × `Member "wakeControls" not found on type "Stage"`**, the same warning the baseline *already* emits 3 × for the same method (`Main.qml:1454-1455` and `:1459-1460` — the pre-existing cursor-overlay calls, guarded exactly the same way). Cause: the `qg` module has no `qmldir`, so qmllint cannot resolve the local `Stage` type at all — the baseline carries the identical `visible`/`height`/`activeFocus` warnings for it. **No new diagnostic class, no `Window`/`visibility`/`windowedVisibility` diagnostic** (the enum, the read and the write all resolve) |
| Import necessity | lint of the patch **without** `import QtQuick.Window` | identical diagnostic set → the import is explicit-by-choice, not a fix for an error; recorded above |
| Scope | `git diff --stat` | one file (`qml/QG/Main.qml`); no other QML/C++ touched |
| `wakeControls` exists and wakes **both** overlays (§4.4) | source | `Stage.qml:51-53` `function wakeControls() { transport.wake() }`; `TransportBar.qml:107-110` `wake()` sets `opacity = 1` + restarts the 2 s auto-hide; the Cinema top bar binds `opacity: stage.cinema ? transport.opacity : 0` (`Stage.qml:129`), so both overlays appear together |
| No binding loop introduced | source | the state is written imperatively inside a signal handler, not in a binding; `windowedVisibility` is a plain property; nothing binds to `window.visibility` |
| No second window | source | only `visibility` is written; no `Window`/`ApplicationWindow` object is created anywhere in the diff (§1.5) |
| Playback state untouched | source | the diff contains no playback-manager/controller call other than reading `window.cinema` |
| `Stage.height` / Stage sizing untouched | source | §4.2.3's "expand to the fullscreen dimensions" comes from `height: cinema ? parent.height : …` (`Stage.qml:60-62`) reading the now-fullscreen window — no Stage edit needed |

## Deferred verification (recorded in `DEFERRED_TESTS.md`, not executed)

Audit's list verbatim: on X11, F11 must remove decorations and fill the screen; Esc must
restore the previous geometry exactly; double-click on the video must toggle; playback must
not restart or re-buffer (§25.7); the pointer must hide with the overlays after 2 s.
Added, from this item's prompt and the audit's regression note: `tst_cinema.qml` re-run
(the whole file — 3 of its subtests are known-baseline failures: `test_cursorNotHiddenWhileTextEditing`,
`test_escapeAndFullscreenShortcutsGatedByEditorFocus`, `test_fourTogglePointsConsistent`);
`Window.FullScreen` is a **no-op under `QT_QPA_PLATFORM=offscreen`**, so the test must
assert the `window.visibility` **property value**, not a real screen size; verify the
offscreen window is not hidden/disturbed by the visibility writes; verify Stage focus
survives the state change on X11 (a WM focus round-trip could arrive after
`forceActiveFocus()`); verify maximized → Cinema → exit returns to maximized.

## Dependencies / interactions

- **AUD-011 (Esc semantics) is separate and untouched** — the audit's note says it should
  land with this change, "otherwise Esc from fullscreen also stops playback". Today
  `tst_cinema::test_escapeExitsCinemaThenStops` asserts the first Esc exits Cinema without
  stopping (a second Esc stops), and this change does not alter that path: exiting Cinema
  restores the window state and hands focus back to the content view, and a subsequent Esc
  behaves exactly as before.
- **AUD-028 (window geometry persistence) is not implemented** — window geometry is not
  persisted at all, so quitting while in Cinema persists nothing and cannot corrupt a
  saved geometry (the audit's own note).
- AUD-009's reflow is complementary and untouched: `reservedTop` is released in Cinema by
  the existing `!window.cinema` term, since the Stage fills the window there.

## Remaining risks / not proven here

- **Nothing was executed.** The X11 behaviour in §25.7 (decorations, geometry restore,
  pointer hiding, no rebuffer) is unverified, as is the offscreen test interaction.
- **Wayland** was not considered beyond the audit's scope: `visibility` transitions may be
  driven differently there (the project already records Wayland as untested).
- The `windowedVisibility === Window.FullScreen` branch is unreachable while the entry
  guard holds (defensive only) — harmless, and kept because the patch specifies it.
- The fullscreen change is applied on the **GUI thread inside a signal handler**; on a
  slow WM the visual transition happens after the layout swap. No sequencing problem was
  found, but the ordering was not observed.
- If the platform refuses `Window.FullScreen`, `visibility` still reads back the requested
  value in Qt but the window keeps its decorations — which is exactly why the deferred
  check insists on a real X11 session rather than a property assertion alone.
