#!/usr/bin/env python3
import argparse
import datetime as _dt
import json
import os
import queue
import re
import secrets
import subprocess
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
EXE = ROOT / "mimita.exe"
LOG_ROOT = ROOT / "build" / "ice-multiplayer-logs"
EVENT_PREFIX = "[MIMITA_TEST_EVENT] "


def now_stamp():
    return _dt.datetime.now().strftime("%Y%m%d-%H%M%S")


class LocalIceCoordinator:
    def __init__(self):
        self.rooms = {}
        self.tokens = {}
        self.lock = threading.Lock()
        self.httpd = None
        self.thread = None

    def start(self):
        owner = self

        class Handler(BaseHTTPRequestHandler):
            def log_message(self, fmt, *args):
                return

            def do_POST(self):
                length = int(self.headers.get("content-length", "0"))
                body = self.rfile.read(length).decode("utf-8", errors="replace")
                try:
                    data = json.loads(body or "{}")
                except json.JSONDecodeError:
                    self.reply({"ok": False, "error": "bad-json"})
                    return
                result = owner.handle(self.path, data)
                self.reply(result)

            def reply(self, data):
                encoded = json.dumps(data, separators=(",", ":")).encode("utf-8")
                self.send_response(200)
                self.send_header("content-type", "application/json")
                self.send_header("content-length", str(len(encoded)))
                self.end_headers()
                self.wfile.write(encoded)

        self.httpd = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        self.thread = threading.Thread(target=self.httpd.serve_forever, daemon=True)
        self.thread.start()
        return f"http://127.0.0.1:{self.httpd.server_address[1]}"

    def stop(self):
        if self.httpd:
            self.httpd.shutdown()
            self.httpd.server_close()
            self.httpd = None

    def handle(self, path, data):
        with self.lock:
            if path == "/api/coordinator/ice/host":
                code = self._new_code()
                token = self._new_token()
                self.rooms[code] = {
                    "host_slots": [{
                        "host_session_id": data.get("host_session_id", ""),
                        "host_ice_description": data.get("ice_description", ""),
                        "assigned": False,
                    }],
                    "pending": [],
                }
                self.tokens[token] = {"room": code, "used": False}
                return {"ok": True, "room_code": code, "join_token": token}

            if path == "/api/coordinator/ice/host-peer":
                code = data.get("room_code", "")
                room = self.rooms.get(code)
                if not room:
                    return {"ok": False}
                token = self._new_token()
                room["host_slots"].append({
                    "host_session_id": data.get("host_session_id", ""),
                    "host_ice_description": data.get("ice_description", ""),
                    "assigned": False,
                })
                self.tokens[token] = {"room": code, "used": False}
                return {"ok": True, "room_code": code, "join_token": token}

            if path == "/api/coordinator/ice/join":
                code = data.get("room_code", "")
                room = self.rooms.get(code)
                if not room:
                    return {"ok": False}
                slot = next((s for s in room["host_slots"] if not s["assigned"]), None)
                if not slot:
                    return {"ok": False, "error": "no-host-slot"}
                slot["assigned"] = True
                token = self._new_token()
                self.tokens[token] = {
                    "room": code,
                    "client_session_id": data.get("client_session_id", ""),
                    "used": False,
                }
                room["pending"].append({
                    "host_session_id": slot["host_session_id"],
                    "client_session_id": data.get("client_session_id", ""),
                    "client_ice_description": data.get("ice_description", ""),
                })
                return {
                    "ok": True,
                    "host_ice_description": slot["host_ice_description"],
                    "client_session_id": data.get("client_session_id", ""),
                    "join_token": token,
                }

            if path == "/api/coordinator/ice/poll":
                code = data.get("room_code", "")
                room = self.rooms.get(code)
                if not room:
                    return {"ok": False}
                host_session_id = data.get("host_session_id", "")
                for i, client in enumerate(room["pending"]):
                    if client["host_session_id"] != host_session_id:
                        continue
                    room["pending"].pop(i)
                    return {
                        "ok": True,
                        "status": "client_ready",
                        "client_ice_description": client["client_ice_description"],
                        "client_session_id": client["client_session_id"],
                    }
                return {"ok": True, "status": "waiting_client"}

            if path == "/api/coordinator/ice/validate-join":
                code = data.get("room_code", "")
                token = data.get("join_token", "")
                entry = self.tokens.get(token)
                valid = bool(entry and entry["room"] == code and not entry["used"])
                if valid:
                    entry["used"] = True
                return {"ok": True, "valid": valid}

            if path == "/api/coordinator/ice/done":
                code = data.get("room_code", "")
                self.rooms.pop(code, None)
                return {"ok": True}

            return {"ok": False, "error": "unknown-path"}

    def _new_code(self):
        alphabet = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789"
        while True:
            code = "".join(secrets.choice(alphabet) for _ in range(7))
            if code not in self.rooms:
                return code

    def _new_token(self):
        return secrets.token_urlsafe(24)


class ProcessLog:
    def __init__(self, name, proc, log_dir):
        self.name = name
        self.proc = proc
        self.lines = []
        self.events = []
        self.queue = queue.Queue()
        self.stdout_path = log_dir / f"{name}.stdout.log"
        self.stderr_path = log_dir / f"{name}.stderr.log"
        self.stdout_file = self.stdout_path.open("w", encoding="utf-8", errors="replace")
        self.stderr_file = self.stderr_path.open("w", encoding="utf-8", errors="replace")
        self.threads = [
            threading.Thread(target=self._read_stream, args=(proc.stdout, self.stdout_file, False), daemon=True),
            threading.Thread(target=self._read_stream, args=(proc.stderr, self.stderr_file, True), daemon=True),
        ]
        for thread in self.threads:
            thread.start()

    def _read_stream(self, stream, file, is_stderr):
        for line in iter(stream.readline, ""):
            file.write(line)
            file.flush()
            text = line.rstrip("\r\n")
            self.lines.append(text)
            self.queue.put((self.name, text, is_stderr))
            event_pos = text.find(EVENT_PREFIX)
            if event_pos >= 0:
                try:
                    self.events.append(json.loads(text[event_pos + len(EVENT_PREFIX):]))
                except json.JSONDecodeError:
                    pass

    def close_logs(self):
        self.stdout_file.close()
        self.stderr_file.close()


def start_process(name, args, env, log_dir):
    proc = subprocess.Popen(
        [str(EXE)] + args,
        cwd=ROOT,
        env=env,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        bufsize=1,
    )
    return ProcessLog(name, proc, log_dir)


def terminate_all(processes):
    for item in processes:
        if item.proc.poll() is None:
            item.proc.terminate()
    deadline = time.time() + 5
    for item in processes:
        while item.proc.poll() is None and time.time() < deadline:
            time.sleep(0.05)
        if item.proc.poll() is None:
            item.proc.kill()
    for item in processes:
        item.close_logs()


def collect_events(processes):
    events = []
    for item in processes:
        for event in item.events:
            copied = dict(event)
            copied["process"] = item.name
            events.append(copied)
    return events


def find_room(process, timeout):
    deadline = time.time() + timeout
    room_re = re.compile(r"code=([A-Z0-9]+)")
    while time.time() < deadline:
        for event in process.events:
            if event.get("type") == "room_created" and event.get("code"):
                return event["code"]
        for line in process.lines:
            m = room_re.search(line)
            if m:
                return m.group(1)
        if process.proc.poll() is not None:
            break
        time.sleep(0.05)
    return None


def event_count(events, event_type, process_prefix=None):
    return sum(
        1 for event in events
        if event.get("type") == event_type and
        (process_prefix is None or str(event.get("process", "")).startswith(process_prefix))
    )


def diagnose(processes, events):
    all_lines = "\n".join(line for p in processes for line in p.lines)
    if "packet-too-large" in all_lines or "oversized" in all_lines:
        return "oversized application datagram was rejected"
    if "connection-timeout" in all_lines:
        return "ICE connection timed out"
    if "join-rejected" in all_lines or event_count(events, "join_rejected"):
        return "join token was rejected"
    if event_count(events, "join_accepted", "client") == 0:
        return "client never received JoinAccept"
    if event_count(events, "snapshot_received", "client") == 0:
        return "client never received compact snapshots"
    return "expected networking events did not complete"


def main():
    parser = argparse.ArgumentParser(description="Run automated MiMITA ICE multiplayer process test.")
    parser.add_argument("--clients", type=int, default=2)
    parser.add_argument("--duration", type=int, default=60)
    parser.add_argument("--build", action="store_true")
    parser.add_argument("--same-machine", action="store_true", default=False)
    parser.add_argument("--disable-relay", action="store_true", default=False)
    parser.add_argument("--force-relay", action="store_true", default=False)
    parser.add_argument("--packet-loss", type=float, default=0.0)
    parser.add_argument("--latency-ms", type=int, default=0)
    parser.add_argument("--jitter-ms", type=int, default=0)
    parser.add_argument("--keep-logs", action="store_true")
    parser.add_argument("--verbose", action="store_true")
    args = parser.parse_args()

    if args.force_relay and args.disable_relay:
        parser.error("--force-relay and --disable-relay are mutually exclusive")

    if not EXE.exists():
        print(f"FAIL: missing executable {EXE}")
        return 2

    log_dir = LOG_ROOT / now_stamp()
    log_dir.mkdir(parents=True, exist_ok=True)
    print(f"[ICE HARNESS] logs={log_dir}")

    coordinator = LocalIceCoordinator()
    coordinator_url = coordinator.start()
    print(f"[ICE HARNESS] local coordinator={coordinator_url}")

    env = os.environ.copy()
    env["MIMITA_COORDINATOR_URL"] = coordinator_url
    if args.same_machine and args.disable_relay and not args.force_relay:
        env["MIMITA_ICE_LOCAL_ONLY"] = "1"

    mode_flags = ["--timeout-seconds", str(max(args.duration + 5, 15))]
    if args.disable_relay:
        mode_flags.append("--disable-relay")
    if args.force_relay:
        mode_flags.append("--force-relay")

    processes = []
    try:
        host = start_process(
            "host",
            ["--ice-host-only", "--once", "--clients", str(args.clients)] + mode_flags,
            env,
            log_dir,
        )
        processes.append(host)
        room = find_room(host, 30)
        if not room:
            print("FAIL: host did not emit a room code")
            return 3
        print(f"[ICE HARNESS] room={room}")

        for i in range(args.clients):
            client = start_process(
                f"client{i + 1}",
                ["--ice-join-only", room] + mode_flags,
                env,
                log_dir,
            )
            processes.append(client)
            time.sleep(0.25)

        deadline = time.time() + max(args.duration + 30, 45)
        while time.time() < deadline:
            if all(p.proc.poll() is not None for p in processes):
                break
            time.sleep(0.1)

        terminate_all(processes)
        coordinator.stop()

        events = collect_events(processes)
        (log_dir / "events.json").write_text(
            json.dumps(events, indent=2),
            encoding="utf-8",
        )
        for process in processes:
            print(f"[ICE HARNESS] {process.name} returncode={process.proc.returncode}")

        host_ok = host.proc.returncode == 0
        clients_ok = all(p.proc.returncode == 0 for p in processes if p.name.startswith("client"))
        connected_clients = event_count(events, "ice_connected", "client")
        accepted_clients = event_count(events, "join_accepted", "client")
        snapshots = event_count(events, "snapshot_received", "client")
        inputs = event_count(events, "input_received", "host")

        print(
            "[ICE HARNESS SUMMARY] "
            f"clients={args.clients} connected={connected_clients} "
            f"accepted={accepted_clients} snapshots={snapshots} inputs={inputs}"
        )

        if args.clients > 1 and accepted_clients < args.clients:
            print("FAIL: multi-client ICE is not complete; host accepted fewer clients than requested")
            print(f"DIAGNOSIS: {diagnose(processes, events)}")
            return 10

        if not host_ok or not clients_ok:
            print(f"FAIL: child process failed. DIAGNOSIS: {diagnose(processes, events)}")
            return 11

        if connected_clients < args.clients or accepted_clients < args.clients or snapshots == 0:
            print(f"FAIL: missing expected events. DIAGNOSIS: {diagnose(processes, events)}")
            return 12

        if inputs < args.clients:
            print("FAIL: host did not receive input from every client")
            print(f"DIAGNOSIS: {diagnose(processes, events)}")
            return 13

        print("[ICE HARNESS] PASS")
        return 0
    finally:
        terminate_all(processes)
        coordinator.stop()


if __name__ == "__main__":
    raise SystemExit(main())
