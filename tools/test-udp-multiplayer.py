#!/usr/bin/env python3
# 07 21 2026, 18 32
# purpose
# Runs a live UDP gameplay networking proof with a dedicated server and smoke client.
# Captures server readiness, gameplay packet milestones, and transport counters.
# Writes events.json next to stdout/stderr logs for audit-friendly validation.
# Does NOT build mimita.exe, use stale executable fallbacks, or contact the coordinator.
# Does NOT test ICE relay behavior, rendering, UI, or production deployment.
# Does NOT modify movement formulas, weapon behavior, projectile simulation, or packets.

import argparse
import datetime as _dt
import json
import os
import re
import socket
import subprocess
import sys
import threading
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
EXE = Path(os.environ.get("MIMITA_TEST_EXE", ROOT / "mimita.exe"))
SMOKE = ROOT / "build" / "network-protocol-smoke.exe"
LOG_ROOT = ROOT / "build" / "udp-multiplayer-logs"
READY_RE = re.compile(r"\[SERVER TRANSPORT READY\].*actual=([0-9.]+:\d+)")
STATUS_RE = re.compile(r"\[SERVER STATUS\]\s+(.*)")
SMOKE_RE = re.compile(r"\[PROTOCOL SMOKE\]\s+(.*)")


def stamp():
    return _dt.datetime.now().strftime("%Y%m%d-%H%M%S")


def reserve_udp_port():
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.bind(("127.0.0.1", 0))
        return sock.getsockname()[1]


def parse_kv(text):
    out = {}
    for part in text.split():
        if "=" not in part:
            continue
        key, value = part.split("=", 1)
        value = value.rstrip(",")
        if "/" in value:
            out[key] = value
            continue
        try:
            out[key] = int(value)
        except ValueError:
            try:
                out[key] = float(value)
            except ValueError:
                out[key] = value
    return out


class ProcessLog:
    def __init__(self, name, proc, log_dir):
        self.name = name
        self.proc = proc
        self.lines = []
        self.stdout_path = log_dir / f"{name}.stdout.log"
        self.stderr_path = log_dir / f"{name}.stderr.log"
        self.stdout_file = self.stdout_path.open("w", encoding="utf-8", errors="replace")
        self.stderr_file = self.stderr_path.open("w", encoding="utf-8", errors="replace")
        self.threads = [
            threading.Thread(target=self._read_stream, args=(proc.stdout, self.stdout_file), daemon=True),
            threading.Thread(target=self._read_stream, args=(proc.stderr, self.stderr_file), daemon=True),
        ]
        for thread in self.threads:
            thread.start()

    def _read_stream(self, stream, file):
        for line in iter(stream.readline, ""):
            file.write(line)
            file.flush()
            self.lines.append(line.rstrip("\r\n"))

    def close(self):
        self.stdout_file.close()
        self.stderr_file.close()


def find_ready(process, timeout):
    deadline = time.time() + timeout
    while time.time() < deadline:
        for line in process.lines:
            match = READY_RE.search(line)
            if match:
                return match.group(1)
        if process.proc.poll() is not None:
            return None
        time.sleep(0.05)
    return None


def wait_process(process, timeout):
    deadline = time.time() + timeout
    while process.proc.poll() is None and time.time() < deadline:
        time.sleep(0.05)
    if process.proc.poll() is None:
        process.proc.terminate()
        try:
            process.proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.proc.kill()
            process.proc.wait(timeout=5)


def main():
    parser = argparse.ArgumentParser(description="Run MiMITA live UDP gameplay packet proof.")
    parser.add_argument("--timeout", type=int, default=16)
    parser.add_argument("--verbose", action="store_true")
    args = parser.parse_args()

    if not EXE.exists():
        print(f"FAIL: missing executable {EXE}")
        return 2
    if not SMOKE.exists():
        print(f"FAIL: missing smoke test executable {SMOKE}")
        return 3

    log_dir = LOG_ROOT / stamp()
    log_dir.mkdir(parents=True, exist_ok=True)
    print(f"[UDP MULTIPLAYER HARNESS] logs={log_dir}")

    port = reserve_udp_port()
    endpoint = f"127.0.0.1:{port}"
    server_cmd = [
        str(EXE),
        "--server",
        "--bind",
        endpoint,
        "--timeout",
        str(args.timeout),
        "--no-coordinator",
    ]
    proc = subprocess.Popen(
        server_cmd,
        cwd=ROOT,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        bufsize=1,
    )
    server = ProcessLog("udp-game-server", proc, log_dir)
    events = []

    try:
        actual = find_ready(server, 8)
        if not actual:
            events.append({"type": "server_ready", "ok": False, "requested": endpoint})
            wait_process(server, 1)
            (log_dir / "events.json").write_text(json.dumps(events, indent=2), encoding="utf-8")
            print("FAIL: UDP game server did not emit ready endpoint")
            return 4
        events.append({"type": "server_ready", "ok": True, "requested": endpoint, "actual": actual})

        env = os.environ.copy()
        env["MIMITA_TEST_SERVER_ADDR"] = actual
        smoke = subprocess.run(
            [str(SMOKE)],
            cwd=ROOT,
            env=env,
            capture_output=True,
            text=True,
            timeout=25,
        )
        (log_dir / "network-protocol-smoke.stdout.log").write_text(
            smoke.stdout, encoding="utf-8", errors="replace")
        (log_dir / "network-protocol-smoke.stderr.log").write_text(
            smoke.stderr, encoding="utf-8", errors="replace")

        smoke_fields = {}
        for line in smoke.stdout.splitlines():
            match = SMOKE_RE.search(line)
            if match:
                smoke_fields = parse_kv(match.group(1))
        smoke_fields["type"] = "protocol_smoke"
        smoke_fields["returncode"] = smoke.returncode
        events.append(smoke_fields)

        wait_process(server, args.timeout + 3)
        last_status = None
        for line in server.lines:
            match = STATUS_RE.search(line)
            if match:
                last_status = parse_kv(match.group(1))
        if last_status:
            last_status["type"] = "server_status"
            events.append(last_status)

        (log_dir / "events.json").write_text(json.dumps(events, indent=2), encoding="utf-8")

        required = [
            "movement",
            "lifecycleDeath",
            "autoRespawn",
            "instantRespawn",
            "noDouble",
            "livingNoEffect",
            "reconnect",
            "reconnectMovement",
            "grenadeAccept",
            "grenadeIdempotent",
            "grenadeReject",
            "grenadeSecondAccept",
            "statePackets",
        ]
        smoke_ok = smoke.returncode == 0 and all(smoke_fields.get(key) == 1 for key in required)
        server_ok = server.proc.returncode == 0
        if args.verbose or not smoke_ok or not server_ok:
            print(smoke.stdout[-4000:])
            print(smoke.stderr[-4000:])
            for line in server.lines[-120:]:
                print(line)
        print(
            "[UDP MULTIPLAYER SUMMARY] "
            f"serverReturn={server.proc.returncode} smokeReturn={smoke.returncode} "
            f"packetsIn={last_status.get('packetsIn') if last_status else '?'} "
            f"packetsOut={last_status.get('packetsOut') if last_status else '?'}"
        )
        print("[UDP MULTIPLAYER HARNESS] PASS" if smoke_ok and server_ok else "[UDP MULTIPLAYER HARNESS] FAIL")
        return 0 if smoke_ok and server_ok else 5
    finally:
        wait_process(server, 1)
        server.close()


if __name__ == "__main__":
    raise SystemExit(main())
