#!/usr/bin/env python3
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]


def main():
    command = [sys.executable, "tools/test-networking.py", "--quick", "--no-build"]
    result = subprocess.run(command, cwd=ROOT, text=True, capture_output=True, timeout=90)
    if result.stdout:
        print(result.stdout, end="")
    if result.stderr:
        print("[STDERR]")
        print(result.stderr, end="")
    return result.returncode


if __name__ == "__main__":
    raise SystemExit(main())
