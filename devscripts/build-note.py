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
    parser.add_argument("--files", action="append", default=[],
                        help="changed repository path (repeat for multiple paths)")
    parser.add_argument("--agent", default="",
                        help="optional explicit agent label; never inferred")
    args = parser.parse_args()

    summary = " ".join(args.changes.split())
    if not summary or len(summary) > 240:
        parser.error("--changes must contain 1 to 240 characters")
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
        "schema_version": 1,
        "note_id": note_id,
        "created_utc": utc_now(),
        "changes": summary,
        "files": list(dict.fromkeys(files))[:40],
    }
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
    print(f"[BUILD NOTE] paths={len(payload['files'])} note_id={note_id}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
