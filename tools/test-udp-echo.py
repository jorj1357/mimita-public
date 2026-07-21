#!/usr/bin/env python3
# 07 21 2026, 18 32
# purpose
# Runs a live mimita.exe raw UDP echo transport check against an exact executable.
# Captures bind, receive, send, and summary logs into a per-run events.json file.
# Fails instead of falling back when the requested executable is missing.
# Does NOT launch graphics, contact the coordinator, or run gameplay simulation.
# Does NOT test ICE, movement formulas, weapons, projectiles, or rendering.
# Does NOT reuse stale server processes or silently ignore socket failures.

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
LOG_ROOT = ROOT / "build" / "udp-echo-logs"
READY_RE = re.compile(r"\[UDP ECHO READY\].*actual=([0-9.]+:\d+)")
SUMMARY_RE = re.compile(r"\[UDP ECHO SUMMARY\]\s+(.*)")


def stamp():
    return _dt.datetime.now().strftime("%Y%m%d-%H%M%S")


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


def parse_kv(text):
    out = {}
    for part in text.split():
        if "=" not in part:
            continue
        key, value = part.split("=", 1)
        try:
            out[key] = int(value)
        except ValueError:
            out[key] = value
    return out


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
    parser = argparse.ArgumentParser(description="Run MiMITA raw UDP echo transport proof.")
    parser.add_argument("--timeout", type=int, default=8)
    parser.add_argument("--payload", default="mimita-udp-echo-stage3b")
    parser.add_argument("--verbose", action="store_true")
    args = parser.parse_args()

    if not EXE.exists():
        print(f"FAIL: missing executable {EXE}")
        return 2

    log_dir = LOG_ROOT / stamp()
    log_dir.mkdir(parents=True, exist_ok=True)
    print(f"[UDP ECHO HARNESS] logs={log_dir}")

    command = [
        str(EXE),
        "--server",
        "--udp-echo",
        "--bind",
        "127.0.0.1:0",
        "--timeout",
        str(args.timeout),
        "--no-coordinator",
    ]
    proc = subprocess.Popen(
        command,
        cwd=ROOT,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        bufsize=1,
    )
    server = ProcessLog("udp-echo-server", proc, log_dir)
    events = []

    try:
        endpoint = find_ready(server, 5)
        if not endpoint:
            events.append({"type": "server_ready", "ok": False})
            wait_process(server, 1)
            (log_dir / "events.json").write_text(json.dumps(events, indent=2), encoding="utf-8")
            print("FAIL: UDP echo server did not emit ready endpoint")
            return 3

        host, port_text = endpoint.rsplit(":", 1)
        port = int(port_text)
        events.append({"type": "server_ready", "ok": True, "actual": endpoint})

        payload = args.payload.encode("utf-8")
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as client:
            client.settimeout(2.0)
            sent = client.sendto(payload, (host, port))
            data, source = client.recvfrom(2048)

        echoed = data == payload
        events.append({
            "type": "udp_echo",
            "ok": echoed,
            "sentBytes": sent,
            "receivedBytes": len(data),
            "source": f"{source[0]}:{source[1]}",
        })

        wait_process(server, args.timeout + 3)
        for line in server.lines:
            match = SUMMARY_RE.search(line)
            if match:
                stats = parse_kv(match.group(1))
                stats["type"] = "udp_echo_summary"
                events.append(stats)

        (log_dir / "events.json").write_text(json.dumps(events, indent=2), encoding="utf-8")
        if args.verbose:
            for line in server.lines[-80:]:
                print(line)
        print(f"[UDP ECHO HARNESS] returncode={server.proc.returncode} echoed={int(echoed)}")
        print("[UDP ECHO HARNESS] PASS" if echoed and server.proc.returncode == 0 else "[UDP ECHO HARNESS] FAIL")
        return 0 if echoed and server.proc.returncode == 0 else 4
    finally:
        wait_process(server, 1)
        server.close()


if __name__ == "__main__":
    raise SystemExit(main())
