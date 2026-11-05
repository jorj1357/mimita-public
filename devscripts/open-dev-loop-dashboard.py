"""Open the visible Tk dashboard attached to the existing dev loop."""

from __future__ import annotations

import os
import shutil
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DASHBOARD_SCRIPT = ROOT / "devscripts" / "dev-loop.py"


def _windowless_python() -> str:
    if os.name != "nt":
        return sys.executable

    sibling = Path(sys.executable).with_name("pythonw.exe")
    if sibling.is_file():
        return str(sibling)
    installed = shutil.which("pythonw.exe")
    if installed:
        return installed
    raise FileNotFoundError("Could not find pythonw.exe to open the dashboard without a console")


def main() -> int:
    creationflags = 0
    if os.name == "nt":
        creationflags = (subprocess.DETACHED_PROCESS
                         | subprocess.CREATE_NEW_PROCESS_GROUP)

    subprocess.Popen(
        [
            _windowless_python(),
            str(DASHBOARD_SCRIPT),
            "--dashboard",
            "--no-terminal-relaunch",
        ],
        cwd=ROOT,
        stdin=subprocess.DEVNULL,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        close_fds=True,
        creationflags=creationflags,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
