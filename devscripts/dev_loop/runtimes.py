"""Independent per-agent runtimes launched from the shared dev-loop build.

One shared source and one build; each runtime gets its own launch mode, map,
UDP port, room file, run directory, and events.jsonl. Runtimes never build.
"""

from __future__ import annotations

import ctypes
import datetime
import json
import os
import subprocess
import time
from pathlib import Path


def _utc_now() -> str:
    return datetime.datetime.now(datetime.timezone.utc).isoformat(
        timespec="milliseconds").replace("+00:00", "Z")


def _read_json(path: Path) -> dict:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return {}
    return value if isinstance(value, dict) else {}


def process_alive(pid: int) -> bool:
    if pid <= 0 or os.name != "nt":
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


def _kill_tree(pid: int) -> None:
    if not process_alive(pid):
        return
    subprocess.run(["taskkill", "/F", "/T", "/PID", str(pid)],
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                   creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))


class RuntimeManager:
    def __init__(self, root: Path, resolve_build):
        self.root = Path(root)
        self.resolve_build = resolve_build
        self.state_root = self.root / ".dev" / "runtimes"
        self.launch_modes = self.root / "devscripts" / "dev-launch-modes.json"
        self.profile_root = self.root / "devscripts" / "dev-profiles"

    def _state_path(self, runtime_id: str) -> Path:
        return self.state_root / f"{runtime_id}.json"

    def _load_launch_mode(self, mode_id: str) -> dict:
        modes = _read_json(self.launch_modes).get("modes", {})
        mode = modes.get(str(mode_id)) if isinstance(modes, dict) else None
        if not isinstance(mode, dict):
            raise RuntimeError(f"unknown launch mode {mode_id!r}")
        return mode

    def _load_profile(self, name: str) -> dict:
        profile = _read_json(self.profile_root / f"{name}.json")
        if not profile:
            raise RuntimeError(f"unknown dev profile {name!r}")
        return profile

    def list(self) -> list[dict]:
        if not self.state_root.is_dir():
            return []
        rows = []
        for path in sorted(self.state_root.glob("*.json")):
            state = _read_json(path)
            if not state:
                continue
            state["server_alive"] = process_alive(int(state.get("pid", 0)))
            state["client_alive"] = process_alive(int(state.get("client_pid", 0) or 0))
            rows.append(state)
        return rows

    def status(self, runtime_id: str) -> dict:
        state = _read_json(self._state_path(runtime_id))
        if not state:
            raise RuntimeError(f"unknown runtime {runtime_id!r}")
        state["server_alive"] = process_alive(int(state.get("pid", 0)))
        state["client_alive"] = process_alive(int(state.get("client_pid", 0) or 0))
        return state

    def _build_server_args(self, exe: Path, profile: dict, launch_mode: dict,
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
        host = str(profile.get("host_player_name", "") or profile.get("client_name", "")).strip()
        if host:
            args.extend(["--host-player", host])
        if npc_count > 0:
            args.extend(["--npcs", str(npc_count)])
        else:
            args.append("--no-npcs")
        args.extend(["--no-map-rotation", "--no-discord-notification"])
        args.extend(str(value) for value in launch_mode.get("server_args", []))
        return args

    def _wait_for_port(self, journal: Path, timeout: float = 45.0) -> int | None:
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

    def start(self, runtime_id: str, launch_mode_id: str,
              profile_name: str = "npc-navigation", map_name: str = "",
              with_client: bool = True, ready_timeout: float = 45.0) -> dict:
        runtime_id = runtime_id.strip()
        if not runtime_id or any(ch in runtime_id for ch in "\\/:*?\"<>| "):
            raise RuntimeError("runtime id must be a simple name without spaces")
        existing = _read_json(self._state_path(runtime_id))
        if existing and process_alive(int(existing.get("pid", 0))):
            raise RuntimeError(f"runtime {runtime_id!r} is already running")
        self._state_path(runtime_id).unlink(missing_ok=True)

        profile = self._load_profile(profile_name)
        launch_mode = self._load_launch_mode(launch_mode_id)
        exe, dll, build_id = self.resolve_build()
        map_name = map_name or str(launch_mode.get("map") or profile.get("map") or "funworld3")

        stamp = datetime.datetime.now(datetime.timezone.utc)
        run_id = f"{stamp:%Y%m%d_%H%M%S}_{runtime_id}"
        run_dir = self.root / "logs" / f"{stamp:%Y-%m-%d}" / run_id
        server_dir = run_dir / "server-01"
        server_dir.mkdir(parents=True, exist_ok=True)
        room_file = self.root / ".dev" / "tmp" / "runtimes" / runtime_id / "room.txt"
        room_file.parent.mkdir(parents=True, exist_ok=True)
        journal = server_dir / "events.jsonl"
        metadata = {
            "runtime_id": runtime_id,
            "run_id": run_id,
            "launch_mode": str(launch_mode_id),
            "launch_mode_name": launch_mode.get("name", ""),
            "profile": profile_name,
            "map": map_name,
            "build_id": build_id,
            "game_dll": str(dll) if dll else "",
            "started_utc": _utc_now(),
        }
        (run_dir / "run.json").write_text(json.dumps(metadata, indent=2) + "\n",
                                          encoding="utf-8")
        env = {
            **os.environ,
            "MIMITA_RUN_ID": run_id,
            "MIMITA_INSTANCE_ID": "server-01",
            "MIMITA_EVENTS_FILE": str(journal),
            "MIMITA_PROCESS_ROLE": "server",
            "MIMITA_CRASH_DIR": str(self.root / ".dev" / "crashes" / run_id),
            "MIMITA_RUN_METADATA": json.dumps(metadata, separators=(",", ":")),
        }
        if dll:
            env["MIMITA_GAME_DLL"] = str(dll)
        args = self._build_server_args(exe, profile, launch_mode, map_name, room_file)
        server = subprocess.Popen(
            args, cwd=self.root, env=env,
            creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))

        client_pid = None
        client_journal = None
        port = None
        if with_client:
            port = self._wait_for_port(journal, ready_timeout)
            if port:
                client_dir = run_dir / "client-01"
                client_dir.mkdir(parents=True, exist_ok=True)
                client_journal = client_dir / "events.jsonl"
                client_args = [
                    str(exe), "--connect", f"127.0.0.1:{port}",
                    "--room-code-file", str(room_file), "--map", map_name,
                    "--name", str(profile.get("client_name", "NPC Dev")),
                    "--unlock-cursor",
                ]
                client_args.extend(str(v) for v in launch_mode.get("client_args", []))
                client_env = {
                    **env,
                    "MIMITA_INSTANCE_ID": "client-01",
                    "MIMITA_EVENTS_FILE": str(client_journal),
                    "MIMITA_PROCESS_ROLE": "client",
                }
                client = subprocess.Popen(
                    client_args, cwd=self.root, env=client_env,
                    creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
                client_pid = client.pid

        state = {
            "schema_version": 1,
            **metadata,
            "pid": server.pid,
            "client_pid": client_pid,
            "port": port,
            "run_dir": str(run_dir),
            "journal": str(journal),
            "client_journal": str(client_journal) if client_journal else "",
            "room_file": str(room_file),
            "args": args,
        }
        self.state_root.mkdir(parents=True, exist_ok=True)
        self._state_path(runtime_id).write_text(json.dumps(state, indent=2) + "\n",
                                                encoding="utf-8")
        return state

    def stop(self, runtime_id: str) -> dict:
        state = _read_json(self._state_path(runtime_id))
        if not state:
            raise RuntimeError(f"unknown runtime {runtime_id!r}")
        _kill_tree(int(state.get("client_pid", 0) or 0))
        _kill_tree(int(state.get("pid", 0)))
        self._state_path(runtime_id).unlink(missing_ok=True)
        return state

    def reload(self, runtime_id: str) -> dict:
        state = _read_json(self._state_path(runtime_id))
        if not state:
            raise RuntimeError(f"unknown runtime {runtime_id!r}")
        launch = state.get("launch_mode", "1")
        profile = state.get("profile", "npc-navigation")
        map_name = state.get("map", "")
        had_client = bool(state.get("client_pid"))
        self.stop(runtime_id)
        return self.start(runtime_id, launch, profile, map_name, with_client=had_client)

    def reap(self) -> None:
        for row in self.list():
            runtime_id = str(row.get("runtime_id") or "")
            if not runtime_id:
                continue
            if row.get("server_alive") or row.get("client_alive"):
                continue
            self._state_path(runtime_id).unlink(missing_ok=True)
