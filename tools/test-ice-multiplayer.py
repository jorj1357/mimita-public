#!/usr/bin/env python3
# 07 21 2026, 17 32
# purpose
# Runs automated full-server ICE gameplay validation against a local coordinator.
# Routes ICE clients through the authoritative dedicated server packet pipeline.
# Captures lifecycle, movement-validator, snapshot, death, and reconnect proof.
# Does NOT use the lightweight ICE gameplay host as a passing criterion.
# Does NOT fake JoinAccept, inject clients into the server, or bypass tokens.
# Does NOT log credentials, complete tokens, or full ICE descriptions.
import argparse
import datetime as _dt
import hashlib
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
EXE = None
LOG_ROOT = ROOT / "build" / "ice-multiplayer-logs"
EVENT_PREFIX = "[MIMITA_TEST_EVENT] "
SMOKE = ROOT / "build" / "network-protocol-smoke.exe"
ROOM_RE = re.compile(r"code=([A-Z0-9]+)")
READY_RE = re.compile(r"\[SERVER TRANSPORT READY\].*actual=([0-9.]+:\d+)")
STATUS_RE = re.compile(r"\[SERVER STATUS\]\s+(.*)")
SMOKE_RE = re.compile(r"\[PROTOCOL SMOKE\]\s+(.*)")
MOVEMENT_RE = re.compile(r"\[SERVER MOVEMENT RX\].*transport=ice.*decision=([a-z]+).*reason=([a-zA-Z0-9_-]+)")


def now_stamp():
    return _dt.datetime.now().strftime("%Y%m%d-%H%M%S")


def sha256_file(path):
    digest = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest().upper()


def resolve_exe(args):
    selected = args.exe or os.environ.get("MIMITA_TEST_EXE")
    if not selected:
        print("FAIL: exact executable required; pass --exe or set MIMITA_TEST_EXE")
        return None
    exe = Path(selected).resolve()
    if not exe.exists():
        print(f"FAIL: missing executable {exe}")
        return None
    if not exe.is_file():
        print(f"FAIL: executable path is not a file {exe}")
        return None
    return exe


def parse_kv(text):
    out = {}
    for part in text.split():
        if "=" not in part:
            continue
        key, value = part.split("=", 1)
        value = value.rstrip(",")
        try:
            out[key] = int(value)
            continue
        except ValueError:
            pass
        try:
            out[key] = float(value)
            continue
        except ValueError:
            pass
        out[key] = value
    return out


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
                    "requests": {},
                }
                self.tokens[token] = {"room": code, "used": False}
                print(f"[LOCAL ICE COORD] host room={code} slots=1")
                return {"ok": True, "room_code": code, "join_token": token}

            if path == "/api/coordinator/ice/begin-join":
                code = data.get("room_code", "")
                room = self.rooms.get(code)
                if not room:
                    print(f"[LOCAL ICE COORD] begin-join room={code} status=missing")
                    return {"ok": False, "error": "room-not-found"}
                slot = next((s for s in room["host_slots"] if not s["assigned"]), None)
                reused_slot = False
                if not slot:
                    if room["host_slots"]:
                        slot = room["host_slots"][0]
                        reused_slot = True
                    else:
                        print(f"[LOCAL ICE COORD] begin-join room={code} status=no-host-slot")
                        return {"ok": False, "error": "no-host-slot"}
                if not reused_slot:
                    slot["assigned"] = True
                request_id = self._new_token()
                token = self._new_token()
                self.tokens[token] = {
                    "room": code,
                    "client_session_id": data.get("client_session_id", ""),
                    "used": False,
                }
                room.setdefault("requests", {})[request_id] = {
                    "host_session_id": slot["host_session_id"],
                    "client_session_id": data.get("client_session_id", ""),
                    "client_ice_description": data.get("ice_description", ""),
                    "host_ice_description": slot["host_ice_description"],
                    "host_peer_sdp": "",
                    "status": "pending_host",
                }
                print(f"[LOCAL ICE COORD] begin-join room={code} req={request_id[:12]} status=queued reusedSlot={int(reused_slot)}")
                return {
                    "ok": True,
                    "request_id": request_id,
                    "host_ice_description": slot["host_ice_description"],
                    "join_token": token,
                }

            if path == "/api/coordinator/ice/host-poll":
                code = data.get("room_code", "")
                room = self.rooms.get(code)
                if not room:
                    return {"ok": False, "has_request": False, "error": "room-not-found"}
                host_session_id = data.get("host_session_id", "")
                for request_id, request in room.get("requests", {}).items():
                    if request["host_session_id"] == host_session_id and request["status"] == "pending_host":
                        request["status"] = "host_received"
                        print(f"[LOCAL ICE COORD] host-poll room={code} req={request_id[:12]} status=client-ready")
                        return {
                            "ok": True,
                            "has_request": True,
                            "request_id": request_id,
                            "client_session_id": request["client_session_id"],
                            "client_ice_description": request["client_ice_description"],
                        }
                return {"ok": True, "has_request": False}

            if path == "/api/coordinator/ice/host-answer":
                code = data.get("room_code", "")
                request_id = data.get("request_id", "")
                room = self.rooms.get(code)
                request = room and room.get("requests", {}).get(request_id)
                if not request:
                    print(f"[LOCAL ICE COORD] host-answer room={code} req={request_id[:12]} status=missing")
                    return {"ok": False, "error": "request-not-found"}
                request["host_peer_sdp"] = data.get("host_peer_sdp", "")
                request["status"] = "host_answered"
                print(f"[LOCAL ICE COORD] host-answer room={code} req={request_id[:12]} status=stored")
                return {"ok": True}

            if path == "/api/coordinator/ice/client-poll":
                code = data.get("room_code", "")
                request_id = data.get("request_id", "")
                room = self.rooms.get(code)
                request = room and room.get("requests", {}).get(request_id)
                if not request:
                    return {"ok": False, "status": "missing", "error": "request-not-found"}
                if request["host_peer_sdp"] and request["status"] in ("host_answered", "complete"):
                    print(f"[LOCAL ICE COORD] client-poll room={code} req={request_id[:12]} status=answer-ready")
                    return {
                        "ok": True,
                        "status": "host_answer_ready",
                        "host_ice_description": request["host_peer_sdp"],
                    }
                return {"ok": True, "status": "waiting_host"}

            if path == "/api/coordinator/ice/request-complete":
                code = data.get("room_code", "")
                request_id = data.get("request_id", "")
                room = self.rooms.get(code)
                request = room and room.get("requests", {}).get(request_id)
                if request:
                    if not request["host_peer_sdp"]:
                        request["status"] = "complete"
                print(f"[LOCAL ICE COORD] request-complete room={code} req={request_id[:12]} found={int(bool(request))}")
                return {"ok": True}

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
                room.setdefault("requests", {})
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


def start_command(name, command, env, log_dir):
    proc = subprocess.Popen(
        command,
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
    while time.time() < deadline:
        for event in process.events:
            if event.get("type") == "room_created" and event.get("code"):
                return event["code"]
        for line in process.lines:
            m = ROOM_RE.search(line)
            if m:
                return m.group(1)
        if process.proc.poll() is not None:
            break
        time.sleep(0.05)
    return None


def find_udp_endpoint(process, timeout):
    deadline = time.time() + timeout
    while time.time() < deadline:
        for line in process.lines:
            m = READY_RE.search(line)
            if m:
                return m.group(1)
        if process.proc.poll() is not None:
            break
        time.sleep(0.05)
    return None


def parse_smoke_fields(process):
    fields = {}
    for line in process.lines:
        match = SMOKE_RE.search(line)
        if match:
            fields = parse_kv(match.group(1))
    return fields


def event_count(events, event_type, process_prefix=None):
    return sum(
        1 for event in events
        if event.get("type") == event_type and
        (process_prefix is None or str(event.get("process", "")).startswith(process_prefix))
    )


def first_event(events, event_type, process_name=None):
    for event in events:
        if event.get("type") != event_type:
            continue
        if process_name is not None and event.get("process") != process_name:
            continue
        return event
    return None


def wait_for_event(process, event_type, timeout):
    deadline = time.time() + timeout
    seen = 0
    while time.time() < deadline:
        while seen < len(process.events):
            event = process.events[seen]
            seen += 1
            if event.get("type") == event_type:
                return dict(event)
        if process.proc.poll() is not None:
            while seen < len(process.events):
                event = process.events[seen]
                seen += 1
                if event.get("type") == event_type:
                    return dict(event)
            break
        time.sleep(0.05)
    return None


def wait_process(process, timeout):
    deadline = time.time() + timeout
    while process.proc.poll() is None and time.time() < deadline:
        time.sleep(0.05)
    return process.proc.poll() is not None


def derive_server_evidence(server):
    stats = {}
    movement_decisions = []
    for line in server.lines:
        status = STATUS_RE.search(line)
        if status:
            stats = parse_kv(status.group(1))
        movement = MOVEMENT_RE.search(line)
        if movement:
            movement_decisions.append({
                "decision": movement.group(1),
                "reason": movement.group(2),
                "line": line,
            })
    return {
        "server_started": any("[SERVER TRANSPORT READY]" in line for line in server.lines),
        "ice_gather_complete": any("[ICE HOST GATHER]" in line and "complete" in line for line in server.lines),
        "ice_connected": sum(1 for line in server.lines if "[ICE HOST CONNECT]" in line and "connected" in line),
        "join_received": sum(1 for line in server.lines if "[SERVER PLAYER SPAWN] reason=join_request" in line or "[SERVER JOIN]" in line and "token=" in line),
        "map_ready": sum(1 for line in server.lines if "[SERVER MAP READY]" in line),
        "spawn_sent": sum(1 for line in server.lines if "[SPAWN TX CREATE]" in line),
        "spawn_ack": sum(1 for line in server.lines if "[SPAWN ACK ACCEPT]" in line),
        "snapshot_sent": sum(1 for line in server.lines if "[SERVER SNAPSHOT SEND]" in line),
        "death_events": sum(1 for line in server.lines if "[SERVER DEATH]" in line),
        "respawn_events": sum(1 for line in server.lines if "[SERVER RESPAWN]" in line),
        "reconnect_success": sum(1 for line in server.lines if "[SERVER RECONNECT] accepted" in line),
        "movement_decisions": movement_decisions,
        "movement_validator_reached": bool(movement_decisions),
        "movement_accepted": any(item["decision"] in ("accept", "correct") for item in movement_decisions),
        "movement_rejected_dead": any(item["decision"] == "reject" and item["reason"] == "dead" for item in movement_decisions),
        "latest_status": stats,
    }


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
    global EXE
    parser = argparse.ArgumentParser(description="Run automated MiMITA ICE multiplayer process test.")
    parser.add_argument("--exe", type=str, default="")
    parser.add_argument("--clients", type=int, default=2)
    parser.add_argument("--duration", type=int, default=60)
    parser.add_argument("--build", action="store_true")
    parser.add_argument("--same-machine", action="store_true", default=False)
    parser.add_argument("--disable-relay", action="store_true", default=False)
    parser.add_argument("--force-relay", action="store_true", default=False)
    parser.add_argument("--packet-loss", type=float, default=0.0)
    parser.add_argument("--latency-ms", type=int, default=0)
    parser.add_argument("--jitter-ms", type=int, default=0)
    parser.add_argument("--death-respawn-cycles", type=int, default=10)
    parser.add_argument("--reconnect-cycles", type=int, default=3)
    parser.add_argument("--mixed-udp-client", action="store_true")
    parser.add_argument("--keep-logs", action="store_true")
    parser.add_argument("--verbose", action="store_true")
    args = parser.parse_args()

    if args.force_relay and args.disable_relay:
        parser.error("--force-relay and --disable-relay are mutually exclusive")

    EXE = resolve_exe(args)
    if EXE is None:
        return 2
    if args.mixed_udp_client and not SMOKE.exists():
        print(f"FAIL: missing mixed UDP smoke executable {SMOKE}")
        return 2
    exe_sha = sha256_file(EXE)

    log_dir = LOG_ROOT / now_stamp()
    log_dir.mkdir(parents=True, exist_ok=True)
    print(f"[ICE HARNESS] logs={log_dir}")
    print(f"[ICE HARNESS] exe={EXE}")
    print(f"[ICE HARNESS] exe_sha256={exe_sha}")

    coordinator = LocalIceCoordinator()
    coordinator_url = coordinator.start()
    print(f"[ICE HARNESS] local coordinator={coordinator_url}")

    env = os.environ.copy()
    env["MIMITA_COORDINATOR_URL"] = coordinator_url
    if args.same_machine and args.disable_relay and not args.force_relay:
        env["MIMITA_ICE_LOCAL_ONLY"] = "1"

    client_timeout = max(args.duration + 20, args.death_respawn_cycles * 3 + 20, 20)
    mode_flags = ["--timeout-seconds", str(client_timeout)]
    if args.disable_relay:
        mode_flags.append("--disable-relay")
    if args.force_relay:
        mode_flags.append("--force-relay")

    processes = []
    initial_clients = []
    reconnect_processes = []
    mixed_process = None
    try:
        server_timeout = max(client_timeout + args.reconnect_cycles * 15 + 20, 45)
        server_args = [
            "--server", "--ice", "--bind", "127.0.0.1:0",
            "--timeout", str(server_timeout),
        ]
        if not args.mixed_udp_client:
            server_args.append("--no-npcs")
        server = start_process(
            "server",
            server_args,
            env,
            log_dir,
        )
        processes.append(server)
        room = find_room(server, 45)
        if not room:
            print("FAIL: authoritative ICE server did not emit a room code")
            return 3
        print(f"[ICE HARNESS] room={room}")
        udp_endpoint = find_udp_endpoint(server, 8)
        if args.mixed_udp_client and not udp_endpoint:
            print("FAIL: mixed UDP+ICE requested but server did not emit a UDP endpoint")
            return 4

        for i in range(args.clients):
            death_cycles = args.death_respawn_cycles if i == 0 else 0
            client = start_process(
                f"client{i + 1}",
                ["--ice-connect", room,
                 "--client-index", str(i + 1),
                 "--death-respawn-cycles", str(death_cycles)] + mode_flags,
                env,
                log_dir,
            )
            processes.append(client)
            initial_clients.append(client)
            time.sleep(0.25)

        if args.mixed_udp_client:
            mixed_env = env.copy()
            mixed_env["MIMITA_TEST_SERVER_ADDR"] = udp_endpoint
            mixed_process = start_command(
                "mixed-udp-smoke",
                [str(SMOKE)],
                mixed_env,
                log_dir,
            )
            processes.append(mixed_process)

        deadline = time.time() + max(client_timeout + 30, 60)
        while time.time() < deadline:
            initial_done = all(p.proc.poll() is not None for p in initial_clients)
            mixed_done = mixed_process is None or mixed_process.proc.poll() is not None
            if initial_done and mixed_done:
                break
            time.sleep(0.1)

        terminate_all(initial_clients)
        if mixed_process:
            wait_process(mixed_process, 2)
        events = collect_events(processes)
        join_event = first_event(events, "join_accepted", "client1")
        reconnect_token = join_event.get("reconnectToken") if join_event else ""

        if reconnect_token:
            for cycle in range(args.reconnect_cycles):
                reconnect = start_process(
                    f"reconnect{cycle + 1}",
                    ["--ice-connect", room,
                     "--client-index", "1",
                     "--reconnect-token", reconnect_token] + mode_flags,
                    env,
                    log_dir,
                )
                processes.append(reconnect)
                reconnect_processes.append(reconnect)
                confirmed = wait_for_event(reconnect, "reconnect_confirmed", 35)
                if confirmed and confirmed.get("reconnectToken"):
                    reconnect_token = confirmed["reconnectToken"]
                wait_for_event(reconnect, "post_reconnect_movement", 10)
                wait_process(reconnect, 5)
                terminate_all([reconnect])
                time.sleep(0.25)

        time.sleep(1.0)
        events = collect_events(processes)
        server_evidence = derive_server_evidence(server)

        for process in processes:
            print(f"[ICE HARNESS] {process.name} returncode={process.proc.returncode}")

        clients_ok = all(p.proc.returncode == 0 for p in initial_clients)
        reconnect_ok = all(p.proc.returncode == 0 for p in reconnect_processes)
        connected_clients = event_count(events, "ice_connected", "client")
        accepted_clients = event_count(events, "join_accepted", "client")
        clients_spawned = event_count(events, "spawn_sent", "client")
        spawn_acks = event_count(events, "spawn_ack", "client")
        spawn_activated = event_count(events, "spawn_activated", "client")
        snapshots = event_count(events, "snapshot_received", "client")
        remote_snapshots = event_count(events, "remote_snapshot_received", "client")
        deaths = event_count(events, "death_confirmed", "client1")
        respawns = event_count(events, "respawn_confirmed", "client1")
        post_respawn = event_count(events, "post_respawn_movement", "client1") > 0
        reconnects = event_count(events, "reconnect_confirmed", "reconnect")
        post_reconnect = event_count(events, "post_reconnect_movement", "reconnect") >= args.reconnect_cycles
        movement_validator_reached = server_evidence["movement_validator_reached"]
        movement_accepted = server_evidence["movement_accepted"]
        mixed_smoke_fields = parse_smoke_fields(mixed_process) if mixed_process else {}
        mixed_ok = (
            not args.mixed_udp_client or
            (mixed_process is not None and mixed_process.proc.returncode == 0)
        )
        mixed_passed = args.mixed_udp_client and mixed_ok

        summary = {
            "transport": "ice",
            "full_server_path": True,
            "protocol_version": 24,
            "executable": str(EXE),
            "sha256": exe_sha,
            "clients_connected": min(connected_clients, args.clients),
            "clients_joined": accepted_clients,
            "clients_spawned": clients_spawned,
            "spawn_acks": spawn_acks,
            "spawn_activated": spawn_activated,
            "movement_validator_reached": movement_validator_reached,
            "movement_accepted": movement_accepted,
            "remote_snapshot_received": remote_snapshots > 0,
            "death_respawn_cycles": respawns,
            "reconnect_cycles": reconnects,
            "post_respawn_movement": post_respawn,
            "post_reconnect_movement": post_reconnect,
            "mixed_udp_ice": args.mixed_udp_client,
            "mixed_udp_ice_passed": mixed_passed,
            "mixed_udp_returncode": mixed_process.proc.returncode if mixed_process else None,
            "mixed_udp_smoke": mixed_smoke_fields,
            "server_evidence": server_evidence,
            "events": events,
        }
        (log_dir / "events.json").write_text(
            json.dumps(summary, indent=2),
            encoding="utf-8",
        )

        print(
            "[ICE HARNESS SUMMARY] "
            f"clients={args.clients} connected={connected_clients} "
            f"accepted={accepted_clients} spawned={clients_spawned} "
            f"spawnAck={spawn_acks} snapshots={snapshots} "
            f"remoteSnapshots={remote_snapshots} validator={int(movement_validator_reached)} "
            f"movementAccepted={int(movement_accepted)} deaths={deaths} "
            f"respawns={respawns} reconnects={reconnects} mixedUdp={int(mixed_passed)}"
        )

        if args.clients > 1 and accepted_clients < args.clients:
            print("FAIL: multi-client ICE is not complete; host accepted fewer clients than requested")
            print(f"DIAGNOSIS: {diagnose(processes, events)}")
            return 10

        if not clients_ok or not reconnect_ok:
            print(f"FAIL: child process failed. DIAGNOSIS: {diagnose(processes, events)}")
            return 11

        if not mixed_ok:
            print("FAIL: mixed UDP+ICE smoke client failed")
            return 19

        if connected_clients < args.clients or accepted_clients < args.clients or snapshots == 0:
            print(f"FAIL: missing expected events. DIAGNOSIS: {diagnose(processes, events)}")
            return 12

        if clients_spawned < args.clients or spawn_acks < args.clients or spawn_activated < args.clients:
            print("FAIL: ICE clients did not complete authoritative spawn lifecycle")
            print(f"DIAGNOSIS: {diagnose(processes, events)}")
            return 13

        if not movement_validator_reached or not movement_accepted:
            print("FAIL: server did not prove Stage 3A movement validation over ICE")
            return 14

        if remote_snapshots == 0:
            print("FAIL: no ICE client saw a remote authoritative player snapshot")
            return 15

        if respawns < args.death_respawn_cycles or deaths < args.death_respawn_cycles:
            print("FAIL: ICE death/respawn cycle count was not met")
            return 16

        if args.death_respawn_cycles > 0 and not post_respawn:
            print("FAIL: no post-respawn movement event was observed")
            return 17

        if reconnects < args.reconnect_cycles or not post_reconnect:
            print("FAIL: ICE reconnect cycle count was not met")
            return 18

        print("[ICE HARNESS] PASS")
        return 0
    finally:
        terminate_all(processes)
        coordinator.stop()


if __name__ == "__main__":
    raise SystemExit(main())
