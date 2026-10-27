---
type: session
project: QG
created: 2026-10-26
updated: 2026-10-26
status: complete
aliases:
  - Fix-AUD-005-Search-Manifest
tags:
  - session
  - search
  - manifest
  - qml-bindings
  - aud-005
source:
  - proj-audit.md §AUD-005 (FIX-SRCH-01)
  - DESIGN.md §7.1, §7.2, §7.3, §7.4, §23.4, §25.4
  - src/controllers/search_controller.{hpp,cpp}
  - qml/QG/{Main,PathBar,ContentView}.qml
  - src/models/search_proxy.hpp
  - tests/qml/tst_wiring.qml, tests/unit/test_search_controller.cpp
  - qmllint control-pair run + g++ -fsyntax-only control run (2026-10-26, static only)
---

# 2026-10-26 — AUD-005: the Manifest intensity is reachable

## Objective

Resolve AUD-005 ("Search can never enter the Manifest intensity") in the checked-out
tree — make DESIGN.md §7.2 reachable — after verifying the finding's two claimed
mechanisms against the actual source rather than trusting the audit's quotes. Per the
active policy ([[Agent-Workflow]] § Development Workflow Mode) this session delivered
**IMPLEMENTED + STATICALLY REVIEWED** only: no build, no CTest/QML run, no launch, no
screenshot. Every check below is a *static* instrument, and the runtime checks are
recorded in `DEFERRED_TESTS.md` instead of being claimed.

## Verified root cause

**Cause (a) — CONFIRMED, observed.** `SearchController` declared **no `Q_PROPERTY` at
all** (`src/controllers/search_controller.hpp` at HEAD: only `Q_INVOKABLE` methods plus
`signals:`). `qml/QG/Main.qml:157-158` (HEAD: 104-105) reads `window.searchController.active === true`;
on a QObject with no such property that read is `undefined`, so `manifestActive` was
permanently `false`. It also gates `contentState` (Main.qml:140-142; HEAD 87-88), so the browse
overlay state was evaluated as if no search were running.

**Cause (b) — CONFIRMED, observed.** `setFolderName`/`setSortLabel` were plain member
functions (HEAD `search_controller.hpp:39-40`, no `Q_INVOKABLE`). The QML call inside `onActiveChanged`
(`Main.qml:945-946`; pre-fix 908-909) therefore raises a TypeError inside `onActiveChanged`, and the
handler aborts before `contentView.enterManifest(...)` (`Main.qml:948-949`; pre-fix 911-912). The
`else` branch (`exitManifest()`) contains no non-invokable call, which is exactly why
the audit's "only half of the handler works" claim holds.

The two causes are independent and both had to be fixed: with only (b) fixed the
surface would still not switch (`manifestActive` false), and with only (a) fixed the
handler would still throw before `enterManifest`.

**Refuted / corrected:**

- The audit's `Q_PROPERTY(QString query …)` and `Q_PROPERTY(bool scopeRecursive …)` are
  **not** applied: both names are *called* as functions from QML
  (`Main.qml:88-92` is the only `query()` call; `Main.qml:626`, `Main.qml:975` and
  `ContentView.qml:968` call `scopeRecursive()`). QML name resolution between a same-named property and an
  invokable method is not something this session can validate without running the
  engine, and both call sites sit on the §7.1/§20.4 scope path this change must not
  regress, so those two stay invokable (the in-repo precedent from AUD-004 is the same
  rule: a name QML *binds* becomes a property, a name QML only *calls* stays invokable).
- The audit's "update the test" notes are not applicable here: both tests were left
  byte-identical, and the production code is tolerant of their shapes instead.
- The audit's "Expected implementation" line numbers were stale (AUD-004/AUD-009 landed
  since); every citation in this note was re-anchored against the current tree.
- `REQUIREMENTS_MATRIX.md` lists SEARCH-004/005/006 as `PLANNED`, but the Manifest list +
  sortable header (`ContentView.qml:722-921`), the no-results overlay
  (`qml/QG/overlays/NoResults.qml`, wired at `ContentView.qml:897-902`) and the scope
  chip (`PathBar.qml:396-417`) **exist at HEAD**. Those rows are stale, not gaps.

## Changes

| File | Change |
|------|--------|
| `src/controllers/search_controller.hpp` | Declared the observable surface: `Q_PROPERTY(bool active READ active NOTIFY activeChanged)`, `Q_PROPERTY(QString summary READ summary NOTIFY resultsChanged)`, `Q_PROPERTY(int resultCount READ resultCount NOTIFY resultsChanged)`; `summary()` loses `Q_INVOKABLE` (no QML caller — it is bound, not called); new `int resultCount() const` and `bool active() const`; `setFolderName`/`setSortLabel` became `Q_INVOKABLE`; new private `void setActive(bool)` and member `bool active_ = false` |
| `src/controllers/search_controller.cpp` | `setActive()` owns the transition: member assigned **before** `emit activeChanged(active_)`, and emitted only on a real change. `flushQuery()` uses it; `clear()` now notifies `active` *and* `resultsChanged(summary())` (previously `activeChanged(false)` only, which left a bound summary stale after Esc). `resultCount()` is the single scope-aware count — `summary()` calls it, so the count the strip announces and the count a binding reads cannot drift |
| `qml/QG/Main.qml` | Added `searchIsActive()` / `searchSummaryText()` / `searchAppliedQuery()` accessors that read the controller's **properties** while still accepting an invokable-shaped double. The `Connections` handler reads the state from the property, keeps `setFolderName`/`setSortLabel` before `enterManifest`, refreshes `summaryText` **and** the applied query from the properties, no longer steals focus from the open field on the first Esc, and routes the §7.4 no-results "Clear search" to the field |
| `qml/QG/PathBar.qml` | New `clearSearchQuery()` — the field's ✕ and the no-results "Clear search" are the same intent, so both now take one path |
| `qml/QG/ContentView.qml` | New `searchClearRequested()` signal; the no-results overlay forwards it to the shell instead of dropping the Manifest on its own |

No existing signal, QML type registration or public C++ entry point changed name or
signature. `tests/` was not touched.

### Chosen property/signal contract

- **`activeChanged(bool)` keeps its argument.** The unit test asserts
  `activeSpy.takeFirst().at(0).toBool()` (`tests/unit/test_search_controller.cpp:67`) and
  the QML double declares `signal activeChanged(bool active)` and emits it with one
  (`tests/qml/tst_wiring.qml:73, 302`). A property NOTIFY signal may carry arguments, and
  because `setActive()` writes the member *before* emitting, the argument and the
  property are the same value by construction — so nothing is lost by keeping the
  parameterless variant un-adopted, and both tests stay valid.
- **Repeated `clear()` no longer re-emits `activeChanged`** (deduplicated by
  `setActive`). It now emits `resultsChanged("")` on every clear, which is what keeps a
  *bound* `summary`/`resultCount` truthful after Esc.
- **`resultCount` is exposed, but not as a second source of truth.** `summary()` and the
  property both call the same `resultCount()`; in the default "This folder" scope it
  equals `searchProxy.resultCount` (which stays the live count bound by the PathBar
  field, `Main.qml:545-549`), and in "Subfolders" scope the proxy's value only reflects
  the stale in-folder filter, so the controller is the authoritative count there.

### Deviations from the audit's patch text

1. `query` and `scopeRecursive` are **not** properties (reason above).
2. The audit's `m_resultCount` cached member is **not** added: `resultCount()` computes
   from the owning model, so there is no ordering window in which a notification
   announces a value the getter would not return.
3. Three additions beyond the audit's fix block, each reachable only *because* the
   Manifest now appears, each traced to an explicit DESIGN clause:
   - the first Esc keeps focus in the field (§7.2 "Escape clears the query; a second
     Escape leaves the field" is unreachable if the handler forces focus into the browse
     view on clear);
   - `onResultsChanged` refreshes the applied query as well as the summary (§7.4/§25.4
     "No matches for \"<q>\"" — the view only received the query once, at
     `enterManifest`);
   - the no-results "Clear search" routes to the field (§7.4 Clear action; §25.4
     "restores the exact prior scroll position").
   If the reviewer prefers these split out, they are three independent hunks.

## Static verification (executed this session)

Nothing here is a build, a test run, or a launch; no claim of runtime behaviour is made.

| Check | Instrument | Result |
|---|---|---|
| C++ syntax/declaration integrity | `g++ -std=c++20 -fsyntax-only` on `src/controllers/search_controller.cpp` with the private Qt 6.10.2 prefix (`~/.hermes/cache/scratch/qtdev/prefix/usr`) | **exit 0, no diagnostics** |
| …control (does the instrument go red?) | same command on a copy with one deliberate rename (`active_` → `activeX_`) | **exit 1**, `error: 'activeX_' was not declared in this scope; did you mean 'active_'?` — the check is live |
| QML diagnostics (PathBar) | `qmllint 6.10.2` vs a *pre-fix-worktree* control file (this change reverse-applied) | **128 vs 128 diagnostics, identical sets** — no new diagnostic |
| QML diagnostics (ContentView) | same | **74 vs 74, identical sets** |
| QML diagnostics (Main) | same | **139 vs 139, identical sets** |
| QML diagnostics vs `git show HEAD:` (all three) | same, HEAD copies placed inside `qml/QG/` so sibling types resolve | only differences are the pre-existing AUD-004/AUD-009 work (`stage.height`, `stage.visible`, `Theme.space3`) — none from this change |
| Consumers re-grepped before the API change | `searchController.active\|summary\|query\|resultCount\|activeChanged\|scopeRecursive` over `qml/ src/ tests/` | Main.qml (5 sites), ContentView.qml (1 Connections), `src/app.cpp:241`, `src/core/composition.hpp:73`, `tst_wiring.qml:285/320`, `test_composition.cpp:83/97`. Every one still valid; no QML caller of `summary()` exists |
| Control files removed | `git status --porcelain` after cleanup | untracked set back to the 8 pre-existing entries (`?? .cache/`, 5 session notes, 2 unit tests) — nothing of this session left behind |

### Path trace, re-read end to end (static)

1. **Ctrl+F** → the central `kbSearch` Shortcut → `routeKey("view.search")` →
   `KeyboardFramework::dispatch` (`src/controllers/keyboard_framework.cpp:103-104`) →
   `searchFocusRequested()` → the shell's `Connections` on `window.keyboard`
   (`Main.qml:1109-1116`) → `pathBar.toggleSearch()` → `searchActive = true`, overlay
   shown, field `forceActiveFocus()`, text cleared.
2. **Typing** → `searchField.onTextChanged` → `pathBar.searchQueryChanged(text)` →
   `Main.qml` `onSearchQueryChanged` → `searchController.setQuery(text)` (80 ms debounce).
3. **Flush** → `flushQuery()` → `proxy.setQuery()` (result count updates the field) →
   `setActive(true)` → member then `activeChanged(true)` → `searchIsActive()` reads the
   property → `setFolderName`/`setSortLabel` (now invokable) → `enterManifest(proxy,
   query)` → `manifest = true`, `savedGridViewY` captured → `resultsChanged(summary())` →
   `summaryText` from the property.
4. **§7.2 chrome** → `manifestActive` reads the property → true → `contentState` returns
   `""`, and the Manifest list/no-results/summary surfaces take over inside ContentView.
5. **Esc (first)** → the field's `Keys.onEscapePressed` clears the text →
   `searchQueryChanged("")` → `setQuery("")` → `clear()` → proxy reset → `setActive(false)`
   → `activeChanged(false)` → `exitManifest()` (scroll restored from `savedGridViewY`) →
   `resultsChanged("")`. The handler now leaves focus in the field because
   `pathBar.searchActive` is still true.
6. **Esc (second)** → `pathBar.exitSearch()` → morph closed; the controller was already
   inactive, so `setQuery("")` early-returns and no further notification is expected.
7. **Empty query** → `setQuery("")` routes to `clear()`; **already-inactive `clear()`** →
   `setActive` no-ops, only `resultsChanged("")` is emitted. **Whitespace** is *not*
   trimmed: `"  "` is a literal substring query (observed behaviour, unchanged by this
   fix; DESIGN specifies no trimming).
8. **AUD-023 exposure** — yes. `enterManifest()` calls `selectionState.setFocusIndex(0)` +
   `selectionState.clear()`, and `exitManifest()` restores only `contentY`, so §25.4's
   "restores the exact prior scroll position **and selection**" is not met. The defect is
   pre-existing; this change makes it *reachable*, so it is now user-visible. Not fixed
   here (it is AUD-023 / FIX-SRCH-02 scope) and recorded as a dependent finding.

## Dependent findings (reported, not implemented)

| # | Finding | Evidence | Why not here |
|---|---|---|---|
| 1 | Manifest row focus (§7.3 "↑/↓ move the row focus") is not wired | `ContentView.qml` `manifestList` (`focus: contentView.manifest`, line 867) has no `Keys.onPressed`; only grid/compact attach `contentView.handleKey`. The delegate binds `focused: index === contentView.selFocusIndex`, so the ListView's own cursor moves a highlight nobody reads | §7.3 keyboard contract, explicitly out of AUD-005's fix block |
| 2 | Focus ownership between the search field and the Manifest list is unverified — and if the list wins, §7.2's first-Esc and §7.1's incremental typing stop reaching the field | `ContentView.qml:867` `focus: contentView.manifest` vs `grid.focus: !compact && !manifest` (`ContentView.qml:596`); the shell's `textEditing` (`Main.qml:175-183`) is derived from `activeFocusItem`. **INFERRED, not observed** — QML focus resolution was not run | Needs a runtime/batch check (recorded in `DEFERRED_TESTS.md`); choosing "the list keeps focus" vs "the field keeps focus" is a product decision tied to finding 1 |
| 3 | §25.4 selection restore on Esc (AUD-023 / FIX-SRCH-02) | `ContentView.enterManifest`/`exitManifest` (lines 77-97) | Separate audit item |
| 4 | No-results "Clear search" previously left the morph open and the controller active over a Browse grid | `NoResults.onClearRequested` used to call `contentView.exitManifest()` | **Fixed** here (2-line routing), because the path is reachable only after this change and §7.4/§25.4 name the Clear action |
| 5 | `REQUIREMENTS_MATRIX` SEARCH-004/005/006 rows claim `PLANNED` while the surfaces exist at HEAD | see "Refuted" above | Matrix rows corrected in this session |

## Remaining risks / not proven here

- **Nothing was built, run, launched or screenshotted.** `moc` was not run, so the
  `Q_PROPERTY` READ/NOTIFY wiring is validated by inspection only (READ accessors exist
  and are defined; NOTIFY signals exist with the declared `bool`/`QString` types).
- **The QML double cannot validate the missing notification**, exactly as AUD-004
  established: `MockSearchController` is reactive by accident and declares no `active`,
  no `resultCount` and a *function* `summary()`. The production code tolerates those
  shapes, which is what keeps `tst_wiring` valid — but a green mock-driven suite is not
  evidence for the real object either way.
- **The PathBar field's live count still comes from `searchProxy.resultCount`.** In
  "Subfolders" scope that value reflects the stale in-folder filter while the summary now
  reports the walk's count, so the two can disagree. Rebinding it was not done: the
  binding is asserted by `tst_wiring` (MockSearchProxy) and belongs to SEARCH-001/006.
- **Focus chain after the second Esc is inferred**: the field becomes invisible and
  `grid.focus` is true, so the browse view should become the focus item — not observed.
- No new repository test was added (policy: no test suite for this fix; tests untouched).