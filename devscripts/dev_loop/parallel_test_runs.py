"""Isolated gameplay runs launched by the canonical dev-loop CLI.

Each run pins one already-published executable and owns its own server/client
processes, manifest, command queues, and per-process journals. This module
never builds and never starts another dev-loop daemon.
"""

from __future__ import annotations

import datetime
import ctypes
from ctypes import wintypes
import hashlib
import json
import os
import re
import subprocess
import threading
import time
import uuid
from collections import deque
from pathlib import Path

from devscripts.dev_loop.instances import (
    RunLayout, create_run_layout, read_run_manifest, write_run_manifest,
)
from devscripts.source_manifest import OwnerResolver

ROOT = Path(__file__).resolve().parents[2]
DEV_ROOT = ROOT / ".dev"
BUILD_ROOT = DEV_ROOT / "builds"
STATE_PATH = DEV_ROOT / "state.json"
GAME_CURRENT = ROOT / "build" / "game" / "current.json"
GAME_READY = ROOT / "build" / "game" / "ready.json"
PROFILE_ROOT = ROOT / "devscripts" / "dev-profiles"
LAUNCH_MODES = ROOT / "devscripts" / "dev-launch-modes.json"
_PROCESS_OUTPUT_SECRET = re.compile(
    r"(?i)(\b(?:join[_ -]?token|reconnect[_ -]?token|token|password|"
    r"room[_ -]?code|server[_ -]?code)\b\s*[:=]\s*)"
    r"(\"[^\"]*\"|'[^']*'|[^\s,;]+)"
)


def _read_json(path: Path) -> dict:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return {}
    return value if isinstance(value, dict) else {}


def _capture_process_output_tail(process: subprocess.Popen, *, max_lines: int = 24,
                                 max_line_chars: int = 512):
    """Drain child output without blocking startup and keep only a redacted tail."""
    tail = deque(maxlen=max_lines)
    lock = threading.Lock()

    def read_output() -> None:
        stream = process.stdout
        if stream is None:
            return
        try:
            for line in stream:
                safe = _PROCESS_OUTPUT_SECRET.sub(r"\1[redacted]", line.rstrip())
                with lock:
                    tail.append(safe[:max_line_chars])
        except (OSError, ValueError):
            return
        finally:
            try:
                stream.close()
            except OSError:
                pass

    reader = threading.Thread(
        target=read_output, name=f"dev-loop-output-{process.pid}", daemon=True)
    reader.start()
    return tail, lock, reader


def _same_process_start_time(actual: str, expected: str) -> bool:
    """Compare OS and manifest start times at the manifest's millisecond precision."""
    try:
        actual_time = datetime.datetime.fromisoformat(
            str(actual).replace("Z", "+00:00"))
        expected_time = datetime.datetime.fromisoformat(
            str(expected).replace("Z", "+00:00"))
        actual_ms = actual_time.astimezone(datetime.timezone.utc).replace(
            microsecond=(actual_time.microsecond // 1000) * 1000)
        expected_ms = expected_time.astimezone(datetime.timezone.utc).replace(
            microsecond=(expected_time.microsecond // 1000) * 1000)
        return actual_ms == expected_ms
    except (TypeError, ValueError, OverflowError):
        return False


def _atomic_json(path: Path, value: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temp = path.with_suffix(path.suffix + ".tmp")
    temp.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")
    os.replace(temp, path)


_COLD_WATCH_FILES = {
    "mimita.rc", "CMakeLists.txt", "build.py", "build_game_dll.py",
    "build_lock.py", "build_toolchain.py", "tools/build_manifold.py",
    "config/gamemode-good-maps.json", "devscripts/dev-launch-modes.json",
}


def _git_changed_paths() -> set[str]:
    result = subprocess.run(
        ["git", "status", "--porcelain", "-z", "--untracked-files=all"],
        cwd=ROOT, capture_output=True, check=False,
    )
    if result.returncode != 0:
        raise RuntimeError("could not audit current compiler inputs with git status")
    rows = result.stdout.decode("utf-8", errors="replace").split("\0")
    paths: set[str] = set()
    index = 0
    while index < len(rows):
        row = rows[index]
        index += 1
        if len(row) < 4:
            continue
        paths.add(row[3:].replace("\\", "/"))
        if row[:2].strip() and any(flag in row[:2] for flag in ("R", "C")):
            index += 1
    return paths


def _cold_compiler_changes() -> list[str]:
    """Return dirty inputs that the root loop classifies as EXE/cold."""
    resolver = OwnerResolver(ROOT)
    cold = []
    for relative in _git_changed_paths():
        path = Path(relative)
        suffix = path.suffix.lower()
        if relative in _COLD_WATCH_FILES or relative.startswith(
                ("external/", "include/", "tools/", "src/host/")):
            cold.append(relative)
        elif suffix in {".cpp", ".cc", ".cxx"}:
            owner, _ = resolver.resolve(relative)
            if owner != "game":
                cold.append(relative)
        elif suffix in {".h", ".hh", ".hpp", ".hxx", ".inl", ".ipp"}:
            if not relative.startswith("src/") or relative.startswith("src/host/"):
                cold.append(relative)
        elif suffix in {".c", ".rc"}:
            cold.append(relative)
    return sorted(set(cold))


def _append_event(layout: RunLayout, event: str, fields: dict,
                  reason: str = "") -> None:
    layout.dev_loop_journal.parent.mkdir(parents=True, exist_ok=True)
    record = {
        "category": "GENERAL", "event": event,
        "event_id": f"DEV_RUNTIME_{uuid.uuid4().hex}",
        "fields": fields, "level": "IMPORTANT", "pid": os.getpid(),
        "process": "dev-loop", "process_role": "dev-loop",
        "reason": reason, "run_id": layout.run_id,
        "wall_time": datetime.datetime.now(datetime.timezone.utc).isoformat(
            timespec="milliseconds").replace("+00:00", "Z"),
    }
    with layout.dev_loop_journal.open("a", encoding="utf-8", newline="\n") as out:
        out.write(json.dumps(record, separators=(",", ":")) + "\n")


def _open_run_log_lease(layout: RunLayout):
    """Reserve this run folder before a game logger can acquire its lease."""
    root = os.path.normcase(os.path.abspath(str(layout.run_dir.parent)))
    path_hash = 1469598103934665603
    for byte in root.encode("utf-8"):
        path_hash = ((path_hash ^ byte) * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    run_id = "".join(char if char.isalnum() or char in "_-" else "_"
                     for char in layout.run_id)
    name = f"Local\\MiMITA_v9_log_run_{path_hash:016x}_{run_id}"
    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel32.CreateEventW.argtypes = [ctypes.c_void_p, ctypes.c_int,
                                      ctypes.c_int, ctypes.c_wchar_p]
    kernel32.CreateEventW.restype = ctypes.c_void_p
    kernel32.CloseHandle.argtypes = [ctypes.c_void_p]
    kernel32.CloseHandle.restype = ctypes.c_int
    handle = kernel32.CreateEventW(None, 1, 0, name)
    if not handle:
        raise ctypes.WinError(ctypes.get_last_error())
    return kernel32, handle


def _artifact_for_build(build_id: str) -> tuple[Path, dict]:
    wanted = str(build_id or "").strip()
    if not wanted:
        raise RuntimeError("build id is empty")
    for folder in BUILD_ROOT.iterdir():
        if not folder.is_dir():
            continue
        identity = _read_json(folder / "build-identity.json")
        if not identity:
            continue
        values = {str(identity.get("build_id") or ""),
                  str(identity.get("artifact_id") or ""),
                  folder.name}
        if wanted in values:
            artifact = folder.resolve()
            if not artifact.is_relative_to(BUILD_ROOT.resolve()):
                raise RuntimeError("pinned artifact path escaped .dev/builds")
            return artifact, identity
    raise RuntimeError(f"published build {wanted!r} was not found")


def _pinned_game_identity(generation: int) -> dict:
    number = int(generation or 0)
    if number <= 0:
        raise RuntimeError("pinned game generation must be positive")
    path = (ROOT / "build" / "game" / f"generation-{number}" /
            f"game-g{number}.dll")
    if not path.is_file():
        raise RuntimeError(f"pinned game DLL generation {number} was not found")
    digest = hashlib.sha256(path.read_bytes()).hexdigest()
    return {
        "generation": number,
        "revision": "",
        "code_hash": "",
        "source_generation": None,
        "game_dll": str(path.resolve()),
        "game_dll_sha256": digest,
        "source_validation": {"result": "pinned_candidate_generation",
                              "pinned": True,
                              "pinned_game_generation": number},
    }


def _published_identity(build_id: str = "", game_generation: int = 0,
                        agent: str = "") -> tuple[Path, dict, dict]:
    state = _read_json(STATE_PATH)
    if not state:
        raise RuntimeError("root dev-loop status is unavailable; refusing an unverified launch")
    pinned = bool(str(build_id or "").strip())
    if not pinned and (
            state.get("build_pending") or
            state.get("active_build_generation") is not None or
            state.get("build_status") in {"building", "pending", "failed"}):
        raise RuntimeError("root dev-loop has a pending or active EXE build")

    if pinned:
        artifact, identity = _artifact_for_build(build_id)
    else:
        folder = str(state.get("latest_artifact_folder") or "").strip()
        if not folder:
            raise RuntimeError("root dev-loop has no published EXE artifact")
        artifact = (BUILD_ROOT / folder).resolve()
        if not artifact.is_relative_to(BUILD_ROOT.resolve()):
            raise RuntimeError("published artifact path escaped .dev/builds")
        identity = _read_json(artifact / "build-identity.json")
    exe = artifact / "mimita.exe"
    if not exe.is_file() or not identity or not identity.get("sha256"):
        raise RuntimeError("published executable or build identity is missing")
    digest = hashlib.sha256(exe.read_bytes()).hexdigest()
    if digest.lower() != str(identity.get("sha256", "")).lower():
        raise RuntimeError("published executable hash does not match its build identity")

    if int(game_generation or 0) > 0:
        game_identity = _pinned_game_identity(int(game_generation))
        source_validation = {
            "result": "pinnedbuild_pinned_game_generation",
            "pinned_exe": pinned,
            "pinned_game_generation": int(game_generation),
            "requested_agent": agent,
            "exe_source_generation": identity.get("source_generation"),
        }
    else:
        if pinned:
            raise RuntimeError(
                "a pinned EXE build requires --test-game-generation to avoid "
                "using an unrelated current game DLL")
        game_current = _read_json(GAME_CURRENT)
        game_ready = _read_json(GAME_READY)
        if game_current.get("status") != "ok" or game_ready.get("status") != "ready":
            raise RuntimeError("current gameplay DLL is not published and ready")
        if (game_current.get("generation") != game_ready.get("generation") or
                game_current.get("revision") != game_ready.get("revision") or
                game_current.get("code_hash") != game_ready.get("code_hash")):
            raise RuntimeError("gameplay DLL ready identity does not match current identity")
        source_generation = int(state.get("source_generation") or 0)
        game_source_generation = int(
            state.get("game_build_source_generation") or
            (state.get("game_build") or {}).get("source_generation") or 0)
        game_build = state.get("game_build") or {}
        if source_generation and (game_build.get("status") != "published" or
                                  game_source_generation != source_generation):
            raise RuntimeError(
                f"gameplay DLL source generation {game_source_generation} is not current {source_generation}"
            )
        cold_changes = _cold_compiler_changes()
        if cold_changes:
            raise RuntimeError("cold EXE compiler inputs are dirty: " + ", ".join(cold_changes[:8]))
        source_validation = {
            "result": "cold_inputs_clean_game_dll_current",
            "cold_input_changes": cold_changes,
            "daemon_reported_stale": bool(state.get("latest_build_stale")),
            "build_source_changed_during_build": bool(identity.get("source_changed_during_build")),
            "current_source_generation": source_generation,
            "exe_source_generation": identity.get("source_generation"),
            "game_dll_source_generation": game_source_generation,
            "pinned": False,
            "requested_agent": agent,
        }
        game_identity = {
            "generation": game_current.get("generation"),
            "revision": game_current.get("revision"),
            "code_hash": game_current.get("code_hash"),
            "source_generation": game_source_generation or source_generation or None,
            "game_dll": str((ROOT / str(game_current.get("dll") or "")).resolve())
            if game_current.get("dll") else "",
            "source_validation": source_validation,
        }
    source_validation = dict(game_identity.get("source_validation") or {})
    source_validation["pinned_exe"] = pinned
    source_validation["requested_agent"] = agent
    game_identity["source_validation"] = source_validation
    return exe, identity, game_identity


def _profile(profile_name: str) -> dict:
    path = Path(profile_name)
    if path.suffix.lower() != ".json":
        path = PROFILE_ROOT / f"{profile_name}.json"
    elif not path.is_absolute():
        path = ROOT / path
    if not path.is_file() or not path.resolve().is_relative_to(ROOT.resolve()):
        raise RuntimeError(f"profile not found inside checkout: {path}")
    result = _read_json(path)
    if not result:
        raise RuntimeError(f"profile is empty or invalid: {path}")
    result["_path"] = str(path.resolve())
    return result


def _launch_mode(mode_id: str) -> dict:
    config = _read_json(LAUNCH_MODES)
    modes = config.get("modes") if isinstance(config, dict) else None
    mode = modes.get(str(mode_id)) if isinstance(modes, dict) else None
    if not isinstance(mode, dict):
        raise RuntimeError(f"unknown launch mode {mode_id}")
    return mode


def _map_name(profile: dict, mode: dict, requested: str = "") -> str:
    map_name = requested or str(mode.get("map") or profile.get("map") or "")
    if not map_name:
        raise RuntimeError("test run has no configured map")
    asset = ROOT / "assets" / "maps" / f"{map_name}.glb"
    if not asset.is_file():
        raise RuntimeError(f"test map asset is missing: {map_name}")
    return map_name


def _server_args(exe: Path, profile: dict, mode: dict, map_name: str,
                 room_file: Path) -> list[str]:
    bind = str(profile.get("server_bind", "127.0.0.1:0")).strip()
    host = bind.rsplit(":", 1)[0].strip("[]").lower()
    if host not in {"127.0.0.1", "localhost", "::1"}:
        raise RuntimeError(f"isolated test runs require loopback bind, got {bind}")
    npc_count = max(0, int(profile.get("npc_count", 0) or 0))
    weapon_set = mode.get("weapon_set", profile.get("weapon_set", "auto"))
    args = [str(exe), "--server", "--bind", bind,
            "--name", str(profile.get("server_name", "MiMITA Test Server")),
            "--map", map_name, "--mode", str(profile.get("mode", "sandbox")),
            "--max-players", str(max(1, int(profile.get("max_players", 999) or 999))),
            "--map-rotation-minutes", str(max(1, int(profile.get("map_rotation_minutes", 15) or 15))),
            "--password-protected", "0", "--password", "", "--room-file", str(room_file)]
    if str(weapon_set).lower() != "auto":
        args.extend(["--weapon-set", str(weapon_set)])
    host_player = str(profile.get("host_player_name", "") or profile.get("client_name", "")).strip()
    if host_player:
        args.extend(["--host-player", host_player])
    if npc_count:
        args.extend(["--npcs", str(npc_count)])
    else:
        args.append("--no-npcs")
    if not profile.get("auto_map_rotation", False):
        args.append("--no-map-rotation")
    if not profile.get("discord_notification", False):
        args.append("--no-discord-notification")
    if profile.get("duel", False):
        args.extend(["--duel", "--gamemode", str(profile.get("gamemode", "duel"))])
    args.extend(str(value) for value in mode.get("server_args", []))
    return args


def append_command_queue(queue_path: Path, request_id: str, command: str,
                         scenario_id: str = "parallel_agent_test") -> None:
    record = {
        "request_id": request_id, "command": command,
        "scenario_id": scenario_id, "step_id": request_id,
        "step_index": 0, "step_count": 1,
        "not_before_ms": int(time.time() * 1000),
    }
    queue_path.parent.mkdir(parents=True, exist_ok=True)
    with queue_path.open("a", encoding="utf-8", newline="\n") as out:
        out.write(json.dumps(record, separators=(",", ":")) + "\n")


def pid_alive(pid: int) -> bool:
    if pid <= 0:
        return False
    if os.name == "nt":
        kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
        handle = kernel32.OpenProcess(0x1000, False, int(pid))
        if not handle:
            return False
        try:
            exit_code = ctypes.c_ulong()
            return bool(kernel32.GetExitCodeProcess(handle, ctypes.byref(exit_code)) and
                        exit_code.value == 259)
        finally:
            kernel32.CloseHandle(handle)
    try:
        os.kill(pid, 0)
        return True
    except OSError:
        return False


def _process_identity(pid: int) -> dict:
    if os.name != "nt" or pid <= 0:
        return {}
    class FileTime(ctypes.Structure):
        _fields_ = [("low", wintypes.DWORD), ("high", wintypes.DWORD)]

    try:
        kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
        kernel32.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL,
                                         wintypes.DWORD]
        kernel32.OpenProcess.restype = wintypes.HANDLE
        kernel32.QueryFullProcessImageNameW.argtypes = [
            wintypes.HANDLE, wintypes.DWORD, wintypes.LPWSTR,
            ctypes.POINTER(wintypes.DWORD)]
        kernel32.QueryFullProcessImageNameW.restype = wintypes.BOOL
        kernel32.GetProcessTimes.argtypes = [
            wintypes.HANDLE, ctypes.POINTER(FileTime), ctypes.POINTER(FileTime),
            ctypes.POINTER(FileTime), ctypes.POINTER(FileTime)]
        kernel32.GetProcessTimes.restype = wintypes.BOOL
        kernel32.CloseHandle.argtypes = [wintypes.HANDLE]
        kernel32.CloseHandle.restype = wintypes.BOOL

        handle = kernel32.OpenProcess(0x1000, False, int(pid))
        if not handle:
            return {}
        try:
            image = ctypes.create_unicode_buffer(32768)
            image_size = wintypes.DWORD(len(image))
            created = FileTime()
            exited = FileTime()
            kernel = FileTime()
            user = FileTime()
            if not kernel32.QueryFullProcessImageNameW(
                    handle, 0, image, ctypes.byref(image_size)):
                return {}
            if not kernel32.GetProcessTimes(
                    handle, ctypes.byref(created), ctypes.byref(exited),
                    ctypes.byref(kernel), ctypes.byref(user)):
                return {}
            ticks = (int(created.high) << 32) | int(created.low)
            started = (datetime.datetime(1601, 1, 1, tzinfo=datetime.timezone.utc)
                       + datetime.timedelta(microseconds=ticks // 10))
            return {"executable": image.value,
                    "process_started_utc": started.isoformat(
                        timespec="milliseconds").replace("+00:00", "Z")}
        finally:
            kernel32.CloseHandle(handle)
    except (AttributeError, OSError, TypeError, ValueError):
        return {}


def _wait_event(journal: Path, event_name: str, pid: int, timeout: float,
                *, role: str = "", field: str = "", value=None,
                process: subprocess.Popen | None = None) -> dict | None:
    deadline = time.monotonic() + timeout
    offset = 0
    while time.monotonic() < deadline:
        try:
            with journal.open("rb") as stream:
                stream.seek(offset)
                while True:
                    line = stream.readline()
                    if not line:
                        break
                    offset = stream.tell()
                    try:
                        event = json.loads(line.decode("utf-8", errors="replace"))
                    except json.JSONDecodeError:
                        continue
                    if event.get("event") != event_name or event.get("pid") != pid:
                        continue
                    fields = event.get("fields", {})
                    if role and fields.get("process_role", fields.get("process")) != role:
                        continue
                    if field and fields.get(field) != value:
                        continue
                    return event
        except OSError:
            pass
        if process is not None and process.poll() is not None:
            return None
        time.sleep(0.1)
    return None


def _scan_server_health(journal: Path, offset: int, state: dict) -> int:
    """Accumulate server tick progress and stall evidence since the last poll."""
    try:
        with journal.open("rb") as stream:
            stream.seek(offset)
            while True:
                line = stream.readline()
                if not line:
                    break
                offset = stream.tell()
                try:
                    event = json.loads(line.decode("utf-8", errors="replace"))
                except json.JSONDecodeError:
                    continue
                tick = event.get("tick")
                if isinstance(tick, int) and tick > state["latest_tick"]:
                    state["latest_tick"] = tick
                name = event.get("event")
                fields = event.get("fields", {})
                if name == "performance.server-tick-stall":
                    total = fields.get("total_ms")
                    if isinstance(total, (int, float)):
                        state["max_stall_ms"] = max(
                            state["max_stall_ms"], float(total))
                elif name == "performance.server-tick-window":
                    percent = fields.get("over_100ms_percent")
                    if isinstance(percent, (int, float)):
                        state["over_100ms_percent"] = max(
                            state["over_100ms_percent"], float(percent))
    except OSError:
        pass
    return offset


def _wait_mode_active(client_journal: Path, server_journal: Path,
                      client_pid: int, server_pid: int,
                      process: subprocess.Popen | None = None) -> dict:
    """Wait for active mode using simulation progress and server health.

    A wall-clock-only wait misclassifies a heavily stalled server as a mode
    failure. The server can be in intermission for many wall seconds when one
    tick costs hundreds of milliseconds, so this gate distinguishes
    ``server_stalled`` from a client that never observed active mode.
    """
    timeout = float(os.environ.get(
        "MIMITA_TEST_MODE_ACTIVE_TIMEOUT_SECONDS", "240"))
    sim_budget = int(os.environ.get(
        "MIMITA_TEST_MODE_ACTIVE_SIM_TICKS", "2400"))
    stall_budget_ms = float(os.environ.get(
        "MIMITA_TEST_MODE_STALL_MS", "1500"))
    client_offset = 0
    server_offset = 0
    health = {
        "initial_tick": 0,
        "latest_tick": 0,
        "max_stall_ms": 0.0,
        "over_100ms_percent": 0.0,
    }
    started = time.monotonic()
    while time.monotonic() - started < timeout:
        try:
            with client_journal.open("rb") as stream:
                stream.seek(client_offset)
                while True:
                    line = stream.readline()
                    if not line:
                        break
                    client_offset = stream.tell()
                    try:
                        event = json.loads(
                            line.decode("utf-8", errors="replace"))
                    except json.JSONDecodeError:
                        continue
                    if event.get("pid") != client_pid:
                        continue
                    name = event.get("event")
                    fields = event.get("fields", {})
                    if name == "network.mode.active":
                        return event
                    # Compatibility for generations that predate the canonical
                    # active event.
                    if (name == "network.mode.start" and
                            fields.get("stage") == "active"):
                        return event
        except OSError:
            pass

        server_offset = _scan_server_health(
            server_journal, server_offset, health)
        if health["initial_tick"] == 0 and health["latest_tick"] > 0:
            health["initial_tick"] = health["latest_tick"]
        elapsed = time.monotonic() - started
        advanced = max(0, health["latest_tick"] - health["initial_tick"])
        if elapsed > 30.0 and advanced > 0:
            tick_rate = advanced / elapsed
            if tick_rate < 15.0 or health["max_stall_ms"] > stall_budget_ms:
                raise RuntimeError(
                    "server stalled before active mode: "
                    f"tick {health['initial_tick']}->{health['latest_tick']} "
                    f"in {elapsed:.1f}s, "
                    f"max_tick_stall_ms={health['max_stall_ms']:.1f}, "
                    f"over_100ms_percent={health['over_100ms_percent']:.1f}")
        if advanced >= sim_budget:
            raise RuntimeError(
                f"client did not observe active mode after {advanced} server "
                f"ticks (max_tick_stall_ms={health['max_stall_ms']:.1f})")
        if process is not None and process.poll() is not None:
            raise RuntimeError("test client exited before active mode")
        time.sleep(0.2)
    raise RuntimeError(
        f"timed out after {timeout:.0f}s waiting for active mode; "
        f"server tick {health['initial_tick']}->{health['latest_tick']}, "
        f"max_tick_stall_ms={health['max_stall_ms']:.1f}, "
        f"over_100ms_percent={health['over_100ms_percent']:.1f}")


def _write_instance_record(layout: RunLayout, manifest: dict,
                           instance_id: str, values: dict) -> None:
    rows = manifest.setdefault("instances", [])
    for index, row in enumerate(rows):
        if row.get("instance_id") == instance_id:
            rows[index] = {**row, **values}
            break
    else:
        rows.append({"instance_id": instance_id, **values})
    write_run_manifest(layout, manifest)


def start_parallel_run(profile_name: str, launch_mode_id: str,
                       requested_map: str = "", *, build_id: str = "",
                       game_generation: int = 0,
                       agent: str = "") -> dict:
    """Start a fresh, isolated server/client pair from a pinned artifact."""
    exe, exe_identity, game_identity = _published_identity(
        build_id, game_generation, agent)
    profile = _profile(profile_name)
    mode = _launch_mode(launch_mode_id)
    map_name = _map_name(profile, mode, requested_map)
    layout = create_run_layout()
    run_lease = _open_run_log_lease(layout)
    run_id = layout.run_id
    launch_id = datetime.datetime.now(datetime.timezone.utc).isoformat(
        timespec="milliseconds").replace("+00:00", "Z")
    queue_path = DEV_ROOT / f"agent-command-{run_id}-client-01.jsonl"
    room_file = layout.run_dir / "room-code.txt"
    metadata = {
        "run_id": run_id, "build_id": str(exe_identity.get("build_id") or ""),
        "artifact_id": str(exe_identity.get("artifact_id") or ""),
        "source_generation": exe_identity.get("source_generation"),
        "executable": str(exe.resolve()), "executable_sha256": exe_identity.get("sha256"),
        "game_generation": game_identity.get("generation"),
        "game_revision": game_identity.get("revision"),
        "game_code_hash": game_identity.get("code_hash"),
        "game_source_generation": game_identity.get("source_generation"),
        "game_dll": game_identity.get("game_dll", ""),
        "game_dll_sha256": game_identity.get("game_dll_sha256", ""),
        "source_validation": game_identity.get("source_validation", {}),
        "agent": agent,
        "profile": profile_name, "launch_mode": str(launch_mode_id),
        "mode": mode.get("name", launch_mode_id), "map": map_name,
    }
    manifest = {
        "schema_version": 1, "run_id": run_id,
        "test_run_kind": "parallel_agent_test",
        "checkout_root": str(ROOT), "created_utc": launch_id,
        "build_id": metadata["build_id"], "artifact_id": metadata["artifact_id"],
        "source_generation": metadata["source_generation"],
        "build_identity": metadata, "profile": profile_name,
        "launch_mode": str(launch_mode_id), "map": map_name,
        "mode": mode.get("name", launch_mode_id), "status": "starting",
        "instances": [],
    }
    write_run_manifest(layout, manifest)
    _append_event(layout, "dev-loop.parallel-run.started",
                  {"build_id": metadata["build_id"], "artifact_id": metadata["artifact_id"],
                   "executable": metadata["executable"], "executable_sha256": metadata["executable_sha256"],
                   "game_generation": metadata["game_generation"],
                   "game_code_hash": metadata["game_code_hash"],
                   "source_validation": metadata["source_validation"],
                   "profile": profile_name, "launch_mode": str(launch_mode_id),
                   "map": map_name}, "isolated test run started under the root dev-loop CLI")
    server: subprocess.Popen | None = None
    client: subprocess.Popen | None = None
    server_output_capture = None
    server_logger_started = False
    server_start_monotonic = 0.0
    try:
        server_args = _server_args(exe, profile, mode, map_name, room_file)
        server_env = {
            **os.environ, "MIMITA_EVENTS_FILE": str(layout.server_journal),
            "MIMITA_INSTANCE_ID": "server-01", "MIMITA_LAUNCH_ID": launch_id,
            "MIMITA_DEV_LOOP_SERVER": "1", "MIMITA_RUN_ID": run_id,
            "MIMITA_EXPECTED_RUN_ID": run_id,
            "MIMITA_EXPECTED_BUILD_ID": metadata["build_id"],
            "MIMITA_BUILD_ID": metadata["build_id"],
            "MIMITA_RUN_METADATA": json.dumps(metadata, separators=(",", ":")),
            "MIMITA_PROCESS_ROLE": "server",
            **({"MIMITA_GAME_DLL": str(metadata["game_dll"])}
               if metadata.get("game_dll") else {}),
        }
        server_start_monotonic = time.monotonic()
        server = subprocess.Popen(
            server_args, cwd=ROOT, env=server_env, stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, text=True, encoding="utf-8",
            errors="replace", bufsize=1,
            creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
        server_output_capture = _capture_process_output_tail(server)
        server_identity = _process_identity(server.pid)
        if os.name == "nt" and not server_identity.get("process_started_utc"):
            raise RuntimeError(f"cannot verify test server process identity for pid {server.pid}")
        server_info = {"role": "server", "pid": server.pid, "executable": str(exe.resolve()),
                       "process_started_utc": server_identity.get("process_started_utc", ""),
                       "build_id": metadata["build_id"], "launch_id": launch_id,
                       "started_utc": launch_id, "state": "starting",
                       "mode": metadata["mode"], "map": map_name,
                       "journal_path": str(layout.server_journal), "args": server_args}
        _write_instance_record(layout, manifest, "server-01", server_info)
        _append_event(layout, "dev-loop.instance.started",
                      {"instance_id": "server-01", "role": "server", "pid": server.pid,
                       "build_id": metadata["build_id"], "journal_path": str(layout.server_journal)},
                      "isolated test server process created")
        if _wait_event(layout.server_journal, "logger.started", server.pid, 25.0,
                       role="server", process=server) is None:
            raise RuntimeError(f"test server {server.pid} did not report logger.started")
        server_logger_started = True
        if _wait_event(layout.server_journal, "module.host.started", server.pid, 30.0) is None:
            raise RuntimeError("test server did not report module.host.started")
        expected_generation = int(game_identity.get("generation") or 0)
        pinned_game = bool(
            (game_identity.get("source_validation") or {}).get(
                "pinned_game_generation"))
        if expected_generation and not pinned_game and _wait_event(
                layout.server_journal, "performance.server-phase-budget", server.pid,
                30.0, field="policy_generation", value=expected_generation) is None:
            raise RuntimeError(
                f"test server did not run gameplay policy generation {expected_generation}"
            )
        ready = _wait_event(layout.server_journal, "network.server.local-gameplay-ready",
                            server.pid, 60.0)
        port = int(((ready or {}).get("fields") or {}).get("port") or 0)
        if not port:
            raise RuntimeError(f"test server {server.pid} did not publish a local gameplay port")
        room_code = room_file.read_text(encoding="utf-8").strip() if room_file.is_file() else ""
        server_info.update({"state": "online", "bind": f"127.0.0.1:{port}",
                            "local_port": port, "room_code": room_code})
        _write_instance_record(layout, manifest, "server-01", server_info)

        client_name = str(profile.get("client_name", "Melee Test Client"))
        client_args = [str(exe), "--connect", f"127.0.0.1:{port}",
                       "--room-code-file", str(room_file), "--map", map_name,
                       "--name", client_name, "--unlock-cursor",
                       "--agent-command-file", str(queue_path)]
        client_args.extend(str(value) for value in mode.get("client_args", []))
        client_env = {
            **os.environ, "MIMITA_EVENTS_FILE": str(layout.client_journal),
            "MIMITA_INSTANCE_ID": "client-01", "MIMITA_LAUNCH_ID": launch_id,
            "MIMITA_DEV_LOOP_SERVER": "1", "MIMITA_RUN_ID": run_id,
            "MIMITA_AGENT_HEADLESS_GUI": "1",
            "MIMITA_EXPECTED_RUN_ID": run_id,
            "MIMITA_EXPECTED_BUILD_ID": metadata["build_id"],
            "MIMITA_BUILD_ID": metadata["build_id"],
            "MIMITA_RUN_METADATA": json.dumps(metadata, separators=(",", ":")),
            "MIMITA_PROCESS_ROLE": "client",
            **({"MIMITA_GAME_DLL": str(metadata["game_dll"])}
               if metadata.get("game_dll") else {}),
        }
        client = subprocess.Popen(client_args, cwd=ROOT, env=client_env,
                                  creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
        client_identity = _process_identity(client.pid)
        if os.name == "nt" and not client_identity.get("process_started_utc"):
            raise RuntimeError(f"cannot verify test client process identity for pid {client.pid}")
        client_info = {"role": "client", "pid": client.pid, "executable": str(exe.resolve()),
                       "process_started_utc": client_identity.get("process_started_utc", ""),
                       "build_id": metadata["build_id"], "launch_id": launch_id,
                       "started_utc": launch_id, "state": "starting",
                       "server_instance_id": "server-01", "connected_address": f"127.0.0.1:{port}",
                       "mode": metadata["mode"], "map": map_name,
                       "journal_path": str(layout.client_journal),
                       "command_queue": str(queue_path), "args": client_args}
        _write_instance_record(layout, manifest, "client-01", client_info)
        _append_event(layout, "dev-loop.instance.started",
                      {"instance_id": "client-01", "role": "client", "pid": client.pid,
                       "build_id": metadata["build_id"], "journal_path": str(layout.client_journal)},
                      "isolated test client process created")
        if _wait_event(layout.client_journal, "logger.started", client.pid, 25.0,
                       role="client") is None:
            raise RuntimeError(f"test client {client.pid} did not report logger.started")
        for event_name in ("module.host.started", "game.generation.first_tick"):
            if _wait_event(layout.client_journal, event_name, client.pid, 30.0) is None:
                raise RuntimeError(f"test client did not report {event_name}")
        if _wait_event(layout.client_journal, "network.connection-state.changed",
                       client.pid, 45.0, field="to", value="Connected") is None:
            raise RuntimeError(f"test client {client.pid} did not connect to {port}")
        active = _wait_mode_active(
            layout.client_journal, layout.server_journal,
            client.pid, server.pid, client)
        generation = int(game_identity.get("generation") or 0)
        pinned_game = bool(
            (game_identity.get("source_validation") or {}).get(
                "pinned_game_generation"))
        if not pinned_game:
            current_game = _read_json(GAME_CURRENT)
            ready_game = _read_json(GAME_READY)
            if (current_game.get("generation") != generation or
                    ready_game.get("generation") != generation or
                    current_game.get("code_hash") != game_identity.get("code_hash") or
                    ready_game.get("code_hash") != game_identity.get("code_hash")):
                raise RuntimeError(
                    "gameplay DLL changed while the isolated run was starting")
        client_info["state"] = "online"
        client_info["active_mode_tick"] = active.get("tick")
        _write_instance_record(layout, manifest, "client-01", client_info)
        manifest["status"] = "online"
        write_run_manifest(layout, manifest)
        _append_event(layout, "dev-loop.parallel-run.ready",
                      {"build_id": metadata["build_id"], "game_generation": generation,
                       "server_pid": server.pid, "client_pid": client.pid,
                       "server_journal_path": str(layout.server_journal),
                       "client_journal_path": str(layout.client_journal)},
                      "isolated server/client pair connected and entered active mode")
        run_lease[0].CloseHandle(run_lease[1])
        run_lease = None
        return {"run_id": run_id, "run_dir": str(layout.run_dir),
                "manifest_path": str(layout.manifest_path), "status": "online",
                "build_identity": metadata, "instances": manifest["instances"],
                "server_journal_path": str(layout.server_journal),
                "client_journal_path": str(layout.client_journal)}
    except Exception as error:
        server_exit_code_before_cleanup = server.poll() if server is not None else None
        for process in (client, server):
            if process is not None and process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=3.0)
                except subprocess.TimeoutExpired:
                    process.kill()
        if server_output_capture is not None:
            server_output_capture[2].join(timeout=1.0)
        for row in manifest.get("instances", []):
            pid = int(row.get("pid") or 0)
            row["state"] = "failed" if pid else "not_started"
            row["exit_code"] = next(
                (process.returncode for process in (client, server)
                 if process is not None and process.pid == pid), None)
        manifest["status"] = "failed"
        manifest["failure"] = str(error)
        write_run_manifest(layout, manifest)
        failure_fields = {
            "error": str(error), "server_pid": server.pid if server else None,
            "client_pid": client.pid if client else None,
        }
        if server is not None and not server_logger_started:
            failure_fields.update({
                "server_exit_code_before_cleanup": server_exit_code_before_cleanup,
                "server_startup_elapsed_ms": round(
                    (time.monotonic() - server_start_monotonic) * 1000, 3)
                    if server_start_monotonic else 0.0,
                "server_journal_exists": layout.server_journal.is_file(),
            })
            if server_output_capture is not None:
                tail, tail_lock, _ = server_output_capture
                with tail_lock:
                    failure_fields["server_startup_output_tail"] = list(tail)
        _append_event(layout, "dev-loop.parallel-run.failed", failure_fields,
                      "isolated test run failed during readiness")
        if run_lease is not None:
            run_lease[0].CloseHandle(run_lease[1])
            run_lease = None
        raise


def start_second_client(run_id: str, client_name: str = "") -> dict:
    """Add a second client to a starting or online isolated run for PvP evidence."""
    layout = _run_layout(run_id)
    manifest = read_run_manifest(layout)
    if manifest.get("status") not in ("starting", "online"):
        raise RuntimeError(f"test run {run_id} is not online or starting")
    server = next((item for item in manifest.get("instances", [])
                   if item.get("instance_id") == "server-01" and item.get("role") == "server"), None)
    if not server or not pid_alive(int(server.get("pid") or 0)):
        raise RuntimeError(f"test run {run_id} server is not alive")
    port = int(server.get("local_port") or 0)
    if not port:
        raise RuntimeError(f"test run {run_id} server has no local port")
    existing = next((item for item in manifest.get("instances", [])
                     if item.get("instance_id") == "client-02"), None)
    if existing and pid_alive(int(existing.get("pid") or 0)):
        raise RuntimeError(f"test run {run_id} already has a live client-02")
    identity = manifest.get("build_identity") or {}
    exe = Path(str(identity.get("executable") or "")).resolve()
    if not exe.is_file():
        raise RuntimeError("test run executable is missing")
    mode = _launch_mode(str(manifest.get("launch_mode") or "41"))
    map_name = str(manifest.get("map") or "")
    room_file = layout.run_dir / "room-code.txt"
    journal = layout.run_dir / "client-02" / "events.jsonl"
    journal.parent.mkdir(parents=True, exist_ok=True)
    queue_path = DEV_ROOT / f"agent-command-{run_id}-client-02.jsonl"
    name = client_name or "Melee Test Client 2"
    launch_id = datetime.datetime.now(datetime.timezone.utc).isoformat(
        timespec="milliseconds").replace("+00:00", "Z")
    args = [str(exe), "--connect", f"127.0.0.1:{port}",
            "--room-code-file", str(room_file), "--map", map_name,
            "--name", name, "--unlock-cursor",
            "--agent-command-file", str(queue_path)]
    args.extend(str(value) for value in mode.get("client_args", []))
    env = {
        **os.environ, "MIMITA_EVENTS_FILE": str(journal),
        "MIMITA_INSTANCE_ID": "client-02", "MIMITA_LAUNCH_ID": launch_id,
        "MIMITA_DEV_LOOP_SERVER": "1", "MIMITA_RUN_ID": run_id,
        "MIMITA_AGENT_HEADLESS_GUI": "1",
        "MIMITA_EXPECTED_RUN_ID": run_id,
        "MIMITA_EXPECTED_BUILD_ID": str(identity.get("build_id") or ""),
        "MIMITA_BUILD_ID": str(identity.get("build_id") or ""),
        "MIMITA_RUN_METADATA": json.dumps(identity, separators=(",", ":")),
        "MIMITA_PROCESS_ROLE": "client",
    }
    client = subprocess.Popen(args, cwd=ROOT, env=env,
                              creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
    try:
        process_identity = _process_identity(client.pid)
        if os.name == "nt" and not process_identity.get("process_started_utc"):
            raise RuntimeError(f"cannot verify test client process identity for pid {client.pid}")
        info = {"role": "client", "pid": client.pid, "executable": str(exe),
                "process_started_utc": process_identity.get("process_started_utc", ""),
                "build_id": str(identity.get("build_id") or ""),
                "launch_id": launch_id, "started_utc": launch_id,
                "state": "starting", "server_instance_id": "server-01",
                "connected_address": f"127.0.0.1:{port}",
                "mode": manifest.get("mode"), "map": map_name,
                "journal_path": str(journal), "command_queue": str(queue_path),
                "args": args}
        _write_instance_record(layout, manifest, "client-02", info)
        _append_event(layout, "dev-loop.instance.started",
                      {"instance_id": "client-02", "role": "client", "pid": client.pid,
                       "build_id": info["build_id"], "journal_path": str(journal)},
                      "isolated second test client process created")
        if _wait_event(journal, "logger.started", client.pid, 25.0, role="client") is None:
            raise RuntimeError(f"test client {client.pid} did not report logger.started")
        for event_name in ("module.host.started", "game.generation.first_tick"):
            if _wait_event(journal, event_name, client.pid, 30.0) is None:
                raise RuntimeError(f"second test client did not report {event_name}")
        if _wait_event(journal, "network.connection-state.changed",
                       client.pid, 45.0, field="to", value="Connected") is None:
            raise RuntimeError(f"second test client {client.pid} did not connect to {port}")
        active = _wait_mode_active(
            journal, layout.server_journal, client.pid,
            int(server.get("pid") or 0), client)
        info.update({"state": "online", "active_mode_tick": active.get("tick")})
        _write_instance_record(layout, manifest, "client-02", info)
        _append_event(layout, "dev-loop.parallel-run.client02-ready",
                      {"client_pid": client.pid, "journal_path": str(journal),
                       "active_mode_tick": active.get("tick")},
                      "second isolated test client connected and entered active mode")
        return {"run_id": run_id, "client_id": "client-02", "client_pid": client.pid,
                "client_journal_path": str(journal), "queue_path": str(queue_path),
                "state": "online"}
    except Exception:
        if client.poll() is None:
            client.terminate()
            try:
                client.wait(timeout=3.0)
            except subprocess.TimeoutExpired:
                client.kill()
        raise


def _run_layout(run_id: str) -> RunLayout:
    logs_root = (ROOT / "logs").resolve()
    matches = list((ROOT / "logs").glob(f"*/{run_id}"))
    path = matches[0].resolve() if len(matches) == 1 else logs_root / "__missing__"
    if not path.is_relative_to(logs_root) or not path.is_dir():
        raise RuntimeError(f"test run not found inside logs/: {run_id}")
    manifest_path = path / "run.json"
    data = _read_json(manifest_path)
    if data.get("run_id") != run_id or data.get("test_run_kind") != "parallel_agent_test":
        raise RuntimeError(f"run {run_id} is not a managed parallel test run")
    return RunLayout(path, run_id, manifest_path, path / "dev-loop" / "events.jsonl",
                     path / "server-01" / "events.jsonl", path / "client-01" / "events.jsonl")


def list_parallel_runs() -> list[dict]:
    rows = []
    logs = ROOT / "logs"
    if not logs.is_dir():
        return rows
    for manifest_path in logs.glob("*/*/run.json"):
        payload = _read_json(manifest_path)
        if payload.get("test_run_kind") != "parallel_agent_test":
            continue
        rows.append({"run_id": payload.get("run_id"), "status": payload.get("status"),
                     "build_id": (payload.get("build_identity") or {}).get("build_id"),
                     "mode": payload.get("mode"), "map": payload.get("map"),
                     "run_dir": str(manifest_path.parent),
                     "instances": payload.get("instances", [])})
    return sorted(rows, key=lambda row: str(row.get("run_id") or ""), reverse=True)


def queue_parallel_command(run_id: str, client_id: str, command: str) -> dict:
    if not command.strip():
        raise RuntimeError("command is empty")
    layout = _run_layout(run_id)
    manifest = read_run_manifest(layout)
    if manifest.get("status") not in ("starting", "online"):
        raise RuntimeError(f"test run {run_id} is not online or starting")
    row = next((item for item in manifest.get("instances", [])
                if item.get("instance_id") == client_id and item.get("role") == "client"), None)
    if not row or not pid_alive(int(row.get("pid") or 0)):
        raise RuntimeError(f"test client {client_id} is not alive")
    queue_path = Path(str(row.get("command_queue") or ""))
    if not queue_path.resolve().is_relative_to(DEV_ROOT.resolve()):
        raise RuntimeError("test command queue path escaped .dev")
    request_id = f"parallel-{uuid.uuid4().hex[:16]}"
    append_command_queue(queue_path, request_id, command)
    _append_event(layout, "dev-loop.command.request",
                  {"request_id": request_id, "client_id": client_id,
                   "client_pid": row.get("pid"), "command": command,
                   "queue_path": str(queue_path), "journal_path": row.get("journal_path")},
                  "queued command to isolated test client")
    return {"request_id": request_id, "client_journal_path": row.get("journal_path"),
            "queue_path": str(queue_path), "client_pid": row.get("pid")}


def stop_parallel_run(run_id: str, process_identity) -> dict:
    layout = _run_layout(run_id)
    manifest = read_run_manifest(layout)
    stopped = []
    failures = []
    expected_exe = str((manifest.get("build_identity") or {}).get("executable") or "")
    for row in manifest.get("instances", []):
        pid = int(row.get("pid") or 0)
        if not pid or not pid_alive(pid):
            row["state"] = "offline"
            continue
        identity = process_identity(pid)
        actual_exe = str((identity or {}).get("executable") or "")
        command_line = str((identity or {}).get("command_line") or "").lower()
        role = str(row.get("role") or "")
        if (not identity or not row.get("process_started_utc") or
                os.path.normcase(actual_exe) != os.path.normcase(expected_exe) or
                not _same_process_start_time(
                    str(identity.get("process_started_utc") or ""),
                    str(row.get("process_started_utc") or "")) or
                ((role == "server") != ("--server" in command_line))):
            failures.append({"pid": pid, "reason": "process identity does not match run manifest"})
            continue
        if os.name == "nt":
            result = subprocess.run(["taskkill", "/PID", str(pid), "/T", "/F"],
                                    cwd=ROOT, capture_output=True, text=True, check=False)
            if result.returncode != 0 and pid_alive(pid):
                failures.append({"pid": pid, "reason": result.stderr.strip() or result.stdout.strip()})
                continue
        else:
            os.kill(pid, 15)
        deadline = time.monotonic() + 5.0
        while pid_alive(pid) and time.monotonic() < deadline:
            time.sleep(0.1)
        if pid_alive(pid):
            failures.append({"pid": pid, "reason": "process remained alive after stop"})
        else:
            row["state"] = "stopped"
            stopped.append(pid)
    manifest["status"] = "stopped" if not failures else "stop_partial"
    write_run_manifest(layout, manifest)
    _append_event(layout, "dev-loop.parallel-run.stopped",
                  {"stopped_pids": stopped, "failures": failures},
                  "explicit stop scoped to the isolated test run")
    return {"run_id": run_id, "status": manifest["status"],
            "stopped_pids": stopped, "failures": failures,
            "run_dir": str(layout.run_dir)}
