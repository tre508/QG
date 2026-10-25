---
type: conventions
project: QG
created: 2026-10-20
updated: 2026-10-22
status: active
tags:
  - conventions
  - workflow
  - agent
  - memory
source:
  - GUIDE_STATE.md
  - dev-guide.md
  - .hermes.md
---

# Agent Workflow Conventions

## Development Workflow Mode — ACTIVE POLICY (2026-10-22)

**Logic-first implementation, deferred batch testing.** This supersedes every
"build + test before COMPLETE" rule below while it is in force.

Per task: **inspect → identify root cause → implement → static review → check
integration → document → record deferred test → report → stop.**

Do **not**, unless the user explicitly opens a testing phase:

- run CMake/Ninja builds;
- run CTest, unit tests, or Qt Quick Test;
- launch QG, capture screenshots, or do visual/viewport comparisons;
- run benchmarks, integration tests, or hardware/device validation;
- author a new test suite solely for the current fix.

Preserve existing tests and infrastructure; never delete tests to simplify a change.

### Status vocabulary (use exactly these)

| Status | Meaning |
|--------|---------|
| `IMPLEMENTED` | Required code changes complete |
| `STATICALLY REVIEWED` | Logic and integration inspected |
| `NEEDS_REVISION` | Static review found problems |
| `BLOCKED` | Missing dependency or decision |
| `TESTING DEFERRED` | Runtime/automated validation postponed by policy |
| `VERIFIED` | **Only** after the relevant tests actually ran successfully |

An item is *implementation-complete* when its code is written and statically
reviewed. It is not runtime-verified until the batch testing phase.

### Deferred Test Register

One consolidated backlog, never per-fix test runs:

```text
/home/treshen/Projects/QG/DEFERRED_TESTS.md   (authoritative, in-repo)
06-Testing/Deferred-Test-Register.md          (vault mirror + narrative)
```

Each completed fix adds **one row**: fix ID, changes, static-review status,
deferred checks, risk (Low/Medium/High). Related checks are grouped into a single
future batch and never duplicated across rows.

### Exception

If a change touches data loss, filesystem mutation, device mounting, concurrency,
or memory safety: name the risk, add code-level safeguards, state what remains
unverified, and **ask before running anything**. Never silently run tests as an
"exception".

## Development Pipeline (per .hermes.md)

The project uses an autonomous pipeline driven by `hermes/qg-pipeline.py`. Each Hermes invocation executes **exactly one work item**:

```
runner mode:
select → mark IN_PROGRESS → inspect → implement → [validate] → persist state → exit

interactive mode (ACTIVE POLICY):
select → inspect → implement → static review → document → record deferred test → report
```

Under the active policy the `[validate]` step is not executed: the item is recorded
as `IMPLEMENTED` / `STATICALLY REVIEWED` and its checks go to the deferred register.
External runner launches next fresh Hermes invocation.

## Work Item Types

1. **Development Prompt** — Feature implementation from dev-guide.md
2. **Mandatory Checkpoint** — Architecture review, integration test
3. **FI-01** — Final Integration
4. **RV-01** — Release Validation

## Completion Rules

- Under the active policy: work item = **IMPLEMENTED + STATICALLY REVIEWED**, with
  its checks recorded in `DEFERRED_TESTS.md`. Do not mark it COMPLETE/VERIFIED
  until the batch phase actually runs them.
- Once testing is authorized again: work item = COMPLETE only when **completion
  criteria AND validation gate pass**
- Validation evidence > stale status text
- Genuine external blocker → BLOCKED with exact evidence
- Full pipeline COMPLETE only after ALL prompts, checkpoints, FI-01, RV-01 pass

## Session Startup Protocol

For each development turn:

1. Read `GUIDE_STATE.md`
2. Read relevant `DECISIONS.md`
3. Read relevant `REQUIREMENTS_MATRIX.md`
4. Locate current/next work item in `dev-guide.md`
5. Read that work item in full
6. Read relevant `DESIGN.md` sections
7. Inspect actual implementation/tests/build definitions

**Use fresh filesystem evidence** — not conversational memory.

## Existing Project Rule

- Treat QG as existing project
- Preserve compatible implementation
- When recorded state and actual implementation disagree → **reconcile using source, build, tests, runtime evidence**

## Source of Truth Priority

```
1. DESIGN.md
2. Validated actual implementation behavior
3. DECISIONS.md
4. dev-guide.md
5. GUIDE_STATE.md
6. Current task instruction
```

## Implementation Discipline

- Inspect existing files and direct dependencies before editing
- Use actual existing API/type names
- One development work item per automated Hermes invocation
- Under the active policy: do **not** run focused tests — record the required checks
  in `DEFERRED_TESTS.md` instead (when testing is authorized: focused tests first,
  then the broader gate)
- Use temporary/disposable filesystem fixtures for destructive file-operation tests
- Keep `GUIDE_STATE.md` concise
- Update `DECISIONS.md` only for durable decisions
- Update `REQUIREMENTS_MATRIX.md` only when requirement state materially changes

## Memory Integration Workflow

### BEFORE DEVELOPMENT
- Identify the repository
- Read current project state (Project-Dashboard.md, Current-State.md, Session-Handoff.md)
- Retrieve relevant memory (component notes, decisions, known issues)
- Check architectural decisions (Decision-Index.md)
- Check known issues and dependencies
- Verify memory against authoritative sources (DESIGN.md, implementation)

### DURING DEVELOPMENT
- Follow established architecture (layering, ownership, invariants)
- Avoid repeating previously documented mistakes (Known-Issues.md, Technical-Debt.md)
- Record important new findings (debugging discoveries, root causes)
- Maintain accurate task status (Active-Tasks.md)
- Do not treat memory as infallible — verify against implementation

### AFTER DEVELOPMENT
- Record required verification in `DEFERRED_TESTS.md` (build, tests, benchmarks,
  visual checks) — **do not execute it** unless a testing phase is authorized;
  complete static review of the changed code instead
- Update authoritative project documents (GUIDE_STATE.md, DECISIONS.md, REQUIREMENTS_MATRIX.md)
- Update relevant Obsidian notes (Component-Map.md, Known-Issues.md, etc.)
- Record decisions and debugging discoveries
- Update session handoff (Session-Handoff.md)
- Preserve the next actionable step

## Documentation Update Rules

### Update Immediately
- `GUIDE_STATE.md` — Task status, test results, blockers
- `Session-Handoff.md` — Current objective, last completed, active task, next action

### Update When State Changes Materially
- `DECISIONS.md` — New architectural decisions only
- `REQUIREMENTS_MATRIX.md` — Requirement coverage changes
- `Active-Tasks.md` / `Completed-Tasks.md` — Task progression
- `Known-Issues.md` — New issues, resolved issues
- `Technical-Debt.md` — New debt, paid-down debt

### Update for Knowledge Capture
- Component files (Component-Map.md, Architecture/*.md) — When discovering new relationships
- Debugging files (Resolved-Issues.md) — After root cause analysis
- Decision files (Decision-Index.md) — When making architectural choices

## Verification Requirements

### Deferred by policy — execute only in an authorized batch testing phase
- `cmake --build build` — PASS
- `TMPDIR=/tmp ctest --test-dir build --output-on-failure` — relevant tests PASS
- `QT_QPA_PLATFORM=offscreen ./build/tests/qg_qml_test -input tests/qml` — QML suite
- For playback: `./tests/run_gl_test.sh` — PASS
- For performance: `./build/tests/qg_performance_test` — meets targets

These are recorded per fix in `DEFERRED_TESTS.md` §1 and executed as one batch
(`DEFERRED_TESTS.md` §4 checklist), not after each change.

### Evidence Required **when a check is run**
- Test output logs
- Build output
- Benchmark measurements
- Screenshots (for visual verification) — mark UNVERIFIED when no desktop session

### Evidence that must never be claimed without execution
A build, a test result, a screenshot, or a successful launch. Static review is not
runtime proof — say `STATICALLY REVIEWED`, not `VERIFIED`.

## Communication Style

Per .hermes.md:
- **Calm, technical, matter-of-fact**
- **Concise, precise, professional, analytical, understated**
- **Direct without being rude**
- **Confident only when justified**
- Lead with result/blocker/diagnosis
- Distinguish observed from inferred
- When blocked, name minimum change needed

### Avoid
- Enthusiasm, praise for ordinary questions
- Repetitive acknowledgments
- Motivational language
- Fake certainty
- Unnecessary apologies
- Verbose preambles
- Generic troubleshooting advice
- Overexplaining the obvious
- Repeating the request
- Decorative rhetorical questions
- Unexplained "magic" commands
- Burying the conclusion

## Decision Making

- Favor clear decision over exhaustive option list
- Separate technically possible from operationally sensible
- Label workarounds with fragility/maintenance burden
- Unsupported/incompatible/impractical/impossible are valid answers — discover early

## Handling Uncertainty

- Treat uncertainty explicitly — don't disguise guesses as conclusions
- Calibrated language: "confirmed because...", "most likely cause...", "cannot distinguish...", "assumption...", "verify with..."
- When uncertain → run smallest test that reduces uncertainty most
- Disagreement = comparison of models, not social conflict
- Evidence that counts: observed behavior, logs, specs, source code, reproducible tests

## Session Handoff Template

```markdown
## Current Objective
What Hermes is working toward.

## Last Completed Task
Most recent verified completion.

## Active Task
Current task identifier and status.

## Files Modified
Relevant files changed during the session.

## Decisions Made
Important architectural or implementation choices.

## Verification
Build and test commands actually executed, with results.

## Outstanding Issues
Failures, blockers, unresolved questions.

## Next Recommended Action
The exact next development step.
```

## Prohibited Actions

- ❌ Modify unrelated documentation
- ❌ Overwrite existing Obsidian vault
- ❌ Duplicate existing functioning memory system
- ❌ Store secrets, credentials, tokens, private env values
- ❌ Claim automated unit tests prove cross-app desktop interoperability
- ❌ Claim simulated mount events prove physical disconnect/reconnect behavior
- ❌ Fabricate screenshots, logs, or test outcomes
- ❌ Invent Hermes configuration syntax
- ❌ Modify unrelated global agent settings
- ❌ Introduce database/vector-search dependency without justification
- ❌ Create competing task-state systems (GUIDE_STATE.md is authoritative)
- ❌ Run builds/tests/app-launches while the deferred-testing policy is active without authorization
- ❌ Mark an item VERIFIED or COMPLETE on the strength of static review alone
- ❌ Report a deferred check as passed, or omit a deferred check from the register