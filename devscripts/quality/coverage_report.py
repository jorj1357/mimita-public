#!/usr/bin/env python3
"""Locate coverage artifacts and report available never-executed functions."""

from __future__ import annotations

import argparse
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CANDIDATE_ROOTS = (ROOT / ".dev", ROOT / "logs", ROOT / "build")
CANDIDATE_NAMES = ("lcov.info", "coverage.json", "coverage.xml", ".coverage")


def find_artifacts() -> list[Path]:
    found: list[Path] = []
    for base in CANDIDATE_ROOTS:
        if not base.exists():
            continue
        for name in CANDIDATE_NAMES:
            found.extend(base.rglob(name))
    return sorted(set(found))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--gate", action="store_true")
    args = parser.parse_args()

    artifacts = find_artifacts()
    print(f"[coverage] artifacts={len(artifacts)}")
    for path in artifacts[:40]:
        print(f"  {path.relative_to(ROOT)}")
    if not artifacts:
        print("  No coverage artifact found. Run the scenario matrix with coverage enabled.")
    # This script intentionally does not guess which functions are dead.
    if args.gate and not artifacts:
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
