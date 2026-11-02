#!/usr/bin/env python3
"""One mandatory gate for AI/human edits.

Runs the canonical build, focused selftests, lint against the accepted
pre-existing baseline, JSON sync checks, and the fast runtime scenarios.
Exit nonzero when any required step fails.

Usage:
  python devscripts/quality/agent-gate.py
  python devscripts/quality/agent-gate.py --fast
  python devscripts/quality/agent-gate.py --no-build --skip-scenarios
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
EXE = ROOT / ".dev" / "current" / "mimita.exe"
SELFTESTS = (
    "--grenade-selftest",
    "--area-effect-selftest",
    "--team-brain-selftest",
    "--objective-selftest",
    "--npc-navigation-selftest",
    "--collision-selftest",
    "--canonical-contact-selftest",
)
SCENARIOS = (
    "devscripts/scenarios/grenade_tick_contract_v1.json",
    "devscripts/scenarios/npc_wall_containment_v1.json",
)


def run(label: str, command: list[str], timeout: int = 900) -> int:
    print(f"\n[GATE] {label}: {' '.join(command)}")
    result = subprocess.run(command, cwd=ROOT, timeout=timeout)
    print(f"[GATE] {label}: {'PASS' if result.returncode == 0 else 'FAIL'}")
    return result.returncode


def latest_exe() -> Path:
    try:
        raw = subprocess.check_output(
            [sys.executable, "devscripts/dev-loop.py", "--status", "--json"],
            cwd=ROOT, text=True, timeout=30)
        state = json.loads(raw)
        path = state.get("build_identity", {}).get("path")
        if path and Path(path).exists():
            return Path(path)
    except (OSError, ValueError, subprocess.SubprocessError):
        pass
    return EXE


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--no-build", action="store_true")
    parser.add_argument("--fast", action="store_true",
                        help="skip runtime scenarios")
    parser.add_argument("--skip-scenarios", action="store_true")
    args = parser.parse_args()

    failures: list[str] = []

    if not args.no_build:
        if run("build", [sys.executable, "build.py", "build-only"]) != 0:
            failures.append("build")

    exe = latest_exe()
    if not exe.exists():
        failures.append(f"executable missing: {exe}")
    else:
        for selftest in SELFTESTS:
            if run(f"selftest {selftest}", [str(exe), selftest]) != 0:
                failures.append(selftest)

    if run("lint baseline gate",
           [sys.executable, "devscripts/quality/lint.py", "--gate"]) != 0:
        failures.append("lint")

    if run("weapon JSON sync",
           [sys.executable, "devscripts/weapon_json_sync.py", "--check"]) != 0:
        failures.append("weapon-json-sync")
    if run("gamemode JSON sync",
           [sys.executable, "devscripts/gamemode_json_sync.py", "--check"]) != 0:
        failures.append("gamemode-json-sync")

    if not (args.fast or args.skip_scenarios):
        for scenario in SCENARIOS:
            if not (ROOT / scenario).exists():
                print(f"[GATE] scenario missing: {scenario}")
                continue
            if run(f"scenario {scenario}",
                   [sys.executable, "devscripts/dev-loop.py",
                    "--scenario-file", scenario, "--json"],
                   timeout=900) != 0:
                failures.append(scenario)

    print("\n" + "=" * 60)
    if failures:
        print(f"[GATE] RED — {len(failures)} failure(s):")
        for failure in failures:
            print(f"  - {failure}")
        print("Fix the edit before considering the task complete.")
        return 1
    print("[GATE] GREEN — build, selftests, lint baseline, config sync, "
          "and scenarios passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
