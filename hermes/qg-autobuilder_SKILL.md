---
name: qg-autobuilder
description: Execute exactly one validated unit of the existing QG development pipeline from persistent project state.
version: 2.0.0
platforms: [linux]
metadata:
  hermes:
    tags: [qg, cpp, qt6, qt-quick, qml, libmpv, x11, autonomous-development]
    category: software-development
    requires_toolsets: [terminal]
---

# QG Autobuilder v2

## Purpose

Operate the existing QG project at:

```text
/home/treshen/Projects/QG/
```

This skill is the **atomic executor** for the QG development pipeline.

Continuous autonomous execution is owned by the external project runner:

```text
/home/treshen/Projects/QG/hermes/qg-pipeline.py
```

One Hermes invocation executes **one work item**, validates it, persists state,
then exits. The external runner launches the next fresh Hermes invocation.

```text
qg-pipeline.py
    ↓
fresh Hermes invocation
    ↓
qg-autobuilder
    ↓
one work item
    ↓
build + test + validate
    ↓
persist project state
    ↓
exit
    ↓
qg-pipeline.py invokes Hermes again
```

## Project State

The project is already in progress.

Treat the repository/filesystem as an existing implementation. Never infer that
the project is blank from the state documents.

Persistent control files:

```text
DESIGN.md
dev-guide.md
GUIDE_STATE.md
DECISIONS.md
REQUIREMENTS_MATRIX.md
.hermes.md
```

## Authority

Use this order when resolving conflicts:

```text
1. DESIGN.md
2. validated behavior of the actual current implementation
3. DECISIONS.md
4. dev-guide.md
5. GUIDE_STATE.md
6. current runner/manual instruction
```

`GUIDE_STATE.md` is an execution ledger, not proof that code exists.
Actual source, build output, tests, and runtime validation are evidence.

## Work Item Types

A work item is one of:

```text
development prompt
mandatory checkpoint
FI-01 final integration
RV-01 release validation
```

The next item is determined from `dev-guide.md` plus `GUIDE_STATE.md`.
Mandatory checkpoints execute before downstream prompts that depend on the
completed milestone.

After all development prompts/checkpoints:

```text
FI-01
↓
RV-01
↓
Pipeline Status: COMPLETE
```

## Allowed Execution States

```text
PENDING
IN_PROGRESS
COMPLETE
BLOCKED
```

Pipeline states:

```text
READY
RUNNING
BLOCKED
COMPLETE
```

## Atomic Turn Protocol

For a runner-driven implementation turn:

1. Confirm working directory is `/home/treshen/Projects/QG/`.
2. Read `.hermes.md`.
3. Read `GUIDE_STATE.md`.
4. Read `DECISIONS.md`.
5. Read `REQUIREMENTS_MATRIX.md`.
6. Locate the earliest dependency-ready incomplete work item.
7. Read that item from `dev-guide.md` in full.
8. Read only the relevant `DESIGN.md` sections required for that item.
9. Inspect the actual existing implementation, direct dependencies, tests, and build definitions.
10. Reconcile obvious state mismatch before editing.
11. Mark the selected item `IN_PROGRESS`.
12. Implement exactly that item.
13. Build the affected target(s).
14. Run the item's focused tests.
15. Run its validation gate.
16. Repair failures caused by the current item and repeat validation.
17. On success, mark the item `COMPLETE` and record concise evidence.
18. Update `DECISIONS.md` only when a real durable decision was made.
19. Update `REQUIREMENTS_MATRIX.md` only where implementation or verification status materially changed.
20. Stop the Hermes invocation.

## Selection Rules

Choose the earliest incomplete item whose dependencies are COMPLETE.

A development prompt is ready only when:

- every declared prompt dependency is COMPLETE;
- every mandatory prior checkpoint is COMPLETE;
- no blocker prevents the work.

When a checkpoint becomes due, the checkpoint is the next work item.
After the last checkpoint, execute FI-01 before RV-01.

## State Reconciliation

Manual development occurred before v2. Recorded state can therefore be stale.

When state disagrees with the actual project:

- inspect the implementation;
- inspect tests;
- run focused validation where evidence is insufficient;
- correct the recorded status;
- record a concise reconciliation note.

Existing code alone does not prove COMPLETE.
COMPLETE requires the item's completion criteria and validation gate.

## Scope Discipline

The current work item defines the scope.
Inspect direct dependencies when necessary for correctness.
Preserve compatible existing implementation.
Use existing project abstractions where they satisfy the design.
Keep downstream work for its own prompt.

## Required Architecture Invariants

Preserve these QG invariants:

- C++20 native application.
- Qt 6 Quick/QML presentation.
- C++ owns domain/application state.
- QML owns presentation and interaction state.
- Filesystem is the source of truth for media/folders.
- High-volume directory/media state lives in C++ item models.
- Expensive filesystem/media/database work stays off the GUI thread.
- Background work is bounded and cancellable.
- Stale asynchronous results are rejected.
- Qt Quick uses the approved OpenGL path on the X11/xcb target.
- libmpv uses one heavy playback session/handle.
- Stage and Cinema share the same playback session.
- `mpv_render_context` integrates through the approved Qt Quick rendering path.
- Native Linux file semantics are preserved.
- SQLite persistence stays separate from media files.
- CMake + Ninja is the build path.
- Native Ubuntu `.deb` is the primary package.

## Validation Contract

An item becomes COMPLETE only after the relevant validation has actually run.

Evidence can include:

```text
CMake configure
Ninja build
CTest / Qt Test / Qt Quick Test
focused unit/integration tests
runtime/UI validation
performance validation
package validation
```

Use the exact validation expected by the work item.
Never convert a failed validation into COMPLETE.

## Failure Handling

### Current-task defect

Keep the item IN_PROGRESS, repair the defect, and rerun focused validation.

### Pre-existing defect required by the current task

Record the evidence and repair it when it is inside the current dependency boundary.

### Genuine external blocker

Set:

```text
item: BLOCKED
Pipeline Status: BLOCKED
```

Record the exact action, failure, evidence, and condition required to continue,
then stop.

### Hermes/tool/provider failure

Leave project state representing the last proven state. The external runner
stops on a nonzero Hermes process result.

## Document Update Contract

### GUIDE_STATE.md

Update after every completed work item. Record:

- item status;
- concise implementation result;
- build result;
- test result;
- validation result;
- next dependency-ready item;
- blockers.

Keep it concise.

### DECISIONS.md

Update only for durable choices that future work must preserve.
Do not use it as a session log.

### REQUIREMENTS_MATRIX.md

Update when a requirement changes materially among:

```text
PLANNED
PARTIAL
IMPLEMENTED
VERIFIED
BLOCKED
```

`VERIFIED` requires actual validation evidence.

## Manual Modes

### `/qg-autobuilder next`

Execute exactly one dependency-ready work item using the Atomic Turn Protocol,
update state, report, and stop.

### `/qg-autobuilder status`

Read control files and actual project state as needed. Report current pipeline,
next work item, blockers, and last validation. Do not implement a new item.

### `/qg-autobuilder validate`

Validate the current work item/stage and reconcile state. Do not begin a new feature prompt.

### `/qg-autobuilder checkpoint`

Execute exactly the next due checkpoint, validate it, update state, and stop.

## Continuous Automation

Continuous execution is started outside the chat with:

```bash
python3 /home/treshen/Projects/QG/hermes/qg-pipeline.py run
```

The external runner is responsible for starting another Hermes invocation.
The skill itself never claims that an exited Hermes process can autonomously restart itself.

## Runner Result Contract

When the active instruction is the project runner turn, finish with exactly one
machine-readable line:

```text
QG_RUNNER_RESULT=CONTINUE
```

when the current item completed and more pipeline work exists.

Use:

```text
QG_RUNNER_RESULT=BLOCKED
```

when pipeline state is BLOCKED.

Use:

```text
QG_RUNNER_RESULT=COMPLETE
```

only after all development prompts, checkpoints, FI-01, and RV-01 are complete.

The marker must be the final non-empty output line.

## Completion Contract

Pipeline completion means:

```text
all development prompts COMPLETE
all mandatory checkpoints COMPLETE
FI-01 COMPLETE
RV-01 COMPLETE
Pipeline Status: COMPLETE
Blockers: None
```

Release completion is never inferred merely from source presence.
