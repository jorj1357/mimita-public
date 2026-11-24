#!/usr/bin/env python3
"""Ask the shared agent brain who owns a cross-agent signal.

Use this when a build error, dirty file, lease, runtime, or stale proof looks
like another agent's work. The tool reads the coordination board, finds agents
whose reported activity mentions the supplied path/build/run, and sends one
bounded question to those agents. If no owner is found it broadcasts a
response-required message to all agents.

It never edits source, stops a runtime, or creates a build. It only uses the
existing agent-coord CLI.
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parent.parent
COORD = ROOT / "devscripts" / "agent-coord.py"


def run_coord(args: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(COORD), *args],
        cwd=ROOT,
        capture_output=True,
        text=True,
    )


def load_board(agent: str) -> dict[str, Any] | None:
    result = run_coord(["--agent", agent, "--json", "board"])
    if result.returncode != 0:
        return None
    try:
        value = json.loads(result.stdout)
    except json.JSONDecodeError:
        return None
    return value if isinstance(value, dict) else None


def activity_paths(activity: dict[str, Any]) -> list[str]:
    raw = activity.get("paths")
    if isinstance(raw, list):
        return [str(item) for item in raw]
    if not isinstance(raw, str) or not raw.strip():
        return []
    try:
        value = json.loads(raw)
    except json.JSONDecodeError:
        return [raw]
    return [str(item) for item in value] if isinstance(value, list) else []


def pathway_match(requested: str, reported: str) -> bool:
    wanted = requested.replace("\\", "/").lstrip("./")
    have = reported.replace("\\", "/").lstrip("./")
    return bool(wanted) and bool(have) and (wanted in have or have in wanted)


def find_owners(board: dict[str, Any], sender: str, paths: list[str],
                build_id: str, run_id: str,
                explicit: list[str]) -> list[tuple[str, str]]:
    found: dict[str, str] = {}
    known = {row.get("agent") for row in board.get("agents", [])}
    for name in explicit:
        if name and name != sender:
            found[name] = "explicit"
    for row in board.get("agents", []):
        name = str(row.get("agent") or "")
        if not name or name == sender or name in found:
            continue
        activity = row.get("activity") or {}
        if build_id and str(activity.get("build_id") or "") == build_id:
            found[name] = "build_id"
            continue
        if run_id and str(activity.get("run_id") or "") == run_id:
            found[name] = "run_id"
            continue
        reported = activity_paths(activity)
        for requested in paths:
            if any(pathway_match(requested, item) for item in reported):
                found[name] = f"path:{requested}"
                break
    return sorted(found.items())


def signal_body(question: str, paths: list[str], build_id: str,
                run_id: str, sender: str) -> str:
    lines = [
        question.strip(),
        "",
        f"Sender: {sender}",
    ]
    if paths:
        lines.append("Paths: " + ", ".join(paths))
    if build_id:
        lines.append(f"Build attempt: {build_id}")
    if run_id:
        lines.append(f"Run: {run_id}")
    lines.extend([
        "",
        "This is a cross-agent signal. Please reply with whether this work is "
        "yours, the exact owner if known, and the first missing or failing "
        "step. Do not assume the sender will stop or overwrite your work.",
    ])
    return "\n".join(lines)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Ask the shared agent brain who owns a cross-agent signal.")
    parser.add_argument("--agent", required=True, help="your coordination agent id")
    parser.add_argument("--question", required=True, help="bounded question to ask")
    parser.add_argument("--path", action="append", default=[], help="path to match")
    parser.add_argument("--build-id", default="", help="build attempt or build id")
    parser.add_argument("--run-id", default="", help="test/runtime run id")
    parser.add_argument("--to", action="append", default=[], help="explicit recipient")
    parser.add_argument("--subject", default="Cross-agent ownership signal")
    parser.add_argument("--issue", default="", help="append to this issue discussion")
    parser.add_argument("--wait", type=float, default=0.0,
                        help="bounded wait seconds for a reply (0 = do not wait)")
    parser.add_argument("--dry-run", action="store_true",
                        help="show matches without sending")
    args = parser.parse_args(argv)

    board = load_board(args.agent)
    if board is None:
        print("Could not read the shared coordination board.", file=sys.stderr)
        return 2

    owners = find_owners(board, args.agent, args.path, args.build_id,
                         args.run_id, args.to)
    body = signal_body(args.question, args.path, args.build_id, args.run_id,
                       args.agent)
    if args.dry_run:
        print("matches:")
        if owners:
            for name, reason in owners:
                print(f"  {name} ({reason})")
        else:
            print("  <none: would broadcast to all>")
        return 0

    if owners:
        failures: list[str] = []
        for name, reason in owners:
            command = [
                "--agent", args.agent, "ask", "--to", name,
                "--subject", args.subject,
                "--wait", str(max(0.0, args.wait)),
            ]
            if args.issue:
                command.extend(["--issue", args.issue])
            command.append(body)
            result = run_coord(command)
            status = "sent" if result.returncode == 0 else "failed"
            print(f"[SIGNAL] {status} to={name} reason={reason}")
            if result.returncode != 0:
                failures.append(name)
                detail = (result.stderr or result.stdout).strip()
                if detail:
                    print(f"  {detail}", file=sys.stderr)
        if failures and len(failures) == len(owners):
            return 1
    else:
        command = [
            "--agent", args.agent, "send", "--to", "all",
            "--subject", args.subject, "--kind", "question",
            "--requires-response",
        ]
        if args.issue:
            command.extend(["--issue", args.issue])
        command.append(body)
        result = run_coord(command)
        if result.returncode != 0:
            print("Could not broadcast the cross-agent signal.", file=sys.stderr)
            detail = (result.stderr or result.stdout).strip()
            if detail:
                print(detail, file=sys.stderr)
            return 1
        print("[SIGNAL] broadcast to all (no matching owner found)")

    inbox = run_coord(["--agent", args.agent, "inbox"])
    if inbox.returncode == 0 and inbox.stdout.strip():
        print(inbox.stdout.strip())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
