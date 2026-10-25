## `/qg-autobuilder auto`

Interpret as:

> Enter `/home/treshen/Projects/QG/` and determine whether this is a new
> development pipeline or an existing one.
>
> If `GUIDE_STATE.md`, `DECISIONS.md`, and `REQUIREMENTS_MATRIX.md` do not yet
> exist, perform the QG bootstrap procedure before writing application code:
>
> 1. Read `.hermes.md`.
> 2. Read `DESIGN.md` completely.
> 3. Read `dev-guide.md` completely.
> 4. Extract the approved architecture, MVP requirements, acceptance criteria,
>    development prompts, prompt dependencies, checkpoints, final-integration
>    task, and release-validation task.
> 5. Create `GUIDE_STATE.md` containing every development prompt and checkpoint,
>    initialized to the correct starting state.
> 6. Create `DECISIONS.md` containing only architectural decisions explicitly
>    established by `DESIGN.md`; do not invent implementation decisions.
> 7. Create `REQUIREMENTS_MATRIX.md` mapping every applicable MVP requirement
>    and acceptance criterion from `DESIGN.md` to its planned implementation
>    prompt and planned validation coverage.
> 8. Cross-check all three generated control documents against both authoritative
>    source documents.
> 9. Confirm that Prompt 01 is dependency-ready.
> 10. Only then begin implementation.
>
> If the control documents already exist, treat this as a resume operation:
>
> 1. Read `.hermes.md`.
> 2. Read `GUIDE_STATE.md`.
> 3. Read `DECISIONS.md`.
> 4. Read `REQUIREMENTS_MATRIX.md`.
> 5. Read the relevant portions of `DESIGN.md` and `dev-guide.md`.
> 6. Reconcile recorded state against the actual repository.
> 7. Select the earliest dependency-ready incomplete prompt.
>
> After bootstrap or resume, execute one prompt at a time:
>
> inspect → implement → build → test → validate → update control documents.
>
> Continue automatically through dependency-ready prompts and mandatory
> checkpoints until release validation passes or a genuine blocker is reached.