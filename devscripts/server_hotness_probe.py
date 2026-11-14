#!/usr/bin/env python3
# 2026-11-14
# purpose
# Runtime proof for the hot server cutover. Launches the published EXE with the
# current whole-game DLL, waits for the server to listen, sends a real PING, and
# asserts the canonical journal shows DLL packet handling and DLL server calls.
# Optionally publishes a ready manifest to prove an in-process generation swap.
#
#   python devscripts/server_hotness_probe.py
#   python devscripts/server_hotness_probe.py --swap
#
# This probe launches a real dedicated server process and stops it before exit.
# It does not request a build and does not touch the normal ready.json.

from __future__ import annotations

import argparse
import json
import os
import shutil
import socket
import struct
import subprocess
import sys
import tempfile
import time
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MAGIC = 0x4D494D38
PROTOCOL_VERSION = 52
PACKET_PING = 6
PONG_BYTES = 28


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="milliseconds").replace("+00:00", "Z")


def read_json(path: Path) -> dict | None:
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return None


def default_exe() -> str:
    state = read_json(ROOT / ".dev" / "state.json") or {}
    identity = state.get("build_identity") or {}
    path = identity.get("path") or ""
    if path and Path(path).is_file():
        return path
    return ""


def default_dll() -> str:
    current = read_json(ROOT / "build" / "game" / "current.json") or {}
    path = current.get("dll") or ""
    if path and not Path(path).is_absolute():
        path = str(ROOT / path)
    return path if path and Path(path).is_file() else ""


def free_udp_port() -> int:
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.bind(("127.0.0.1", 0))
        return int(sock.getsockname()[1])


def ping(port: int, player_id: int, timeout: float = 0.5) -> bytes | None:
    packet = struct.pack("<IHBBIII", MAGIC, PROTOCOL_VERSION, PACKET_PING, 0, 1,
                         player_id, 0)
    packet += struct.pack("<Q", int(time.time() * 1000))
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.settimeout(timeout)
        try:
            sock.sendto(packet, ("127.0.0.1", port))
            response, _ = sock.recvfrom(2048)
        except OSError:
            return None
    if len(response) < PONG_BYTES or response[6] != PACKET_PING:
        return None
    return response


def event_files_since(start_wall: float) -> list[Path]:
    candidates: list[Path] = []
    logs = ROOT / "logs"
    if not logs.is_dir():
        return candidates
    for path in logs.glob("*/*/events.jsonl"):
        try:
            if path.stat().st_mtime >= start_wall - 2.0:
                candidates.append(path)
        except OSError:
            continue
    return sorted(candidates, key=lambda item: item.stat().st_mtime, reverse=True)


def read_events(path: Path, pid: int) -> list[dict]:
    events: list[dict] = []
    try:
        text = path.read_text(encoding="utf-8", errors="ignore")
    except OSError:
        return events
    needle = f'"pid":{pid}'
    for line in text.splitlines():
        if needle not in line:
            continue
        try:
            events.append(json.loads(line))
        except json.JSONDecodeError:
            continue
    return events


def find_events(pid: int, start_wall: float) -> tuple[Path | None, list[dict]]:
    for path in event_files_since(start_wall):
        events = read_events(path, pid)
        if events:
            return path, events
    return None, []


def has_event(events: list[dict], name: str, predicate) -> bool:
    return any(event.get("event") == name and predicate(event) for event in events)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Prove the hot server cutover at runtime")
    parser.add_argument("--exe", default="")
    parser.add_argument("--dll", default="")
    parser.add_argument("--map", default="funworld3")
    parser.add_argument("--mode", default="sandbox")
    parser.add_argument("--gamemode", default="sandbox")
    parser.add_argument("--timeout", type=float, default=60.0)
    parser.add_argument("--swap", action="store_true",
                        help="publish a ready manifest and require a generation swap")
    parser.add_argument("--swap-dll", default="",
                        help="DLL path to swap to (default: a copy of --dll)")
    parser.add_argument("--expect-runtime-revision", type=int, default=0,
                        help="require this runtime_revision after the swap")
    parser.add_argument("--client", action="store_true",
                        help="launch a full client and require a hot join")
    parser.add_argument("--chat", default="",
                        help="send this chat message after the hot join")
    parser.add_argument("--client-timeout", type=float, default=50.0)
    parser.add_argument("--json", default="")
    args = parser.parse_args(argv)

    exe = Path(args.exe) if args.exe else Path(default_exe())
    dll = Path(args.dll) if args.dll else Path(default_dll())
    if not exe.is_file():
        print(f"[HOTNESS] missing EXE: {exe}", file=sys.stderr)
        return 2
    if not dll.is_file():
        print(f"[HOTNESS] missing DLL: {dll}", file=sys.stderr)
        return 2

    port = free_udp_port()
    start_wall = time.time()
    ready_path = ROOT / "build" / "game" / "ready-server-probe.json"
    try:
        ready_path.unlink()
    except OSError:
        pass

    env = dict(os.environ)
    env["MIMITA_GAME_DLL"] = str(dll)
    env["MIMITA_GAME_PROBE"] = "1"
    env["MIMITA_GAME_READY"] = str(ready_path)
    env["MIMITA_GAME_OWN_FRAME"] = "0"

    stdout = tempfile.TemporaryFile(mode="w+b")
    stderr = tempfile.TemporaryFile(mode="w+b")
    command = [
        str(exe), "--server", "--no-npcs",
        "--mode", args.mode, "--gamemode", args.gamemode,
        "--bind", f"127.0.0.1:{port}", "--map", args.map,
    ]
    print(f"[HOTNESS] start port={port} exe={exe.name} dll={dll.name}")
    process = subprocess.Popen(
        command, cwd=str(ROOT), env=env,
        stdout=stdout, stderr=stderr,
        creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0,
    )

    result: dict = {
        "status": "failed",
        "pid": process.pid,
        "port": port,
        "exe": str(exe),
        "dll": str(dll),
        "dll_bytes": 0,
        "events_path": None,
        "server_calls": 0,
        "server_failures": 0,
        "server_path": None,
        "players": 0,
        "swap": False,
        "client": False,
        "chat": False,
        "runtime_revision": 0,
        "generated_utc": utc_now(),
    }
    player_id = 7000 + (process.pid % 1000)
    deadline = time.time() + args.timeout
    pong: bytes | None = None
    swap_dll: Path | None = None
    client_process: subprocess.Popen | None = None
    try:
        while time.time() < deadline:
            if process.poll() is not None:
                break
            pong = ping(port, player_id, timeout=0.5)
            if pong:
                break
            time.sleep(0.7)
        if pong is None:
            print("[HOTNESS] no PONG before timeout", file=sys.stderr)
            return 1
        result["dll_bytes"] = len(pong)
        print(f"[HOTNESS] pong bytes={len(pong)}")

        # Let the first one-second journal windows flush.
        time.sleep(1.5)
        events_path, events = find_events(process.pid, start_wall)
        if events_path is None:
            print("[HOTNESS] no journal for server pid", file=sys.stderr)
            return 1
        result["events_path"] = str(events_path.relative_to(ROOT))

        if not has_event(events, "game.host.probe.started", lambda event: True):
            print("[HOTNESS] missing game.host.probe.started", file=sys.stderr)
            return 1
        def dll_ping(event: dict) -> bool:
            fields = event.get("fields") or {}
            return (fields.get("type") == PACKET_PING
                    and fields.get("path") == "dll")

        if not (has_event(events, "game.transport.path", dll_ping)
                or has_event(events, "game.transport.handled", dll_ping)):
            print("[HOTNESS] missing dll PING handling", file=sys.stderr)
            return 1
        summaries = [
            event for event in events if event.get("event") == "game.hotness.summary"
        ]
        if not summaries:
            print("[HOTNESS] missing game.hotness.summary", file=sys.stderr)
            return 1
        summary = summaries[-1].get("fields") or {}
        result["server_calls"] = summary.get("server_calls", 0)
        result["server_failures"] = summary.get("server_failures", 0)
        result["server_path"] = summary.get("server_path")
        if result["server_path"] != "dll" or result["server_calls"] <= 0:
            print(f"[HOTNESS] server path/calls invalid: {summary}", file=sys.stderr)
            return 1
        if result["server_failures"] != 0:
            print(f"[HOTNESS] server failures: {result['server_failures']}", file=sys.stderr)
            return 1
        print(f"[HOTNESS] server path=dll calls={result['server_calls']} "
              f"failures={result['server_failures']}")

        client_process = None
        if args.client:
            client_stdout = tempfile.TemporaryFile(mode="w+b")
            client_stderr = tempfile.TemporaryFile(mode="w+b")
            client_command = [
                str(exe), "--connect", f"127.0.0.1:{port}",
                "--name", "HotnessProbe", "--ai-no-console",
            ]
            if args.chat:
                client_command.extend(["--startup-command", f"chat {args.chat}"])
            client_process = subprocess.Popen(
                client_command,
                cwd=str(ROOT), env=env,
                stdout=client_stdout, stderr=client_stderr,
                creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0,
            )
            print(f"[HOTNESS] client pid={client_process.pid} connecting")
            joined = False
            client_deadline = time.time() + args.client_timeout
            while time.time() < client_deadline:
                if client_process.poll() is not None:
                    break
                _, events = find_events(process.pid, start_wall)
                summaries = [
                    event for event in events
                    if event.get("event") == "game.hotness.summary"
                ]
                if summaries:
                    fields = summaries[-1].get("fields") or {}
                    if (fields.get("server_path") == "dll"
                            and int(fields.get("players", 0)) >= 1
                            and int(fields.get("dll", 0)) > 0
                            and int(fields.get("server_failures", 0)) == 0):
                        joined = True
                        result["players"] = int(fields.get("players", 0))
                        result["server_calls"] = fields.get("server_calls", 0)
                        result["server_failures"] = fields.get("server_failures", 0)
                        break
                time.sleep(0.5)
            if not joined:
                print("[HOTNESS] client did not produce a hot join", file=sys.stderr)
                return 1
            result["client"] = True
            if ping(port, player_id, timeout=1.0) is None:
                print("[HOTNESS] server stopped answering after join", file=sys.stderr)
                return 1
            print(f"[HOTNESS] client joined players={result['players']} "
                  f"server_calls={result['server_calls']}")

            if args.chat:
                chat_seen = False
                chat_deadline = time.time() + 15.0
                while time.time() < chat_deadline:
                    _, events = find_events(process.pid, start_wall)
                    if has_event(
                            events, "game.transport.path",
                            lambda event: (event.get("fields") or {}).get("type")
                            == 48
                            and (event.get("fields") or {}).get("path") == "dll"):
                        chat_seen = True
                        break
                    time.sleep(0.5)
                if not chat_seen:
                    print("[HOTNESS] chat request did not reach the DLL",
                          file=sys.stderr)
                    return 1
                result["chat"] = True
                print("[HOTNESS] chat request handled path=dll")

        if args.swap:
            # A distinct file path forces a second module instance instead of
            # LoadLibrary returning the already-loaded module handle.
            if args.swap_dll:
                swap_dll = Path(args.swap_dll)
                if not swap_dll.is_absolute():
                    swap_dll = ROOT / swap_dll
                if not swap_dll.is_file():
                    print(f"[HOTNESS] missing swap DLL: {swap_dll}", file=sys.stderr)
                    return 1
            else:
                swap_dll = ready_path.parent / f"probe-swap-{process.pid}.dll"
                swap_dll.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(dll, swap_dll)
            result["swap_dll"] = str(swap_dll.relative_to(ROOT))
            ready = {
                "status": "ready",
                "generation": 2,
                "dll": str(swap_dll),
                "code_hash": "",
                "published_utc": utc_now(),
            }
            ready_path.parent.mkdir(parents=True, exist_ok=True)
            ready_path.write_text(json.dumps(ready) + "\n", encoding="utf-8")
            swapped = False
            swap_deadline = time.time() + 20.0
            while time.time() < swap_deadline:
                if process.poll() is not None:
                    break
                _, events = find_events(process.pid, start_wall)
                if has_event(
                        events, "game.generation.activated",
                        lambda event: (event.get("fields") or {}).get("generation") == 2):
                    swapped = True
                    break
                time.sleep(0.5)
            if not swapped:
                print("[HOTNESS] generation swap not observed", file=sys.stderr)
                return 1
            result["swap"] = True
            if args.expect_runtime_revision:
                seen_revision = 0
                revision_deadline = time.time() + 12.0
                while time.time() < revision_deadline:
                    _, events = find_events(process.pid, start_wall)
                    summaries = [
                        event for event in events
                        if event.get("event") == "game.hotness.summary"
                    ]
                    if summaries:
                        fields = summaries[-1].get("fields") or {}
                        seen_revision = int(fields.get("runtime_revision", 0))
                        if (seen_revision == args.expect_runtime_revision
                                and int(fields.get("server_failures", 0)) == 0):
                            break
                    time.sleep(0.5)
                result["runtime_revision"] = seen_revision
                if seen_revision != args.expect_runtime_revision:
                    print(f"[HOTNESS] runtime_revision={seen_revision} "
                          f"expected={args.expect_runtime_revision}",
                          file=sys.stderr)
                    return 1
                print(f"[HOTNESS] behavior revision after swap={seen_revision}")
            if args.client:
                stayed = False
                stay_deadline = time.time() + 12.0
                while time.time() < stay_deadline:
                    _, events = find_events(process.pid, start_wall)
                    summaries = [
                        event for event in events
                        if event.get("event") == "game.hotness.summary"
                    ]
                    if summaries:
                        fields = summaries[-1].get("fields") or {}
                        if (fields.get("server_path") == "dll"
                                and int(fields.get("players", 0)) >= 1
                                and int(fields.get("server_failures", 0)) == 0):
                            result["players"] = int(fields.get("players", 0))
                            result["server_calls"] = fields.get("server_calls", 0)
                            stayed = True
                            break
                    time.sleep(0.5)
                if not stayed:
                    print("[HOTNESS] client did not stay connected after swap",
                          file=sys.stderr)
                    return 1
                print(f"[HOTNESS] client stayed connected players={result['players']}")
            pong_after = ping(port, player_id, timeout=1.0)
            if pong_after is None:
                print("[HOTNESS] server stopped answering after swap", file=sys.stderr)
                return 1
            print("[HOTNESS] generation swap observed and PING still answered")

        result["status"] = "ok"
        return 0
    finally:
        if client_process is not None and client_process.poll() is None:
            client_process.terminate()
            try:
                client_process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                client_process.kill()
        if process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
        try:
            ready_path.unlink()
        except OSError:
            pass
        if swap_dll is not None and not args.swap_dll:
            try:
                swap_dll.unlink()
            except OSError:
                pass
        if result["status"] != "ok" and process.returncode not in (None, 0):
            stdout.seek(0)
            tail = stdout.read().decode("utf-8", errors="replace").splitlines()[-20:]
            for line in tail:
                print(f"[HOTNESS] {line}", file=sys.stderr)
        stdout.close()
        stderr.close()
        if args.json:
            Path(args.json).write_text(json.dumps(result, indent=2, sort_keys=True) + "\n",
                                       encoding="utf-8")
        print(f"[HOTNESS] {result['status']}")


if __name__ == "__main__":
    raise SystemExit(main())
