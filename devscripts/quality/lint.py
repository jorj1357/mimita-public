#!/usr/bin/env python3
"""Run MiMITA repository-local quality reports.

Report-only by default. Use --gate to turn local findings into a failure.
"""

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
QUALITY = ROOT / "devscripts" / "quality"
REPORTS = (
    "check_damage_authority.py",
    "check_contact_ownership.py",
    "dead_code_report.py",
    "duplicate_report.py",
    "coverage_report.py",
)


def run_report(name: str, gate: bool) -> int:
    command = [sys.executable, str(QUALITY / name)]
    if gate:
        command.append("--gate")
    print(f"\n=== {name} ===")
    return subprocess.call(command, cwd=ROOT)


def optional_tools() -> None:
    print("\n=== optional C++ tools ===")
    for tool in ("clang-format", "clang-tidy", "cppcheck", "jscpd"):
        path = shutil.which(tool)
        print(f"  {tool}: {path or 'not installed'}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--all", action="store_true", help="report the whole tree (default)")
    parser.add_argument("--gate", action="store_true", help="exit nonzero on findings")
    args = parser.parse_args()

    failures = 0
    for report in REPORTS:
        failures += 1 if run_report(report, args.gate) != 0 else 0
    optional_tools()

    print(f"\n[lint] report_failures={failures} gate={args.gate}")
    return 1 if args.gate and failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
