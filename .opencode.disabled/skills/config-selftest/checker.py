#!/usr/bin/env python3
import os
import subprocess
import sys


REPO_ROOT = os.path.dirname(os.path.abspath(__file__))
while not os.path.isfile(os.path.join(REPO_ROOT, "overseer.py")):
    parent = os.path.dirname(REPO_ROOT)
    if parent == REPO_ROOT:
        print("Could not locate repository root.")
        sys.exit(1)
    REPO_ROOT = parent


def main():
    script = os.path.join(REPO_ROOT, "devscripts", "config_selftest.py")
    proc = subprocess.run([sys.executable, script], cwd=REPO_ROOT, text=True)
    return proc.returncode


if __name__ == "__main__":
    sys.exit(main())
