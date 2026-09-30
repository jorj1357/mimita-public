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
import json
import os
import random
import shutil
import subprocess
import sys
import threading
import time
import tempfile
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


def select_dev_map(profile: dict, requested_map: str = "") -> str:
    maps = allowed_dev_maps(profile)
    if requested_map:
        requested_map = requested_map.strip()
        if requested_map in maps:
            return requested_map
        print(f"[DEV] launch-mode map {requested_map} is not allowed; using {maps[0]}")
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
    """Return the newest numbered artifact that contains a usable EXE."""
    for path in published_builds():
        exe = path / "mimita.exe"
        try:
            if exe.is_file() and exe.stat().st_size > 0:
                return int(path.name)
        except OSError:
            continue
    return None


def publish_build(number: int) -> Path:
    source_exe = ROOT / "mimita.exe"
    if not source_exe.is_file():
        raise RuntimeError("build succeeded but mimita.exe was not produced")

    destination = BUILD_ROOT / f"{number:04d}"
    destination.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source_exe, destination / "mimita.exe")

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
        self.status_visible = False
        self.status_line_count = 0

    def state(self, status: str = "idle") -> dict:
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
            "updated_at": time.strftime("%Y-%m-%dT%H:%M:%S%z"),
        }

    def save_state(self, status: str = "idle") -> None:
        write_json(STATE_PATH, self.state(status))

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
            # A background build only launches when auto-start is ON. A [1]
            # request is an explicit one-shot override and still launches
            # even when auto-start is OFF.
            should_launch = self.auto_restart or self.manual_launch_requested
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

        # Stop only the previous dev-loop client. A GUI-style server remains
        # open until its own console is closed or an explicit server-stop
        # action terminates it.
        self.stop_processes()
        map_name = select_dev_map(
            self.profile,
            str(self.launch_mode.get("map", "")),
        )
        print(f"[DEV] selected allowed map: {map_name}")
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
            print(f"[DEV] launching build {self.latest_build} server")
            server = subprocess.Popen(
                server_args,
                cwd=ROOT,
                creationflags=getattr(subprocess, "CREATE_NEW_CONSOLE", 0),
            )
            self.server_process = server
            self.server_pid = server.pid
            self.server_exe = str(exe)
            self.server_args = list(server_args)
            self.server_launch_ms = time.time() * 1000.0
            self.server_unavailable = False
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
            print(f"[DEV SERVER] room={room_code} alive=1")
            print(f"[DEV] ROOM CODE: {room_code}")

        client_args = [
            str(exe), "--room", room_code,
            "--map", map_name,
            "--name", str(self.profile.get("client_name", "NPC Dev")),
        ]
        client_args.extend(str(value) for value in self.launch_mode.get("client_args", []))
        print(f"[DEV] launching build {self.latest_build} client")
        client = subprocess.Popen(
            client_args,
            cwd=ROOT,
            creationflags=getattr(subprocess, "CREATE_NEW_CONSOLE", 0),
        )
        self.processes.append(client)
        self.running_build = self.latest_build
        self.save_state("running")
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
        print("[DEV] Choose a launch mode. Press its number:")
        for mode_id in sorted(modes, key=lambda value: (not str(value).isdigit(), str(value))):
            mode = modes[mode_id]
            name = mode.get("name", "unnamed") if isinstance(mode, dict) else "unnamed"
            selected = " (selected)" if str(mode_id) == self.launch_mode_id else ""
            print(f"[DEV]   {mode_id} = {name}{selected}")
        print("[DEV] Mode selection changes the next server/client launch.")

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
        key = msvcrt.getwch().lower()
        if self.mode_picker_open:
            try:
                selected = load_launch_mode(key)
            except SystemExit:
                selected = None
            if selected is not None:
                self.launch_mode_id = key
                self.launch_mode = selected
                self.mode_picker_open = False
                self.last_message = (
                    f"launch mode {key} selected: {selected.get('name', 'unnamed')}"
                )
                self.save_state("launch_mode_selected")
                print(f"[DEV] selected launch mode {key}: {selected.get('name', 'unnamed')}")
                self.print_status(force=True)
            elif key == "q":
                self.mode_picker_open = False
                print("[DEV] launch mode selection cancelled")
            return
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
        self.save_state("starting")
        print(f"[DEV] profile={self.profile.get('name', 'unnamed')}")
        print(f"[DEV] launch mode={self.launch_mode_id} ({self.launch_mode.get('name', 'unnamed')})")
        print(f"[DEV] root={ROOT}")
        print("[DEV] one build at a time; edits during a build queue another build")

        try:
            while not self.stop_event.is_set():
                self.key_commands()
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
            if self.server_health():
                print(f"[DEV SERVER] shutdown requested; server left running "
                      f"pid={self.server_pid} room={self.room_code}")
            self.save_state("stopped")
            print("[DEV] stopped; client closed; persistent server left running")


def main() -> int:
    parser = argparse.ArgumentParser(description="MiMITA incremental development loop")
    parser.add_argument("--profile", default="npc-navigation")
    parser.add_argument("--auto-restart", action="store_true")
    parser.add_argument(
        "--launch-mode",
        help="numeric dev launch mode from devscripts/dev-launch-modes.json",
    )
    args = parser.parse_args()
    owner_pid = acquire_dev_loop_lock()
    profile = load_profile(args.profile)
    try:
        DevLoop(profile, args.auto_restart, args.launch_mode).run()
    finally:
        release_dev_loop_lock(owner_pid)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
