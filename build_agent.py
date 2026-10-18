"""Compatibility entry point that routes builds through the dev-loop owner.

New workflows should call ``devscripts/dev-loop.py --request build`` directly.
This wrapper remains for older scripts and waits for the daemon's correlated
build result instead of creating another compiler/build owner.
"""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parent
DEV_LOOP = ROOT / "devscripts" / "dev-loop.py"


def main() -> int:
    mode = "release" if any(argument.lower() == "release" for argument in sys.argv[1:]) else "debug"
    command = [
        sys.executable,
        str(DEV_LOOP),
        "--request", "build",
        "--build-mode", mode,
        "--wait-for-build",
    ]
    return subprocess.run(command, cwd=ROOT, check=False).returncode


if __name__ == "__main__":
    raise SystemExit(main())
