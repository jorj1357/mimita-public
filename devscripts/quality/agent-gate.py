#!/usr/bin/env python3
"""Optional quality gate for manual use; it is not required to complete work.

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
import os
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
    "devscripts/scenarios/scenario_record_roundtrip_v1.json",
)


def run(label: str, command: list[str], timeout: int = 900) -> int:
    print(f"\n[GATE] {label}: {' '.join(command)}")
    result = subprocess.run(command, cwd=ROOT, timeout=timeout)
    print(f"[GATE] {label}: {'PASS' if result.returncode == 0 else 'FAIL'}")
    return result.returncode


def stop_dev_loop() -> None:
    try:
        subprocess.run(
            [sys.executable, "devscripts/dev-loop.py", "--request", "stop", "--json"],
            cwd=ROOT, timeout=60,
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    except (OSError, subprocess.SubprocessError):
        pass
    # Stop an older watchdog/daemon so it cannot rebuild or restart the client
    # while the gate is running.
    for lock_name in ("dev-loop.lock", "dev-loop-watch.lock"):
        lock = ROOT / ".dev" / lock_name
        try:
            pid = int(lock.read_text(encoding="utf-8").strip())
        except (OSError, ValueError):
            continue
        try:
            subprocess.run(["taskkill", "/PID", str(pid), "/F"],
                           stdout=subprocess.DEVNULL,
                           stderr=subprocess.DEVNULL, timeout=15)
        except (OSError, subprocess.SubprocessError):
            pass
    # A daemon can leave a build.py child holding the serialization lock.
    try:
        subprocess.run(
            ["powershell", "-NoProfile", "-Command",
             "Get-CimInstance Win32_Process -Filter \"Name='python.exe'\" | "
             "Where-Object { $_.CommandLine -match 'build.py' } | "
             "ForEach-Object { Stop-Process -Id $_.ProcessId -Force }"],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=20)
    except (OSError, subprocess.SubprocessError):
        pass
    lock = ROOT / ".dev" / "build-serialization.lock"
    try:
        data = json.loads(lock.read_text(encoding="utf-8"))
        pid = int(data.get("pid", 0))
        try:
            os.kill(pid, 0)
        except OSError:
            lock.unlink(missing_ok=True)
    except (OSError, ValueError, TypeError, json.JSONDecodeError):
        pass


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

    # A previous scenario/dev-loop build can still own the serialization lock.
    # Stop it before the canonical build so the gate does not fail on a stale
    # concurrent writer.
    stop_dev_loop()

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
                    "--scenario-file", scenario, "--json",
                    "--command-timeout", "300"],
                   timeout=1200) != 0:
                failures.append(scenario)
        if run("scenario recording roundtrip check",
               [sys.executable,
                "devscripts/quality/check_scenario_roundtrip.py"]) != 0:
            failures.append("scenario-recording-roundtrip")
        stop_dev_loop()

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
