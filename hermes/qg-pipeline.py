#!/usr/bin/env python3
"""Reliable external orchestrator for the QG Hermes development pipeline."""

from __future__ import annotations

import argparse
import fcntl
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import time
from datetime import datetime

DEFAULT_PROJECT = Path("/home/treshen/Projects/QG")
DEFAULT_PROFILE = "qg-builder"

PIPELINE_RE = re.compile(
    r"(?im)^\s*(?:Pipeline Status|Pipeline|Status)\s*:\s*(READY|RUNNING|BLOCKED|COMPLETE)\s*$"
)


def sha256(path: Path) -> str:
    if not path.exists():
        return ""
    h = hashlib.sha256()
    with path.open("rb") as f:
        for block in iter(lambda: f.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def pipeline_state(state_file: Path) -> str | None:
    if not state_file.exists():
        return None
    m = PIPELINE_RE.search(state_file.read_text(errors="replace"))
    return m.group(1).upper() if m else None


def hermes_cmd(project: Path, profile: str, query_file: Path) -> list[str]:
    return [
        "hermes", "-p", profile, "chat",
        "--in", str(project),
        "--checkpoints",
        "--toolsets", "coding",
        "-s", "qg-autobuilder",
        "--oneshot",
        "--max-turns", "500",
        "--source", "qg-autobuilder-v2",
        "--query-file", str(query_file),
        "--format", "stream-json",
    ]


def run_hermes(project: Path, profile: str, query_file: Path, log_file: Path) -> tuple[int, str]:
    cmd = hermes_cmd(project, profile, query_file)
    final_text = ""
    result_exit = None

    with log_file.open("w", encoding="utf-8") as log:
        log.write("$ " + " ".join(cmd) + "\n")
        log.flush()

        proc = subprocess.Popen(
            cmd,
            cwd=project,
            stdin=subprocess.DEVNULL,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            encoding="utf-8",
            errors="replace",
            bufsize=1,
        )

        assert proc.stdout is not None
        for line in proc.stdout:
            sys.stdout.write(line)
            log.write(line)
            try:
                event = json.loads(line)
            except json.JSONDecodeError:
                continue
            if event.get("type") == "result":
                final_text = event.get("text") or ""
                result_exit = event.get("exit_code")

        stderr = proc.stderr.read() if proc.stderr is not None else ""
        if stderr:
            sys.stderr.write(stderr)
            log.write("\n--- STDERR ---\n")
            log.write(stderr)

        rc = proc.wait()

    if result_exit is not None and int(result_exit) != 0:
        return int(result_exit), final_text
    return rc, final_text


def require_files(project: Path, names: list[str]) -> None:
    missing = [name for name in names if not (project / name).exists()]
    if missing:
        raise SystemExit("Missing required QG files:\n  " + "\n  ".join(missing))


def extract_marker(text: str, prefix: str) -> str | None:
    lines = [x.strip() for x in text.splitlines() if x.strip()]
    if not lines:
        return None
    last = lines[-1]
    return last if last.startswith(prefix) else None


def acquire_lock(project: Path):
    lock_path = project / "hermes" / ".qg-pipeline.lock"
    lock_path.parent.mkdir(parents=True, exist_ok=True)
    fh = lock_path.open("w")
    try:
        fcntl.flock(fh.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
    except BlockingIOError:
        raise SystemExit(f"Another QG pipeline runner is active: {lock_path}")
    fh.write(str(os.getpid()))
    fh.flush()
    return fh


def reconcile(project: Path, profile: str) -> int:
    require_files(project, [
        ".hermes.md",
        "DESIGN.md",
        "dev-guide.md",
        "GUIDE_STATE.md",
        "DECISIONS.md",
        "REQUIREMENTS_MATRIX.md",
        "hermes/qg-reconcile-existing-project.md",
    ])
    logs = project / "hermes" / "autobuilder-logs"
    logs.mkdir(parents=True, exist_ok=True)
    stamp = datetime.now().strftime("%Y%m%d-%H%M%S")
    log = logs / f"reconcile-{stamp}.jsonl"

    print(f"QG reconciliation log: {log}")
    rc, final = run_hermes(
        project, profile, project / "hermes" / "qg-reconcile-existing-project.md", log
    )
    if rc != 0:
        print(f"Reconciliation Hermes process failed with exit code {rc}.", file=sys.stderr)
        return rc

    marker = extract_marker(final, "QG_RECONCILE_RESULT=")
    if marker == "QG_RECONCILE_RESULT=READY":
        print("QG reconciliation: READY")
        return 0
    if marker == "QG_RECONCILE_RESULT=BLOCKED":
        print("QG reconciliation: BLOCKED")
        return 3

    print("Reconciliation finished without a valid final marker.", file=sys.stderr)
    return 4


def run_pipeline(project: Path, profile: str, max_runs: int, once: bool) -> int:
    require_files(project, [
        ".hermes.md",
        "DESIGN.md",
        "dev-guide.md",
        "GUIDE_STATE.md",
        "DECISIONS.md",
        "REQUIREMENTS_MATRIX.md",
        "hermes/qg-runner-turn.md",
    ])

    state_file = project / "GUIDE_STATE.md"
    logs = project / "hermes" / "autobuilder-logs"
    logs.mkdir(parents=True, exist_ok=True)

    for n in range(1, max_runs + 1):
        state = pipeline_state(state_file)
        if state == "COMPLETE":
            print("QG pipeline is COMPLETE.")
            return 0
        if state == "BLOCKED":
            print(f"QG pipeline is BLOCKED. Inspect {state_file}")
            return 3

        before = sha256(state_file)
        stamp = datetime.now().strftime("%Y%m%d-%H%M%S")
        log = logs / f"run-{n:03d}-{stamp}.jsonl"

        print()
        print("=" * 72)
        print(f"QG pipeline turn {n}/{max_runs}")
        print(f"State before: {state or 'UNPARSED'}")
        print(f"Log: {log}")
        print("=" * 72)

        rc, final = run_hermes(
            project, profile, project / "hermes" / "qg-runner-turn.md", log
        )
        if rc != 0:
            print(f"Hermes failed with exit code {rc}. Pipeline stopped.", file=sys.stderr)
            return rc

        marker = extract_marker(final, "QG_RUNNER_RESULT=")
        after = sha256(state_file)

        if marker == "QG_RUNNER_RESULT=BLOCKED":
            print("Hermes reported BLOCKED.")
            return 3

        if marker == "QG_RUNNER_RESULT=COMPLETE":
            if pipeline_state(state_file) != "COMPLETE":
                print(
                    "Hermes reported COMPLETE but GUIDE_STATE.md is not COMPLETE. "
                    "Stopping on state-contract violation.",
                    file=sys.stderr,
                )
                return 6
            print("QG pipeline COMPLETE.")
            return 0

        if marker != "QG_RUNNER_RESULT=CONTINUE":
            print(
                "Hermes did not emit a valid final QG_RUNNER_RESULT marker. "
                f"Inspect {log}",
                file=sys.stderr,
            )
            return 4

        if before == after:
            print(
                "Hermes returned CONTINUE but GUIDE_STATE.md did not change. "
                "Stopping to prevent an infinite loop.",
                file=sys.stderr,
            )
            return 5

        new_state = pipeline_state(state_file)
        print(f"State after: {new_state or 'UNPARSED'}")

        if once:
            print("One pipeline turn completed.")
            return 0

        if new_state == "BLOCKED":
            return 3
        if new_state == "COMPLETE":
            return 0

        time.sleep(1)

    print(f"Stopped after max_runs={max_runs} without COMPLETE/BLOCKED.", file=sys.stderr)
    return 7


def show_status(project: Path) -> int:
    state_file = project / "GUIDE_STATE.md"
    if not state_file.exists():
        print(f"Missing {state_file}", file=sys.stderr)
        return 2
    print(f"Pipeline state: {pipeline_state(state_file) or 'UNPARSED'}")
    print(f"State file: {state_file}")
    print()
    print(state_file.read_text(errors="replace")[:12000])
    return 0


def main() -> int:
    p = argparse.ArgumentParser(description="QG Hermes external pipeline orchestrator")
    p.add_argument("--project", type=Path, default=DEFAULT_PROJECT)
    p.add_argument("--profile", default=DEFAULT_PROFILE)

    sub = p.add_subparsers(dest="command", required=True)
    sub.add_parser("reconcile", help="Repair state from the existing implementation")

    runp = sub.add_parser("run", help="Run fresh Hermes turns until COMPLETE/BLOCKED")
    runp.add_argument("--max-runs", type=int, default=200)

    sub.add_parser("once", help="Execute exactly one fresh Hermes pipeline turn")
    sub.add_parser("status", help="Show parsed GUIDE_STATE status")

    args = p.parse_args()
    project = args.project.expanduser().resolve()
    if not project.exists():
        print(f"Project not found: {project}", file=sys.stderr)
        return 2

    lock = acquire_lock(project)
    try:
        if args.command == "reconcile":
            return reconcile(project, args.profile)
        if args.command == "run":
            return run_pipeline(project, args.profile, args.max_runs, False)
        if args.command == "once":
            return run_pipeline(project, args.profile, 1, True)
        if args.command == "status":
            return show_status(project)
        return 2
    finally:
        lock.close()


if __name__ == "__main__":
    raise SystemExit(main())
