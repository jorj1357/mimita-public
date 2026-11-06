#!/usr/bin/env python3
"""Select legacy shared-checkout mode 1 or isolated agent-lane mode 2."""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mode", choices=("1", "2"), default="1",
                        help="1 keeps the existing shared-checkout workflow; 2 uses an isolated lane")
    parser.add_argument("action", choices=("build", "run", "status", "check", "create",
                                             "cleanup", "promote"))
    parser.add_argument("args", nargs=argparse.REMAINDER)
    options = parser.parse_args()
    if options.mode == "2":
        return subprocess.call([sys.executable, str(ROOT / "devscripts" / "agent-lane.py"),
                                options.action, *options.args], cwd=ROOT)

    if options.action in {"create", "cleanup", "promote", "check"}:
        parser.error(f"{options.action} is available in mode 2; mode 1 preserves the shared dev-loop workflow")
    if options.action == "build":
        command = [sys.executable, str(ROOT / "devscripts" / "dev-loop.py"),
                   "--request", "build", "--wait-for-build"]
    elif options.action == "run":
        if not options.args:
            parser.error("mode 1 run requires a launch mode, for example: run 1")
        command = [sys.executable, str(ROOT / "devscripts" / "dev-loop.py"),
                   "--request", "launch", "--launch-mode", options.args[0]]
        command.extend(options.args[1:])
    else:
        command = [sys.executable, str(ROOT / "devscripts" / "dev-loop.py"), "--status", "--json"]
    return subprocess.call(command, cwd=ROOT)


if __name__ == "__main__":
    raise SystemExit(main())
