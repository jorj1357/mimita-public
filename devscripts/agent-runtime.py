#!/usr/bin/env python3
"""Launch independent MiMITA server runtimes from the shared dev-loop build.

Each runtime uses the same published EXE and newest game DLL generation as the
dev-loop, but gets its own launch mode, map, room file, run directory, and
events.jsonl. It never builds; the root dev-loop remains the build authority.
"""

from __future__ import annotations

import argparse
import ctypes
import datetime
import hashlib
import json
import os
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RUNTIME_ROOT = ROOT / ".dev" / "agent-runtimes"
LAUNCH_MODES = ROOT / "devscripts" / "dev-launch-modes.json"
PROFILE_ROOT = ROOT / "devscripts" / "dev-profiles"


def utc_now() -> str:
    return datetime.datetime.now(datetime.timezone.utc).isoformat(
        timespec="milliseconds").replace("+00:00", "Z")


def read_json(path: Path) -> dict:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return {}
    return value if isinstance(value, dict) else {}


def runtime_state_path(runtime_id: str) -> Path:
    return RUNTIME_ROOT / f"{runtime_id}.json"


def load_launch_mode(mode_id: str) -> dict:
    data = read_json(LAUNCH_MODES)
    modes = data.get("modes", {}) if isinstance(data, dict) else {}
    mode = modes.get(str(mode_id))
    if not isinstance(mode, dict):
        raise SystemExit(f"unknown launch mode {mode_id!r}")
    return mode


def load_profile(name: str) -> dict:
    profile = read_json(PROFILE_ROOT / f"{name}.json")
    if not profile:
        raise SystemExit(f"unknown dev profile {name!r}")
    return profile


def resolve_executable() -> Path:
    current = ROOT / ".dev" / "current" / "mimita.exe"
    if current.is_file():
        return current
    state = read_json(ROOT / ".dev" / "state.json")
    metadata = state.get("run_metadata") or {}
    for key in ("artifact_executable", "executable"):
        candidate = Path(str(metadata.get(key) or ""))
        if candidate.is_file():
            return candidate
    raise SystemExit("no published mimita.exe found; let the dev-loop publish a build first")


def resolve_game_dll(generation: int = 0, dll: str = "") -> Path | None:
    if dll:
        path = Path(dll)
        if not path.is_absolute():
            path = ROOT / path
        return path.resolve() if path.is_file() else None
    if generation > 0:
        path = (ROOT / "build" / "game" / f"generation-{generation}"
                / f"game-g{generation}.dll")
        return path if path.is_file() else None
    ready = read_json(ROOT / "build" / "game" / "ready.json")
    value = str(ready.get("dll") or "")
    if not value:
        return None
    path = Path(value)
    if not path.is_absolute():
        path = ROOT / path
    return path if path.is_file() else None


def dll_identity(path: Path | None) -> dict:
    if path is None:
        return {}
    digest = ""
    try:
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
    except OSError:
        pass
    generation = 0
    for part in path.parts:
        if part.startswith("generation-") and part[len("generation-"):].isdigit():
            generation = int(part[len("generation-"):])
    return {"game_generation": generation, "game_dll_sha256": digest}


def process_alive(pid: int) -> bool:
    if pid <= 0:
        return False
    handle = ctypes.windll.kernel32.OpenProcess(0x1400, False, pid)
    if not handle:
        return False
    try:
        code = ctypes.c_ulong(0)
        if not ctypes.windll.kernel32.GetExitCodeProcess(handle, ctypes.byref(code)):
            return False
        return code.value == 259
    finally:
        ctypes.windll.kernel32.CloseHandle(handle)


def wait_for_server_port(journal: Path, timeout: float = 20.0) -> int | None:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        try:
            for line in journal.read_text(encoding="utf-8", errors="replace").splitlines():
                if "local-gameplay-ready" not in line and "transport.open" not in line:
                    continue
                try:
                    event = json.loads(line)
                except json.JSONDecodeError:
                    continue
                port = (event.get("fields") or {}).get("port")
                if isinstance(port, int) and port > 0:
                    return port
        except OSError:
            pass
        time.sleep(0.2)
    return None


def build_server_args(exe: Path, profile: dict, launch_mode: dict,
                      map_name: str, room_file: Path) -> list[str]:
    npc_count = int(profile.get("npc_count", 1) or 0)
    args = [
        str(exe), "--server", "--bind", "127.0.0.1:0",
        "--name", str(profile.get("server_name", "MiMITA Dev Server")),
        "--map", map_name,
        "--mode", str(profile.get("mode", "sandbox")),
        "--max-players", str(max(1, int(profile.get("max_players", 999) or 999))),
        "--map-rotation-minutes", "15",
        "--password-protected", "0", "--password", "",
        "--room-file", str(room_file),
    ]
    host_player = str(profile.get("host_player_name", "") or profile.get("client_name", "")).strip()
    if host_player:
        args.extend(["--host-player", host_player])
    if npc_count > 0:
        args.extend(["--npcs", str(npc_count)])
    else:
        args.append("--no-npcs")
    args.extend(["--no-map-rotation", "--no-discord-notification"])
    args.extend(str(value) for value in launch_mode.get("server_args", []))
    return args


def start(args: argparse.Namespace) -> int:
    runtime_id = args.runtime_id.strip()
    if not runtime_id or any(ch in runtime_id for ch in "\\/:*?\"<>| "):
        raise SystemExit("runtime id must be a simple name without spaces or path separators")
    state_path = runtime_state_path(runtime_id)
    if state_path.exists():
        state = read_json(state_path)
        if process_alive(int(state.get("pid", 0))):
            raise SystemExit(f"runtime {runtime_id!r} is already running (pid {state.get('pid')})")
        state_path.unlink(missing_ok=True)

    profile = load_profile(args.profile)
    launch_mode = load_launch_mode(args.launch_mode)
    exe = resolve_executable()
    dll = resolve_game_dll(args.game_generation, args.dll)
    if dll is None:
        raise SystemExit("requested game DLL was not found")
    map_name = args.map or str(launch_mode.get("map") or profile.get("map") or "funworld3")

    stamp = datetime.datetime.now(datetime.timezone.utc)
    run_id = f"{stamp:%Y%m%d_%H%M%S}_{runtime_id}"
    run_dir = ROOT / "logs" / f"{stamp:%Y-%m-%d}" / run_id
    server_dir = run_dir / "server-01"
    server_dir.mkdir(parents=True, exist_ok=True)
    room_file = ROOT / ".dev" / "tmp" / "agent-runtime" / runtime_id / "room.txt"
    room_file.parent.mkdir(parents=True, exist_ok=True)
    journal = server_dir / "events.jsonl"
    metadata = {
        "run_id": run_id,
        "runtime_id": runtime_id,
        "launch_mode": args.launch_mode,
        "launch_mode_name": launch_mode.get("name", ""),
        "map": map_name,
        "profile": args.profile,
        "executable": str(exe),
        "game_dll": str(dll),
        "agent": args.agent,
        **dll_identity(dll),
        "started_utc": utc_now(),
    }
    (run_dir / "run.json").write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")

    server_args = build_server_args(exe, profile, launch_mode, map_name, room_file)
    env = {
        **os.environ,
        "MIMITA_RUN_ID": run_id,
        "MIMITA_INSTANCE_ID": "server-01",
        "MIMITA_EVENTS_FILE": str(journal),
        "MIMITA_PROCESS_ROLE": "server",
        "MIMITA_CRASH_DIR": str(ROOT / ".dev" / "crashes" / run_id),
        "MIMITA_RUN_METADATA": json.dumps(metadata, separators=(",", ":")),
    }
    if dll:
        env["MIMITA_GAME_DLL"] = str(dll)
    process = subprocess.Popen(
        server_args, cwd=ROOT, env=env,
        creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
    )
    client_pid = None
    client_journal = None
    port = None
    if not args.no_client:
        port = wait_for_server_port(journal, timeout=args.ready_timeout)
        if port:
            client_dir = run_dir / "client-01"
            client_dir.mkdir(parents=True, exist_ok=True)
            client_journal = client_dir / "events.jsonl"
            client_args = [
                str(exe), "--connect", f"127.0.0.1:{port}",
                "--room-code-file", str(room_file),
                "--map", map_name,
                "--name", str(profile.get("client_name", "NPC Dev")),
                "--unlock-cursor",
            ]
            client_args.extend(str(value) for value in launch_mode.get("client_args", []))
            client_env = {
                **env,
                "MIMITA_INSTANCE_ID": "client-01",
                "MIMITA_EVENTS_FILE": str(client_journal),
                "MIMITA_PROCESS_ROLE": "client",
            }
            client = subprocess.Popen(
                client_args, cwd=ROOT, env=client_env,
                creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
            )
            client_pid = client.pid
    state = {
        "schema_version": 1,
        **metadata,
        "pid": process.pid,
        "port": port,
        "client_pid": client_pid,
        "run_dir": str(run_dir),
        "journal": str(journal),
        "client_journal": str(client_journal) if client_journal else "",
        "room_file": str(room_file),
        "args": server_args,
    }
    RUNTIME_ROOT.mkdir(parents=True, exist_ok=True)
    state_path.write_text(json.dumps(state, indent=2) + "\n", encoding="utf-8")
    print(f"runtime={runtime_id} pid={process.pid} run={run_id}")
    print(f"journal={journal}")
    if client_pid:
        print(f"client_pid={client_pid} port={port} journal={client_journal}")
    print(f"room_file={room_file}")
    return 0


def stop(args: argparse.Namespace) -> int:
    state_path = runtime_state_path(args.runtime_id)
    state = read_json(state_path)
    if not state:
        raise SystemExit(f"unknown runtime {args.runtime_id!r}")
    pid = int(state.get("pid", 0))
    client_pid = int(state.get("client_pid", 0) or 0)
    for target in (client_pid, pid):
        if target and process_alive(target):
            subprocess.run(["taskkill", "/F", "/T", "/PID", str(target)],
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                           creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
    state_path.unlink(missing_ok=True)
    print(f"runtime={args.runtime_id} pid={pid} stopped")
    return 0


def status(args: argparse.Namespace) -> int:
    state = read_json(runtime_state_path(args.runtime_id))
    if not state:
        raise SystemExit(f"unknown runtime {args.runtime_id!r}")
    alive = process_alive(int(state.get("pid", 0)))
    client_pid = int(state.get("client_pid", 0) or 0)
    client_alive = process_alive(client_pid)
    print(f"runtime={args.runtime_id} pid={state.get('pid')} alive={str(alive).lower()} "
          f"client_pid={client_pid or '-'} client_alive={str(client_alive).lower()}")
    print(f"run={state.get('run_id')} mode={state.get('launch_mode')} map={state.get('map')} "
          f"port={state.get('port')}")
    print(f"journal={state.get('journal')}")
    return 0 if alive else 1


def listing(_args: argparse.Namespace) -> int:
    if not RUNTIME_ROOT.is_dir():
        print("no runtimes")
        return 0
    for path in sorted(RUNTIME_ROOT.glob("*.json")):
        state = read_json(path)
        alive = process_alive(int(state.get("pid", 0)))
        client_alive = process_alive(int(state.get("client_pid", 0) or 0))
        print(f"{path.stem} pid={state.get('pid')} alive={str(alive).lower()} "
              f"client_alive={str(client_alive).lower()} "
              f"mode={state.get('launch_mode')} map={state.get('map')} "
              f"journal={state.get('journal')}")
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="action", required=True)
    start_p = sub.add_parser("start", help="start an independent server runtime")
    start_p.add_argument("runtime_id")
    start_p.add_argument("--launch-mode", required=True)
    start_p.add_argument("--profile", default="npc-navigation")
    start_p.add_argument("--map", default="")
    start_p.add_argument("--no-client", action="store_true",
                         help="start only the server (no local client)")
    start_p.add_argument("--ready-timeout", type=float, default=45.0)
    start_p.add_argument("--agent", default="",
                         help="agent/cohort tag for this runtime")
    start_p.add_argument("--game-generation", type=int, default=0,
                         help="pin a game DLL generation")
    start_p.add_argument("--dll", default="",
                         help="pin an explicit game DLL path")
    start_p.set_defaults(func=start)
    stop_p = sub.add_parser("stop", help="stop a runtime")
    stop_p.add_argument("runtime_id")
    stop_p.set_defaults(func=stop)
    status_p = sub.add_parser("status", help="show one runtime")
    status_p.add_argument("runtime_id")
    status_p.set_defaults(func=status)
    list_p = sub.add_parser("list", help="list runtimes")
    list_p.set_defaults(func=listing)
    args = parser.parse_args(argv)
    return int(args.func(args))


if __name__ == "__main__":
    raise SystemExit(main())


