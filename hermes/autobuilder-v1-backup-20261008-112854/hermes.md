# QG Project Instructions — .hermes.md

## Project

```text
QG
```

Project root:

```text
/home/treshen/Projects/QG/
```

QG is a native Ubuntu/Linux video file explorer and player.

The filesystem is the source of truth.

The application browses real folders, displays supported video files and directories, provides inline playback, fullscreen playback, search, sorting, file operations, and lightweight persistent media state without importing media into a proprietary library.

---

# Instruction Priority

When instructions conflict, use this priority order:

```text
1. DESIGN.md
2. existing working implementation
3. established architectural decisions in DECISIONS.md
4. dev-guide.md
5. GUIDE_STATE.md execution state
6. current development prompt
7. local implementation preference
```

`DESIGN.md` defines approved product behavior and architecture.

`dev-guide.md` defines implementation order.

`GUIDE_STATE.md` defines which prompts are complete, pending, active, or blocked.

`DECISIONS.md` records approved implementation decisions already made.

`REQUIREMENTS_MATRIX.md` tracks requirement coverage.

Do not redesign QG while implementing it.

---

# Required Files to Read

At the beginning of a development session, inspect these files when present:

```text
DESIGN.md
dev-guide.md
GUIDE_STATE.md
DECISIONS.md
REQUIREMENTS_MATRIX.md
```

Do not blindly read the entire source tree.

After identifying the current task, inspect only the relevant implementation files, direct dependencies, tests, and build definitions needed for that task.

---

# Approved Technology Stack

QG uses:

```text
C++20
Qt 6
Qt Quick / QML
Qt Quick Controls 2
C++ QAbstractItemModel / QAbstractListModel
Qt Quick Scene Graph
OpenGL
X11 / xcb
libmpv C API
mpv_render_context
SQLite
POSIX/Linux filesystem APIs
GIO / GVfs
UDisks2 / D-Bus where required
FFmpeg / libavformat / libavcodec where required
CMake
Ninja
CTest
Qt Test
native Ubuntu .deb packaging
```

Do not replace this stack with Python, PyQt, PySide, Electron, Chromium, or a browser UI.

Do not introduce a second UI framework.

---

# Platform Target

Primary runtime:

```text
Ubuntu
+
X11
+
Qt xcb
```

The implementation may preserve future Wayland portability where inexpensive, but X11 is the current optimization target.

The application should use normal Linux desktop conventions and XDG paths.

---

# Rendering Architecture

The preferred UI rendering path is:

```text
Qt Quick / QML
↓
Qt Quick Scene Graph
↓
OpenGL
↓
X11/xcb
```

The preferred video rendering path is:

```text
libmpv C API
↓
one mpv handle
↓
mpv_render_context
↓
custom Qt Quick video item
↓
Qt Quick scene graph / OpenGL
↓
X11/xcb
```

Do not make `--wid` child-window embedding the primary playback implementation.

Do not create separate playback engines for Stage and Cinema.

---

# Presentation / Domain Boundary

QML is presentation.

C++ is authoritative application logic.

QML may handle:

- layout;
- transitions;
- visual state;
- delegates;
- interactions;
- animations;
- local presentation-only state.

C++ should own:

- filesystem enumeration;
- models;
- navigation state;
- media identity;
- sorting;
- search state and filtering logic;
- playback;
- playback context;
- persistence;
- background tasks;
- thumbnail pipeline;
- metadata probing;
- file operations;
- mounted-volume state;
- application settings.

Do not place core domain logic in QML JavaScript.

---

# Model Rules

Use C++ Qt item models for directory/media data.

Preferred flow:

```text
filesystem/service results
↓
C++ FolderModel / related models
↓
roles/signals
↓
QML GridView / ListView
```

Keep model rows lightweight.

Do not create permanent QML objects for every media entry in large directories.

Use Qt Quick view virtualization.

Do not duplicate authoritative model contents in JavaScript arrays.

---

# Filesystem Rules

The filesystem is authoritative.

The browser must represent:

- every child directory;
- supported video files;
- real paths;
- mounted volumes where visible to the OS.

Unsupported regular files are filtered according to the approved media format list.

Child directories remain navigable even if they contain no videos.

File operations must operate on real filesystem entries.

Preserve Linux filename and path semantics.

Use native Trash behavior where supported.

Do not silently permanently delete files.

---

# Playback Rules

Maintain:

```text
one mpv handle
+
one active playback session
+
one authoritative ordered playback context
```

Playback state must include:

- media identity;
- path;
- duration;
- position;
- play/pause state;
- volume;
- mute state;
- error state;
- ordered folder context;
- current index.

Previous/Next, Reel highlighting, auto-next, and playlist position must derive from the same ordered folder context.

Do not independently sort duplicate copies of folder media.

Stage → Cinema must preserve the current mpv session.

Cinema → Stage must preserve the current mpv session.

Do not reload media merely to change layout state.

---

# Threading / Responsiveness

The Qt GUI thread must not perform heavy work.

Do not perform on the GUI thread:

- large directory scans;
- repeated blocking filesystem calls;
- thumbnail extraction;
- media probing;
- image decoding/downscaling;
- bulk file operations;
- long database work;
- blocking subprocess waits;
- large unbounded loops.

Use the project's common worker/task infrastructure.

Use bounded concurrency.

Use cancellation or generation tokens.

Reject stale results.

Deliver worker results back to GUI-owned models through safe queued mechanisms.

Do not mutate GUI models directly from worker threads.

Do not scatter detached `std::thread` instances throughout features.

---

# Large Directory Requirements

The architecture must remain viable for very large directories.

Preserve the design goals for:

- incremental enumeration;
- first-screen responsiveness;
- virtualized views;
- visible-item thumbnail prioritization;
- bounded thumbnail caches;
- stale-result cancellation;
- lightweight model rows;
- in-memory search over loaded model state;
- deterministic sorting.

Do not introduce an implementation whose resource usage scales with one heavyweight UI object per filesystem entry.

---

# Persistence

Persistent state uses SQLite unless DESIGN.md states otherwise.

Persistent state may include:

- favorite;
- watched;
- last playback position;
- duration/resolution cache;
- thumbnail metadata;
- recents;
- bookmarks;
- per-folder sorting/view settings.

Use stable media identity according to DESIGN.md.

Use WAL where defined by the architecture.

Keep transactions short.

Use thread-safe connection ownership.

Do not share SQLite connection objects unsafely across threads.

---

# Build System

The project uses CMake and Ninja.

Preferred development commands:

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

Use the actual build directory/preset already established by the project if it differs.

Do not create competing build systems.

Do not add qmake.

Do not add Meson unless DESIGN.md explicitly changes the build strategy.

Prefer target-based CMake.

Keep compiler and linker settings scoped to targets.

Register QML modules/resources through the approved Qt/CMake mechanism.

---

# Build Directory

Use an out-of-tree build.

Default:

```text
/home/treshen/Projects/QG/build/
```

Do not place generated binaries or CMake intermediate files into `src/` or `qml/`.

Do not commit generated build outputs.

---

# Expected Project Areas

The exact tree may evolve, but expect responsibilities similar to:

```text
QG/
├── CMakeLists.txt
├── CMakePresets.json
├── cmake/
├── src/
│   ├── app/
│   ├── core/
│   ├── controllers/
│   ├── models/
│   ├── filesystem/
│   ├── media/
│   ├── playback/
│   ├── persistence/
│   ├── workers/
│   └── linux/
├── qml/
│   ├── components/
│   ├── views/
│   ├── delegates/
│   ├── playback/
│   └── theme/
├── tests/
├── resources/
├── packaging/
├── desktop/
├── debian/
├── DESIGN.md
├── dev-guide.md
├── GUIDE_STATE.md
├── DECISIONS.md
└── REQUIREMENTS_MATRIX.md
```

Do not force this exact tree if the existing project has already established an equivalent compatible layout.

Inspect first.

Extend compatible existing structure instead of rebuilding it.

---

# C++ Standards

Use C++20.

Use RAII.

Prefer:

- value semantics;
- explicit ownership;
- `std::unique_ptr` for exclusive heap ownership;
- Qt parent ownership where appropriate;
- scoped wrappers for C APIs;
- const-correctness;
- clear interfaces;
- narrowly scoped types.

Avoid:

- global mutable state;
- raw owning pointers;
- manual cleanup protocols where RAII is possible;
- unnecessary inheritance;
- premature generic/template frameworks.

---

# libmpv Ownership

Only the designated playback backend/manager layer should create and destroy the mpv handle.

Do not let QML or unrelated controllers create mpv instances.

Wrap libmpv resources so teardown is deterministic.

Observe mpv events without blocking the GUI thread.

Marshal event-driven state safely into Qt/C++ state.

Rendering integration must respect Qt Quick scene-graph threading and OpenGL context ownership.

---

# FFmpeg / Thumbnail / Probe Rules

Use the project's approved media extraction strategy.

Thumbnail generation and probing must be asynchronous and bounded.

Do not decode thumbnails on the GUI thread.

Cache expensive results according to DESIGN.md.

Validate cache entries against source modification metadata.

A failed probe or thumbnail must not break directory browsing.

Use failure markers/backoff where the design requires them.

---

# Search Rules

Search must preserve the design's interaction model.

For the MVP, search is driven by already-loaded directory data unless the design explicitly expands scope.

Do not trigger blocking disk traversal on each keystroke.

Keep input-to-result latency low.

Search presentation must derive from the same authoritative model state.

Do not create a second unrelated media inventory for search.

---

# Sorting Rules

Sorting must be deterministic.

Use one ordered source of truth.

Browser order, Reel order, playback-context order, and Previous/Next must agree.

Do not independently resort the same media list in multiple UI components.

Persist per-folder sort state where DESIGN.md requires it.

---

# File Operation Rules

Rename, move, copy, Trash, reveal, and open-with must preserve real filesystem semantics.

Use temporary directories in tests.

Never test destructive behavior on valuable user media.

Bulk operations must report partial failures coherently.

Do not present one modal dialog per failed file when the design requires a summarized result.

---

# Linux Integration

Use native Linux facilities where appropriate.

Potential integration includes:

- XDG base directories;
- GIO;
- GVfs;
- UDisks2;
- D-Bus;
- freedesktop Trash;
- `.desktop` metadata;
- AppStream metadata;
- system file manager reveal/open;
- mounted-volume discovery.

Do not recreate system services unnecessarily.

---

# Packaging

Primary release artifact:

```text
native Ubuntu .deb
```

The package should install appropriate:

- executable;
- shared resources;
- QML resources/modules as required;
- icons;
- `.desktop` file;
- AppStream metadata;
- runtime dependency declarations.

AppImage may be added as a secondary portable format if the project requires it.

Do not make Flatpak/Snap the primary distribution unless DESIGN.md changes.

---

# Testing

Testing is incremental.

Use focused tests for each development task.

Relevant tests may include:

```text
C++ unit tests
Qt Test
QML/UI tests
integration tests
filesystem tests using temporary directories
SQLite persistence tests
navigation tests
sort/search tests
model tests
cancellation/stale-result tests
playback manager tests
Stage/Cinema transition tests
keyboard/focus tests
large-directory performance tests
```

Do not claim a test passed without running it.

---

# Standard Validation

Where applicable:

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

Prefer focused build/test targets first.

At major checkpoints, run broader regression coverage.

At release readiness, run the complete required suite.

---

# Runtime Validation

When a prompt requires visual or interaction validation, building successfully is insufficient.

Validate the actual requested behavior.

Examples:

- directory navigation updates the visible model;
- card delegates display real data;
- search changes the correct presentation;
- playback starts;
- Stage/Cinema transitions preserve playback;
- Previous/Next follow folder order;
- file operations affect the real test filesystem;
- persistent state survives application restart/reopen.

Do not mark the prompt complete based only on compilation.

---

# Project-State Files

## GUIDE_STATE.md

`GUIDE_STATE.md` is the authoritative execution ledger for `dev-guide.md`.

Track at least:

- pipeline status;
- current prompt;
- prompt status;
- dependencies;
- files changed;
- tests/commands executed;
- validation results;
- blockers;
- completion notes.

Valid prompt states should remain simple and consistent, for example:

```text
PENDING
IN_PROGRESS
COMPLETE
BLOCKED
```

Do not mark COMPLETE until validation passes.

---

## DECISIONS.md

Use `DECISIONS.md` for real implementation or architecture decisions.

Record only meaningful decisions.

Each decision should include:

- identifier;
- decision;
- rationale;
- affected systems;
- relevant prompt/task where useful.

Do not use DECISIONS.md as a general activity log.

---

## REQUIREMENTS_MATRIX.md

Use `REQUIREMENTS_MATRIX.md` to connect product requirements to implementation and tests.

Track, where useful:

- requirement;
- source section;
- implementation component;
- tests;
- status.

Update it when requirement coverage changes.

Do not mark a requirement satisfied solely because a file or class exists.

---

# Development Guide Execution

When instructed to execute `dev-guide.md` autonomously:

1. read `GUIDE_STATE.md`;
2. identify the earliest incomplete prompt whose dependencies are COMPLETE;
3. read the full prompt;
4. inspect the listed files and direct dependencies;
5. inspect relevant DESIGN.md sections;
6. implement only that prompt;
7. create/update focused tests;
8. build;
9. run the prompt's validation gate;
10. fix task-caused failures;
11. update project-state documents;
12. continue to the next dependency-ready prompt only after successful validation.

Do not skip prompts.

Do not execute multiple unrelated prompts as one implementation batch.

Do not silently change prompt order.

---

# Checkpoint Prompts

Checkpoint prompts are validation milestones.

They should:

- run relevant tests;
- inspect integration boundaries;
- identify regressions;
- correct regressions caused by completed work;
- confirm architecture has not drifted;
- confirm project-state documents are accurate.

Do not use checkpoints to introduce major missing features that should have been implemented earlier.

---

# Final Integration

The final integration task exists to validate that completed systems work together.

It must not become a rescue phase for components that were never wired during their implementation phases.

Important boundaries must be connected incrementally during development.

---

# Release Validation

Release validation must cover the complete approved MVP, including:

- application startup;
- filesystem navigation;
- child-directory navigation;
- mounted storage;
- media filtering;
- Browse/Grid behavior;
- Manifest/search behavior;
- sorting;
- thumbnails;
- metadata;
- favorite/watched state;
- persistence;
- inline Stage playback;
- Previous/Next;
- Reel/folder playback context;
- Cinema/fullscreen;
- keyboard interactions;
- file operations;
- empty/loading/error states;
- large-directory responsiveness;
- stale-result cancellation;
- Linux desktop integration;
- `.desktop` launch;
- `.deb` package build/install validation where required.

Do not declare release readiness while a required acceptance item is failing or untested.

---

# Scope Guard

For every implementation task:

- do not implement unrelated future features;
- do not redesign unrelated modules;
- do not change architecture without necessity;
- do not replace working compatible systems;
- do not modify unrelated files merely for cleanup;
- do not introduce speculative abstractions.

If tightly coupled supporting work is required to keep the project compiling or valid, keep it minimal and document it.

---

# Inspect Before Editing

Before editing a file:

- inspect the current version;
- inspect directly related headers/sources/QML;
- inspect callers/consumers where relevant;
- inspect relevant tests;
- inspect CMake target ownership if build integration changes.

Do not assume class/function names before confirming them.

Use existing APIs where compatible.

Do not create duplicate abstractions.

---

# Regression Control

When changing shared systems, run focused related regressions.

Examples:

Playback change:

```text
playback manager tests
video surface tests
Stage/Cinema tests
keyboard playback tests
folder-context Previous/Next tests
```

Model change:

```text
model tests
sorting tests
search tests
view integration tests
```

Filesystem change:

```text
enumeration tests
navigation tests
temporary-directory file-operation tests
mount/disconnect tests where applicable
```

Do not run the full suite after every trivial edit unless needed.

Run the full required suite at checkpoints and release validation.

---

# Blocker Handling

If a task cannot be completed:

1. verify the blocker;
2. capture the exact command and output;
3. determine whether it is:
   - code;
   - dependency;
   - environment;
   - driver;
   - API;
   - permission;
   - design contradiction;
4. attempt reasonable fixes within task scope;
5. if still blocked, mark the current prompt BLOCKED in `GUIDE_STATE.md`.

Record:

- prompt number;
- blocker;
- evidence;
- attempted fixes;
- required next action.

Do not continue to dependent prompts.

---

# Git Safety

Always inspect:

```bash
git status --short
```

before broad edits.

Do not discard existing user changes.

Do not use destructive commands such as:

```text
git reset --hard
git clean -fd
git checkout -- .
```

unless explicitly instructed.

Do not rewrite history.

Do not commit unless the active workflow requires commits.

---

# Completion Reporting

After completing a development prompt, report:

- prompt completed;
- files changed;
- implementation summary;
- tests/commands run;
- results;
- project-state files updated;
- blockers, if any.

Keep the report concise.

Do not claim success without actual validation.

---

# Autonomous Stop Conditions

When executing the development pipeline automatically, continue normally until one of these is true:

1. all development prompts and release validation are COMPLETE;
2. the active prompt is genuinely BLOCKED;
3. implementation requires unavailable credentials or user authorization;
4. the authoritative design contains an unresolved contradiction that cannot be handled within the current task;
5. continuing would require destructive action outside the approved workflow.

Normal successful completion of a prompt is not a reason to stop.

Update persistent state and continue to the next dependency-ready prompt.
