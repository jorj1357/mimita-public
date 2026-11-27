#!/usr/bin/env python3
"""Attach a short explanation to the next whole-game DLL build."""

from __future__ import annotations

import argparse
import json
import os
import uuid
from datetime import datetime, timezone
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="milliseconds").replace(
        "+00:00", "Z")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--changes", required=True,
                        help="short human-readable summary of the code change")
    parser.add_argument("--why", default="",
                        help="required by policy: why this build exists now "
                             "(user request, regression, proof, dependency)")
    parser.add_argument("--header-fanout", type=int, default=None,
                        help="affected source count for a changed header "
                             "(dependency_graph.py --affected)")
    parser.add_argument("--expected-seconds", type=float, default=None,
                        help="expected wall time for this build")
    parser.add_argument("--risk", default="",
                        help="resource/cache/runtime risk for this build")
    parser.add_argument("--proof", default="",
                        help="runtime evidence this build must produce")
    parser.add_argument("--files", action="append", default=[],
                        help="changed repository path (repeat for multiple paths)")
    parser.add_argument("--agent", default="",
                        help="optional explicit agent label; never inferred")
    args = parser.parse_args()

    summary = " ".join(args.changes.split())
    if not summary or len(summary) > 240:
        parser.error("--changes must contain 1 to 240 characters")
    why = " ".join(args.why.split())
    if len(why) > 400:
        parser.error("--why must be at most 400 characters")
    risk = " ".join(args.risk.split())
    if len(risk) > 300:
        parser.error("--risk must be at most 300 characters")
    proof = " ".join(args.proof.split())
    if len(proof) > 300:
        parser.error("--proof must be at most 300 characters")
    if args.header_fanout is not None and args.header_fanout < 0:
        parser.error("--header-fanout must be >= 0")
    if args.expected_seconds is not None and args.expected_seconds < 0:
        parser.error("--expected-seconds must be >= 0")
    agent = " ".join(args.agent.split())
    if len(agent) > 80:
        parser.error("--agent must be at most 80 characters")

    files: list[str] = []
    for value in args.files:
        candidate = Path(value)
        if candidate.is_absolute() or ".." in candidate.parts:
            parser.error(f"--files must stay inside the repository: {value}")
        resolved = (ROOT / candidate).resolve()
        try:
            rel = resolved.relative_to(ROOT).as_posix()
        except ValueError:
            parser.error(f"--files must stay inside the repository: {value}")
        files.append(rel)

    note_root = (ROOT / ".dev" / "build-notes" / "pending").resolve()
    try:
        note_root.relative_to(ROOT)
    except ValueError:
        parser.error("build note directory resolves outside the repository")
    note_root.mkdir(parents=True, exist_ok=True)

    note_id = uuid.uuid4().hex[:12]
    payload = {
        "schema_version": 2,
        "note_id": note_id,
        "created_utc": utc_now(),
        "changes": summary,
        "why": why or "missing",
        "files": list(dict.fromkeys(files))[:40],
    }
    if args.header_fanout is not None:
        payload["header_fanout"] = args.header_fanout
    if args.expected_seconds is not None:
        payload["expected_seconds"] = args.expected_seconds
    if risk:
        payload["risk"] = risk
    if proof:
        payload["proof"] = proof
    if agent:
        payload["agent"] = agent
    path = note_root / f"{payload['created_utc'].replace(':', '-')}-{note_id}.json"
    flags = os.O_WRONLY | os.O_CREAT | os.O_EXCL
    descriptor = os.open(path, flags, 0o600)
    with os.fdopen(descriptor, "w", encoding="utf-8") as output:
        json.dump(payload, output, indent=2, sort_keys=True)
        output.write("\n")
        output.flush()
        os.fsync(output.fileno())

    print(f"[BUILD NOTE] queued for next whole-game DLL build: {summary}")
    if not why:
        print("[BUILD NOTE] WARNING: --why is missing; add why this build exists")
    else:
        print(f"[BUILD NOTE] why: {why}")
    print(f"[BUILD NOTE] paths={len(payload['files'])} note_id={note_id}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
