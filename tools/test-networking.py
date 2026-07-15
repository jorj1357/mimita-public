#!/usr/bin/env python3
import argparse
import datetime as _dt
import os
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
EXE = ROOT / "mimita.exe"
LOG_ROOT = ROOT / "build" / "network-test-logs"


def stamp():
    return _dt.datetime.now().strftime("%Y%m%d-%H%M%S")


def run_capture(name, command, log_dir, env=None, timeout=120, verbose=False):
    print(f"[TEST] {name}: {' '.join(map(str, command))}")
    proc = subprocess.run(
        command,
        cwd=ROOT,
        env=env,
        capture_output=True,
        text=True,
        timeout=timeout,
    )
    (log_dir / f"{name}.stdout.log").write_text(proc.stdout, encoding="utf-8", errors="replace")
    (log_dir / f"{name}.stderr.log").write_text(proc.stderr, encoding="utf-8", errors="replace")
    if verbose or proc.returncode != 0:
        if proc.stdout:
            print(proc.stdout[-4000:])
        if proc.stderr:
            print(proc.stderr[-4000:])
    return proc.returncode == 0, proc.returncode


def build_if_requested(args, log_dir):
    if not args.build:
        return True
    ok, code = run_capture(
        "build-agent",
        [sys.executable, "build_agent.py"],
        log_dir,
        timeout=300,
        verbose=args.verbose,
    )
    if not ok:
        print(f"[FAIL] build_agent.py failed returncode={code}")
        return False
    changelog = ROOT / "build" / "changelog.txt"
    if changelog.exists():
        lines = changelog.read_text(encoding="utf-8", errors="replace").splitlines()
        status = next((line for line in lines if line.startswith("Status:")), "")
        print(f"[BUILD] {status}")
        if "FAILED" in status:
            return False
    return True


def run_snapshot_selftest(args, log_dir):
    if not EXE.exists():
        print(f"[FAIL] missing executable: {EXE}")
        return False
    ok, code = run_capture(
        "snapshot-chunk-selftest",
        [str(EXE), "--snapshot-chunk-selftest"],
        log_dir,
        timeout=30,
        verbose=args.verbose,
    )
    if not ok:
        print(f"[FAIL] snapshot chunk selftest failed returncode={code}")
    else:
        print("[PASS] snapshot chunk selftest")
    return ok


def run_ice_harness(args, log_dir):
    harness = ROOT / "tools" / "test-ice-multiplayer.py"
    command = [
        sys.executable,
        str(harness),
        "--clients",
        str(args.clients),
        "--duration",
        str(args.duration),
    ]
    if args.disable_relay:
        command.append("--disable-relay")
    if args.force_relay:
        command.append("--force-relay")
    if args.verbose:
        command.append("--verbose")
    if args.keep_logs:
        command.append("--keep-logs")
    ok, code = run_capture(
        "ice-multiplayer",
        command,
        log_dir,
        timeout=max(60, args.duration + 90),
        verbose=True,
    )
    if not ok:
        print(f"[FAIL] ICE multiplayer harness failed returncode={code}")
    else:
        print("[PASS] ICE multiplayer harness")
    return ok


def main():
    parser = argparse.ArgumentParser(description="Run MiMITA networking tests.")
    parser.add_argument("--all", action="store_true", help="Run all available networking tests.")
    parser.add_argument("--quick", action="store_true", help="Run only fast deterministic checks.")
    parser.add_argument("--build", action="store_true", help="Build mimita.exe first.")
    parser.add_argument("--no-build", action="store_true", help="Do not build, even under --all.")
    parser.add_argument("--clients", type=int, default=2)
    parser.add_argument("--duration", type=int, default=60)
    parser.add_argument("--same-machine", action="store_true", default=True)
    parser.add_argument("--disable-relay", action="store_true", default=True)
    parser.add_argument("--force-relay", action="store_true")
    parser.add_argument("--packet-loss", type=float, default=0.0)
    parser.add_argument("--latency-ms", type=int, default=0)
    parser.add_argument("--jitter-ms", type=int, default=0)
    parser.add_argument("--keep-logs", action="store_true")
    parser.add_argument("--verbose", action="store_true")
    args = parser.parse_args()

    if args.no_build:
        args.build = False
    elif args.all:
        args.build = True

    log_dir = LOG_ROOT / stamp()
    log_dir.mkdir(parents=True, exist_ok=True)
    print(f"[TEST] logs={log_dir}")

    ok = True
    ok = build_if_requested(args, log_dir) and ok
    ok = run_snapshot_selftest(args, log_dir) and ok

    if args.all and not args.quick:
        if args.packet_loss or args.latency_ms or args.jitter_ms:
            print("[WARN] network condition simulation flags are parsed but not wired yet")
        ok = run_ice_harness(args, log_dir) and ok

    if ok:
        print("[TEST NETWORKING] PASS")
        return 0
    print("[TEST NETWORKING] FAIL")
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
