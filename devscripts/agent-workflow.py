#!/usr/bin/env python3
"""Agent entry point for the canonical shared-checkout dev-loop (Mode 1).

Mode 2 lane creation and startup are deferred as of 2026-11-11 18:41 EST.
"""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mode", choices=("1",), default="1",
                        help="Mode 1 uses the canonical shared checkout")
    parser.add_argument("action", choices=("ensure", "build", "run", "status", "check",
                                             "create", "cleanup", "promote", "list"))
    parser.add_argument("args", nargs=argparse.REMAINDER)
    options = parser.parse_args()
    if options.action in {"create", "cleanup", "promote", "check", "list"}:
        parser.error(f"{options.action} is deferred with Mode 2; use the canonical root dev-loop")
    if options.action == "build":
        print("[AGENT WORKFLOW] agents must not build. Use the canonical root dev-loop status.", file=sys.stderr)
        return 2
    if options.action == "ensure":
        command = [sys.executable, str(ROOT / "devscripts" / "dev-loop.py"),
                   "--ensure-running"]
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
