---
type: session
project: QG
created: 2026-10-25
updated: 2026-10-25
status: complete
aliases:
  - Fix-AUD-004-Selection-Focus
tags:
  - session
  - selection
  - focus
  - qml-bindings
  - aud-004
source:
  - proj-audit.md §AUD-004 (FIX-SEL-01)
  - DESIGN.md §5.5, §6.2, §13.1, §13.2, §11.2, §3.6
  - src/models/selection_state.{hpp,cpp}
  - src/models/{folder_model.hpp,search_proxy.*,recursive_search_model.*}
  - qml/QG/{ContentView,Main}.qml
  - ctest + qg_qml runs, C++ control probe, rendered-pixel probe (2026-10-25)
---

# 2026-10-25 — AUD-004: selection and focus reach the surface

## Objective

Resolve AUD-004 ("selection and focus never reach the view; select-all and
range-select are stubs") in the checked-out tree, verifying the finding's claims
rather than applying its patch text verbatim, and prove the result with an
instrument that goes **red** on the pre-fix code. Per instruction, no test file
was modified: the production wiring is the only thing that changed, and the
verification instruments live outside the repository.

## Verified root cause

**CONFIRMED, mechanically, with a control run.** `SelectionState` had no
`Q_PROPERTY` at all: `focusIndex()`, `selectedCount()`, `selectedIdentities()` and
`isSelected()` were invokable getters only. A QML binding re-evaluates only when a
*property* it read changes, so every consumer bound once at creation and never
again — card borders, the cyan focus ring, the status-strip count/bytes and the
context-menu title.

The control probe (`probe_prefix_control`, built from `git show HEAD:` sources)
shows the mechanism directly: after `selectOne`/`toggle`/`extendRange`/`selectAll`
the probe's `count` stayed **0**, `focus` **−1**, `row0=false`, `summary="No
selection"`, and its notification log stayed at the two creation-time entries. The
same probe against the fixed class logged every change. The audit's secondary
claims also checked out: `extendRange()` inserted one identity and never filled the
gap, `selectAll()` selected nothing, and `clear()` reset `focusIndex_` to −1 (so
`moveFocus` early-returned and the cursor died on the first Esc/arrow pair).

## Changes

| File | Change |
|------|--------|
| `src/models/selection_state.hpp` | Declared the observable surface: `Q_PROPERTY` for `selectedCount`, `selectedIdentities`, `focusIndex`, `selectionRevision`, `summary`; identity-list contract `extendRangeTo(QStringList, int)` / `selectAll(QStringList)`; `clearFocus()`. The old no-arg `selectAll()` / one-identity `extendRange()` are kept as **non-invokable** C++ overloads (documented as legacy) so the pre-existing unit test still compiles without editing it; `moveFocus*` helpers stay but are documented as view-bounded |
| `src/models/selection_state.cpp` | Every selection mutator now bumps `selectionRevision_` and emits `selectionChanged()`; `focusChanged()` only when the cursor actually moves. `extendRangeTo` replaces the selection with the caller's range, `selectAll` keeps an existing cursor (only seeding 0 when there was none), `clear()` clears the selection and **keeps** focus (the cursor becomes the range anchor), `clearFocus()` drops it. `selectedIdentities()` sorted for determinism; empty identities ignored in every mutator |
| `qml/QG/ContentView.qml` | Added the observable adapters (`selFocusIndex`, `selRevision`) that read the **properties** (creating the binding dependency) while still accepting an invokable-shaped double; declared the previously undeclared legacy `focusIndex`/`selectedIndex`; view-order identity helpers (`identityAt`, `allIdentities`, `identitiesInRange`) that use the manifest model for Manifest rows; rewrote the four delegate binding sites (`focused`/`selected`) on all three densities + Manifest; Shift+arrows now move the cursor **and** extend the range from the anchor; Ctrl+A builds the identity list from the view; Ctrl+Space uses the adapter; Esc clears the selection in Browse (leaves the key to the search morph/playback) |
| `qml/QG/Main.qml` | `selectionCount()`/`selectionIds()` accessors replace the invokable calls at all five sites (status strip `selectedCount` + `selectedBytes`, `menuSelectionCount`, bulk-delete gate, Ctrl+X/Ctrl+C), so the strip, the aggregate size and the "N items" title track the selection |
| `src/models/folder_model.hpp` | `indexOfIdentity` is now `Q_INVOKABLE` — the status strip's aggregate size resolves each selected identity through it, and without this the byte total stayed 0 even after the binding was fixed |
| `src/models/search_proxy.{hpp,cpp}` | `Q_INVOKABLE identityAt(proxyRow)` — a Manifest row is a filtered source row, so its identity is resolved through `mapToSource` on the source `FolderModel` (also the missing piece for AUD-023 manifest activation) |
| `src/models/recursive_search_model.{hpp,cpp}` | `Q_INVOKABLE identityAt(row)` for recursive hits (no `FolderModel` row exists for them) |

No signal, no QML type registration and no public C++ entry point changed name;
QML that read `selectionState.selectedCount()` now reads the property.

## Verification (all executed)

Host: Qt runtime only, no dev packages → user-local Qt prefix
(`~/.hermes/cache/scratch/qtdev/prefix/usr`) for headers, system Qt runtime libs,
Xvfb `:77` extracted from `qtdev/xvfb/xvfb_21.1.22-1ubuntu1.3_amd64.deb`.
Build tree `build-work`. Note: a *background* terminal call does not inherit the
session's exported `LD_LIBRARY_PATH`, and `qmlcachegen` needs it — builds must run
with the env inline.

| Check | Command | Result |
|---|---|---|
| Control (mechanism) | `probe_prefix_control` (HEAD `selection_state` sources, invokable-shaped probe) | **red as designed** — no binding re-evaluation after any mutation |
| Fixed (mechanism) | `probe_fixed` (same probe, fixed class) | every mutation notifies; `count=1/2/2/4/0`, `focus=0/1/3/3/3`, `row0/row1` track, `clear()` keeps focus 3, `clearFocus()` → −1 |
| Rendered pixels | `render_probe` (real `SelectionState` + real `ContentView`, `grabWindow()` → ARGB32) | **16/16 checks** across all four surfaces. Standard: 1955 cyan (`#38E1E1`) px with **0** blue while nothing is selected; after selecting row 1 with the cursor on row 0 → cyan x[26..329] **and** blue (`#4DA3FF`) x[347..652]; cursor→row 2 moves the ring to x[670..973]; Ctrl+A → 6880 blue px across x[25..974]; **control card** (old invokable-only binding) with `id3` selected renders **0** blue px. Gallery: 2296 cyan / 2312 blue px. Compact: 4208 / 4224 px. **Manifest** (real `RecursiveSearchModel` injected from C++): 4 rows listed, `Ctrl+A` resolves `id0..id3` through the manifest model, the rows render the border, a row-2 click selects `id2`, a range from that anchor yields `id2,id3` |
| QML density harness | `aud004_matrix.qml` via `qg_qml_test -input` (outside the repo tree) | **3/3**: in Gallery and Compact the focus ring / selection border bindings and the Ctrl+A–Esc–Shift+Right identity contract hold; Standard's identity contract is asserted by the same harness in its default density and by the render probe's keyboard section |
| Build | `cmake --build build-work` | all targets, zero new warnings from the touched files |
| Full CTest | `ctest --test-dir build-work -E ^qg_qml$` | **48/49** — only `qg_watcher::modelDiffPreservesUnaffectedRows`, the pre-existing host-flaky suite (identical failure recorded in the AUD-003 session with a pre-change binary) |
| QML suite | `DISPLAY=:77 ./build-work/tests/qg_qml_test -input tests/qml` | **200 passed / 8 failed** — byte-identical failure set to the pre-fix control run (`aud004-prefix-qml.log`); no new failure |

### Pre-existing failures (not caused by this work)

| Test | Control evidence |
|---|---|
| `CardActions::test_keyboardDispatchesFocusedCardActions` | Identical failure before the change (`aud004-prefix-qml.log`); it drives `handleKey` with F/W keys that this view deliberately does not handle (KeyboardFramework owns them) |
| `Keyboard::test_noDuplicateEnabledSequences` (`Alt+Left`), `Cinema::test_cursorNotHiddenWhileTextEditing`, `Cinema::test_escapeAndFullscreenShortcutsGatedByEditorFocus`, `Cinema::test_fourTogglePointsConsistent`, `Keyboard::test_textEditingSuppressesSingleKeyShortcuts`, `TransportBar::test_reflectsPlaybackState`, `ContentView::test_focusAndSelectionAreDistinct` | All present in the pre-fix control run with the same messages |
| `qg_watcher::modelDiffPreservesUnaffectedRows` | Documented pre-existing (AUD-003 session, pre-change binary) |

## Residual risks / not done here

- **Test files were left untouched by instruction**, so three known-bad test
  artifacts remain and are recorded here rather than fixed:
  `tests/unit/test_selection_state.cpp` (a) never executes — its `main()` does not
  use `QTEST_MAIN`, so the `selection` ctest target is a zero-test green gate, and
  (b) encodes the old buggy contract (`clear()` resetting focus to −1, and the
  stub `selectAll()`).
- **The in-repo QML fixtures cannot exercise identity-keyed selection**: assigning
  `identityAt` to a `ListModel` fails ("Cannot assign to non-existent property"),
  and the doubles expose invokable getters whose *method bodies'* property reads
  are captured by the caller's binding — which is exactly why the QML suite could
  never have caught AUD-004. The production tolerance for invokable-shaped doubles
  is what keeps those suites green.
- **Live mouse input was not synthesised**: the click / Ctrl+click paths are
  asserted at `ContentView.selectIndex` / `SelectionState.toggle` level and their
  rendered marks are verified from pixels in all four surfaces, but no X11 input
  event round-trip was run.
- **Two environment quirks constrained the instruments** (not the product): Quick
  Test's case item is invisible and Qt propagates that to the whole subtree, so an
  in-process read of `visible` is meaningless — drawn marks are therefore asserted
  from pixels in a real `QQuickView` (`render_probe`), and a JS-array harness model
  loses its attached accessor functions when a `ListView`'s QVariant-typed `model`
  converts it, so the Manifest identity path is verified with a real
  `RecursiveSearchModel` instead.
- **Manifest selection is now identity-correct, but `ContentView`'s
  `pathForIndex`/activation for Manifest rows is untouched** (`playAt`/`reveal`
  still resolve a Manifest row through the browse model) — that is AUD-023's
  (FIX-SRCH-02) scope, deliberately out of this change.
- **Arrow keys are still shadowed while a session is active**: `Main.qml`'s
  playback `Left`/`Right`/`Shift+Left`/`Shift+Right`/`Space` shortcuts are gated on
  `stageKeys` (= `sessionActive && !textEditing`), not on the Stage actually holding
  focus, so during playback they win over the grid and Shift-range cannot be
  produced from the keyboard. That routing defect is AUD-029/AUD-011 territory and
  was left alone; it is recorded here because it limits how much of the new
  keyboard selection model is reachable in a live session.
- **`SelectionState::moveFocus*` remains unbounded** (upper edge) and `moveFocusEnd`
  still uses a `9999` sentinel. It is not called by QML or the view (the view
  clamps against `itemCount`); it is kept only because the legacy unit test calls
  `moveFocusUp()`. Removing it belongs with that test's repair.
- **No new repository test was added** for AUD-004: the instruments are scratch
  harnesses under `~/.hermes/cache/scratch/aud004/` (probe.cpp, render_probe.cpp,
  aud004_render.qml, render_scene.qml, four PNGs). Promoting them into
  `tests/unit/` + `tests/qml/` is the follow-up that makes the gate permanent.
