# QG Runner Turn

Execute one atomic unit of the existing QG development pipeline.

Project:

```text
/home/treshen/Projects/QG/
```

The `qg-autobuilder` skill is preloaded.

## Execute

1. Read `.hermes.md`.
2. Read `GUIDE_STATE.md`.
3. Read relevant `DECISIONS.md`.
4. Read relevant `REQUIREMENTS_MATRIX.md`.
5. Determine the earliest dependency-ready incomplete work item from `dev-guide.md`.
6. If a mandatory checkpoint is due, select it before downstream development.
7. If all development/checkpoints are complete, select FI-01.
8. If FI-01 is complete and RV-01 is pending, select RV-01.
9. Read the selected work item in full.
10. Read only the relevant `DESIGN.md` sections.
11. Inspect actual source/tests/build definitions for the selected item.
12. Reconcile obvious stale state before editing.
13. Mark the selected item IN_PROGRESS.
14. Execute exactly that one item.
15. Build affected targets.
16. Run focused tests.
17. Run the item's validation gate.
18. Repair current-item failures and repeat validation until pass or a genuine
    external blocker is proven.
19. On success:
    - mark the item COMPLETE;
    - record concise build/test/validation evidence in `GUIDE_STATE.md`;
    - set the next work item;
    - update `DECISIONS.md` only for a real durable decision;
    - update `REQUIREMENTS_MATRIX.md` only for materially changed requirement coverage.
20. On blocker:
    - mark the item BLOCKED;
    - set Pipeline Status: BLOCKED;
    - record exact evidence and the condition required to continue.
21. Stop after this one work item.

## Existing-project rule

Earlier prompts were sometimes executed manually. Recorded state can be stale.
Actual implementation plus executed validation is the evidence used to reconcile state.

## Final marker

The final non-empty response line must be exactly one of:

QG_RUNNER_RESULT=CONTINUE
QG_RUNNER_RESULT=BLOCKED
QG_RUNNER_RESULT=COMPLETE
