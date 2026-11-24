#!/usr/bin/env python3
"""Launch the shared dev loop directly into a two-client Bloodtest match."""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEV_LOOP = ROOT / "devscripts" / "dev-loop.py"
LAUNCH_MODE = "51"


def dev_loop(*args: str, timeout: float = 60.0) -> subprocess.CompletedProcess:
    return subprocess.run(
        [sys.executable, str(DEV_LOOP), *args],
        cwd=ROOT,
        text=True,
        capture_output=True,
        timeout=timeout,
    )


def read_status() -> dict:
    result = dev_loop("--status", "--json", timeout=30.0)
    text = result.stdout + result.stderr
    start = text.find("{")
    if start < 0:
        raise RuntimeError("dev-loop status did not return JSON")
    return json.loads(text[start:])


def online_clients(status: dict) -> list[dict]:
    return [
        instance for instance in status.get("instances", [])
        if instance.get("role") == "client" and instance.get("state") == "online"
    ]


def mode_active(run_dir: str) -> bool:
    if not run_dir:
        return False
    for path in Path(run_dir).glob("client-*/events*.jsonl"):
        try:
            text = path.read_text(errors="replace")
        except OSError:
            continue
        if '"event":"network.mode.start"' in text and '"stage":"active"' in text:
            return True
    return False


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Launch and wait for a ready two-client Bloodtest match.")
    parser.add_argument(
        "--timeout", type=float, default=300.0,
        help="seconds to wait for build, server, and both clients")
    parser.add_argument(
        "--replace", action="store_true",
        help="replace a non-Bloodtest run owned by another agent")
    args = parser.parse_args()

    ensure = dev_loop("--ensure-running", "--no-dashboard", timeout=120.0)
    if ensure.returncode != 0:
        print("[BLOODTEST QUICK] " + (ensure.stdout + ensure.stderr).strip(),
              file=sys.stderr)
        return ensure.returncode

    status = read_status()
    current_mode = str(status.get("effective_mode") or "")
    if status.get("run_alive") and current_mode and current_mode != "bloodtest":
        if not args.replace:
            print(
                f"[BLOODTEST QUICK] dev loop is running mode={current_mode} "
                f"run={status.get('run_id')}; use --replace to stop it",
                file=sys.stderr)
            return 2

    launch = dev_loop(
        "--request", "launch", "--launch-mode", LAUNCH_MODE,
        timeout=120.0)
    print("[BLOODTEST QUICK] " + (launch.stdout + launch.stderr).strip())
    if launch.returncode != 0:
        return launch.returncode

    deadline = time.monotonic() + max(1.0, args.timeout)
    last_summary = ""
    second_requested = False
    teams_sent = False
    while time.monotonic() < deadline:
        status = read_status()
        mode = str(status.get("effective_mode") or "")
        run_id = str(status.get("run_id") or "")
        clients = online_clients(status)
        summary = (
            f"build={status.get('build_status')} "
            f"server_ready={bool(status.get('server_ready'))} "
            f"clients={len(clients)} mode={mode or '-'}")
        if summary != last_summary:
            print(f"[BLOODTEST QUICK] {summary}")
            last_summary = summary

        if mode_active(str(status.get("run_dir") or "")):
            print("[BLOODTEST QUICK] ready")
            print(f"  run: {run_id}")
            print(f"  server journal: {status.get('server_journal_path')}")
            print(f"  client journal: {status.get('client_journal_path')}")
            print(
                "  verify: rg 'gamemode.match.configured|client.movement.profile.applied|"
                "gamemode.bodycam.applied' "
                f"\"{status.get('run_dir')}\"")
            return 0

        if status.get("run_alive") and status.get("server_ready") and \
                run_id and mode == "bloodtest":
            if len(clients) < 2 and not second_requested:
                server = next(
                    (i for i in status.get("instances", [])
                     if i.get("role") == "server" and i.get("state") == "online"),
                    None)
                if server:
                    second_requested = True
                    started = dev_loop(
                        "--start-client", "--server",
                        str(server.get("instance_id") or "server-01"),
                        "--name", "Bloodtest B", timeout=120.0)
                    print("[BLOODTEST QUICK] second client: " +
                          (started.stdout + started.stderr).strip())
            elif len(clients) >= 2 and not teams_sent:
                teams_sent = True
                time.sleep(2.0)
                for client_id, team in (("client-01", "2"), ("client-02", "1")):
                    result = dev_loop(
                        "--command", f"team_pick {team}",
                        "--client", client_id,
                        "--expected-run-id", run_id, timeout=30.0)
                    print(f"[BLOODTEST QUICK] {client_id} team_pick {team}: " +
                          (result.stdout + result.stderr).strip())
        time.sleep(2.0)

    print("[BLOODTEST QUICK] timed out waiting for a ready match", file=sys.stderr)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())


