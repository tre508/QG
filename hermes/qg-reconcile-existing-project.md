# QG Existing-Project Reconciliation

The QG project is already in progress. Earlier dev-guide prompts were executed
manually and the control documents can be behind the actual implementation.

This is a one-time state repair before the external runner takes control.

Project:

```text
/home/treshen/Projects/QG/
```

The `qg-autobuilder` skill is preloaded.

## Read

- `.hermes.md`
- `DESIGN.md`
- `dev-guide.md`
- `GUIDE_STATE.md`
- `DECISIONS.md`
- `REQUIREMENTS_MATRIX.md`
- actual project source/tests/build files

## Reconcile

Walk `dev-guide.md` in dependency order.

For each prompt/checkpoint that may already have been executed:

1. read its complete criteria;
2. inspect the actual implementation;
3. inspect its tests;
4. run focused validation when current evidence is insufficient;
5. classify it PENDING, IN_PROGRESS, COMPLETE, or BLOCKED.

Use actual source plus executed validation as evidence.

Rebuild `GUIDE_STATE.md` so it identifies:

- every known work item;
- recovered statuses;
- checkpoint statuses;
- earliest dependency-ready incomplete item;
- final integration status;
- release validation status;
- blockers;
- concise reconciliation evidence.

Reconcile `DECISIONS.md` only where existing durable implementation proves a
decision that future work must preserve.

Reconcile `REQUIREMENTS_MATRIX.md` using:

```text
PLANNED
PARTIAL
IMPLEMENTED
VERIFIED
BLOCKED
```

`VERIFIED` requires validation evidence.

This pass repairs state only. It does not begin the next implementation prompt.

## Final marker

End with exactly:

QG_RECONCILE_RESULT=READY

when autonomous execution can resume.

Use:

QG_RECONCILE_RESULT=BLOCKED

when an external condition prevents reliable continuation.
