# QG Project Instructions

## Project

```text
QG
/home/treshen/Projects/QG/
```

QG is an existing, in-progress native Ubuntu/Linux video explorer and player.
Earlier manual Hermes runs may have advanced the implementation farther than the
control documents indicate. Use actual source, build, tests, and runtime evidence
when reconciling stale state.

## Source of Truth

Priority:

```text
1. DESIGN.md
2. validated actual implementation behavior
3. DECISIONS.md
4. dev-guide.md
5. GUIDE_STATE.md
6. current task instruction
```

`DESIGN.md` defines approved product behavior and architecture.
`dev-guide.md` defines the ordered implementation pipeline.
`GUIDE_STATE.md` records execution progress.
`DECISIONS.md` records durable architecture/implementation decisions.
`REQUIREMENTS_MATRIX.md` records requirement coverage.

## Session Startup

For a development turn:

1. read `GUIDE_STATE.md`;
2. read relevant `DECISIONS.md`;
3. read relevant `REQUIREMENTS_MATRIX.md`;
4. locate the current/next work item in `dev-guide.md`;
5. read that work item in full;
6. read only the relevant `DESIGN.md` sections;
7. inspect the actual implementation/tests/build definitions required by that item.

Use fresh filesystem evidence rather than conversational memory.

## Existing-Project Rule

Treat QG as an existing project.
Preserve compatible implementation.
When recorded state and actual implementation disagree, reconcile them using
source, build, tests, and runtime evidence before advancing.

## Execution Model

Continuous autonomous development is driven by:

```text
/home/treshen/Projects/QG/hermes/qg-pipeline.py
```

Each Hermes invocation executes exactly one work item:

```text
select
→ mark IN_PROGRESS
→ inspect
→ implement
→ build
→ test
→ validate
→ persist state
→ exit
```

The external runner launches the next fresh Hermes invocation.

A work item is:

- a development prompt;
- a mandatory checkpoint;
- FI-01 final integration;
- RV-01 release validation.

## Completion Rules

A work item becomes COMPLETE only when its completion criteria and validation
gate have actually passed.

Validation evidence takes priority over stale status text.
A genuine external blocker sets the item and pipeline to BLOCKED with exact evidence.
The full pipeline becomes COMPLETE only after all prompts, checkpoints, FI-01,
and RV-01 pass.

## Approved Architecture

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
FFmpeg libraries
CMake
Ninja
CTest / Qt Test
native Ubuntu .deb packaging
```

Architecture boundaries:

```text
QML presentation
↓
C++ controllers/coordinators
↓
C++ models/services
↓
filesystem / persistence / playback / Linux integration
```

Key invariants:

- filesystem is authoritative for files/folders;
- application metadata is separate;
- one canonical ordered folder model/context drives Browse, Reel, Previous/Next,
  and auto-next;
- one heavy libmpv playback session exists;
- Stage and Cinema share that session;
- primary playback uses libmpv Render API integrated with Qt Quick/OpenGL;
- GUI/render threads remain responsive;
- high-volume state lives in C++ models;
- background work is bounded, cancellable, and stale-proof;
- native Linux file behavior is preserved.

## Implementation Discipline

Inspect existing files and direct dependencies before editing.
Use actual existing API/type names.
Keep one development work item per automated Hermes invocation.
Run focused tests first, then the work item's broader validation gate.
Use temporary/disposable filesystem fixtures for destructive file-operation tests.
Keep `GUIDE_STATE.md` concise.
Update `DECISIONS.md` only for durable decisions.
Update `REQUIREMENTS_MATRIX.md` only when requirement state materially changes.

## Local Filesystem Workflow

This project uses the local project filesystem directly.
Hermes filesystem checkpoints are enabled by the external runner with
`--checkpoints`.

## Runner Output

When invoked by `qg-pipeline.py`, the final non-empty response line is one of:

```text
QG_RUNNER_RESULT=CONTINUE
QG_RUNNER_RESULT=BLOCKED
QG_RUNNER_RESULT=COMPLETE
```
