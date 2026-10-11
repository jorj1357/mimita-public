#!/usr/bin/env python3
"""MiMITA's small external development loop.

This tool deliberately owns development orchestration only. The existing
build.py remains responsible for dependency tracking, PCH use, parallel
compilation, and linking. The game remains responsible for gameplay.

Usage:
    python devscripts/dev-loop.py --profile npc-navigation
    python devscripts/dev-loop.py --profile npc-navigation --auto-restart

The loop uses ccache automatically when build_toolchain.py can find it. It
never kills arbitrary mimita.exe processes; only children that it launched.
"""

from __future__ import annotations

import argparse
import builtins
import colorsys
import datetime
import hashlib
import json
import os
import random
import secrets
import shutil
import socket
import struct
import subprocess
import sys
import threading
import time
import tempfile
import uuid
from queue import Empty, Queue
from pathlib import Path

# Running this file as `python devscripts/dev-loop.py` puts `devscripts/`
# first on sys.path; add the repository root for the shared toolchain owner.
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from build_toolchain import compiler as resolve_compiler
from build_toolchain import glfw_lib as resolve_glfw_lib
from jsonc import load as load_jsonc

DEV_ROOT = ROOT / ".dev"
BUILD_ROOT = DEV_ROOT / "builds"
STATE_PATH = DEV_ROOT / "state.json"
CURRENT_RUN_PATH = DEV_ROOT / "current-run.json"
LOCK_PATH = DEV_ROOT / "dev-loop.lock"
PROFILE_ROOT = ROOT / "devscripts" / "dev-profiles"
LAUNCH_MODE_PATH = ROOT / "devscripts" / "dev-launch-modes.json"
DEFAULT_MAP_POOL_PATH = ROOT / "config" / "gamemode-good-maps.json"

# Snapshotting the watched source tree takes about 60 ms on this checkout.
# Polling at 150 ms consumed too much background CPU during client startup and
# could briefly steal frame time from the newly launched game.
POLL_SECONDS = 0.50
DEBOUNCE_SECONDS = 0.30
KEEP_BUILDS = 5
RAINBOW_SECONDS = 5.0
RUNTIME_DLLS = (
    "libgcc_s_seh-1.dll",
    "libstdc++-6.dll",
    "libwinpthread-1.dll",
    "glfw3.dll",
)

WATCH_EXTENSIONS = {".c", ".cc", ".cpp", ".h", ".hh", ".hpp", ".rc", ".py"}
WATCH_DIRECTORIES = (ROOT / "src", ROOT / "external" / "libjuice" / "src")
WATCH_FILES = (
    ROOT / "mimita.rc",
    ROOT / "build.py",
    ROOT / "build_agent.py",
    ROOT / "build_game_dll.py",
    ROOT / "build_toolchain.py",
    DEFAULT_MAP_POOL_PATH,
    LAUNCH_MODE_PATH,
)


def _rainbow_enabled() -> bool:
    return bool(getattr(sys.stdout, "isatty", lambda: False)())


def _rainbowize(value: str) -> str:
    if not value or not _rainbow_enabled():
        return value
    phase = (time.monotonic() % RAINBOW_SECONDS) / RAINBOW_SECONDS
    visible = max(1, sum(1 for char in value if char not in "\r\n"))
    index = 0
    output = []
    for char in value:
        if char in "\r\n":
            output.append(char)
            continue
        hue = (phase + (index / visible) * 0.72) % 1.0
        red, green, blue = (round(channel * 255) for channel in colorsys.hsv_to_rgb(hue, 0.86, 1.0))
        output.append(f"\x1b[38;2;{red};{green};{blue}m{char}")
        index += 1
    output.append("\x1b[0m")
    return "".join(output)


_status_clear_hook = None
_status_rendering = False


def dev_print(*values, sep=" ", end="\n", file=None, flush=False):
    global _status_clear_hook
    output = sep.join(str(value) for value in values)
    target = sys.stdout if file is None else file
    if target is sys.stdout and not _status_rendering and _status_clear_hook is not None:
        _status_clear_hook()
    if target in (sys.stdout, sys.stderr):
        output = _rainbowize(output)
    builtins.print(output, end=end, file=target, flush=flush)


# All development-loop and build-child output uses the same five-second
# moving rainbow when attached to a real terminal.
print = dev_print


def write_json(path: Path, value: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")
    os.replace(temporary, path)


def read_json(path: Path) -> dict:
    return load_jsonc(path)


def allowed_dev_maps(profile: dict) -> list[str]:
    configured = profile.get("allowed_maps_file", str(DEFAULT_MAP_POOL_PATH))
    path = Path(configured)
    if not path.is_absolute():
        path = ROOT / path
    try:
        data = read_json(path)
    except (OSError, json.JSONDecodeError) as error:
        raise RuntimeError(f"cannot read allowed map pool {path}: {error}") from error
    maps = data.get("maps") if isinstance(data, dict) else None
    if not isinstance(maps, list):
        raise RuntimeError(f"allowed map pool has no maps array: {path}")
    result = [str(item).strip() for item in maps if isinstance(item, str) and item.strip()]
    if not result:
        raise RuntimeError(f"allowed map pool is empty: {path}")
    return result


def create_shared_events_path() -> str:
    """One run directory + events.jsonl shared by the dev server and client."""
    now = datetime.datetime.now(datetime.timezone.utc)
    run_dir = ROOT / "logs" / now.strftime("%Y-%m-%d") / now.strftime("%Y%m%d_%H%M%S")
    suffix = 1
    base = run_dir
    while run_dir.exists():
        run_dir = Path(f"{base}_{suffix:02d}")
        suffix += 1
    run_dir.mkdir(parents=True, exist_ok=True)
    return str(run_dir / "events.jsonl")


def utc_now() -> str:
    return datetime.datetime.now(datetime.timezone.utc).isoformat(timespec="milliseconds").replace("+00:00", "Z")


def executable_identity(path: Path) -> dict:
    valid, reason, digest = _executable_snapshot_is_valid(path)
    stat = path.stat() if path.exists() else None
    return {
        "path": str(path),
        "exists": bool(stat),
        "valid": valid,
        "validation": reason,
        "sha256": digest if valid else "",
        "created_utc": datetime.datetime.fromtimestamp(
            stat.st_ctime, datetime.timezone.utc
        ).isoformat(timespec="milliseconds").replace("+00:00", "Z") if stat else "",
        "modified_utc": datetime.datetime.fromtimestamp(
            stat.st_mtime, datetime.timezone.utc
        ).isoformat(timespec="milliseconds").replace("+00:00", "Z") if stat else "",
        "size_bytes": stat.st_size if stat else 0,
    }


def select_dev_map(profile: dict, requested_map: str = "") -> str:
    maps = allowed_dev_maps(profile)
    if requested_map:
        requested_map = requested_map.strip()
        # A launch mode's explicit map is an operator choice, so it may be
        # outside the generic rotation pool. Keep the safety check by requiring
        # the actual map asset to exist before allowing the override.
        requested_asset = ROOT / "assets" / "maps" / f"{requested_map}.glb"
        if requested_map in maps or requested_asset.is_file():
            return requested_map
        print(f"[DEV] launch-mode map {requested_map} has no map asset; using {maps[0]}")
        return maps[0]
    selection = str(profile.get("map_selection", "configured")).lower()
    configured = str(profile.get("map", "")).strip()
    if selection == "random":
        return random.choice(maps)
    if configured in maps:
        return configured
    print(f"[DEV] configured map {configured or '(empty)'} is not allowed; using {maps[0]}")
    return maps[0]


def file_signature(path: Path):
    try:
        stat = path.stat()
        return (stat.st_mtime_ns, stat.st_size)
    except OSError:
        return None


def snapshot_inputs() -> dict[str, tuple[int, int] | None]:
    result = {}
    for path in WATCH_FILES:
        result[str(path)] = file_signature(path)
    for directory in WATCH_DIRECTORIES:
        if not directory.is_dir():
            continue
        for path in directory.rglob("*"):
            if path.is_file() and path.suffix.lower() in WATCH_EXTENSIONS:
                result[str(path)] = file_signature(path)
    return result


def find_ccache() -> str | None:
    configured = os.environ.get("MIMITA_CCACHE")
    if configured and Path(configured).is_file():
        return configured
    found = shutil.which("ccache")
    return found


def existing_dev_loop_pids() -> list[int]:
    """Find other dev-loop processes for this exact checkout on Windows."""
    if os.name != "nt":
        return []
    script_text = str(Path(__file__).resolve()).replace("/", "\\").lower()
    try:
        result = subprocess.run(
            [
                "powershell", "-NoProfile", "-Command",
                "Get-CimInstance Win32_Process -Filter \"Name='python.exe'\" "
                "| ForEach-Object { \"$($_.ProcessId)`t$($_.CommandLine)\" }",
            ],
            capture_output=True,
            text=True,
            timeout=3,
            check=False,
        )
    except (OSError, subprocess.SubprocessError):
        return []

    pids = []
    for line in result.stdout.splitlines():
        try:
            pid_text, command_line = line.split("\t", 1)
            pid = int(pid_text.strip())
        except (ValueError, TypeError):
            continue
        if pid != os.getpid() and script_text in command_line.replace("/", "\\").lower():
            pids.append(pid)
    return pids


def acquire_dev_loop_lock() -> int:
    """Allow exactly one dev-loop daemon for this checkout."""
    DEV_ROOT.mkdir(parents=True, exist_ok=True)
    existing = existing_dev_loop_pids()
    if existing:
        joined = ", ".join(str(pid) for pid in existing)
        raise SystemExit(
            f"[DEV] another dev-loop is already running for this checkout "
            f"(PID {joined}); close it before starting a new one"
        )
    try:
        descriptor = os.open(
            str(LOCK_PATH), os.O_CREAT | os.O_EXCL | os.O_WRONLY
        )
    except FileExistsError:
        owner = "unknown"
        try:
            owner = LOCK_PATH.read_text(encoding="ascii").strip() or owner
            owner_pid = int(owner)
            os.kill(owner_pid, 0)
        except (OSError, ValueError):
            try:
                LOCK_PATH.unlink()
            except OSError as error:
                raise SystemExit(
                    f"[DEV] another dev-loop owns {LOCK_PATH}: {error}"
                ) from error
            return acquire_dev_loop_lock()
        raise SystemExit(
            f"[DEV] dev-loop already running for this checkout (PID {owner_pid})"
        )
    os.write(descriptor, str(os.getpid()).encode("ascii"))
    os.close(descriptor)
    return os.getpid()


def release_dev_loop_lock(owner_pid: int) -> None:
    try:
        if LOCK_PATH.read_text(encoding="ascii").strip() == str(owner_pid):
            LOCK_PATH.unlink()
    except (FileNotFoundError, OSError):
        pass


def load_profile(name: str) -> dict:
    path = Path(name)
    if path.suffix.lower() != ".json":
        path = PROFILE_ROOT / (name + ".json")
    if not path.is_absolute():
        path = ROOT / path
    if not path.is_file():
        raise SystemExit(f"[DEV] profile not found: {path}")
    profile = read_json(path)
    profile["_path"] = str(path)
    return profile


def load_launch_mode(mode_id: str) -> dict:
    """Load the small JSON table that maps dev-loop mode numbers to args."""
    try:
        config = read_json(LAUNCH_MODE_PATH)
    except (OSError, json.JSONDecodeError) as error:
        raise SystemExit(f"[DEV] launch-mode config invalid {LAUNCH_MODE_PATH}: {error}") from error
    modes = config.get("modes") if isinstance(config, dict) else None
    mode = modes.get(str(mode_id)) if isinstance(modes, dict) else None
    if not isinstance(mode, dict):
        available = ", ".join(sorted(modes.keys())) if isinstance(modes, dict) else "(none)"
        raise SystemExit(f"[DEV] unknown launch mode {mode_id}; available: {available}")
    for key in ("server_args", "client_args"):
        if not isinstance(mode.get(key, []), list) or not all(
            isinstance(value, (str, int, float)) for value in mode.get(key, [])
        ):
            raise SystemExit(f"[DEV] launch mode {mode_id} has invalid {key}")
    return mode


def next_build_number() -> int:
    numbers = []
    if BUILD_ROOT.is_dir():
        for path in BUILD_ROOT.iterdir():
            if path.is_dir() and path.name.isdigit():
                numbers.append(int(path.name))
    return max(numbers, default=0) + 1


def published_builds() -> list[Path]:
    if not BUILD_ROOT.is_dir():
        return []
    return sorted(
        (p for p in BUILD_ROOT.iterdir() if p.is_dir() and p.name.isdigit()),
        key=lambda p: int(p.name),
        reverse=True,
    )


def newest_valid_published_build() -> int | None:
    """Return the newest numbered artifact that contains a runnable EXE."""
    for path in published_builds():
        exe = path / "mimita.exe"
        try:
            if exe.is_file() and _executable_snapshot_is_valid(exe)[0]:
                return int(path.name)
        except OSError:
            continue
    return None


def _executable_snapshot_is_valid(path: Path) -> tuple[bool, str, str]:
    """Check that a published PE has real executable bytes, not just headers."""
    try:
        data = path.read_bytes()
    except OSError as error:
        return False, "read failed", str(error)

    if len(data) < 0x1000 or data[:2] != b"MZ":
        return False, "invalid DOS header or file too small", ""
    pe_offset = struct.unpack_from("<I", data, 0x3C)[0]
    if pe_offset + 24 > len(data) or data[pe_offset:pe_offset + 4] != b"PE\0\0":
        return False, "invalid PE header", ""

    section_count = struct.unpack_from("<H", data, pe_offset + 6)[0]
    optional_offset = pe_offset + 24
    magic = struct.unpack_from("<H", data, optional_offset)[0]
    entry_rva = struct.unpack_from("<I", data, optional_offset + 16)[0]
    optional_size = struct.unpack_from("<H", data, pe_offset + 20)[0]
    section_offset = optional_offset + optional_size
    if magic not in (0x10B, 0x20B) or section_offset + section_count * 40 > len(data):
        return False, "invalid optional header or section table", ""

    executable_sections = 0
    entrypoint_checked = False
    for index in range(section_count):
        offset = section_offset + index * 40
        virtual_size, virtual_address = struct.unpack_from("<II", data, offset + 8)
        raw_size, raw_pointer = struct.unpack_from("<II", data, offset + 16)
        characteristics = struct.unpack_from("<I", data, offset + 36)[0]
        # IMAGE_SCN_MEM_EXECUTE = 0x20000000.
        if characteristics & 0x20000000:
            executable_sections += 1
            if raw_size == 0 or raw_pointer + raw_size > len(data):
                return False, "executable section is outside the file", ""
            section_bytes = data[raw_pointer:raw_pointer + raw_size]
            if any(section_bytes):
                if virtual_address <= entry_rva < virtual_address + max(virtual_size, raw_size):
                    entry_offset = raw_pointer + (entry_rva - virtual_address)
                    entry_bytes = data[entry_offset:entry_offset + 64]
                    if len(entry_bytes) < 16 or not any(entry_bytes):
                        return False, "entrypoint bytes are zero-filled", ""
                    entrypoint_checked = True
                continue
            return False, "executable section is entirely zero-filled", ""

    if executable_sections == 0:
        return False, "no executable sections", ""
    if not entrypoint_checked:
        return False, "entrypoint is not in an executable section", ""
    return True, "ok", hashlib.sha256(data).hexdigest()


def _copy_verified_executable(source: Path, destination: Path) -> None:
    """Copy a linked EXE atomically and reject an unstable/zero-filled copy."""
    last_reason = "unknown"
    for attempt in range(10):
        source_ok, source_reason, source_hash = _executable_snapshot_is_valid(source)
        if source_ok:
            # A second read catches a linker/antivirus still changing the file
            # after the build process reports success.
            _, _, source_hash_again = _executable_snapshot_is_valid(source)
            if source_hash_again != source_hash:
                last_reason = "source executable changed while being copied"
            else:
                temporary = destination.with_suffix(destination.suffix + ".tmp")
                shutil.copyfile(source, temporary)
                destination_ok, destination_reason, destination_hash = (
                    _executable_snapshot_is_valid(temporary)
                )
                if destination_ok and destination_hash == source_hash:
                    os.replace(temporary, destination)
                    return
                last_reason = (
                    f"published copy invalid ({destination_reason}) or hash mismatch"
                )
                try:
                    temporary.unlink()
                except FileNotFoundError:
                    pass
        else:
            last_reason = f"source executable invalid ({source_reason})"
        time.sleep(0.2 * (attempt + 1))
    raise RuntimeError(f"refusing to publish unstable executable: {last_reason}")


def publish_build(number: int) -> Path:
    source_exe = ROOT / "mimita.exe"
    if not source_exe.is_file():
        raise RuntimeError("build succeeded but mimita.exe was not produced")

    destination = BUILD_ROOT / f"{number:04d}"
    destination.mkdir(parents=True, exist_ok=True)
    _copy_verified_executable(source_exe, destination / "mimita.exe")

    runtime_sources = {name: ROOT / name for name in RUNTIME_DLLS}
    try:
        compiler_dir = Path(resolve_compiler()).parent
        runtime_sources.update({
            "libgcc_s_seh-1.dll": compiler_dir / "libgcc_s_seh-1.dll",
            "libstdc++-6.dll": compiler_dir / "libstdc++-6.dll",
            "libwinpthread-1.dll": compiler_dir / "libwinpthread-1.dll",
        })
        runtime_sources["glfw3.dll"] = Path(resolve_glfw_lib()) / "glfw3.dll"
    except FileNotFoundError:
        pass

    for name in RUNTIME_DLLS:
        source = runtime_sources[name]
        if source.is_file():
            shutil.copy2(source, destination / name)
        else:
            print(f"[DEV] published build missing runtime DLL: {source}")

    for old in published_builds()[KEEP_BUILDS:]:
        shutil.rmtree(old, ignore_errors=True)
    return destination


class DevLoop:
    def __init__(self, profile: dict, auto_restart: bool, launch_mode: str | None = None):
        self.profile = profile
        self.launch_mode_id = str(
            launch_mode if launch_mode is not None
            else profile.get("launch_mode", 1)
        )
        self.launch_mode = load_launch_mode(self.launch_mode_id)
        self.mode_picker_open = False
        self.mode_picker_input = ""
        # Builds always run while this loop is open. Auto-start only controls
        # whether a successful build or an exited child may launch the EXE.
        self.auto_restart = bool(auto_restart)
        self.manual_launch_requested = False
        self.stop_event = threading.Event()
        self.change_event = threading.Event()
        self.lock = threading.Lock()
        self.generation = 0
        self.last_snapshot = snapshot_inputs()
        self.processes: list[subprocess.Popen] = []
        # Durable independent-server record, mirroring the GUI external-server
        # contract. This is intentionally NOT part of self.processes (which is
        # daemon-owned and terminated on shutdown). Only an explicit server-stop
        # action may end the server.
        self.server_process: subprocess.Popen | None = None
        self.server_pid: int | None = None
        self.server_exe: str | None = None
        self.server_args: list[str] = []
        self.server_launch_ms: float = 0.0
        self.server_unavailable = False
        self.running_build = None
        self.latest_build = newest_valid_published_build()
        self.latest_generation = 0
        # A disk-restored artifact is usable, but it has not been proven
        # against this loop's current source snapshot yet. The initial build
        # must validate the current tree before the artifact can be launched.
        self.latest_stale = self.latest_build is not None
        self.build_pending = True
        self.last_message = (
            f"restored published build {self.latest_build:04d}; validating current source"
            if self.latest_build is not None
            else "starting; no published build found"
        )
        self.last_status_line = None
        self.room_file_path: Path | None = None
        self.room_code = None
        # Shared canonical events.jsonl for the server and its client. Created
        # once when the durable server starts and reused across client launches.
        self.events_path: str | None = None
        self.run_id: str | None = None
        self.run_started_utc: str | None = None
        self.run_metadata: dict = {}
        self.control_token = secrets.token_urlsafe(24)
        self.control_socket: socket.socket | None = None
        self.control_thread: threading.Thread | None = None
        self.control_requests: Queue[dict] = Queue()
        self.control_event_lock = threading.Lock()
        self.control_port: int | None = None
        self.current_map = ""
        self.client_args: list[str] = []
        self.server_ready = False
        self.client_ready = False
        self.builds_match = False
        self.status_visible = False
        self.status_line_count = 0

    def state(self, status: str = "idle") -> dict:
        client_executable = self.client_args[0] if self.client_args else None
        latest_identity = self.latest_build_identity()
        identities_match = bool(
            self.server_exe and client_executable and
            os.path.normcase(self.server_exe) == os.path.normcase(client_executable) and
            self.run_metadata.get("build_id") == (
                f"{latest_identity.get('build_number')}:{latest_identity.get('sha256')}"
                if latest_identity else ""
            )
        )
        return {
            "profile": self.profile.get("name", "unnamed"),
            "launch_mode": self.launch_mode_id,
            "running_build": self.running_build,
            "latest_successful_build": self.latest_build,
            "source_generation": self.generation,
            "latest_successful_source_generation": self.latest_generation,
            "latest_build_stale": self.latest_stale,
            "auto_restart": self.auto_restart,
            "build_status": status,
            "message": self.last_message,
            "updated_at": utc_now(),
            "run_id": self.run_id,
            "run_started_utc": self.run_started_utc,
            "run_metadata": dict(self.run_metadata),
            "events_path": self.events_path,
            "control_port": self.control_port,
            "control_token": self.control_token if self.control_port else None,
            "server_pid": self.server_pid,
            "server_executable": self.server_exe,
            "server_args": list(self.server_args),
            "server_alive": self.server_health(),
            "server_ready": self.server_ready,
            "client_pids": [p.pid for p in self.processes if p.poll() is None],
            "client_executables": [client_executable] if self.processes and client_executable else [],
            "client_args": list(self.client_args),
            "client_ready": self.client_ready,
            "builds_match": bool(self.builds_match and identities_match),
            "map": self.current_map,
            "build_identity": self.latest_build_identity(),
            "launch_mode_name": self.launch_mode.get("name", "unnamed"),
            "launch_mode_config": str(LAUNCH_MODE_PATH),
        }

    def save_state(self, status: str = "idle") -> None:
        snapshot = self.state(status)
        write_json(STATE_PATH, snapshot)
        write_json(CURRENT_RUN_PATH, snapshot)

    def record_control_event(self, request: dict, status: str, message: str) -> None:
        """Append control-plane evidence to the active canonical journal."""
        if not self.events_path:
            return
        record = {
            "category": "GENERAL",
            "event": "dev-loop.control",
            "event_id": f"DEV_CONTROL_{request.get('request_id', uuid.uuid4().hex)}",
            "fields": {
                "action": request.get("action", ""),
                "launch_mode": request.get("launch_mode"),
                "request_id": request.get("request_id", ""),
                "requested_by": request.get("requested_by", "ai"),
                "status": status,
                "message": message,
            },
            "level": "IMPORTANT",
            "pid": os.getpid(),
            "process": "dev-loop",
            "process_role": "dev-loop",
            "reason": message,
            "run_id": self.run_id or "",
            "wall_time": utc_now(),
        }
        try:
            with self.control_event_lock:
                with open(self.events_path, "a", encoding="utf-8", newline="\n") as journal:
                    journal.write(json.dumps(record, separators=(",", ":")) + "\n")
        except OSError as error:
            print(f"[DEV CONTROL] unable to record journal event: {error}")

    def latest_executable(self) -> Path:
        if self.latest_build is None:
            return BUILD_ROOT / "(none)" / "mimita.exe"
        return BUILD_ROOT / f"{self.latest_build:04d}" / "mimita.exe"

    def latest_build_identity(self) -> dict | None:
        if self.latest_build is None:
            return None
        exe = self.latest_executable()
        info = executable_identity(exe)
        info["build_number"] = self.latest_build
        info["source_generation"] = self.latest_generation
        info["source_current"] = not self.latest_stale
        return info

    def start_control_server(self) -> None:
        if self.control_socket is not None:
            return
        server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        server.bind(("127.0.0.1", 0))
        server.listen(8)
        server.settimeout(0.5)
        self.control_socket = server
        self.control_port = int(server.getsockname()[1])
        self.control_thread = threading.Thread(
            target=self.control_loop, name="mimita-dev-control", daemon=True
        )
        self.control_thread.start()

    def stop_control_server(self) -> None:
        server = self.control_socket
        self.control_socket = None
        self.control_port = None
        if server is not None:
            try:
                server.close()
            except OSError:
                pass

    def control_loop(self) -> None:
        while not self.stop_event.is_set():
            server = self.control_socket
            if server is None:
                return
            try:
                connection, _ = server.accept()
            except socket.timeout:
                continue
            except OSError:
                return
            try:
                connection.settimeout(3.0)
                raw = connection.recv(65536)
                request = json.loads(raw.decode("utf-8"))
                if request.get("token") != self.control_token:
                    response = {"accepted": False, "error": "invalid control token"}
                elif request.get("action") == "status":
                    response = {"accepted": True, "state": self.state("status")}
                else:
                    request["requested_by"] = "ai"
                    self.control_requests.put(request)
                    response = {
                        "accepted": True,
                        "queued": True,
                        "request_id": request.get("request_id", ""),
                        "state": self.state("request_queued"),
                    }
                connection.sendall((json.dumps(response) + "\n").encode("utf-8"))
            except (OSError, ValueError, json.JSONDecodeError) as error:
                try:
                    connection.sendall((json.dumps({"accepted": False, "error": str(error)}) + "\n").encode("utf-8"))
                except OSError:
                    pass
            finally:
                try:
                    connection.close()
                except OSError:
                    pass

    def process_control_requests(self) -> None:
        while True:
            try:
                request = self.control_requests.get_nowait()
            except Empty:
                return
            action = str(request.get("action", "")).strip().lower()
            request_id = str(request.get("request_id", ""))
            print(f"[DEV CONTROL] request={request_id or '(none)'} action={action}")
            if action == "launch":
                requested_mode = request.get("launch_mode")
                if requested_mode is not None:
                    try:
                        selected = load_launch_mode(str(requested_mode))
                    except SystemExit as error:
                        self.last_message = str(error)
                        self.record_control_event(request, "rejected", self.last_message)
                        self.save_state("request_rejected")
                        continue
                    self.launch_mode_id = str(requested_mode)
                    self.launch_mode = selected
                self.manual_launch_requested = True
                self.build_pending = True
                self.change_event.clear()
                self.last_message = f"AI launch queued for mode {self.launch_mode_id}"
                self.record_control_event(request, "accepted", self.last_message)
                self.save_state("build_queued")
            elif action == "stop-client":
                self.stop_processes()
                self.record_control_event(request, "completed", "client stopped")
                self.save_state("client_stopped")
            elif action == "stop-server":
                self.stop_server()
                self.record_control_event(request, "completed", "server stop requested")
                self.save_state("server_stopped")
            elif action == "stop":
                self.stop_event.set()
                self.last_message = "stop requested by AI"
                self.record_control_event(request, "accepted", self.last_message)
                self.save_state("stop_requested")
            else:
                self.last_message = f"unknown AI action: {action}"
                self.record_control_event(request, "rejected", self.last_message)
                self.save_state("request_rejected")

    def watch(self) -> None:
        while not self.stop_event.wait(POLL_SECONDS):
            current = snapshot_inputs()
            if current != self.last_snapshot:
                self.last_snapshot = current
                with self.lock:
                    self.generation += 1
                    self.latest_stale = self.latest_build is not None
                    self.last_message = f"source changed, generation {self.generation}"
                self.change_event.set()
                self.save_state("source_changed")

    def run_build(self) -> bool:
        with self.lock:
            requested_generation = self.generation
            self.running_build = requested_generation
            self.last_message = "building"
        self.save_state("building")

        command = [sys.executable, str(ROOT / "build.py"), "build-only"]
        environment = os.environ.copy()
        ccache = find_ccache()
        if ccache:
            environment["MIMITA_CCACHE"] = ccache
            print(f"[DEV] ccache: {ccache}")
        else:
            print("[DEV] ccache: not found; build will still work without it")

        print("[DEV] starting incremental build")
        output = []
        process = subprocess.Popen(
            command,
            cwd=ROOT,
            env=environment,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1,
        )
        assert process.stdout is not None
        for line in process.stdout:
            print(line, end="")
            output.append(line)
        result = process.wait()
        successful = result == 0 and (
            "BUILD SUCCESS" in "".join(output)
            or "Nothing changed." in "".join(output)
        )

        with self.lock:
            self.running_build = None
            current_generation = self.generation

        if successful:
            number = next_build_number()
            try:
                destination = publish_build(number)
            except Exception as error:
                self.last_message = f"publish failed: {error}"
                self.save_state("publish_failed")
                print(f"[DEV] publish failed: {error}")
                return False
            with self.lock:
                self.latest_build = number
                self.latest_generation = requested_generation
                self.latest_stale = current_generation != requested_generation
                self.last_message = f"published build {number} at {destination}"
                self.build_pending = self.latest_stale
            print(f"[DEV] published build {number}: {destination}")
            self.save_state("success")
            # File-watcher builds are background compilation/publish only.
            # Launching a new client is an explicit [1] action, so editing a
            # C++ file never interrupts the currently running game.
            should_launch = self.manual_launch_requested
            if not self.latest_stale and should_launch:
                self.manual_launch_requested = False
                self.restart_latest()
            else:
                self.print_status()
            return True

        with self.lock:
            self.last_message = f"build failed with exit code {result}"
            # The previous artifact is intentionally retained for inspection,
            # but it must never be presented as current or launched by [1] or
            # automatic process recovery.
            self.latest_stale = True
            self.build_pending = False
        self.save_state("failed")
        print(f"[DEV] build failed with exit code {result}; current game was left running")
        print("[DEV] compiler/build diagnostics (full build output was streamed above):")
        diagnostics = [
            line.rstrip()
            for line in output
            if any(marker in line.lower() for marker in (
                "error:", "fatal error", "undefined reference", "collect2:",
                "linker command failed", "build failed",
            ))
        ]
        if diagnostics:
            for line in diagnostics:
                print(f"[DEV][BUILD ERROR] {line}")
        else:
            print("[DEV][BUILD ERROR] No filtered compiler diagnostic found; review the full output above.")
        return False

    def build_server_args(self, exe: Path, map_name: str,
                          room_file_path: Path) -> list[str]:
        """Dedicated-server arguments with GUI launchServerProcess semantics.

        The flag set mirrors the GUI external-server path (bind, name, map,
        mode, max players, weapon set, map rotation, password, room handshake,
        host player, NPCs, Discord, optional duel). Profile values supply the
        dev-specific settings; no listen-server path is used.
        """
        profile = self.profile
        server_bind = str(profile.get("server_bind", "0.0.0.0:1357"))
        npc_count = int(profile.get("npc_count", 1) or 0)
        max_players = max(1, int(profile.get("max_players", 999) or 999))
        rotation_minutes = max(1, min(9999, int(profile.get("map_rotation_minutes", 15) or 15)))
        password_protected = "1" if profile.get("password_protected", False) else "0"
        # Host identity falls back to the client name so host-only commands keep
        # working even when the profile omits a separate host_player_name.
        host_player = str(profile.get("host_player_name", "")).strip()
        if not host_player:
            host_player = str(profile.get("client_name", "")).strip()

        args = [
            str(exe), "--server", "--bind", server_bind,
            "--name", str(profile.get("server_name", "MiMITA Dev Server")),
            "--map", map_name,
            "--mode", str(profile.get("mode", "sandbox")),
            "--max-players", str(max_players),
            "--weapon-set", str(profile.get("weapon_set", 1)),
            "--map-rotation-minutes", str(rotation_minutes),
            "--password-protected", password_protected,
            "--password", str(profile.get("password", "")),
            "--room-file", str(room_file_path),
        ]
        if host_player:
            args.extend(["--host-player", host_player])
        if npc_count > 0:
            args.extend(["--npcs", str(npc_count)])
        else:
            args.append("--no-npcs")
        if not profile.get("auto_map_rotation", False):
            args.append("--no-map-rotation")
        if not profile.get("discord_notification", False):
            args.append("--no-discord-notification")
        if profile.get("duel", False):
            args.extend(["--duel", "--gamemode", str(profile.get("gamemode", "duel"))])
        args.extend(str(value) for value in self.launch_mode.get("server_args", []))
        return args

    def server_health(self) -> bool:
        """True when the durable external server process is still alive."""
        process = self.server_process
        return process is not None and process.poll() is None

    def server_map_name(self) -> str:
        """The --map value the running external server was launched with."""
        args = self.server_args or []
        for index, value in enumerate(args):
            if value == "--map" and index + 1 < len(args):
                return str(args[index + 1])
        return ""

    def stop_server(self) -> bool:
        """Terminate only the external dev server, leaving clients untouched.

        Used when the launch-mode map changed and the old server would otherwise
        keep serving the previous map for the whole session.
        """
        process = self.server_process
        stopped = True
        if process is not None and process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=3.0)
            except subprocess.TimeoutExpired:
                process.kill()
                try:
                    process.wait(timeout=3.0)
                except subprocess.TimeoutExpired:
                    stopped = False
        if process is not None and process.poll() is None:
            stopped = False
        self.server_process = None
        self.server_pid = None
        self.server_exe = None
        self.server_args = []
        self.server_unavailable = True
        self.server_ready = False
        self.builds_match = False
        self.events_path = None
        self.run_metadata = {}
        self.cleanup_room_file()
        if not stopped:
            self.last_message = f"refused to replace server pid={process.pid if process else '(none)'}; process remains alive"
            print(f"[DEV SERVER] {self.last_message}")
        return stopped

    def wait_for_logger_started(self, role: str, pid: int, timeout: float = 20.0) -> bool:
        """Confirm the exact child wrote logger.started to any run segment."""
        if not self.events_path:
            return False
        deadline = time.time() + timeout
        while time.time() < deadline:
            try:
                base = Path(self.events_path)
                paths = [base]
                if base.parent.is_dir():
                    paths.extend(sorted(base.parent.glob("events-*.jsonl")))
                for path in paths:
                    try:
                        with open(path, "r", encoding="utf-8") as journal:
                            for line in journal:
                                try:
                                    event = json.loads(line)
                                except json.JSONDecodeError:
                                    continue
                                if event.get("event") != "logger.started":
                                    continue
                                fields = event.get("fields", {})
                                if (fields.get("pid") == pid and
                                        fields.get("process_role", fields.get("process")) == role):
                                    return True
                    except OSError:
                        continue
            except OSError:
                pass
            time.sleep(0.1)
        return False

    def server_matches_executable(self, exe: Path) -> bool:
        if not self.server_health() or not self.server_exe:
            return False
        try:
            return Path(self.server_exe).resolve() == exe.resolve()
        except OSError:
            return os.path.normcase(self.server_exe) == os.path.normcase(str(exe))

    @staticmethod
    def _normalized_server_args(args: list[str]) -> list[str]:
        """Compare launch contracts without treating the temporary room path as state."""
        normalized = []
        index = 0
        while index < len(args):
            value = str(args[index])
            normalized.append(value)
            if value == "--room-file" and index + 1 < len(args):
                normalized.append("<room-file>")
                index += 2
                continue
            index += 1
        return normalized

    def server_matches_launch_configuration(self, exe: Path, map_name: str) -> bool:
        """Ensure a durable server belongs to the currently selected mode/profile."""
        if not self.server_matches_executable(exe):
            return False
        desired = self.build_server_args(exe, map_name, Path("<room-file>"))
        return self._normalized_server_args(self.server_args) == self._normalized_server_args(desired)

    def check_server_after_client(self) -> None:
        """Observe and repair durable-server state after a client exits.

        The server is never terminated here; only its liveness is checked and a
        stale room code/file is cleared if it died unexpectedly.
        """
        if self.server_process is None:
            return
        code = self.server_process.poll()
        if code is None:
            print(f"[DEV SERVER] health check alive=1 pid={self.server_pid} "
                  f"room={self.room_code}")
            print(f"[DEV SERVER] client exited; server retained pid={self.server_pid} "
                  f"room={self.room_code}")
            return
        print(f"[DEV SERVER] unexpected exit code={code} pid={self.server_pid} "
              f"room={self.room_code}")
        self.server_process = None
        self.server_pid = None
        self.server_exe = None
        self.server_args = []
        self.room_code = None
        self.server_unavailable = True
        self.cleanup_room_file()

    def launch_latest(self) -> None:
        if self.latest_build is None:
            return
        if self.latest_stale:
            print("[DEV] refusing to launch stale build; press [1] to retry the build")
            return
        build_dir = BUILD_ROOT / f"{self.latest_build:04d}"
        exe = build_dir / "mimita.exe"
        if not exe.is_file():
            print(f"[DEV] missing published executable: {exe}")
            return

        # Stop the previous dev-loop client. A server from an older published
        # build must never remain available to receive the new client.
        self.stop_processes()
        self.client_ready = False
        self.builds_match = False
        map_name = select_dev_map(
            self.profile,
            str(self.launch_mode.get("map", "")),
        )
        print(f"[DEV] selected allowed map: {map_name}")
        running_map = self.server_map_name()
        if self.server_health() and not self.server_matches_launch_configuration(exe, map_name):
            print(f"[DEV SERVER] launch configuration changed; replacing old server "
                  f"pid={self.server_pid} old_map={running_map} new_map={map_name}")
            if not self.stop_server():
                self.save_state("server_stop_failed")
                return
        if self.server_health() and bool(self.room_code):
            room_code = self.room_code
            print(
                f"[DEV SERVER] reusing pid={self.server_pid} room={room_code} alive=1"
            )
        else:
            # A dead server's room code must never be reused.
            if self.server_process is not None:
                code = self.server_process.poll()
                print(f"[DEV SERVER] unexpected exit code={code} pid={self.server_pid} "
                      f"room={self.room_code}")
            self.server_process = None
            self.server_pid = None
            self.server_exe = None
            self.server_args = []
            self.room_code = None
            self.server_unavailable = True
            self.cleanup_room_file()
            room_fd, room_file_name = tempfile.mkstemp(prefix="mimita-dev-room-", suffix=".txt")
            os.close(room_fd)
            self.room_file_path = Path(room_file_name)
            self.room_file_path.write_text("", encoding="utf-8")
            server_args = self.build_server_args(exe, map_name, self.room_file_path)
            # One shared canonical events.jsonl for this server and its client.
            self.events_path = create_shared_events_path()
            self.run_id = Path(self.events_path).parent.name
            self.run_started_utc = utc_now()
            self.current_map = map_name
            self.server_ready = False
            self.client_ready = False
            self.builds_match = False
            identity = self.latest_build_identity() or {}
            build_id = f"{identity.get('build_number', self.latest_build)}:{identity.get('sha256', '')}"
            metadata = {
                "run_id": self.run_id,
                "build_number": self.latest_build,
                "build_id": build_id,
                "source_generation": self.latest_generation,
                "source_current": not self.latest_stale,
                "launch_mode": self.launch_mode_id,
                "launch_mode_name": self.launch_mode.get("name", "unnamed"),
                "map": map_name,
                "events_path": self.events_path,
                "executable": str(exe),
                "started_utc": self.run_started_utc,
            }
            self.run_metadata = metadata
            common_env = {
                **os.environ,
                "MIMITA_EVENTS_FILE": self.events_path,
                "MIMITA_RUN_ID": self.run_id,
                "MIMITA_BUILD_ID": build_id,
                "MIMITA_RUN_METADATA": json.dumps(metadata, separators=(",", ":")),
            }
            server_env = {**common_env, "MIMITA_PROCESS_ROLE": "server"}
            self.save_state("launching_server")
            version_env = {**common_env, "MIMITA_PROCESS_ROLE": "preflight"}
            try:
                version = subprocess.run(
                    [str(exe), "--versioninfo"], cwd=ROOT, env=version_env,
                    capture_output=True, text=True, timeout=20, check=False,
                )
                print(f"[DEV] versioninfo exit={version.returncode}")
                if version.stdout:
                    print(version.stdout, end="")
                if version.stderr:
                    print(version.stderr, end="", file=sys.stderr)
            except (OSError, subprocess.TimeoutExpired) as error:
                self.last_message = f"versioninfo failed: {error}"
                self.save_state("versioninfo_failed")
                print(f"[DEV] {self.last_message}")
                return
            print(f"[DEV] launching build {self.latest_build} server")
            print(f"[DEV] shared events file: {self.events_path}")
            server = subprocess.Popen(
                server_args,
                cwd=ROOT,
                env=server_env,
                creationflags=getattr(subprocess, "CREATE_NEW_CONSOLE", 0),
            )
            self.server_process = server
            self.server_pid = server.pid
            self.server_exe = str(exe)
            self.server_args = list(server_args)
            self.server_launch_ms = time.time() * 1000.0
            self.server_unavailable = False
            self.server_ready = self.wait_for_logger_started("server", server.pid)
            if not self.server_ready:
                self.last_message = f"server pid={server.pid} did not write logger.started"
                self.save_state("server_logger_missing")
                if not self.stop_server():
                    print(f"[DEV SERVER] {self.last_message}; server could not be stopped")
                return
            print(f"[DEV SERVER] launched pid={server.pid} args={' '.join(server_args[1:])}")

            # The dedicated server registers with the coordinator, then writes
            # the room code. This is the same handshake the GUI uses before it
            # starts the client join.
            deadline = time.time() + 20.0
            room_code = ""
            while time.time() < deadline:
                if server.poll() is not None:
                    self.last_message = (
                        f"server exited before room-code handshake: {server.returncode}"
                    )
                    self.save_state("server_failed")
                    print(f"[DEV SERVER] unexpected exit code={server.returncode} "
                          f"pid={server.pid} before room handshake")
                    self.server_process = None
                    self.server_pid = None
                    self.cleanup_room_file()
                    self.print_status()
                    return
                try:
                    room_code = self.room_file_path.read_text(encoding="utf-8").strip()
                except OSError:
                    room_code = ""
                if room_code:
                    break
                time.sleep(0.1)

            if not room_code:
                self.last_message = "server did not publish a room code within 20 seconds"
                self.save_state("server_failed")
                print(f"[DEV] {self.last_message}")
                self.server_process = None
                self.server_pid = None
                self.cleanup_room_file()
                self.print_status()
                return

            self.room_code = room_code
            # The server has consumed the room-file handshake. Remove the
            # temporary file like the GUI does, but retain room_code for reuse.
            self.cleanup_room_file(clear_code=False)
            self.server_ready = True
            print(f"[DEV SERVER] room={room_code} alive=1")
            print(f"[DEV] ROOM CODE: {room_code}")

        client_args = [
            str(exe), "--room", room_code,
            "--map", map_name,
            "--name", str(self.profile.get("client_name", "NPC Dev")),
        ]
        client_args.extend(str(value) for value in self.launch_mode.get("client_args", []))
        if not self.events_path:
            self.events_path = create_shared_events_path()
        # The client may run generic leave/queue cleanup that is allowed to
        # stop a server it launched through the GUI. This server belongs to
        # the dev-loop instead, so the client must never claim ownership of it.
        client_env = {
            **os.environ,
            "MIMITA_EVENTS_FILE": self.events_path,
            "MIMITA_DEV_LOOP_SERVER": "1",
            "MIMITA_RUN_ID": self.run_id or Path(self.events_path).parent.name,
            "MIMITA_EXPECTED_RUN_ID": self.run_id or Path(self.events_path).parent.name,
            "MIMITA_EXPECTED_BUILD_ID": f"{(self.latest_build_identity() or {}).get('build_number', self.latest_build)}:{(self.latest_build_identity() or {}).get('sha256', '')}",
            "MIMITA_BUILD_ID": f"{(self.latest_build_identity() or {}).get('build_number', self.latest_build)}:{(self.latest_build_identity() or {}).get('sha256', '')}",
            "MIMITA_RUN_METADATA": json.dumps(self.run_metadata, separators=(",", ":")),
            "MIMITA_PROCESS_ROLE": "client",
        }
        print(f"[DEV] launching build {self.latest_build} client")
        print(f"[DEV] shared events file: {self.events_path}")
        client = subprocess.Popen(
            client_args,
            cwd=ROOT,
            env=client_env,
            creationflags=getattr(subprocess, "CREATE_NEW_CONSOLE", 0),
        )
        self.processes.append(client)
        self.client_args = list(client_args)
        self.running_build = self.latest_build
        self.client_ready = self.wait_for_logger_started("client", client.pid)
        self.builds_match = bool(self.server_ready and self.client_ready and
                                 self.server_exe == str(exe) and
                                 self.latest_build_identity() and
                                 self.latest_build_identity().get("valid"))
        self.save_state("running" if self.client_ready else "client_logger_missing")
        self.print_status()

    def restart_latest(self) -> None:
        self.launch_latest()

    def stop_processes(self) -> None:
        for process in self.processes:
            if process.poll() is None:
                process.terminate()
        deadline = time.time() + 3.0
        for process in self.processes:
            remaining = max(0.0, deadline - time.time())
            try:
                process.wait(timeout=remaining)
            except subprocess.TimeoutExpired:
                process.kill()
        self.processes = []
        self.running_build = None

    def maintain_process(self) -> None:
        had_clients = bool(self.processes)
        live = [process for process in self.processes if process.poll() is None]
        self.processes = live
        # The external server has an independent lifetime: after the client
        # exits, keep it and verify it is still healthy.
        if had_clients and not live:
            self.check_server_after_client()
        # Auto-start OFF means a dead/absent child stays absent until [1] is
        # pressed. Auto-start ON preserves automatic recovery of a good build.
        if not self.auto_restart or live or self.latest_build is None:
            return
        if self.latest_stale:
            # Never recover a dead process by relaunching an artifact that is
            # known not to match the current source. Wait for [1] to queue a
            # retry, which will print the compiler diagnostics on failure.
            if not self.build_pending:
                message = "latest build is stale; press [1] to retry the build"
                if self.last_message != message:
                    self.last_message = message
                    self.save_state("stale_waiting")
                    self.print_status(force=True)
            return
        message = "no MiMITA process running; press [1] to launch one server/client pair"
        if self.last_message != message:
            self.last_message = message
            self.save_state("stopped_waiting")
            self.print_status(force=True)

    def cleanup_room_file(self, clear_code: bool = True) -> None:
        if self.room_file_path is not None:
            try:
                self.room_file_path.unlink()
            except FileNotFoundError:
                pass
            except OSError as error:
                print(f"[DEV] could not remove room file: {error}")
            self.room_file_path = None
        if clear_code:
            self.room_code = None

    def _status_lines(self) -> list[str]:
        lines = [
            "",
            f"LAUNCH MODE: {self.launch_mode_id} ({self.launch_mode.get('name', 'unnamed')})",
            f"RUNNING: {self.running_build or '(none)'}",
            f"LATEST:  {self.latest_build or '(none)'}",
            f"AUTO-START: {'ON' if self.auto_restart else 'OFF'}",
        ]
        if self.server_process is not None:
            lines.append(
                f"SERVER: pid={self.server_pid} room={self.room_code or '(none)'} "
                f"{'alive' if self.server_health() else 'exited'}"
            )
        if self.latest_stale:
            lines.append("LATEST BUILD IS STALE")
        lines.append("[1] Build/retry or launch  [2] Choose launch mode  [A] Auto-start  [Q] Quit")
        return lines

    def show_launch_modes(self) -> None:
        try:
            config = read_json(LAUNCH_MODE_PATH)
            modes = config.get("modes", {}) if isinstance(config, dict) else {}
        except (OSError, json.JSONDecodeError) as error:
            print(f"[DEV] could not read launch modes: {error}")
            return
        if not isinstance(modes, dict) or not modes:
            print("[DEV] no launch modes are configured")
            return
        self.mode_picker_open = True
        self.mode_picker_input = ""
        print("[DEV] Choose a launch mode. Type the full number and press Enter:")
        for mode_id in sorted(
            modes,
            key=lambda value: (
                not str(value).isdigit(),
                int(str(value)) if str(value).isdigit() else str(value),
            ),
        ):
            mode = modes[mode_id]
            name = mode.get("name", "unnamed") if isinstance(mode, dict) else "unnamed"
            selected = " (selected)" if str(mode_id) == self.launch_mode_id else ""
            print(f"[DEV]   {mode_id} = {name}{selected}")
        print("[DEV] Mode selection changes the next server/client launch.")
        print("[DEV] Enter mode number (Q cancels): ", end="", flush=True)

    def print_status(self, force: bool = False) -> None:
        status_line = (
            self.running_build,
            self.latest_build,
            self.latest_stale,
        )
        if not force and status_line == self.last_status_line:
            return
        self.last_status_line = status_line
        self._render_status()

    def _render_status(self) -> None:
        global _status_rendering
        if self.status_visible and _rainbow_enabled():
            sys.stdout.write(f"\x1b[{self.status_line_count}A\x1b[0J")
        lines = self._status_lines()
        _status_rendering = True
        try:
            for line in lines:
                dev_print(line)
        finally:
            _status_rendering = False
        self.status_line_count = len(lines)
        self.status_visible = True
        sys.stdout.flush()

    def _invalidate_status(self) -> None:
        if not self.status_visible:
            return
        if _rainbow_enabled():
            sys.stdout.write(f"\x1b[{self.status_line_count}A\x1b[0J")
            sys.stdout.flush()
        self.status_visible = False
        self.status_line_count = 0

    def refresh_status_animation(self) -> None:
        if self.status_visible and _rainbow_enabled():
            self._render_status()

    def key_commands(self) -> None:
        if os.name != "nt":
            return
        import msvcrt
        if not msvcrt.kbhit():
            return
        key = msvcrt.getwch()
        if self.mode_picker_open:
            lowered = key.lower()
            if lowered == "q":
                self.mode_picker_open = False
                self.mode_picker_input = ""
                print("\n[DEV] launch mode selection cancelled")
                return
            if key in ("\r", "\n"):
                selected = None
                if self.mode_picker_input:
                    try:
                        selected = load_launch_mode(self.mode_picker_input)
                    except SystemExit:
                        selected = None
                if selected is not None:
                    selected_id = self.mode_picker_input
                    self.launch_mode_id = selected_id
                    self.launch_mode = selected
                    self.mode_picker_input = ""
                    self.mode_picker_open = False
                    self.last_message = (
                        f"launch mode {selected_id} selected: {selected.get('name', 'unnamed')}"
                    )
                    self.save_state("launch_mode_selected")
                    print(f"\n[DEV] selected launch mode {selected_id}: {selected.get('name', 'unnamed')}")
                    self.print_status(force=True)
                else:
                    print(f"\n[DEV] unknown launch mode {self.mode_picker_input}; type a listed number and press Enter")
                    self.mode_picker_input = ""
                    print("[DEV] Enter mode number (Q cancels): ", end="", flush=True)
                return
            if key == "\b":
                self.mode_picker_input = self.mode_picker_input[:-1]
                print(
                    f"\r[DEV] Enter mode number (Q cancels): {self.mode_picker_input}",
                    end="",
                    flush=True,
                )
                return
            if key.isdigit():
                self.mode_picker_input += key
                print(
                    f"\r[DEV] Enter mode number (Q cancels): {self.mode_picker_input}",
                    end="",
                    flush=True,
                )
                return
            return
        key = key.lower()
        if key == "2":
            self.show_launch_modes()
            return
        if key == "1":
            if self.latest_stale or self.latest_build is None:
                self.manual_launch_requested = True
                self.build_pending = True
                self.change_event.clear()
                self.last_message = "manual build retry queued"
                self.save_state("build_queued")
                print("[DEV] [1] queued a build retry; stale builds will not be launched")
            else:
                self.launch_latest()
            self.print_status()
        elif key == "a":
            self.auto_restart = not self.auto_restart
            self.last_message = (
                "auto-start enabled" if self.auto_restart else "auto-start disabled"
            )
            self.save_state("auto_start_toggled")
            print(f"[DEV] auto-start {'ON' if self.auto_restart else 'OFF'}")
            self.print_status(force=True)
        elif key == "q":
            self.stop_event.set()

    def run(self) -> None:
        global _status_clear_hook
        _status_clear_hook = self._invalidate_status
        watcher = threading.Thread(target=self.watch, daemon=True)
        watcher.start()
        self.start_control_server()
        self.save_state("starting")
        print(f"[DEV] profile={self.profile.get('name', 'unnamed')}")
        print(f"[DEV] launch mode={self.launch_mode_id} ({self.launch_mode.get('name', 'unnamed')})")
        print(f"[DEV] root={ROOT}")
        print(f"[DEV] control=127.0.0.1:{self.control_port}")
        print(f"[DEV] status={CURRENT_RUN_PATH}")
        print("[DEV] one build at a time; edits during a build queue another build")

        try:
            while not self.stop_event.is_set():
                self.key_commands()
                self.process_control_requests()
                self.maintain_process()
                if self.build_pending:
                    self.change_event.clear()
                    time.sleep(DEBOUNCE_SECONDS)
                    self.run_build()
                    continue
                if self.change_event.wait(0.20):
                    self.build_pending = True
                self.refresh_status_animation()
        finally:
            self.stop_event.set()
            # Only daemon-owned clients are terminated. The external server is
            # never killed here; it keeps its own console/lifetime.
            self.stop_processes()
            _status_clear_hook = None
            self.cleanup_room_file(clear_code=False)
            self.stop_control_server()
            if self.server_health():
                print(f"[DEV SERVER] shutdown requested; server left running "
                      f"pid={self.server_pid} room={self.room_code}")
            self.save_state("stopped")
            print("[DEV] stopped; client closed; persistent server left running")


def read_current_run() -> dict | None:
    try:
        value = read_json(CURRENT_RUN_PATH)
    except (OSError, json.JSONDecodeError):
        return None
    return value if isinstance(value, dict) else None


def run_control_client(args: argparse.Namespace) -> int:
    current = read_current_run()
    if current is None:
        print(f"[DEV CONTROL] no current run state: {CURRENT_RUN_PATH}", file=sys.stderr)
        return 2

    if args.status:
        if args.json:
            print(json.dumps(current, indent=2))
        else:
            print(f"[DEV STATUS] status={current.get('build_status')} run={current.get('run_id') or '(none)'}")
            print(f"[DEV STATUS] build={current.get('latest_successful_build')} stale={current.get('latest_build_stale')}")
            print(f"[DEV STATUS] events={current.get('events_path') or '(none)'}")
            print(f"[DEV STATUS] server pid={current.get('server_pid')} alive={current.get('server_alive')}")
            print(f"[DEV STATUS] client pids={current.get('client_pids', [])}")
        if not args.request:
            return 0

    port = current.get("control_port")
    token = current.get("control_token")
    if not port or not token:
        print("[DEV CONTROL] no running daemon control endpoint; start the dev loop once", file=sys.stderr)
        return 2

    request = {
        "token": token,
        "action": args.request,
        "launch_mode": args.launch_mode,
        "request_id": f"{utc_now()}-{uuid.uuid4().hex[:8]}",
    }
    try:
        with socket.create_connection(("127.0.0.1", int(port)), timeout=5.0) as connection:
            connection.sendall((json.dumps(request) + "\n").encode("utf-8"))
            response = json.loads(connection.recv(65536).decode("utf-8"))
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"[DEV CONTROL] unable to contact daemon: {error}", file=sys.stderr)
        return 2

    if args.json:
        print(json.dumps(response, indent=2))
    else:
        print(f"[DEV CONTROL] accepted={response.get('accepted')} queued={response.get('queued', False)} ")
        if response.get("error"):
            print(f"[DEV CONTROL] error={response['error']}", file=sys.stderr)
    return 0 if response.get("accepted") else 1


def main() -> int:
    parser = argparse.ArgumentParser(description="MiMITA incremental development loop")
    parser.add_argument("--profile", default="npc-navigation")
    parser.add_argument("--auto-restart", action="store_true")
    parser.add_argument(
        "--launch-mode",
        help="numeric dev launch mode from devscripts/dev-launch-modes.json",
    )
    parser.add_argument("--status", action="store_true", help="read the current dev-loop status and exit")
    parser.add_argument("--json", action="store_true", help="print control/status responses as JSON")
    parser.add_argument(
        "--request",
        choices=("launch", "stop-client", "stop-server", "stop"),
        help="send a command to the already-running dev-loop daemon and exit",
    )
    args = parser.parse_args()

    if args.status or args.request:
        return run_control_client(args)

    owner_pid = acquire_dev_loop_lock()
    profile = load_profile(args.profile)
    try:
        DevLoop(profile, args.auto_restart, args.launch_mode).run()
    finally:
        release_dev_loop_lock(owner_pid)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
