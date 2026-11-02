#!/usr/bin/env python3
"""Run MiMITA repository-local quality reports.

Report-only by default. Use --gate to compare report metrics against the
accepted pre-existing baseline. The gate fails when a metric increases, a new
report appears, or the baseline is missing.
"""

from __future__ import annotations

import argparse
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
QUALITY = ROOT / "devscripts" / "quality"
BASELINE = QUALITY / "gate_baseline.json"
REPORTS = (
    "check_damage_authority.py",
    "check_contact_ownership.py",
    "dead_code_report.py",
    "duplicate_report.py",
    "coverage_report.py",
)

METRIC_PATTERNS = {
    "damage-authority": {"findings": re.compile(r"findings=(\d+)")},
    "contact-ownership": {
        "weapon_id_branches": re.compile(r"weapon_id_branches=(\d+)"),
        "duplicate_function_names": re.compile(r"duplicate_function_names=(\d+)"),
    },
    "dead-code": {"markers": re.compile(r"markers=(\d+)")},
    "duplicate-report": {
        "repeated_windows": re.compile(r"repeated_windows=(\d+)")
    },
    "coverage": {"artifacts": re.compile(r"artifacts=(\d+)")},
}


def load_baseline() -> dict:
    if not BASELINE.exists():
        return {}
    try:
        return json.loads(BASELINE.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        print(f"[lint] baseline invalid {BASELINE}: {error}")
        return {}


def metrics_for(name: str, text: str) -> dict:
    found: dict[str, int] = {}
    for key, pattern in METRIC_PATTERNS.get(name, {}).items():
        match = pattern.search(text)
        if match:
            found[key] = int(match.group(1))
    return found


def run_report(name: str) -> tuple[int, str, dict]:
    command = [sys.executable, str(QUALITY / name)]
    print(f"\n=== {name} ===")
    result = subprocess.run(command, cwd=ROOT, text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    text = result.stdout or ""
    print(text, end="" if text.endswith("\n") else "\n")
    key = {
        "check_damage_authority.py": "damage-authority",
        "check_contact_ownership.py": "contact-ownership",
        "dead_code_report.py": "dead-code",
        "duplicate_report.py": "duplicate-report",
        "coverage_report.py": "coverage",
    }[name]
    return result.returncode, key, metrics_for(key, text)


def optional_tools() -> None:
    print("\n=== optional C++ tools ===")
    for tool in ("clang-format", "clang-tidy", "cppcheck", "jscpd"):
        path = shutil.which(tool)
        print(f"  {tool}: {path or 'not installed'}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--all", action="store_true",
                        help="report the whole tree (default)")
    parser.add_argument("--gate", action="store_true",
                        help="compare metrics against gate_baseline.json")
    parser.add_argument("--update-baseline", action="store_true",
                        help="write current metrics as the accepted baseline")
    args = parser.parse_args()

    baseline = load_baseline()
    current: dict[str, dict[str, int]] = {}
    failures: list[str] = []
    for report in REPORTS:
        _, key, metrics = run_report(report)
        current[key] = metrics
        if not args.gate or args.update_baseline:
            continue
        accepted = baseline.get(key)
        if accepted is None:
            failures.append(f"{key}: no baseline entry")
            continue
        for metric, value in metrics.items():
            limit = accepted.get(metric)
            if limit is None:
                failures.append(f"{key}.{metric}: new metric ({value})")
            elif value > limit:
                failures.append(f"{key}.{metric}: {value} > baseline {limit}")

    optional_tools()

    if args.update_baseline:
        BASELINE.write_text(json.dumps(current, indent=2) + "\n",
                            encoding="utf-8")
        print(f"\n[lint] baseline updated {BASELINE}")
        return 0

    if not args.gate:
        print("\n[lint] report-only")
        return 0

    if failures:
        print("\n[lint] GATE FAILED")
        for failure in failures:
            print(f"  - {failure}")
        print("  Fix the new finding or deliberately update "
              "devscripts/quality/gate_baseline.json with a changelog note.")
        return 1

    print("\n[lint] GATE PASSED against accepted pre-existing baseline")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
