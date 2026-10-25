---
name: qg-autobuilder
description: Build QG from its ordered development guide.
version: 1.0.0
author: Gleshen
platforms: [linux]
metadata:
  hermes:
    tags: [qg, cpp, qt6, qt-quick, qml, libmpv, opengl, x11, cmake, autonomous-development]
    category: software-development
    related_skills: [hermes-agent, requesting-code-review]
    requires_toolsets: [terminal]
---

# QG Autobuilder

## Purpose

Autonomously execute the QG development pipeline in `/home/treshen/Projects/QG/`.

This skill is an implementation orchestrator. It does not redesign QG and it
does not replace the development guide.

Its normal cycle is:

```text
read project control documents
↓
select earliest dependency-ready incomplete prompt
↓
inspect relevant implementation and tests
↓
implement only that prompt
↓
build
↓
run focused tests and validation
↓
repair current-task failures
↓
update persistent project state
↓
continue
```

The skill must be resumable across fresh Hermes sessions without relying on
chat history.

---

## When to Use

Use this skill when the user asks Hermes to:

- build QG;
- continue building QG;
- execute `dev-guide.md`;
- resume QG development;
- execute the next QG prompt;
- autonomously implement QG;
- complete the QG MVP;
- run a QG checkpoint;
- perform QG release validation.

Manual invocation:

```text
/qg-autobuilder
```

Optional modes:

```text
/qg-autobuilder auto
/qg-autobuilder next
/qg-autobuilder checkpoint
/qg-autobuilder validate
/qg-autobuilder status
```

Default mode: `auto`.

---

# Operating Modes

## AUTO

Execute the earliest dependency-ready incomplete prompt.

When it passes validation:

1. update project state;
2. check whether a checkpoint is now required;
3. continue automatically.

Stop only at a defined stop condition.

Do not ask for routine confirmation between validated prompts.

---

## NEXT

Execute exactly one dependency-ready incomplete prompt.

Validate it, update project state, report the result, and stop.

Do not start the following prompt.

---

## CHECKPOINT

Execute the next required checkpoint from `dev-guide.md`.

A checkpoint validates completed work. It must not become a substitute for
missing feature implementation.

---

## VALIDATE

Do not begin a new implementation prompt.

Validate the current project stage and reconcile persistent state with actual
build/test/runtime evidence.

If implementation and checkpoints are complete, execute release validation as
defined by `dev-guide.md`.

---

## STATUS

Do not edit application source.

Read project control files and report:

- pipeline status;
- current prompt;
- next prompt;
- completed prompt count;
- pending prompt count;
- checkpoint status;
- blockers;
- last recorded validation;
- release-validation status.

---

# Project Root

Expected root:

```text
/home/treshen/Projects/QG/
```

Operate from this directory unless the project itself has been relocated and
the user explicitly provides the new path.

---

# Authoritative Project Documents

Expected control documents:

```text
DESIGN.md
dev-guide.md
.hermes.md
GUIDE_STATE.md
DECISIONS.md
REQUIREMENTS_MATRIX.md
```

They have distinct responsibilities.

## DESIGN.md

Authoritative product and architecture specification.

It defines:

- product behavior;
- UI behavior;
- technical architecture;
- threading expectations;
- playback behavior;
- filesystem semantics;
- performance requirements;
- acceptance criteria.

Do not contradict it silently.

## dev-guide.md

Authoritative ordered implementation pipeline.

It defines:

- prompt identifiers;
- dependency order;
- task scope;
- inspect-first requirements;
- implementation requirements;
- tests;
- validation gates;
- checkpoints;
- final integration;
- release validation.

Do not skip prompts because a later prompt seems easier.

## .hermes.md

Project-specific operating rules for Hermes.

Read and obey it before implementation.

## GUIDE_STATE.md

Persistent execution ledger.

It records:

- prompt status;
- checkpoint status;
- active prompt;
- blockers;
- previous validation evidence.

Use it instead of conversational memory to determine progress.

## DECISIONS.md

Established architectural and implementation decisions.

Inspect it before making a choice that could reverse earlier work.

## REQUIREMENTS_MATRIX.md

Maps design requirements to implementation and tests.

Use it to prevent false completion when required behavior remains unverified.

---

# Instruction Priority

Resolve conflicts in this order:

```text
1. DESIGN.md
2. compatible existing working implementation
3. DECISIONS.md
4. dev-guide.md
5. GUIDE_STATE.md execution state
6. current prompt
7. implementation preference
```

If a lower-priority source conflicts with a higher-priority source, follow the
higher-priority source and record the discrepancy when relevant.

---

# Approved Stack

The project uses:

```text
C++20
Qt 6
Qt Quick / QML
Qt Quick Controls 2
QAbstractItemModel / QAbstractListModel
Qt Quick Scene Graph
OpenGL
Ubuntu Linux
X11 / xcb
libmpv C API
mpv_render_context
SQLite
POSIX/Linux filesystem APIs
GIO / GVfs
UDisks2 / D-Bus where needed
FFmpeg / libavformat / libavcodec where needed
CMake
Ninja
CTest
Qt Test
native Ubuntu .deb packaging
```

Do not replace this architecture with:

- Python;
- PyQt;
- PySide;
- Electron;
- Chromium;
- browser UI;
- another primary UI framework;
- a second unrelated media framework.

---

# Architecture Boundary

Preserve:

```text
QML presentation
↓
C++ controllers/coordinators
↓
C++ models/services
↓
filesystem / persistence / media / playback / Linux integration
```

QML owns presentation.

C++ owns authoritative application behavior.

Do not put filesystem access, persistence ownership, playback ownership, or
large-data business logic into QML JavaScript.

---

# Playback Architecture

Preserve:

```text
one libmpv handle
+
one active playback session
+
one authoritative ordered folder playback context
```

Primary rendering path:

```text
libmpv C API
↓
mpv_render_context
↓
custom Qt Quick video item
↓
Qt Quick scene graph
↓
OpenGL
↓
X11/xcb
```

Do not make `--wid` child-window embedding the primary implementation.

Do not create separate playback engines for Stage and Cinema.

Stage ↔ Cinema must preserve the current playback session.

---

# Filesystem Principle

The real Linux filesystem is authoritative.

QG may persist metadata about media but must not replace real folders/files
with a proprietary imported library.

Preserve:

- real paths;
- child directory navigation;
- mounted-volume behavior;
- Linux filename semantics;
- native Trash semantics;
- native reveal/open behavior where supported.

---

# Tool Use

Prefer native Hermes tools where available.

Use:

- `search_files` to locate files, symbols, and project structure;
- `read_file` to inspect current content;
- `patch` for focused edits;
- the available file-writing mechanism for new files or complete rewrites;
- `terminal` for commands that must actually execute, including CMake, Ninja,
  CTest, application runs, dependency probes, and packaging commands.

Do not use shell commands merely to emulate native Hermes file tools.

Do not run destructive terminal commands for convenience.

---

# Session Start Protocol

At the beginning of AUTO, NEXT, CHECKPOINT, or VALIDATE mode:

1. move to the project root;
2. inspect `.hermes.md`;
3. inspect `GUIDE_STATE.md`;
4. inspect `DECISIONS.md`;
5. inspect the `dev-guide.md` prompt index;
6. inspect relevant `DESIGN.md` sections;
7. inspect `REQUIREMENTS_MATRIX.md` when requirement coverage matters;
8. inspect Git state if this is a Git repository;
9. determine the correct next task.

Do not read the entire source tree blindly.

Read implementation files only after identifying the current task.

---

# Git Preflight

If the project is a Git repository, run:

```bash
git status --short
```

before broad edits.

Never discard pre-existing user changes.

Do not run:

```text
git reset --hard
git clean -fd
git checkout -- .
git restore .
```

unless the user explicitly authorizes it.

Do not rewrite Git history.

Do not commit automatically unless required by `dev-guide.md` or explicitly
requested by the user.

---

# Missing Control Documents

## Missing DESIGN.md

Hard blocker.

Do not implement the application without the authoritative specification.

Record:

```text
BLOCKED: DESIGN.md is missing.
```

## Missing dev-guide.md

Hard blocker for autonomous execution.

Do not invent a replacement implementation pipeline.

Record:

```text
BLOCKED: dev-guide.md is missing.
```

## Missing .hermes.md

Continue only if the current skill and `DESIGN.md` provide enough constraints.

Do not create `.hermes.md` unless instructed.

## Missing GUIDE_STATE.md

Create it before implementing Prompt 01.

Derive the prompt list and dependencies from `dev-guide.md`.

Recommended structure:

```markdown
# QG Development Guide State

## Pipeline
Status: NOT_STARTED
Current Prompt: None
Last Updated: YYYY-MM-DD

## Prompt State
| Prompt | Task | Depends On | Status |
|---|---|---|---|

## Checkpoints
| Checkpoint | Status |
|---|---|

## Last Validation
None.

## Blockers
None.
```

Allowed task states:

```text
PENDING
IN_PROGRESS
COMPLETE
BLOCKED
```

Existing code is not proof that a prompt is COMPLETE.

Validation is required.

## Missing DECISIONS.md

Create:

```markdown
# QG Engineering Decisions

No recorded decisions yet.
```

## Missing REQUIREMENTS_MATRIX.md

Create a matrix based on `DESIGN.md` acceptance requirements:

```markdown
# QG Requirements Matrix

| Requirement | Source | Implementation | Tests | Status |
|---|---|---|---|---|
```

Suggested statuses:

```text
PLANNED
PARTIAL
IMPLEMENTED
VERIFIED
BLOCKED
```

`VERIFIED` requires actual validation evidence.

---

# Development Guide Parsing

For each development prompt determine:

- identifier;
- title;
- dependencies;
- primary area;
- goal;
- inspect-first files;
- implementation requirements;
- constraints;
- tests;
- validation gate;
- completion criteria.

Do not execute a task based only on the prompt title.

---

# Dependency Resolution

A prompt is dependency-ready only if:

1. every listed dependency is COMPLETE;
2. every mandatory checkpoint before it is COMPLETE;
3. no active blocker prevents it;
4. the previous milestone is valid enough for the prompt to begin.

Choose the earliest dependency-ready incomplete prompt.

Do not reorder the guide for convenience.

---

# State Reconciliation

Before new implementation, compare `GUIDE_STATE.md` with the actual repository.

If state says COMPLETE but implementation is missing or validation evidence is
invalid:

- inspect the discrepancy;
- rerun the prompt's validation;
- downgrade the prompt if necessary.

If implementation already exists for a PENDING prompt:

- inspect it;
- run required validation;
- mark COMPLETE only when completion criteria are truly met.

This allows the skill to resume partially implemented repositories without
duplicating working code.

---

# Prompt Start Protocol

Before editing for the selected prompt:

1. mark it `IN_PROGRESS`;
2. record the prompt identifier/title;
3. read the complete prompt;
4. read relevant `DESIGN.md` sections;
5. inspect every `Inspect First` file;
6. inspect direct callers/dependencies;
7. inspect relevant tests;
8. inspect relevant CMake targets if build integration may change.

Then implement.

---

# Inspect First Rule

Use the actual repository.

Before creating a class, service, controller, model, QML component, helper, or
test fixture:

- search for an existing equivalent;
- inspect its API;
- inspect consumers;
- inspect tests.

Do not create duplicate abstractions because their existence was overlooked.

---

# One-Prompt Scope Rule

Implement one development prompt as one coherent unit.

Tightly coupled supporting work is allowed only when required to keep the
project valid.

Do not implement unrelated future prompts.

Examples of drift to avoid:

```text
filesystem prompt → implementing playback
search prompt → replacing persistence
video-card prompt → adding file operations
playback prompt → redesigning directory enumeration
```

---

# Editing Strategy

Prefer the smallest architecture-compatible change that satisfies the prompt.

Before creating a new abstraction ask:

1. does a compatible one already exist?
2. can an existing component be safely extended?
3. does `DESIGN.md` require this responsibility?
4. does the current prompt need it now?

Do not build speculative frameworks for future work.

Do not refactor unrelated working code for aesthetics.

---

# Build Rules

Use the project's established CMake/Ninja workflow.

Default commands when no project-specific preset overrides them:

```bash
cmake -S . -B build -G Ninja
cmake --build build
```

If CMake presets already exist, prefer the project's documented preset.

Do not create a competing build system.

Do not add qmake.

Do not add Meson unless the authoritative architecture changes.

Use target-based CMake.

---

# Compilation Failure Protocol

When compilation or linking fails:

1. read the complete relevant failure;
2. identify the first causal error;
3. inspect referenced code/build definitions;
4. make the smallest correct fix;
5. rebuild the focused target;
6. repeat until valid or genuinely blocked.

Do not suppress warnings/errors to hide defects.

Do not remove required functionality merely to obtain a green build.

---

# Per-Prompt Test Strategy

Run the tests specified by the prompt.

If coverage is insufficient, add the smallest focused tests that prove the
requested behavior.

Possible test categories:

- C++ unit tests;
- Qt Test;
- QML UI tests;
- model tests;
- temporary-directory filesystem tests;
- SQLite tests;
- navigation tests;
- search tests;
- sorting tests;
- worker/cancellation tests;
- stale-result tests;
- playback-manager tests;
- playback adapter fakes;
- Stage/Cinema lifecycle tests;
- keyboard/focus tests.

Start focused.

Run broader regression suites at checkpoints.

---

# Filesystem Test Safety

Never perform destructive tests on valuable user media.

Tests for:

- rename;
- move;
- copy;
- Trash;
- delete;
- collision handling;
- permission handling;

must use temporary directories and disposable fixtures.

If a test appears to require real user data, redesign the test.

---

# Test Failure Protocol

If a focused test fails:

1. identify whether it is pre-existing or caused by the current task;
2. inspect the exact failure;
3. fix regressions caused by the current task;
4. rerun the focused test;
5. run related regressions if shared behavior changed.

Do not rewrite a correct test merely to match broken code.

If a test conflicts with `DESIGN.md`, resolve according to source priority.

---

# Runtime Validation

Compilation does not prove runtime behavior.

When required by the prompt, validate the actual application behavior.

Examples:

- QML loads;
- directory navigation changes visible content;
- C++ model roles bind correctly;
- search transitions to the intended presentation;
- playback starts;
- Stage/Cinema transitions preserve playback;
- Previous/Next follow folder order;
- persistent state survives reopen.

Do not claim runtime success without executing the relevant validation.

---

# QML Rules

When editing QML:

- keep QML declarative;
- keep domain logic in C++;
- avoid large imperative JavaScript blocks;
- avoid duplicated authoritative state;
- keep delegates lightweight;
- preserve virtualized views;
- watch for binding loops;
- avoid expensive object creation per media row;
- verify exposed C++ properties/signals/models exist.

Do not use QML JavaScript to bypass missing C++ architecture.

---

# Model Rules

For `QAbstractItemModel` / `QAbstractListModel` work, preserve:

- stable roles;
- valid row insertion/removal/reset semantics;
- GUI-thread ownership;
- deterministic ordering;
- safe async updates;
- selection/focus identity where required;
- search/sort consistency.

Workers must not mutate GUI models directly.

---

# Background Work Rules

Use the project's common worker architecture.

Preserve:

```text
bounded worker execution
+
task/generation identity
+
cancellation
+
safe queued result delivery
+
stale-result rejection
```

Do not scatter detached `std::thread` instances across features.

Do not add one independent thread pool per feature.

---

# Stale Result Rule

Before applying async work, validate relevant identity:

- task generation;
- current folder/path;
- media identity;
- model membership;
- source mtime/size where required.

Drop stale results.

Do not flash results from an old folder into the active view.

---

# Performance Rule

Performance requirements are part of correctness.

Preserve:

- non-blocking GUI thread;
- virtualized QML views;
- lightweight C++ model rows;
- bounded thumbnail memory;
- incremental enumeration;
- visible-item work priority;
- bounded probe concurrency;
- task cancellation;
- one heavy playback engine.

Do not knowingly introduce expensive GUI-thread work into hot paths.

---

# Search Rule

Search must derive from authoritative model state.

Do not create an independent media inventory.

Do not perform blocking filesystem traversal on every keystroke.

Keep Browse, Manifest, Reel, and playback context coherent.

---

# Sorting Rule

Maintain one deterministic order source.

Browser order, Reel order, playback position, Previous, and Next must agree.

Do not independently sort copies of folder media in QML.

---

# Playback Prompt Rules

For playback work:

- preserve one mpv handle;
- keep creation/destruction in the designated backend/manager layer;
- keep mpv ownership out of QML;
- keep rendering integration localized;
- preserve Stage/Cinema continuity;
- preserve ordered folder context;
- keep event callbacks non-blocking;
- respect Qt Quick render-thread/OpenGL ownership.

Never instantiate a player per video card.

---

# Scene Graph Safety

For custom video rendering:

- separate GUI-thread state from render-thread resources;
- use Qt-supported scene-graph lifecycle hooks;
- initialize and release graphics/mpv render resources at valid times;
- avoid accessing GUI-thread-only mutable QObjects from the render thread;
- handle scene-graph invalidation cleanly;
- avoid leaks and double destruction on shutdown/window recreation.

If Qt API behavior is uncertain, inspect the project's installed/current Qt
documentation or headers instead of guessing.

---

# Persistence Rules

For SQLite work:

- preserve schema/migration compatibility;
- keep transactions short;
- use approved WAL behavior;
- respect thread-owned connection architecture;
- do not share unsafe connection objects across threads;
- validate reopen behavior when persistence is part of the prompt.

Application metadata remains separate from the media files.

---

# Linux Integration Rules

Prefer native Linux services when required:

- XDG base directories;
- GIO/GVfs;
- freedesktop Trash;
- UDisks2/D-Bus;
- desktop files;
- AppStream metadata;
- system default application launching.

Do not recreate native platform services without need.

---

# Prompt Validation Gate

A development prompt is COMPLETE only when all applicable conditions are true:

1. required implementation exists;
2. architecture boundaries remain valid;
3. affected targets build;
4. focused tests pass;
5. prompt-specific validation passes;
6. required runtime validation passes;
7. no known current-task regression remains;
8. persistent project state is updated accurately.

Code written is not completion.

---

# Marking Prompt Complete

After validation, update `GUIDE_STATE.md`.

Record concisely:

- prompt identifier/title;
- status `COMPLETE`;
- files changed;
- build/test commands;
- validation result;
- significant notes;
- requirement coverage affected.

Do not paste huge logs into state files.

---

# Requirements Matrix Updates

When requirement coverage changes, update `REQUIREMENTS_MATRIX.md`.

A requirement may progress through:

```text
PLANNED
PARTIAL
IMPLEMENTED
VERIFIED
BLOCKED
```

Do not mark `VERIFIED` merely because implementation exists.

Actual validation is required.

---

# Decision Log Updates

Update `DECISIONS.md` only for meaningful architecture/implementation choices.

A decision should include:

- decision;
- rationale;
- affected components;
- relevant constraints;
- task/prompt reference where useful.

Do not use the decision log as a routine activity log.

---

# Checkpoint Procedure

At a checkpoint:

1. run its defined test set;
2. inspect completed integration boundaries;
3. compare implementation to `DESIGN.md`;
4. audit architecture drift;
5. reconcile state files;
6. repair regressions caused by completed work;
7. mark the checkpoint COMPLETE only after validation.

Do not use checkpoints to implement entire missing subsystems.

If a major feature is absent, return to the relevant earlier prompt.

---

# Architecture Drift Audit

At checkpoints look for:

- filesystem access in QML;
- business logic accumulated in root QML;
- duplicate folder/media inventories;
- multiple mpv handles;
- separate Stage and Cinema players;
- duplicated sort/search sources;
- detached worker threads;
- GUI-thread media probing;
- unbounded thumbnail scheduling;
- permanent heavyweight QML object per file;
- worker-thread GUI model mutation;
- persistence logic inside UI components;
- competing build systems.

Repair drift before advancing.

---

# Final Integration Rule

Final integration validates systems already implemented and incrementally wired.

It must not become a rescue phase for systems that were never connected.

If a major subsystem is missing, reopen the relevant earlier prompt instead.

---

# Release Validation Eligibility

Release validation may begin only when:

- all required development prompts are COMPLETE;
- all mandatory checkpoints are COMPLETE;
- final integration is COMPLETE;
- no active blocker exists.

Otherwise return to the earliest incomplete requirement.

---

# Release Validation

Execute the exact release-validation prompt from `dev-guide.md`.

At minimum cover all applicable requirements for:

- configure/build;
- full required tests;
- application startup;
- filesystem navigation;
- child directories;
- mounted storage;
- media filtering;
- Browse;
- folder/video delegates;
- thumbnails;
- metadata;
- Manifest/search;
- sorting;
- favorite/watched state;
- persistence;
- Stage playback;
- transport controls;
- Previous/Next;
- Reel/folder playback context;
- Cinema/fullscreen;
- Stage ↔ Cinema continuity;
- keyboard interaction;
- file operations;
- loading/empty/error states;
- stale-result handling;
- large-directory responsiveness;
- Linux integration;
- launcher metadata;
- `.deb` packaging where required.

Do not declare release readiness with a required failure.

---

# Clean Release Build

At release readiness, perform a clean build using a disposable build directory
or the project's release preset.

Example when no release preset exists:

```bash
cmake -S . -B build-release-check -G Ninja
cmake --build build-release-check
ctest --test-dir build-release-check --output-on-failure
```

Do not destroy the user's normal build directory unnecessarily.

---

# Packaging Validation

When packaging is in scope:

- build the `.deb`;
- inspect package metadata;
- verify runtime dependencies;
- verify installed desktop metadata/resources as far as safely possible;
- test install/uninstall only in an approved environment.

If root privileges are required and unavailable, record exactly which
validation remains.

Do not fake successful package validation.

---

# Blocker Definition

A blocker is an external or architectural condition that cannot be safely
resolved within current task scope.

Examples:

- missing authoritative document;
- unavailable required dependency;
- unavailable credential;
- required hardware absent;
- unsupported system API/driver;
- permission needing user action;
- irreconcilable design contradiction;
- destructive action requiring explicit approval.

A compilation error is not automatically a blocker.

A failing test is not automatically a blocker.

Investigate first.

---

# Blocker Protocol

Before marking BLOCKED:

1. reproduce the problem;
2. capture the actual failure;
3. inspect relevant code/environment;
4. attempt reasonable in-scope fixes;
5. identify the external requirement.

Then update `GUIDE_STATE.md` with:

```text
Status: BLOCKED
Prompt: XX
Blocker: ...
Evidence: ...
Attempts: ...
Required to continue: ...
```

Do not continue to dependent prompts.

AUTO mode stops.

---

# Design Contradiction Protocol

If implementation and `DESIGN.md` conflict:

1. confirm the design text;
2. inspect actual implementation;
3. inspect `DECISIONS.md`;
4. determine whether a documented decision resolves the divergence;
5. otherwise prefer the authoritative design.

If resolution requires redesign beyond the current prompt, mark BLOCKED and
describe the conflict.

Do not invent an undocumented third architecture.

---

# Pre-existing User Changes

Uncommitted changes are not permission to overwrite them.

When current work overlaps changed files:

- inspect the current diff;
- preserve unrelated edits;
- integrate carefully.

If overlapping changes cannot be reconciled confidently, stop and report the
conflict.

---

# Failure Recovery

After a failed implementation attempt:

- retain useful diagnostics;
- do not stack speculative fixes;
- undo only clearly current-task changes when safe;
- never undo pre-existing user changes;
- return to root-cause analysis.

Prefer one understood repair over layered workarounds.

---

# Context Management

Persistent project files are long-term memory.

After each completed prompt:

1. update `GUIDE_STATE.md`;
2. update `DECISIONS.md` if needed;
3. update `REQUIREMENTS_MATRIX.md` if coverage changed.

Before each new prompt, reconstruct needed context from these files and actual
source.

Do not rely on remembering old chat messages.

---

# Fresh Session Resume

In a new Hermes session:

1. enter project root;
2. read `.hermes.md`;
3. read `GUIDE_STATE.md`;
4. read `DECISIONS.md`;
5. inspect `dev-guide.md`;
6. inspect relevant `DESIGN.md`;
7. identify earliest dependency-ready incomplete work;
8. continue according to requested mode.

No prior conversation should be required.

---

# AUTO Loop

Repeat:

```text
READ STATE
↓
SELECT EARLIEST READY ITEM
↓
MARK IN_PROGRESS
↓
INSPECT
↓
IMPLEMENT
↓
BUILD
↓
TEST
↓
VALIDATE
↓
UPDATE STATE
↓
RUN CHECKPOINT IF REQUIRED
↓
CONTINUE
```

Successful completion of a prompt is not a stop condition in AUTO mode.

---

# Mandatory Checkpoint Handling

If a required checkpoint is next, execute it before later implementation
prompts.

A pending required checkpoint blocks downstream tasks.

---

# AUTO Stop Conditions

Stop when:

1. all required implementation, checkpoints, final integration, and release
   validation are complete;
2. the active prompt is genuinely BLOCKED;
3. required credentials/authorization are unavailable;
4. a design contradiction requires user resolution;
5. continuing requires destructive action outside approved scope;
6. repository conflicts prevent safe modification;
7. the user explicitly stops execution.

Do not stop merely because a prompt passed.

---

# Completion State

When the full pipeline passes, set `GUIDE_STATE.md` to reflect:

```text
Pipeline Status: COMPLETE
Current Prompt: None
Blockers: None
Release Validation: PASSED
```

Ensure the requirements matrix reflects verified required coverage.

Do not mark COMPLETE while required entries remain:

```text
PLANNED
PARTIAL
IMPLEMENTED but unverified
BLOCKED
```

---

# User-Facing Progress

Keep progress reports concise.

Typical message:

```text
Prompt 18 complete — Implement thumbnail cache invalidation
Build: PASS
Tests: PASS (7/7)
Validation: PASS
Next: Prompt 19 — Connect thumbnail results to FolderModel
```

Do not flood the user with complete compiler/test logs unless needed to explain
a failure.

---

# Final Report

When complete, report:

```text
QG pipeline complete.

Development prompts: X/X complete
Checkpoints: X/X complete
Final integration: PASS
Release validation: PASS
Build: PASS
Tests: PASS
Package validation: PASS or explicit remaining limitation
```

Also identify:

- final executable;
- `.deb` artifact when applicable;
- any intentionally deferred non-MVP work.

---

# STATUS Output

For STATUS mode use:

```markdown
# QG Autobuilder Status

Pipeline: ...
Current: ...
Next: ...

## Progress
Development prompts: X/Y
Checkpoints: X/Y
Final integration: ...
Release validation: ...

## Blockers
...

## Last Validation
...
```

Do not edit application source in STATUS mode.

---

# Pitfalls

## Trusting guide state without verification

If recorded state conflicts with actual code/tests, validate and reconcile it.

## Coding ahead

Do not implement later prompts before dependencies and checkpoints.

## Duplicate systems

Search the repository before creating new models/controllers/services.

## QML domain logic

Move authoritative behavior into C++ rather than growing imperative QML
JavaScript.

## Multiple players

Keep one libmpv playback engine.

## GUI-thread blocking

Use established worker infrastructure.

## Stale async results

Validate generation/path/media/source before applying.

## Unsafe filesystem tests

Use temporary fixtures.

## Chat-memory dependence

Use project-state files.

## Advancing after failure

Keep the current item IN_PROGRESS or BLOCKED until its validation passes.

## Concurrent builder sessions

Two agents editing the same repository corrupt progress even when every individual edit is valid.

Detect: hash the working tree twice ~15 s apart while you are not writing. If the hash changes, another writer is active. Cross-check file mtimes against your own writes, and treat an unexplained advance in `GUIDE_STATE.md` as a second writer.

Action: stop AUTO execution, do not overwrite, record the conflict in `GUIDE_STATE.md`, and report. Only one builder session may run per project.

## Qt Test needs an application object for queued events

Use `QTEST_GUILESS_MAIN` (creates `QCoreApplication`) whenever a test relies on `QMetaObject::invokeMethod(..., Qt::QueuedConnection)`, `QTRY_*` event pumping, or any queued signal. `QTEST_APPLESS_MAIN` creates no application object, so `QCoreApplication::instance()` is null, posted events are never delivered, and the test fails with a delivered flag stuck false even though `invokeMethod` returned true. Debug it by logging `QCoreApplication::instance()` from the posting thread.

---

# Verification of Skill Behavior

The skill is functioning correctly when it:

- reads persistent state before coding;
- follows development-guide dependency order;
- implements one focused prompt at a time;
- preserves `DESIGN.md`;
- validates before marking completion;
- updates state documents;
- automatically continues in AUTO mode;
- stops on genuine blockers;
- resumes correctly in a fresh session.

---

# Minimal Invocation Semantics

## `/qg-autobuilder auto`

Interpret as:

> Resume QG from persistent project state. Execute the earliest
> dependency-ready incomplete prompt, validate it, update state, and continue
> automatically through required checkpoints until release validation passes or
> a genuine blocker is reached.

## `/qg-autobuilder next`

Interpret as:

> Execute exactly one dependency-ready incomplete QG prompt, validate it, update
> state, report the result, and stop.

## `/qg-autobuilder checkpoint`

Interpret as:

> Execute the next required QG checkpoint without beginning unrelated future
> feature work.

## `/qg-autobuilder validate`

Interpret as:

> Validate current QG implementation state and reconcile project-state files
> without starting a new feature prompt.

## `/qg-autobuilder status`

Interpret as:

> Report current QG pipeline status without editing application source.

---

# Non-Negotiable Rules

1. `DESIGN.md` is authoritative.
2. `dev-guide.md` determines implementation order.
3. `GUIDE_STATE.md` records execution progress.
4. One prompt is implemented and validated at a time.
5. Dependencies and checkpoints are not skipped.
6. Validation is never claimed without execution.
7. QG is not redesigned during implementation.
8. The C++20 + Qt 6 Quick/QML architecture is preserved.
9. One heavy libmpv playback session is preserved.
10. Stage and Cinema do not use separate players.
11. Expensive work does not block the GUI thread.
12. Worker threads do not directly mutate GUI models.
13. Destructive tests do not use real user media.
14. Pre-existing user changes are preserved.
15. Genuine blockers stop dependent execution.
16. In AUTO mode, a passing prompt means continue.

---

# End State

This skill succeeds only when the ordered QG development pipeline has produced
an application that satisfies the required `DESIGN.md` acceptance criteria and
the release-validation gate has passed.

Until then, the project remains in progress.
