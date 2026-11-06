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
import ctypes
import datetime
from collections import deque
import hashlib
import json
import os
import random
import secrets
import shutil
import socket
import subprocess
import sys
import tempfile
import threading
import time
import traceback
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
STABLE_LAUNCH_DIR = DEV_ROOT / "current"
STABLE_LAUNCH_EXE = STABLE_LAUNCH_DIR / "mimita.exe"
STATE_PATH = DEV_ROOT / "state.json"
CURRENT_RUN_PATH = DEV_ROOT / "current-run.json"
SERVER_MARKER_PATH = DEV_ROOT / "server-owner.json"
LOCK_PATH = DEV_ROOT / "dev-loop.lock"
# Written when a user/dashboard explicitly stops the dev-loop daemon (key Q or
# --request stop). The watchdog honors it so an explicit shutdown is not
# immediately undone; a later explicit start (dashboard open or --ensure-running)
# clears it. See dev-loop spec sections 19.4 and 22.2.
STOP_INTENT_PATH = DEV_ROOT / "dev-loop-stop-intent"
PROFILE_ROOT = ROOT / "devscripts" / "dev-profiles"
LAUNCH_MODE_PATH = ROOT / "devscripts" / "dev-launch-modes.json"
# The canonical dev-loop entry/daemon script. Its hash is the daemon source
# identity used by the always-on watcher (dev-loop-watch.py) to detect a stale
# daemon and perform a controlled replacement.
DEV_LOOP_SCRIPT = Path(__file__).resolve()
WATCH_LOCK_PATH = DEV_ROOT / "dev-loop-watch.lock"
WATCH_STATE_PATH = DEV_ROOT / "dev-loop-watch.json"
WATCH_SCRIPT = ROOT / "devscripts" / "dev-loop-watch.py"
WATCH_LOG_PATH = DEV_ROOT / "dev-loop-watch.log"
DASHBOARD_LOCK_PATH = DEV_ROOT / "dev-loop-dashboard.lock"
DASHBOARD_LOG_PATH = DEV_ROOT / "dev-loop-dashboard.log"
DAEMON_LOG_PATH = DEV_ROOT / "dev-loop-console.log"
DAEMON_READY_TIMEOUT_SECONDS = 60.0
DAEMON_READY_STATUS_TIMEOUT_SECONDS = 8.0
DASHBOARD_FRAME_SECONDS = 0.05
DASHBOARD_STATUS_REFRESH_SECONDS = 1.0
ROLE_NAMES = {
    "watchdog": "MiMITA Dev Loop Watchdog",
    "daemon": "MiMITA Dev Loop Daemon",
    "dashboard": "MiMITA Dev Loop Dashboard",
}
# Make the sibling terminal helper importable when this file is launched by path
# (for example through the desktop shortcut's `.py` association).
_SCRIPT_DIR = str(Path(__file__).resolve().parent)
if _SCRIPT_DIR not in sys.path:
    sys.path.insert(0, _SCRIPT_DIR)

from dev_loop.artifacts import (
    build_artifact_id as make_build_artifact_id,
    identity_for_folder as published_identity_for_folder,
    newest_valid_build,
    published_builds as discover_published_builds,
)
from dev_loop.build import run_build as run_build_worker
from dev_loop.launch import select_launch_artifact
from dev_loop.instances import (
    RunLayout,
    create_run_layout as runtime_create_run_layout,
    write_run_manifest,
)
from dev_loop.runtime import (
    create_shared_events_path as runtime_create_shared_events_path,
    executable_identity as runtime_executable_identity,
    executable_snapshot_is_valid as runtime_executable_snapshot_is_valid,
    invalidate_executable_identity,
    utc_now as runtime_utc_now,
)
from dev_loop.status import duration_text as format_duration
from dev_loop.state import read_json, write_json


def script_sha256(path: Path) -> str:
    try:
        digest = hashlib.sha256()
        sources = ([path, *sorted((ROOT / "devscripts" / "dev_loop").glob("*.py"),
                                  key=lambda source: source.name)]
                   if path.resolve() == DEV_LOOP_SCRIPT.resolve() else [path])
        for source in sources:
            digest.update(source.name.encode("utf-8"))
            digest.update(b"\0")
            digest.update(source.read_bytes())
            digest.update(b"\0")
        return digest.hexdigest()
    except OSError:
        return ""
DEFAULT_MAP_POOL_PATH = ROOT / "config" / "gamemode-good-maps.json"

# Directory notifications keep broad compiler-input coverage without walking
# every source tree twice a second. The bounded poll also checks the small set
# of standalone build/configuration inputs and is the non-Windows fallback.
POLL_SECONDS = 0.50
DEBOUNCE_SECONDS = 0.20
KEEP_BUILDS = 5
RAINBOW_SECONDS = 5.0

# ── Helper subprocess console hygiene (dev-loop spec section 10.1) ───────────
# The daemon runs detached with no console. A plain subprocess.run of a console
# helper (powershell, taskkill, ...) then allocates a NEW console window, so a
# status render that queries process identity can flash windows several times a
# second. Every short-lived helper must be created hidden. The game server and
# client are launched separately with their own console policy (section 10).
_CREATE_NO_WINDOW = getattr(subprocess, "CREATE_NO_WINDOW", 0)


def _hidden_startupinfo():
    if os.name != "nt":
        return None
    info = subprocess.STARTUPINFO()
    info.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    info.wShowWindow = 0  # SW_HIDE
    return info


def run_hidden(args, **kwargs):
    """subprocess.run with no visible console window for helper processes."""
    kwargs.setdefault("creationflags", _CREATE_NO_WINDOW)
    if os.name == "nt":
        kwargs.setdefault("startupinfo", _hidden_startupinfo())
        kwargs.setdefault("close_fds", True)
    return subprocess.run(args, **kwargs)


# Short-TTL cache so bursty status renders do not spawn a helper per render
# (dev-loop spec section 5.1 process-identity polling cost). Negative results
# are cached too, so a query for a dead PID is not repeated in the window.
_PROCESS_IDENTITY_CACHE: dict[int, tuple[float, dict | None]] = {}
_PROCESS_IDENTITY_TTL_SECONDS = 1.5
RUNTIME_DLLS = (
    "libgcc_s_seh-1.dll",
    "libstdc++-6.dll",
    "libwinpthread-1.dll",
    "glfw3.dll",
)

WATCH_EXTENSIONS = {
    ".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx",
    ".inl", ".ipp", ".tcc", ".inc", ".rc", ".a", ".lib",
}
WATCH_DIRECTORIES = (
    ROOT / "src",
    ROOT / "include",
    ROOT / "external" / "libjuice",
    ROOT / "external" / "recastnavigation",
    ROOT / "external" / "manifold-prebuilt",
)
WATCH_FILES = (
    ROOT / "mimita.rc",
    ROOT / "CMakeLists.txt",
    ROOT / "build.py",
    ROOT / "build_game_dll.py",
    ROOT / "build_lock.py",
    ROOT / "build_toolchain.py",
    ROOT / "tools" / "build_manifold.py",
    DEFAULT_MAP_POOL_PATH,
    LAUNCH_MODE_PATH,
)


def _rainbow_enabled() -> bool:
    forced = os.environ.get("MIMITA_DASHBOARD_ANSI", "").strip().lower()
    if forced in ("1", "true", "yes"):
        return True
    if forced in ("0", "false", "no"):
        return False
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


def _windows_launch_snapshot(process_ids: list[int] | None = None) -> dict:
    """Return bounded foreground/window evidence for the active launch.

    This is intentionally a read-only observation. It records windows owned by
    the dev-loop children plus the current foreground/cursor windows, and only
    includes other visible windows when their title looks like a Windows
    Firewall consent dialog. It does not accept or dismiss security prompts.
    """
    if os.name != "nt":
        return {
            "platform": os.name,
            "foreground": None,
            "cursor_window": None,
            "windows": [],
            "firewall_windows": [],
        }

    import ctypes
    from ctypes import wintypes

    user32 = ctypes.windll.user32
    tracked_pids = {int(pid) for pid in (process_ids or []) if pid}
    foreground_hwnd = int(user32.GetForegroundWindow() or 0)
    point = wintypes.POINT()
    user32.GetCursorPos(ctypes.byref(point))
    cursor_hwnd = int(user32.WindowFromPoint(point) or 0)
    firewall_terms = (
        "windows security alert",
        "windows defender firewall",
        "windows firewall",
        "allow public and private",
        "allow access",
    )

    def title_for(hwnd: int) -> str:
        length = int(user32.GetWindowTextLengthW(hwnd))
        if length <= 0:
            return ""
        buffer = ctypes.create_unicode_buffer(min(length + 1, 512))
        user32.GetWindowTextW(hwnd, buffer, len(buffer))
        return buffer.value[:200]

    def pid_for(hwnd: int) -> int:
        pid = wintypes.DWORD()
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(pid))
        return int(pid.value)

    def record(hwnd: int) -> dict:
        title = title_for(hwnd)
        return {
            "hwnd": hwnd,
            "pid": pid_for(hwnd),
            "title": title,
            "visible": bool(user32.IsWindowVisible(hwnd)),
            "foreground": hwnd == foreground_hwnd,
            "under_cursor": hwnd == cursor_hwnd,
        }

    records: list[dict] = []

    @ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    def enum_window(hwnd, _lparam):
        hwnd_value = int(hwnd)
        title = title_for(hwnd_value)
        if not title:
            return True
        pid = pid_for(hwnd_value)
        title_lower = title.lower()
        is_firewall = any(term in title_lower for term in firewall_terms)
        tracked_or_active = pid in tracked_pids or hwnd_value in (foreground_hwnd, cursor_hwnd)
        visible_or_foreground = bool(user32.IsWindowVisible(hwnd)) or hwnd_value == foreground_hwnd
        if (tracked_or_active and visible_or_foreground) or is_firewall:
            records.append(record(hwnd_value))
        return True

    user32.EnumWindows(enum_window, 0)
    by_hwnd = {item["hwnd"]: item for item in records}

    def selected(hwnd: int) -> dict | None:
        if not hwnd:
            return None
        return by_hwnd.get(hwnd) or record(hwnd)

    firewall_windows = [
        item for item in records
        if any(term in item["title"].lower() for term in firewall_terms)
    ]
    return {
        "platform": "windows",
        "foreground": selected(foreground_hwnd),
        "cursor_window": selected(cursor_hwnd),
        "cursor_position": {"x": int(point.x), "y": int(point.y)},
        "windows": sorted(records, key=lambda item: item["hwnd"]),
        "firewall_windows": firewall_windows,
    }


def _terminal_file_link(path: str | None) -> str:
    """Render a terminal hyperlink when the host terminal supports OSC 8."""
    if not path:
        return "(none)"
    absolute = Path(path).resolve()
    target = absolute.as_uri()
    return f"\x1b]8;;{target}\x1b\\{absolute}\x1b]8;;\x1b\\"


def _set_process_windows_visible(pid: int, visible: bool) -> list[dict]:
    """Hide or show top-level windows owned by one launched child process."""
    if os.name != "nt" or not pid:
        return []
    import ctypes
    from ctypes import wintypes

    user32 = ctypes.windll.user32
    windows: list[dict] = []
    show_command = 8 if visible else 0  # SW_SHOWNA / SW_HIDE; never activate

    @ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    def enum_window(hwnd, _lparam):
        hwnd_value = int(hwnd)
        owner = wintypes.DWORD()
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(owner))
        if int(owner.value) != int(pid):
            return True
        if user32.IsWindowVisible(hwnd) or not visible:
            user32.ShowWindow(hwnd, show_command)
            windows.append({
                "hwnd": hwnd_value,
                "pid": int(pid),
                "visible_requested": bool(visible),
                "visible_after": bool(user32.IsWindowVisible(hwnd)),
            })
        return True

    user32.EnumWindows(enum_window, 0)
    return windows


# All development-loop and build-child output uses the same five-second
# moving rainbow when attached to a real terminal.
print = dev_print


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
    return runtime_create_shared_events_path()


def utc_now() -> str:
    return runtime_utc_now()


def executable_identity(path: Path) -> dict:
    return runtime_executable_identity(path)


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
    result.update(snapshot_watch_files())
    result.update(snapshot_watch_directories())
    return result


def snapshot_watch_files() -> dict[str, tuple[int, int] | None]:
    return {str(path): file_signature(path) for path in WATCH_FILES}


def snapshot_watch_directories() -> dict[str, tuple[int, int] | None]:
    result = {}
    for directory in WATCH_DIRECTORIES:
        if not directory.is_dir():
            continue
        for path in directory.rglob("*"):
            if path.is_file() and path.suffix.lower() in WATCH_EXTENSIONS:
                result[str(path)] = file_signature(path)
    return result


class DirectoryChangeNotifications:
    """Low-cost Windows subtree notifications for build-input directories."""

    FILE_NOTIFY_CHANGE_FILE_NAME = 0x00000001
    FILE_NOTIFY_CHANGE_DIR_NAME = 0x00000002
    FILE_NOTIFY_CHANGE_SIZE = 0x00000008
    FILE_NOTIFY_CHANGE_LAST_WRITE = 0x00000010
    WAIT_OBJECT_0 = 0
    WAIT_TIMEOUT = 258
    INVALID_HANDLE_VALUE = ctypes.c_void_p(-1).value

    def __init__(self, directories: tuple[Path, ...]):
        self.changed = threading.Event()
        self.stop_event = threading.Event()
        self.threads: list[threading.Thread] = []
        self.available = os.name == "nt"
        self.failed = threading.Event()
        self.directories = directories

    def start(self) -> None:
        if not self.available:
            return
        try:
            kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
            kernel32.FindFirstChangeNotificationW.argtypes = [
                ctypes.c_wchar_p, ctypes.c_int, ctypes.c_uint32]
            kernel32.FindFirstChangeNotificationW.restype = ctypes.c_void_p
            kernel32.FindNextChangeNotification.argtypes = [ctypes.c_void_p]
            kernel32.FindNextChangeNotification.restype = ctypes.c_int
            kernel32.FindCloseChangeNotification.argtypes = [ctypes.c_void_p]
            kernel32.FindCloseChangeNotification.restype = ctypes.c_int
            kernel32.WaitForSingleObject.argtypes = [ctypes.c_void_p, ctypes.c_uint32]
            kernel32.WaitForSingleObject.restype = ctypes.c_uint32
            self.kernel32 = kernel32
        except (AttributeError, OSError):
            self.available = False
            return

        started = 0
        for directory in self.directories:
            if not directory.is_dir():
                continue
            thread = threading.Thread(
                target=self._watch_directory,
                args=(directory,),
                name=f"dev-loop-watch-{directory.name}",
                daemon=True,
            )
            thread.start()
            self.threads.append(thread)
            started += 1
        self.available = started > 0

    def _watch_directory(self, directory: Path) -> None:
        flags = (self.FILE_NOTIFY_CHANGE_FILE_NAME | self.FILE_NOTIFY_CHANGE_DIR_NAME
                 | self.FILE_NOTIFY_CHANGE_SIZE | self.FILE_NOTIFY_CHANGE_LAST_WRITE)
        handle = self.kernel32.FindFirstChangeNotificationW(str(directory), 1, flags)
        if not handle or handle == self.INVALID_HANDLE_VALUE:
            self.failed.set()
            return
        try:
            while not self.stop_event.is_set():
                result = self.kernel32.WaitForSingleObject(handle, 500)
                if result == self.WAIT_TIMEOUT:
                    continue
                if result != self.WAIT_OBJECT_0:
                    self.failed.set()
                    return
                self.changed.set()
                if not self.kernel32.FindNextChangeNotification(handle):
                    self.failed.set()
                    return
        finally:
            self.kernel32.FindCloseChangeNotification(handle)

    def close(self) -> None:
        self.stop_event.set()
        for thread in self.threads:
            thread.join(timeout=0.75)


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
        result = run_hidden(
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
        normalized_command = command_line.replace("/", "\\").lower()
        is_daemon = (
            "--daemon" in normalized_command
            or "--dev-loop-role=daemon" in normalized_command
            or "--dev-loop-role daemon" in normalized_command
        )
        if (pid != os.getpid() and script_text in normalized_command
                and is_daemon):
            pids.append(pid)
    return pids


def existing_dashboard_pid() -> int:
    """Find an already-visible dashboard for this checkout."""
    if os.name != "nt":
        return 0
    script_text = str(Path(__file__).resolve()).replace("/", "\\").lower()
    try:
        result = run_hidden(
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
        return 0
    for line in result.stdout.splitlines():
        try:
            pid_text, command_line = line.split("\t", 1)
            pid = int(pid_text.strip())
        except (ValueError, TypeError):
            continue
        normalized_command = command_line.replace("/", "\\").lower()
        if (pid != os.getpid() and script_text in normalized_command
                and "--daemon" not in normalized_command
                and ("--dev-loop-role=dashboard" in normalized_command
                     or "--dev-loop-role dashboard" in normalized_command)):
            return pid
    return 0


def windows_process_identity(pid: int, fresh: bool = False) -> dict | None:
    """Read identity data for one Windows process without changing it.

    Results are cached for a short TTL so that bursty status renders do not spawn
    a helper process per render (see the process-identity polling-cost contract).
    Pass ``fresh=True`` to bypass the cache, for example right after a stop.
    """
    if os.name != "nt" or not pid:
        return None
    now = time.monotonic()
    cached = None if fresh else _PROCESS_IDENTITY_CACHE.get(pid)
    if cached is not None and now - cached[0] < _PROCESS_IDENTITY_TTL_SECONDS:
        return cached[1]
    try:
        result = run_hidden(
            [
                "powershell", "-NoProfile", "-Command",
                "$p = Get-CimInstance Win32_Process -Filter "
                f"\"ProcessId={int(pid)}\"; "
                "if ($null -ne $p) { "
                "[Console]::WriteLine((ConvertTo-Json -Compress "
                "@{pid=[int]$p.ProcessId; executable=$p.ExecutablePath; "
                "command_line=$p.CommandLine})) }",
            ],
            capture_output=True,
            text=True,
            timeout=3,
            check=False,
        )
        value = json.loads(result.stdout.strip()) if result.stdout.strip() else None
    except (OSError, subprocess.SubprocessError, ValueError, json.JSONDecodeError):
        value = None
    identity = value if isinstance(value, dict) else None
    _PROCESS_IDENTITY_CACHE[pid] = (now, identity)
    return identity


def checkout_mimita_processes(*, strict: bool = False) -> list[dict]:
    """List MiMITA executables under this checkout with stable process identity."""
    if os.name != "nt":
        if strict:
            raise RuntimeError("MiMITA process discovery is available on Windows only")
        return []
    command = (
        "Get-CimInstance Win32_Process -Filter \"Name='mimita.exe'\" | "
        "Where-Object { $_.ExecutablePath -and "
        "($_.ExecutablePath -like '" + str(ROOT).replace("'", "''") + "\\*') } | "
        "ForEach-Object { [PSCustomObject]@{ pid=[int]$_.ProcessId; "
        "executable=[string]$_.ExecutablePath; command_line=[string]$_.CommandLine; "
        "process_started_utc=[string]($_.CreationDate.ToUniversalTime().ToString('o')) } } | "
        "ConvertTo-Json -Compress"
    )
    try:
        result = run_hidden(
            ["powershell", "-NoProfile", "-Command", command],
            capture_output=True, text=True, timeout=8, check=False,
        )
    except (OSError, subprocess.SubprocessError) as error:
        if strict:
            raise RuntimeError(f"MiMITA process scan failed: {error}") from error
        return []
    if result.returncode != 0:
        if strict:
            detail = (result.stderr or result.stdout or "process query failed").strip()
            raise RuntimeError(f"MiMITA process scan failed: {detail[:240]}")
        return []
    payload = (result.stdout or "").strip()
    if not payload:
        return []
    try:
        value = json.loads(payload)
    except json.JSONDecodeError as error:
        if strict:
            raise RuntimeError("MiMITA process scan returned invalid JSON") from error
        return []
    if isinstance(value, dict):
        value = [value]
    if not isinstance(value, list):
        if strict:
            raise RuntimeError("MiMITA process scan returned an unexpected result")
        return []
    return [item for item in value if isinstance(item, dict)]


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


def launch_mode_table() -> list[tuple[str, str]]:
    """Return `(id, name)` launch modes in display order for the dashboard picker."""
    try:
        config = read_json(LAUNCH_MODE_PATH)
    except (OSError, json.JSONDecodeError):
        return []
    modes = config.get("modes") if isinstance(config, dict) else None
    if not isinstance(modes, dict):
        return []
    ordered = sorted(
        modes,
        key=lambda value: (
            not bool(modes[value].get("clients")) if isinstance(modes[value], dict) else True,
            not str(value).isdigit(),
            int(str(value)) if str(value).isdigit() else str(value),
        ),
    )
    table: list[tuple[str, str]] = []
    for mode_id in ordered:
        mode = modes[mode_id]
        name = str(mode.get("name", "unnamed")) if isinstance(mode, dict) else "unnamed"
        table.append((str(mode_id), name))
    return table


def published_builds() -> list[Path]:
    return discover_published_builds(BUILD_ROOT)


def newest_valid_published_build() -> str | None:
    """Return the folder selector for the newest fully verified artifact."""
    selected = newest_valid_build(BUILD_ROOT, executable_identity)
    return selected[1] if selected else None


def newest_published_build() -> str | None:
    """Return the newest disk artifact even when identity validation fails."""
    candidates = published_builds()
    return candidates[0].name if candidates else None


def build_artifact_id(now: datetime.datetime | None = None) -> tuple[str, str]:
    """Return a canonical UTC ID and its Windows-safe artifact folder name."""
    return make_build_artifact_id(now, BUILD_ROOT)


def _executable_snapshot_is_valid(path: Path) -> tuple[bool, str, str]:
    return runtime_executable_snapshot_is_valid(path)


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


def stable_launch_matches(build_exe: Path) -> bool:
    """Return true when the fixed launch path is the exact published bundle."""
    try:
        if not os.path.samefile(STABLE_LAUNCH_EXE, build_exe):
            return False
        return all(
            os.path.samefile(STABLE_LAUNCH_DIR / name, build_exe.parent / name)
            for name in RUNTIME_DLLS
        )
    except OSError:
        return False


def stage_stable_launch(build_exe: Path, expected_sha256: str) -> Path:
    """Stage hardlinks from one immutable published artifact for stable-path launch."""
    source_root = BUILD_ROOT.resolve()
    resolved_exe = build_exe.resolve()
    if not resolved_exe.is_relative_to(source_root):
        raise RuntimeError(f"refusing stable launch from outside published builds: {build_exe}")

    valid, reason, digest = _executable_snapshot_is_valid(resolved_exe)
    if not valid or digest != expected_sha256:
        raise RuntimeError(
            f"published artifact changed before stable launch staging ({reason})"
        )

    DEV_ROOT.mkdir(parents=True, exist_ok=True)
    stage_dir = DEV_ROOT / f".current-stage-{os.getpid()}-{uuid.uuid4().hex[:8]}"
    stage_dir.mkdir()
    try:
        os.link(resolved_exe, stage_dir / "mimita.exe")
        for name in RUNTIME_DLLS:
            source_dll = resolved_exe.parent / name
            if not source_dll.is_file():
                raise RuntimeError(f"published artifact is missing runtime DLL: {source_dll}")
            os.link(source_dll, stage_dir / name)

        staged_identity = executable_identity(stage_dir / "mimita.exe")
        if not staged_identity.get("valid") or staged_identity.get("sha256") != expected_sha256:
            raise RuntimeError("stable launch staging failed executable identity verification")
        return stage_dir
    except Exception:
        try:
            if stage_dir.resolve().parent == DEV_ROOT.resolve():
                shutil.rmtree(stage_dir, ignore_errors=True)
        except OSError:
            pass
        raise


def activate_stable_launch(stage_dir: Path) -> Path:
    """Atomically point the constant firewall path at a staged build."""
    resolved_dev_root = DEV_ROOT.resolve()
    resolved_stage = stage_dir.resolve()
    if resolved_stage.parent != resolved_dev_root:
        raise RuntimeError(f"refusing to activate launch staging outside {DEV_ROOT}")
    if STABLE_LAUNCH_DIR.is_symlink() or (
            STABLE_LAUNCH_DIR.exists() and
            STABLE_LAUNCH_DIR.resolve().parent != resolved_dev_root):
        raise RuntimeError(f"refusing to replace unexpected stable launch path: {STABLE_LAUNCH_DIR}")

    backup_dir = DEV_ROOT / f".current-previous-{os.getpid()}-{uuid.uuid4().hex[:8]}"
    moved_old = False
    try:
        if STABLE_LAUNCH_DIR.exists():
            # Windows can keep the executable directory locked briefly after
            # taskkill reports success. Retry the atomic swap while the OS
            # releases those handles; never fall back to deleting the live path.
            deadline = time.monotonic() + 10.0
            while True:
                try:
                    os.replace(STABLE_LAUNCH_DIR, backup_dir)
                    break
                except PermissionError:
                    if time.monotonic() >= deadline:
                        raise
                    time.sleep(0.25)
            moved_old = True
        os.replace(stage_dir, STABLE_LAUNCH_DIR)
    except Exception:
        if moved_old and backup_dir.exists() and not STABLE_LAUNCH_DIR.exists():
            os.replace(backup_dir, STABLE_LAUNCH_DIR)
        raise

    invalidate_executable_identity(STABLE_LAUNCH_EXE)
    if moved_old and backup_dir.resolve().parent == resolved_dev_root:
        shutil.rmtree(backup_dir, ignore_errors=True)
    return STABLE_LAUNCH_EXE


def publish_build(artifact_id: str, folder_name: str, source_generation: int,
                  mode: str, source_at_start: dict | None = None,
                  source_at_publish: dict | None = None) -> Path:
    source_exe = ROOT / "mimita.exe"
    if not source_exe.is_file():
        raise RuntimeError("build succeeded but mimita.exe was not produced")

    destination = BUILD_ROOT / folder_name
    destination.mkdir(parents=True, exist_ok=False)
    try:
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

        published_identity = executable_identity(destination / "mimita.exe")
        metadata = {
            "artifact_id": artifact_id,
            "build_id": f"{artifact_id}:{published_identity.get('sha256', '')}",
            "created_utc": artifact_id,
            "sha256": published_identity.get("sha256", ""),
            "source_generation": source_generation,
            "build_mode": mode,
            "executable": str(destination / "mimita.exe"),
            "source_at_build_start": source_at_start or {},
            "source_at_publish": source_at_publish or {},
            "source_changed_during_build": (source_at_start or {}) != (source_at_publish or {}),
        }
        if not published_identity.get("valid"):
            raise RuntimeError(
                "refusing to publish invalid executable: "
                f"{published_identity.get('validation') or 'identity validation failed'}"
            )
        (destination / "build-identity.json").write_text(
            json.dumps(metadata, indent=2) + "\n", encoding="utf-8"
        )
    except Exception:
        shutil.rmtree(destination, ignore_errors=True)
        raise

    for old in published_builds()[KEEP_BUILDS:]:
        shutil.rmtree(old, ignore_errors=True)
    return destination


class DevLoop:
    def __init__(
        self,
        profile: dict,
        auto_restart: bool,
        launch_mode: str | None = None,
        launch_visibility: str | None = None,
    ):
        self.profile = profile
        try:
            self.max_mimita_processes = max(1, int(profile.get("max_mimita_processes", 1)))
        except (TypeError, ValueError):
            self.max_mimita_processes = 1
        self.launch_mode_id = str(
            launch_mode if launch_mode is not None
            else profile.get("launch_mode", 1)
        )
        self.launch_mode = load_launch_mode(self.launch_mode_id)
        configured_visibility = str(
            launch_visibility if launch_visibility is not None
            else profile.get("launch_visibility", "visible")
        ).strip().lower()
        if configured_visibility not in {"visible", "headless"}:
            raise SystemExit(
                f"[DEV] launch_visibility must be visible or headless, got {configured_visibility!r}"
            )
        self.launch_visibility = configured_visibility
        # Builds always run while this loop is open. Auto-start only controls
        # whether a successful build or an exited child may launch the EXE.
        self.auto_restart = bool(auto_restart)
        # One-shot map override for the next launch, set by a scenario launch
        # request so a scenario can target a specific authoritative map.
        self.override_map = ""
        # Weapon-set override for scenario launches (server --weapon-set).
        self.override_weapon_set = ""
        # Daemon source identity, surfaced in the status record so a stale daemon
        # (whose on-disk dev-loop.py hash changed) is detectable.
        self.daemon_started_utc = utc_now()
        # The last real build/lifecycle status written to the status record.
        # Read-only status queries reuse it instead of reporting the literal
        # query label as the build phase.
        self.last_status_value = "idle"
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
        self.server_build_id = ""
        self.server_local_port = 0
        self.server_args: list[str] = []
        self.server_adopted = False
        self.build_mismatch_reason = ""
        self.firewall_result = "NOT_CHECKED"
        self.server_launch_ms: float = 0.0
        self.client_launch_ms: float = 0.0
        self.server_unavailable = False
        self.running_build = None
        # A failed/interrupted publication may leave a timestamped directory
        # without a complete identity sidecar. Never let that directory prevent
        # daemon startup when an older verified artifact is available.
        self.latest_build_folder = newest_valid_published_build()
        initial_identity = (
            published_identity_for_folder(BUILD_ROOT / self.latest_build_folder,
                                          executable_identity)
            if self.latest_build_folder else None
        )
        initial_metadata = (read_json(BUILD_ROOT / self.latest_build_folder /
                                      "build-identity.json")
                            if self.latest_build_folder else {})
        self.latest_build = str(
            (initial_identity or {}).get("artifact_id")
            or (initial_metadata or {}).get("artifact_id")
            or self.latest_build_folder
        ) if self.latest_build_folder else None
        self.latest_generation = 0
        # A disk-restored artifact is usable, but it has not been proven
        # against this loop's current source snapshot yet. The initial build
        # must validate the current tree before the artifact can be launched.
        self.latest_stale = self.latest_build is not None
        self.build_pending = True
        self.build_status = "pending"
        self.active_build_generation: int | None = None
        self.active_build_started_utc: str | None = None
        self.active_build_started_monotonic: float | None = None
        self.active_build_detail = "waiting for build worker"
        self.build_output_tail: deque[str] = deque(maxlen=10)
        self.build_last_progress_save = 0.0
        self.build_worker: threading.Thread | None = None
        self.build_completions: Queue[dict] = Queue()
        self.pending_build_request_ids: list[str] = []
        self.active_build_request_ids: list[str] = []
        self.pending_build_request_modes: dict[str, str] = {}
        self.active_build_mode = "debug"
        self.pending_build_mode = "debug"
        self.latest_build_mode = "unknown"
        self.build_requests: dict[str, str] = {}
        self.last_build_result = ""
        self.last_build_duration_seconds: float | None = None
        self.launch_after_build = False
        self.shutting_down = False
        self.last_message = (
            f"restored published build {self.latest_build}; validating current source"
            if self.latest_build is not None
            else "starting; no published build found"
        )
        self.last_status_line = None
        self.last_state_save_monotonic = 0.0
        self.room_file_path: Path | None = None
        self.room_code = None
        # Run layout: one run folder with a dev-loop journal and one canonical
        # journal per game executable.
        self.run_layout: RunLayout | None = None
        self.manifest_path: str | None = None
        self.server_journal_path: str | None = None
        self.client_journal_path: str | None = None
        self.launch_id: str | None = None
        self.launch_executable: str | None = None
        self.client_instances: dict[str, dict] = {}
        self.instances: list[dict] = []
        # Legacy alias: the dev-loop journal. Kept for control/status readers.
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
        self.command_queue_path: str | None = None
        self.server_ready = False
        self.client_ready = False
        self.builds_match = False
        self.status_visible = False
        self.status_line_count = 0
        self.adopt_persisted_server()

    def adopt_persisted_server(self) -> None:
        """Adopt only a previously recorded server that still proves its identity."""
        if not SERVER_MARKER_PATH.is_file():
            return
        try:
            marker = read_json(SERVER_MARKER_PATH)
        except (OSError, json.JSONDecodeError):
            return
        if not isinstance(marker, dict) or marker.get("root") != str(ROOT):
            return
        try:
            pid = int(marker.get("pid", 0))
        except (TypeError, ValueError):
            return
        identity = windows_process_identity(pid)
        expected_exe = str(marker.get("executable", ""))
        command_line = str((identity or {}).get("command_line", ""))
        actual_exe = str((identity or {}).get("executable", ""))
        if not identity or not expected_exe:
            return
        if os.path.normcase(actual_exe) != os.path.normcase(expected_exe):
            return
        if "--server" not in command_line.lower():
            return
        self.server_pid = pid
        self.server_exe = expected_exe
        self.server_build_id = str(marker.get("build_id", ""))
        self.server_local_port = int(marker.get("local_port", 0) or 0)
        self.room_code = str(marker.get("room_code", "")) or None
        self.server_args = list(marker.get("args", []))
        for index, value in enumerate(self.server_args[:-1]):
            if value == "--room-file":
                self.room_file_path = Path(self.server_args[index + 1])
                break
        persisted_metadata = marker.get("run_metadata")
        if isinstance(persisted_metadata, dict):
            self.run_metadata = dict(persisted_metadata)
        self.server_adopted = True
        self.server_ready = bool(marker.get("server_ready", False))
        self.server_launch_ms = float(marker.get("launch_ms", 0.0) or 0.0)
        self.run_id = str(marker.get("run_id", "")) or self.run_id
        self.events_path = str(marker.get("events_path", "")) or self.events_path
        self.manifest_path = str(marker.get("manifest_path", "")) or self.manifest_path
        self.server_journal_path = (str(marker.get("server_journal_path", "")) or
                                    self.server_journal_path)
        self.client_journal_path = (str(marker.get("client_journal_path", "")) or
                                    self.client_journal_path)
        self.launch_id = str(marker.get("launch_id", "")) or self.launch_id
        run_dir = str(marker.get("run_dir", "")) or ""
        if run_dir:
            self.run_layout = RunLayout(
                run_dir=Path(run_dir),
                run_id=self.run_id or Path(run_dir).name,
                manifest_path=Path(self.manifest_path or (Path(run_dir) / "run.json")),
                dev_loop_journal=Path(self.events_path or (Path(run_dir) / "dev-loop" / "events.jsonl")),
                server_journal=Path(self.server_journal_path or (Path(run_dir) / "server-01" / "events.jsonl")),
                client_journal=Path(self.client_journal_path or (Path(run_dir) / "client-01" / "events.jsonl")),
            )
        self.last_message = f"adopted verified dev-loop server pid={pid}"
        self.record_runtime_event(
            "dev-loop.process.adopted",
            {
                "role": "server",
                "pid": pid,
                "executable": expected_exe,
                "build_id": marker.get("build_id", ""),
                "run_id": marker.get("run_id", ""),
            },
            "persisted server marker and live process identity matched",
        )

    def write_server_marker(self) -> None:
        write_json(SERVER_MARKER_PATH, {
            "root": str(ROOT),
            "pid": self.server_pid,
            "executable": self.server_exe,
            "args": list(self.server_args),
            "build_id": self.run_metadata.get("build_id", ""),
            "build_identity": self.run_metadata.get("build_identity", {}),
            "build_number": self.run_metadata.get("build_number"),
            "source_generation": self.run_metadata.get("source_generation"),
            "run_metadata": dict(self.run_metadata),
            "local_port": self.server_local_port,
            "room_code": self.room_code or "",
            "run_id": self.run_id,
            "events_path": self.events_path,
            "run_dir": str(self.run_layout.run_dir) if self.run_layout else "",
            "manifest_path": self.manifest_path,
            "server_journal_path": self.server_journal_path,
            "client_journal_path": self.client_journal_path,
            "launch_id": self.launch_id,
            "server_ready": self.server_ready,
            "launch_ms": self.server_launch_ms,
            "updated_utc": utc_now(),
        })

    def clear_server_marker(self) -> None:
        try:
            marker = read_json(SERVER_MARKER_PATH)
        except (OSError, json.JSONDecodeError):
            marker = None
        try:
            marker_pid = int(marker.get("pid", 0) or 0) if isinstance(marker, dict) else 0
        except (TypeError, ValueError):
            marker_pid = 0
        if not isinstance(marker, dict) or marker_pid == int(self.server_pid or 0):
            try:
                SERVER_MARKER_PATH.unlink()
            except FileNotFoundError:
                pass

    def _ensure_run_layout(self) -> RunLayout:
        if self.run_layout is None:
            layout = runtime_create_run_layout()
            self.run_layout = layout
            self.events_path = str(layout.dev_loop_journal)
            self.manifest_path = str(layout.manifest_path)
            self.server_journal_path = str(layout.server_journal)
            self.client_journal_path = str(layout.client_journal)
            self.run_id = layout.run_id
        return self.run_layout

    def _journal_read_paths(self) -> list[str]:
        paths = [self.server_journal_path, self.client_journal_path,
                 self.events_path]
        paths.extend(str(record.get("journal_path") or "")
                     for record in self.client_instances.values())
        return [path for path in dict.fromkeys(paths) if path]

    @staticmethod
    def _tail_records(path: str | None, max_bytes: int = 262144) -> list[dict]:
        if not path:
            return []
        try:
            with open(path, "rb") as handle:
                handle.seek(0, os.SEEK_END)
                size = handle.tell()
                handle.seek(max(0, size - max_bytes))
                raw = handle.read()
        except OSError:
            return []
        records = []
        for line in raw.decode("utf-8", errors="replace").splitlines():
            try:
                value = json.loads(line)
            except json.JSONDecodeError:
                continue
            if isinstance(value, dict):
                records.append(value)
        return records

    def _journal_last(self, path: str | None, events: set[str] | None = None,
                      max_bytes: int = 262144) -> dict | None:
        for record in reversed(self._tail_records(path, max_bytes)):
            if events is None or str(record.get("event", "")) in events:
                return record
        return None

    @staticmethod
    def _age_ms(record: dict | None, now_ms: float) -> float | None:
        if not record:
            return None
        value = record.get("wall_time")
        if not isinstance(value, str):
            return None
        try:
            from datetime import datetime, timezone
            stamp = datetime.fromisoformat(value.replace("Z", "+00:00"))
            return max(0.0, now_ms - stamp.timestamp() * 1000.0)
        except ValueError:
            return None

    def _next_client_instance_id(self) -> str:
        index = 1
        while f"client-{index:02d}" in self.client_instances:
            index += 1
        return f"client-{index:02d}"

    def start_client_instance(self, name: str, badconn_preset: str = "",
                              instance_id: str | None = None,
                              server_instance_id: str = "server-01") -> dict | None:
        """Start one additional client connected to the owned server."""
        if server_instance_id != "server-01":
            self.last_message = f"unknown server instance {server_instance_id}"
            return None
        if not self.server_health() or not self.launch_executable:
            self.last_message = "start-client requires a running server and published build"
            return None
        self._ensure_run_layout()
        iid = instance_id or self._next_client_instance_id()
        if iid in self.client_instances:
            self.last_message = f"duplicate client instance {iid}"
            return None
        journal = self.run_layout.journal_for(iid)
        journal.parent.mkdir(parents=True, exist_ok=True)
        queue = DEV_ROOT / f"agent-command-{self.run_id}-{iid}.jsonl"
        queue.write_text("", encoding="utf-8")
        args = [self.launch_executable]
        if self.room_code:
            args.extend(["--room", self.room_code])
        else:
            args.extend(["--connect", f"127.0.0.1:{self.server_local_port}"])
            if self.room_file_path is not None:
                args.extend(["--room-code-file", str(self.room_file_path)])
        args.extend(["--map", self.current_map or "",
                     "--name", name, "--unlock-cursor"])
        if badconn_preset:
            args.extend(["--badconn-preset", str(badconn_preset)])
        args.extend(["--agent-command-file", str(queue)])
        env = {
            **os.environ,
            "MIMITA_EVENTS_FILE": str(journal),
            "MIMITA_INSTANCE_ID": iid,
            "MIMITA_LAUNCH_ID": self.launch_id or "",
            "MIMITA_DEV_LOOP_SERVER": "1",
            "MIMITA_RUN_ID": self.run_id or "",
            "MIMITA_EXPECTED_RUN_ID": self.run_id or "",
            "MIMITA_EXPECTED_BUILD_ID": self.run_metadata.get("build_id", ""),
            "MIMITA_BUILD_ID": self.run_metadata.get("build_id", ""),
            "MIMITA_RUN_METADATA": json.dumps(self.run_metadata, separators=(",", ":")),
            "MIMITA_PROCESS_ROLE": "client",
        }
        try:
            process = subprocess.Popen(
                args, cwd=ROOT, env=env,
                creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
        except OSError as error:
            self.last_message = f"could not start {iid}: {error}"
            return None
        self.processes.append(process)
        record = {
            "instance_id": iid,
            "name": name,
            "process": process,
            "pid": process.pid,
            "journal_path": str(journal),
            "command_queue": str(queue),
            "badconn_preset": badconn_preset,
            "server_instance_id": "server-01",
            "started_ms": time.time() * 1000.0,
        }
        self.client_instances[iid] = record
        ready = self.wait_for_logger_started("client", process.pid, process=process)
        self.client_ready = self.client_ready or ready
        self.record_runtime_event(
            "dev-loop.instance.started",
            {"instance_id": iid, "role": "client", "pid": process.pid,
             "journal_path": str(journal), "badconn_preset": badconn_preset,
             "ready": ready},
            "additional client instance started")
        self.last_message = f"started {iid} pid={process.pid} ready={ready}"
        self.save_state("client_started")
        return record

    def stop_client_instance(self, instance_id: str) -> bool:
        record = self.client_instances.get(instance_id)
        if not record:
            self.last_message = f"unknown client instance {instance_id}"
            return False
        process = record.get("process")
        if process is not None and process.poll() is None:
            try:
                process.terminate()
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                try:
                    process.kill()
                    process.wait(timeout=3)
                except (OSError, subprocess.TimeoutExpired) as error:
                    self.last_message = f"could not stop {instance_id} pid={process.pid}: {error}"
                    return False
            except OSError as error:
                self.last_message = f"could not stop {instance_id} pid={process.pid}: {error}"
                return False
            if process.poll() is None:
                self.last_message = f"could not stop {instance_id} pid={process.pid}: process remains alive"
                return False
        self.client_instances.pop(instance_id, None)
        self.processes = [p for p in self.processes if p is not process]
        if not self.client_instances:
            self.client_ready = False
        self.record_runtime_event(
            "dev-loop.instance.stopped",
            {"instance_id": instance_id, "role": "client"},
            "client instance stopped")
        self.last_message = f"stopped {instance_id}"
        self.save_state("client_stopped")
        return True

    def apply_configured_client_teams(self, configured_clients: list[dict]) -> None:
        """Force the launch mode's client teams through the host command path."""
        if len(configured_clients) < 2:
            return
        host = self.client_instances.get("client-01")
        if not host:
            return
        entries = []
        for index, client_cfg in enumerate(configured_clients, start=1):
            try:
                team = int(client_cfg.get("team", 0) or 0)
            except (TypeError, ValueError):
                team = 0
            if team not in (1, 2):
                continue
            iid = f"client-{index:02d}"
            record = self.client_instances.get(iid)
            if not record:
                continue
            actor_id = 0
            deadline = time.time() + 30.0
            while time.time() < deadline:
                event = self._journal_last(
                    record.get("journal_path"),
                    {"client.local.spawn_applied"})
                if event:
                    fields = event.get("fields") or {}
                    try:
                        actor_id = int(fields.get("actor_id", 0) or 0)
                    except (TypeError, ValueError):
                        actor_id = 0
                    if actor_id:
                        break
                time.sleep(0.2)
            if actor_id:
                entries.append({"command": "team_assign",
                                "args": [str(actor_id), str(team)]})
        if not entries:
            return
        self._queue_agent_requests(
            {"client_id": "client-01",
             "request_id": f"team-assign-{uuid.uuid4().hex[:8]}"},
            entries)

    def restart_client_instance(self, instance_id: str) -> bool:
        record = self.client_instances.get(instance_id)
        if not record:
            return False
        name = str(record.get("name", instance_id))
        preset = str(record.get("badconn_preset", ""))
        if not self.stop_client_instance(instance_id):
            return False
        return self.start_client_instance(name, preset, instance_id) is not None

    def _instance_records(self) -> list[dict]:
        now_ms = time.time() * 1000.0
        server_alive = bool(self.server_pid and self.server_health())
        server_last = self._journal_last(self.server_journal_path)
        server_event_age = self._age_ms(server_last, now_ms)
        if not server_alive:
            server_state = "stopped" if self.server_unavailable else "offline"
        elif not self.server_ready:
            server_state = "starting"
        elif self.build_mismatch_reason:
            server_state = "mismatch"
        else:
            server_state = "online"
        server_uptime = (max(0.0, (now_ms - self.server_launch_ms) / 1000.0)
                         if self.server_launch_ms and server_alive else None)
        server = {
            "instance_id": "server-01",
            "role": "server",
            "label": self.profile.get("server_name", "server"),
            "pid": self.server_pid,
            "executable": self.server_exe,
            "build_id": self.server_build_id or self.run_metadata.get("server_build_id"),
            "launch_id": self.launch_id,
            "state": server_state,
            "bind": (f"127.0.0.1:{self.server_local_port}"
                     if self.server_local_port else ""),
            "room_code": self.room_code or "",
            "map": self.current_map,
            "mode": self.launch_mode.get("name", self.launch_mode_id),
            "journal_path": self.server_journal_path,
            "last_event_utc": (server_last or {}).get("wall_time"),
            "event_age_ms": server_event_age,
            "uptime_seconds": server_uptime,
            "roster_count": len(self.client_instances) if server_alive else 0,
            "build_match": bool(self.builds_match),
        }
        clients = []
        for iid, record in sorted(self.client_instances.items()):
            process = record.get("process")
            alive = bool(process is not None and process.poll() is None)
            journal = record.get("journal_path")
            last = self._journal_last(journal)
            snap = self._journal_last(
                journal, {"network.client-snapshot-window"},
                max_bytes=4 * 1024 * 1024)
            event_age = self._age_ms(last, now_ms)
            snap_age = self._age_ms(snap, now_ms)
            snap_tick = (snap or {}).get("fields", {}).get("server_tick") \
                if isinstance((snap or {}).get("fields"), dict) else None
            if not alive:
                state = "offline"
            elif snap_age is not None and snap_age <= 3000.0:
                state = "online"
            elif not server_alive:
                state = "frozen"
            else:
                state = "stale"
            clients.append({
                "instance_id": iid,
                "role": "client",
                "label": record.get("name", iid),
                "pid": process.pid if alive else record.get("pid"),
                "executable": self.launch_executable,
                "build_id": self.run_metadata.get("client_build_id"),
                "launch_id": self.launch_id,
                "state": state,
                "server_instance_id": record.get("server_instance_id", "server-01"),
                "connected_address": (f"127.0.0.1:{self.server_local_port}"
                                      if self.server_local_port else ""),
                "badconn_preset": record.get("badconn_preset", ""),
                "journal_path": journal,
                "last_event_utc": (last or {}).get("wall_time"),
                "event_age_ms": event_age,
                "last_snapshot_tick": snap_tick,
                "snapshot_age_ms": snap_age,
                "uptime_seconds": (max(0.0, (now_ms - float(record.get("started_ms", now_ms))) / 1000.0)
                                   if alive else None),
                "command_queue": record.get("command_queue"),
                "build_match": bool(self.builds_match),
            })
        if not clients and self.processes:
            # Backward-compatible single-client view for a run adopted from old state.
            clients.append({
                "instance_id": "client-01", "role": "client",
                "label": self.profile.get("client_name", "client"),
                "pid": self.processes[-1].pid,
                "state": "stale" if not server_alive else "online",
                "server_instance_id": "server-01",
                "journal_path": self.client_journal_path,
                "snapshot_age_ms": None, "uptime_seconds": None,
                "build_match": bool(self.builds_match),
            })
        return [server, *clients]

    def _write_manifest(self) -> None:
        layout = self.run_layout
        if layout is None:
            return
        manifest = {
            "checkout_root": str(ROOT),
            "created_utc": self.run_started_utc,
            "build_id": self.run_metadata.get("build_id"),
            "artifact_id": self.run_metadata.get("artifact_id"),
            "source_generation": self.run_metadata.get("source_generation"),
            "instances": self._instance_records(),
        }
        try:
            write_run_manifest(layout, manifest)
        except OSError as error:
            print(f"[DEV] manifest write failed: {error}")

    def state(self, status: str = "idle") -> dict:
        client_executable = self.client_args[0] if self.client_args else None
        latest_identity = self.latest_build_identity()
        runtime_identity = dict(self.run_metadata.get("build_identity") or {})
        with self.lock:
            build_snapshot = {
                "build_status": self.build_status,
                "build_pending": self.build_pending,
                "active_build_generation": self.active_build_generation,
                "active_build_mode": self.active_build_mode,
                "pending_build_mode": self.pending_build_mode,
                "latest_build_mode": self.latest_build_mode,
                "active_build_started_utc": self.active_build_started_utc,
                "active_build_started_monotonic": self.active_build_started_monotonic,
                "active_build_detail": self.active_build_detail,
                "build_output_tail": list(self.build_output_tail),
                "last_build_result": self.last_build_result,
                "last_build_duration_seconds": self.last_build_duration_seconds,
                "build_requests": dict(self.build_requests),
            }
        identities_match = bool(
            self.server_exe and client_executable and
            os.path.normcase(self.server_exe) == os.path.normcase(client_executable) and
            self.run_metadata.get("build_id") == self.run_metadata.get("client_build_id") and
            self.run_metadata.get("build_id") == self.run_metadata.get("server_build_id") and
            self.server_build_id == self.run_metadata.get("server_build_id") and
            runtime_identity.get("valid")
        )
        mismatch_reason = self.build_mismatch_reason
        if not mismatch_reason and self.server_exe and client_executable:
            if os.path.normcase(self.server_exe) != os.path.normcase(client_executable):
                mismatch_reason = "server_client_executable_path"
            elif not identities_match:
                mismatch_reason = "server_client_build_identity"
        now_ms = time.time() * 1000.0
        run_uptime = None
        if self.run_started_utc:
            try:
                started = datetime.datetime.fromisoformat(
                    self.run_started_utc.replace("Z", "+00:00")
                ).timestamp()
                run_uptime = max(0.0, time.time() - started)
            except ValueError:
                run_uptime = None
        # Query server liveness once per state() and reuse it, so a single status
        # render cannot trigger several identity helper processes (section 5.1).
        server_alive = self.server_health()
        server_uptime = (
            max(0.0, (now_ms - self.server_launch_ms) / 1000.0)
            if self.server_launch_ms and server_alive else None
        )
        client_alive = [p for p in self.processes if p.poll() is None]
        client_uptime = (
            max(0.0, (now_ms - self.client_launch_ms) / 1000.0)
            if self.client_launch_ms and client_alive else None
        )
        build_elapsed = (max(0.0, time.monotonic() - build_snapshot["active_build_started_monotonic"])
                         if build_snapshot["active_build_started_monotonic"] is not None
                         else build_snapshot["last_build_duration_seconds"])
        latest_identity = self.latest_build_identity()
        effective_launch = _effective_launch_identity(list(self.server_args))
        if not effective_launch["map"]:
            effective_launch["map"] = self.current_map or ""
        return {
            "profile": self.profile.get("name", "unnamed"),
            "effective_map": effective_launch["map"],
            "effective_mode": effective_launch["mode"],
            "effective_gamemode": effective_launch["gamemode"],
            "effective_weapon_set": effective_launch["weapon_set"],
            "effective_npcs": effective_launch["npcs"],
            "launch_mode": self.launch_mode_id,
            "running_build": self.running_build,
            "running_build_id": (self.run_metadata.get("build_id")
                                 or self.server_build_id or None),
            "latest_successful_build": self.latest_build,
            "latest_artifact_folder": self.latest_build_folder,
            "latest_artifact_id": (latest_identity or {}).get("artifact_id"),
            "latest_successful_build_id": self.build_id(latest_identity),
            "source_generation": self.generation,
            "latest_successful_source_generation": (latest_identity or {}).get(
                "source_generation", self.latest_generation),
            "latest_build_stale": self.latest_stale,
            "auto_restart": self.auto_restart,
            "launch_visibility": self.launch_visibility,
            "headless": self.launch_visibility == "headless",
            "status": status,
            "build_status": build_snapshot["build_status"],
            "build_pending": build_snapshot["build_pending"],
            "active_build_generation": build_snapshot["active_build_generation"],
            "active_build_mode": build_snapshot["active_build_mode"],
            "pending_build_mode": build_snapshot["pending_build_mode"],
            "latest_build_mode": build_snapshot["latest_build_mode"],
            "active_build_started_utc": build_snapshot["active_build_started_utc"],
            "active_build_elapsed_seconds": build_elapsed,
            "active_build_detail": build_snapshot["active_build_detail"],
            "build_output_tail": build_snapshot["build_output_tail"],
            "last_build_result": build_snapshot["last_build_result"],
            "last_build_duration_seconds": build_snapshot["last_build_duration_seconds"],
            "build_requests": build_snapshot["build_requests"],
            "message": self.last_message,
            "last_control": dict(getattr(self, "last_control", {})),
            "updated_at": utc_now(),
            "run_id": self.run_id,
            "run_started_utc": self.run_started_utc,
            "run_metadata": dict(self.run_metadata),
            "run_dir": str(self.run_layout.run_dir) if self.run_layout else None,
            "manifest_path": self.manifest_path,
            "server_journal_path": self.server_journal_path,
            "client_journal_path": self.client_journal_path,
            "instances": self._instance_records(),
            "events_path": self.events_path,
            "control_port": self.control_port,
            "control_token": self.control_token if self.control_port else None,
            "server_pid": self.server_pid,
            "server_executable": self.server_exe,
            "server_build_id": self.server_build_id,
            "room_code": self.room_code or "",
            "server_adopted": self.server_adopted,
            "server_args": list(self.server_args),
            "server_alive": server_alive,
            "server_uptime_seconds": server_uptime,
            "server_ready": self.server_ready,
            "client_pids": [p.pid for p in client_alive],
            "client_executables": [client_executable] if self.processes and client_executable else [],
            "client_args": list(self.client_args),
            "command_queue_path": self.command_queue_path,
            "client_ready": self.client_ready,
            "client_uptime_seconds": client_uptime,
            "run_alive": bool(server_alive or client_alive),
            "run_uptime_seconds": run_uptime,
            "builds_match": bool(self.builds_match and identities_match),
            "build_mismatch_reason": mismatch_reason,
            "firewall_result": self.firewall_result,
            "map": self.current_map,
            "build_identity": latest_identity,
            "launch_mode_name": self.launch_mode.get("name", "unnamed"),
            "launch_mode_config": str(LAUNCH_MODE_PATH),
            "daemon_pid": os.getpid(),
            "daemon_script_hash": script_sha256(DEV_LOOP_SCRIPT),
            "daemon_started_utc": self.daemon_started_utc,
        }

    def save_state(self, status: str = "idle") -> None:
        self.last_status_value = status
        snapshot = self.state(status)
        write_json(STATE_PATH, snapshot)
        write_json(CURRENT_RUN_PATH, snapshot)
        self._write_manifest()

    def record_control_event(self, request: dict, status: str, message: str) -> None:
        """Append control-plane evidence to the active canonical journal."""
        self.last_control = {
            "request_id": str(request.get("request_id", "")),
            "action": str(request.get("action", "")),
            "command": str(request.get("command", "")),
            "client_id": str(request.get("client_id", "")),
            "status": status,
            "message": message,
            "updated_at": utc_now(),
        }
        if isinstance(request.get("results"), list):
            self.last_control["results"] = request["results"][:32]
        if not self.events_path:
            return
        record = {
            "category": "GENERAL",
            "event": "dev-loop.control",
            "event_id": f"DEV_CONTROL_{request.get('request_id', uuid.uuid4().hex)}",
            "fields": {
                "action": request.get("action", ""),
                "command": request.get("command", ""),
                "client_id": request.get("client_id", ""),
                "launch_mode": request.get("launch_mode"),
                "request_id": request.get("request_id", ""),
                "requested_by": request.get("requested_by", "ai"),
                "status": status,
                "message": message,
                "results": request.get("results", [])[:32]
                if isinstance(request.get("results"), list) else [],
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

    def record_runtime_event(self, event: str, fields: dict, reason: str,
                             events_path: str | None = None,
                             run_id: str | None = None) -> None:
        """Append bounded dev-loop launch evidence to the canonical journal."""
        journal_path = events_path or self.events_path
        if not journal_path:
            return
        record = {
            "category": "GENERAL",
            "event": event,
            "event_id": f"DEV_RUNTIME_{uuid.uuid4().hex}",
            "fields": fields,
            "level": "IMPORTANT",
            "pid": os.getpid(),
            "process": "dev-loop",
            "process_role": "dev-loop",
            "reason": reason,
            "run_id": run_id or self.run_id or "",
            "wall_time": utc_now(),
        }
        try:
            with self.control_event_lock:
                with open(journal_path, "a", encoding="utf-8", newline="\n") as journal:
                    journal.write(json.dumps(record, separators=(",", ":")) + "\n")
        except OSError as error:
            print(f"[DEV RUNTIME] unable to record launch evidence: {error}")

    def _scenario_json(self, path_text: str) -> dict:
        path = Path(path_text).expanduser()
        if not path.is_absolute():
            path = ROOT / path
        text = path.read_text(encoding="utf-8")
        if "```json" in text:
            text = text.split("```json", 1)[1].split("```", 1)[0]
        value = json.loads(text.strip())
        if not isinstance(value, dict) or not value.get("id"):
            raise ValueError("scenario must be an object with a non-empty id")
        return value

    def _queue_agent_requests(self, request: dict, entries: list[dict]) -> dict:
        requested_client = str(request.get("client_id", "") or "")
        if requested_client:
            record = self.client_instances.get(requested_client)
            process = record.get("process") if record else None
            if not record or process is None or process.poll() is not None:
                message = f"client instance {requested_client} is not alive"
                self.record_control_event(request, "rejected", message)
                self.save_state("command_rejected")
                return {"accepted": False, "queued": False, "error": message}
            live_clients = [process]
            queue_path = str(record.get("command_queue") or "")
            journal_path = str(record.get("journal_path") or self.client_journal_path)
        else:
            live_clients = [process for process in self.processes if process.poll() is None]
            if not live_clients or not self.client_ready:
                message = "client is not alive and logger-ready"
                self.record_control_event(request, "rejected", message)
                self.save_state("command_rejected")
                return {"accepted": False, "queued": False, "error": message}
            queue_path = str(self.command_queue_path or "")
            journal_path = self.client_journal_path
        if not queue_path:
            message = "client command queue is not configured"
            self.record_control_event(request, "rejected", message)
            self.save_state("command_rejected")
            return {"accepted": False, "queued": False, "error": message}

        request_id = str(request.get("request_id", "")) or f"agent-{uuid.uuid4().hex[:12]}"
        scenario_id = str(request.get("scenario_id", ""))
        queue_records = []
        not_before_ms = int(time.time() * 1000)
        for index, entry in enumerate(entries):
            command = str(entry.get("command", "")).strip()
            args = entry.get("args", [])
            if not command or not isinstance(args, list):
                raise ValueError(f"scenario entry {index} must have command and args list")
            command_text = " ".join([command, *(str(value) for value in args)]).strip()
            queue_records.append({
                "request_id": f"{request_id}:{index}",
                "command": command_text,
                "scenario_id": scenario_id,
                "step_id": str(entry.get("id", entry.get("label", index))),
                "step_index": index,
                "step_count": len(entries),
                "not_before_ms": not_before_ms,
            })
            not_before_ms += max(0, int(entry.get("delay_ms_after", 0) or 0))

        with open(queue_path, "a", encoding="utf-8", newline="\n") as queue:
            for record in queue_records:
                queue.write(json.dumps(record, separators=(",", ":")) + "\n")

        self.record_runtime_event(
            "dev-loop.command.request",
            {
                "request_id": request_id,
                "command_count": len(queue_records),
                "scenario_id": scenario_id,
                "client_pid": live_clients[0].pid,
                "client_id": requested_client or "client-01",
                "build": self.running_build,
                "events_path": journal_path,
                "queue_path": queue_path,
            },
            "dev-loop queued client command request",
        )
        self.record_control_event(request, "accepted", f"queued {len(queue_records)} client command(s)")
        self.save_state("command_queued")
        return {
            "accepted": True,
            "queued": True,
            "request_id": request_id,
            "command_count": len(queue_records),
            "client_pid": live_clients[0].pid,
            "client_id": requested_client or "client-01",
            "build": self.running_build,
            "events_path": journal_path,
            "queue_path": queue_path,
        }

    def queue_agent_command(self, request: dict) -> dict:
        command = str(request.get("command", "")).strip()
        if not command:
            return {"accepted": False, "queued": False, "error": "missing command"}
        return self._queue_agent_requests(request, [{"command": command, "args": []}])

    def queue_agent_scenario(self, request: dict) -> dict:
        try:
            scenario = self._scenario_json(str(request.get("scenario_file", "")))
            entries = []
            entries.extend(scenario.get("setup", []))
            entries.extend(scenario.get("steps", []))
            if not entries:
                raise ValueError("scenario has no setup or steps")
            request = dict(request)
            request["scenario_id"] = str(scenario["id"])
            return self._queue_agent_requests(request, entries)
        except (OSError, ValueError, json.JSONDecodeError) as error:
            message = f"scenario rejected: {error}"
            self.record_control_event(request, "rejected", message)
            self.save_state("scenario_rejected")
            return {"accepted": False, "queued": False, "error": message}

    def launch_for_control_request(self, request: dict) -> bool:
        """Keep launch exceptions visible and the daemon available for retry."""
        request_id = str(request.get("request_id", ""))
        try:
            return self.launch_latest(request_id=request_id)
        except Exception as error:
            details = traceback.format_exc(limit=12)
            self.last_message = f"launch failed with {type(error).__name__}: {error}"
            self.record_runtime_event(
                "dev-loop.launch.exception",
                {
                    "request_id": request_id,
                    "exception_type": type(error).__name__,
                    "traceback": details[-12000:],
                },
                self.last_message,
            )
            print(f"[DEV] {self.last_message}\n{details}", file=sys.stderr)
            return False

    def observe_launch_windows(self, phase: str) -> dict:
        """Record the current child-window, focus, cursor, and firewall state."""
        pids = [os.getpid()]
        if self.server_pid:
            pids.append(self.server_pid)
        pids.extend(process.pid for process in self.processes)
        snapshot = _windows_launch_snapshot(pids)
        self.record_runtime_event(
            "dev-loop.launch.windows",
            {
                "phase": phase,
                "server_pid": self.server_pid,
                "client_pids": [process.pid for process in self.processes],
                "firewall_prompt_detected": bool(snapshot["firewall_windows"]),
                "foreground": snapshot["foreground"],
                "cursor_window": snapshot["cursor_window"],
                "cursor_position": snapshot.get("cursor_position"),
                "windows": snapshot["windows"],
                "firewall_windows": snapshot["firewall_windows"],
            },
            f"dev-loop launch window snapshot phase={phase}",
        )
        if snapshot["firewall_windows"]:
            self.record_runtime_event(
                "dev-loop.firewall_prompt",
                {
                    "phase": phase,
                    "detected": True,
                    "windows": snapshot["firewall_windows"],
                },
                "Windows Firewall consent window detected; no automatic response was sent",
            )
        return snapshot

    def monitor_launch_windows(self, duration: float = 5.0) -> dict:
        """Bound firewall/focus observation after the client becomes ready."""
        deadline = time.monotonic() + duration
        detected = False
        last_snapshot = self.observe_launch_windows("client_ready")
        while time.monotonic() < deadline:
            time.sleep(0.25)
            last_snapshot = _windows_launch_snapshot(
                [self.server_pid or 0] + [process.pid for process in self.processes]
            )
            if last_snapshot["firewall_windows"] and not detected:
                detected = True
                self.record_runtime_event(
                    "dev-loop.firewall_prompt",
                    {
                        "phase": "post_client_monitor",
                        "detected": True,
                        "windows": last_snapshot["firewall_windows"],
                    },
                    "Windows Firewall consent window detected during bounded launch monitor",
                )
        self.observe_launch_windows("post_client_monitor")
        if last_snapshot.get("firewall_windows"):
            self.firewall_result = "FIREWALL_PROMPT_DETECTED"
        elif self.firewall_result == "LOOPBACK_ONLY":
            self.firewall_result = "FIREWALL_CLEAR"
        return last_snapshot

    def restore_prelaunch_foreground(self, snapshot: dict) -> None:
        """Return focus to the user's prior window after child startup."""
        if os.name != "nt":
            return
        foreground = snapshot.get("foreground") or {}
        hwnd = int(foreground.get("hwnd") or 0)
        current = _windows_launch_snapshot()
        current_foreground = current.get("foreground") or {}
        if not hwnd or hwnd == int(current_foreground.get("hwnd") or 0):
            return
        import ctypes

        user32 = ctypes.windll.user32
        if not user32.IsWindow(hwnd):
            self.record_runtime_event(
                "dev-loop.launch.focus_restore",
                {"requested": True, "restored": False, "reason": "prior window no longer exists"},
                "prior foreground window was unavailable",
            )
            return
        restored = bool(user32.SetForegroundWindow(hwnd))
        after = _windows_launch_snapshot().get("foreground")
        self.record_runtime_event(
            "dev-loop.launch.focus_restore",
            {
                "requested": True,
                "restored": restored,
                "prior_window": foreground,
                "after_window": after,
                "same_window": bool(after and after.get("hwnd") == hwnd),
            },
            "dev-loop attempted to restore the user's prelaunch foreground window",
        )

    def apply_launch_visibility(self) -> None:
        """Apply the profile's visible/headless policy to the client window."""
        if not self.processes:
            return
        client_pid = self.processes[-1].pid
        windows = _set_process_windows_visible(
            client_pid, self.launch_visibility == "visible"
        )
        self.record_runtime_event(
            "dev-loop.launch.visibility",
            {
                "policy": self.launch_visibility,
                "client_pid": client_pid,
                "windows": windows,
            },
            f"dev-loop applied {self.launch_visibility} client-window policy",
        )

    def latest_executable(self) -> Path:
        if self.latest_build_folder is None:
            return BUILD_ROOT / "(none)" / "mimita.exe"
        return BUILD_ROOT / self.latest_build_folder / "mimita.exe"

    def latest_build_identity(self) -> dict | None:
        if self.latest_build is None or self.latest_build_folder is None:
            return None
        info = published_identity_for_folder(BUILD_ROOT / self.latest_build_folder,
                                             executable_identity)
        if info is None:
            return None
        info.setdefault("source_generation", self.latest_generation)
        info["source_current"] = not self.latest_stale
        return info

    @staticmethod
    def build_id(identity: dict | None) -> str:
        if not identity:
            return ""
        return str(identity.get("build_id") or
                   f"{identity.get('artifact_id', identity.get('build_number', ''))}:{identity.get('sha256', '')}")

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
                    response = {"accepted": True, "state": self.state(self.last_status_value)}
                else:
                    request.setdefault("requested_by", "ai")
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

    def queue_build_request(self, request_id: str, mode: str = "debug") -> bool:
        request_id = request_id or f"build-{uuid.uuid4().hex[:12]}"
        mode = str(mode or "debug").strip().lower()
        if mode not in ("debug", "release"):
            return False
        with self.lock:
            if (self.active_build_generation is not None
                    and self.active_build_generation == self.generation
                    and self.active_build_mode == mode):
                if request_id not in self.active_build_request_ids:
                    self.active_build_request_ids.append(request_id)
                self.build_requests[request_id] = "building"
            else:
                if request_id not in self.pending_build_request_ids:
                    self.pending_build_request_ids.append(request_id)
                self.pending_build_request_modes[request_id] = mode
                self.pending_build_mode = mode
                self.build_requests[request_id] = "queued"
                self.build_pending = True
                if self.active_build_generation is None:
                    self.build_status = "pending"
            self.build_requests = dict(list(self.build_requests.items())[-32:])
        return True

    def _queue_build_if_needed(self) -> None:
        with self.lock:
            if (self.active_build_generation is None
                    or self.active_build_generation != self.generation):
                self.build_pending = True
                if self.active_build_generation is None:
                    self.build_status = "pending"

    def process_control_requests(self) -> None:
        while True:
            try:
                request = self.control_requests.get_nowait()
            except Empty:
                return
            action = str(request.get("action", "")).strip().lower()
            request_id = str(request.get("request_id", ""))
            print(f"[DEV CONTROL] request={request_id or '(none)'} action={action}")
            if action in ("launch", "build-or-launch") and not self.events_path:
                self._ensure_run_layout()
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
                self.override_map = str(request.get("map", "") or "").strip()
                self.override_weapon_set = str(request.get("weapon_set", "") or "").strip()
                if self.latest_build is None:
                    self.launch_after_build = True
                    self._queue_build_if_needed()
                    self.last_message = f"AI launch waiting for first successful build, mode {self.launch_mode_id}"
                    self.record_control_event(request, "accepted", self.last_message)
                    self.save_state("launch_waiting_for_build")
                else:
                    launched = self.launch_for_control_request(request)
                    if not launched:
                        self.last_message = self.last_message or (
                            f"AI launch failed for last successful build {self.latest_build}"
                        )
                    self.record_control_event(
                        request, "completed" if launched else "failed", self.last_message
                    )
                    self.save_state("launch_latest" if launched else "launch_failed")
            elif action == "build":
                mode = str(request.get("build_mode", "debug"))
                if not self.queue_build_request(request_id, mode):
                    self.last_message = f"unsupported build mode: {mode}"
                    self.record_control_event(request, "rejected", self.last_message)
                    self.save_state("request_rejected")
                    continue
                self.last_message = "build request queued through dev-loop"
                self.record_control_event(request, "accepted", self.last_message)
                self.save_state("build_queued")
            elif action == "build-or-launch":
                if self.latest_build is None:
                    self.launch_after_build = True
                    self._queue_build_if_needed()
                    self.last_message = "dashboard launch waiting for first successful build"
                    self.record_control_event(request, "accepted", self.last_message)
                    self.save_state("launch_waiting_for_build")
                else:
                    launched = self.launch_for_control_request(request)
                    if not launched:
                        self.last_message = self.last_message or (
                            f"dashboard launch failed for build {self.latest_build}"
                        )
                    self.record_control_event(
                        request, "completed" if launched else "failed", self.last_message
                    )
                    self.save_state("launch_latest" if launched else "launch_failed")
            elif action == "set-launch-mode":
                requested_mode = request.get("launch_mode")
                try:
                    selected = load_launch_mode(str(requested_mode))
                except SystemExit as error:
                    self.last_message = str(error)
                    self.record_control_event(request, "rejected", self.last_message)
                    self.save_state("request_rejected")
                    continue
                self.launch_mode_id = str(requested_mode)
                self.launch_mode = selected
                self.last_message = (
                    f"launch mode {self.launch_mode_id} selected: "
                    f"{selected.get('name', 'unnamed')}"
                )
                self.record_control_event(request, "accepted", self.last_message)
                self.save_state("launch_mode_selected")
            elif action == "toggle-visibility":
                self.launch_visibility = (
                    "headless" if self.launch_visibility == "visible" else "visible"
                )
                self.last_message = (
                    f"launch visibility set to {self.launch_visibility}; applies to the next launch"
                )
                self.record_control_event(request, "accepted", self.last_message)
                self.save_state("launch_visibility_toggled")
            elif action == "toggle-auto-restart":
                self.auto_restart = not self.auto_restart
                self.last_message = (
                    "auto-start enabled" if self.auto_restart else "auto-start disabled"
                )
                self.record_control_event(request, "accepted", self.last_message)
                self.save_state("auto_start_toggled")
            elif action == "start-server":
                if self.server_health():
                    self.last_message = "server is already running"
                    self.record_control_event(request, "rejected", self.last_message)
                    self.save_state("request_rejected")
                    continue
                launched = self.launch_for_control_request(request)
                self.record_control_event(
                    request, "completed" if launched else "failed", self.last_message)
                self.save_state("launch_latest" if launched else "launch_failed")
            elif action == "start-client":
                name = str(request.get("name", "") or "").strip()
                if not name:
                    name = f"Client {len(self.client_instances) + 1}"
                record = self.start_client_instance(
                    name, str(request.get("badconn_preset", "") or ""),
                    str(request.get("instance_id", "") or "") or None,
                    str(request.get("server_instance_id", "") or "server-01"))
                if record:
                    self.record_control_event(request, "completed", self.last_message)
                else:
                    self.record_control_event(request, "rejected", self.last_message)
            elif action == "stop-instance":
                iid = str(request.get("instance_id", "") or "")
                if iid == "server-01":
                    self.stop_server()
                    stopped = True
                else:
                    stopped = self.stop_client_instance(iid)
                self.record_control_event(
                    request, "completed" if stopped else "rejected", self.last_message)
                self.save_state("instance_stopped" if stopped else "request_rejected")
            elif action == "stop-selected":
                targets = request.get("targets")
                if not isinstance(targets, list) or not targets:
                    self.last_message = "select at least one running MiMITA process"
                    request["results"] = []
                    self.record_control_event(request, "rejected", self.last_message)
                    self.save_state("request_rejected")
                    continue
                results = []
                for target in targets[:32]:
                    if not isinstance(target, dict):
                        continue
                    try:
                        if target.get("source") == "external":
                            stopped = self.stop_external_mimita_process(target)
                        else:
                            iid = str(target.get("instance_id") or "")
                            try:
                                expected_pid = int(target.get("pid", 0) or 0)
                            except (TypeError, ValueError):
                                expected_pid = 0
                            if iid == "server-01":
                                if expected_pid and expected_pid == self.server_pid:
                                    stopped = self.stop_server()
                                else:
                                    stopped = False
                                    self.last_message = (
                                        "refused to stop server: process identity changed")
                            else:
                                record = self.client_instances.get(iid)
                                process = record.get("process") if record else None
                                same_process = bool(process and process.pid == expected_pid
                                                    and process.poll() is None)
                                stopped = bool(same_process and self.stop_client_instance(iid))
                                if not same_process:
                                    self.last_message = f"refused to stop {iid}: process identity changed"
                    except (OSError, RuntimeError, subprocess.SubprocessError) as error:
                        stopped = False
                        self.last_message = f"stop failed: {error}"
                    results.append({
                        "instance_id": target.get("instance_id"),
                        "source": target.get("source", "managed"),
                        "pid": target.get("pid"),
                        "role": target.get("role"),
                        "executable": target.get("executable"),
                        "process_started_utc": target.get("process_started_utc"),
                        "stopped": bool(stopped),
                        "message": self.last_message,
                    })
                request["results"] = results
                successes = sum(1 for item in results if item["stopped"])
                failures = len(results) - successes
                self.last_message = (f"stopped {successes} of {len(results)} selected MiMITA processes"
                                     if not failures else
                                     f"stopped {successes} of {len(results)} selected MiMITA processes; "
                                     f"{failures} failed")
                self.record_control_event(
                    request, "completed" if not failures else "failed", self.last_message)
                self.save_state("selected_processes_stopped" if not failures
                                else "selected_process_stop_failed")
            elif action == "restart-instance":
                iid = str(request.get("instance_id", "") or "")
                if iid == "server-01":
                    restarted = self.launch_for_control_request(request)
                else:
                    restarted = self.restart_client_instance(iid)
                self.record_control_event(
                    request, "completed" if restarted else "rejected", self.last_message)
                self.save_state("instance_restarted" if restarted else "request_rejected")
            elif action == "stop-client":
                self.stop_processes()
                self.client_instances.clear()
                self.record_control_event(request, "completed", "client stopped")
                self.save_state("client_stopped")
            elif action == "command":
                self.queue_agent_command(request)
            elif action == "scenario-file":
                self.queue_agent_scenario(request)
            elif action == "stop-server":
                self.stop_server()
                self.record_control_event(request, "completed", "server stop requested")
                self.save_state("server_stopped")
            elif action == "stop":
                # An explicit dashboard/CLI stop records intent so the watchdog
                # does not immediately restart the daemon (spec section 19.4).
                # The watchdog's own replacement stop omits explicit_stop.
                if request.get("explicit_stop"):
                    write_stop_intent(str(request.get("requested_by") or "explicit"))
                self.stop_event.set()
                self.last_message = "stop requested by AI"
                self.record_control_event(request, "accepted", self.last_message)
                self.save_state("stop_requested")
            else:
                self.last_message = f"unknown AI action: {action}"
                self.record_control_event(request, "rejected", self.last_message)
                self.save_state("request_rejected")

    def watch(self) -> None:
        notifications = DirectoryChangeNotifications(WATCH_DIRECTORIES)
        notifications.start()

        def record_source_change() -> None:
            with self.lock:
                self.generation += 1
                self.latest_stale = self.latest_build is not None
                self.last_message = f"source changed, generation {self.generation}"
            self.change_event.set()
            self.save_state("source_changed")

        # Catch any input changed between daemon initialization and watcher start.
        current = snapshot_inputs()
        changed = current != self.last_snapshot
        self.last_snapshot = current
        if changed:
            record_source_change()
        while not self.stop_event.is_set():
            if notifications.available and not notifications.failed.is_set():
                directory_changed = notifications.changed.wait(POLL_SECONDS)
                notifications.changed.clear()
                explicit = snapshot_watch_files()
                changed = any(self.last_snapshot.get(path) != signature
                              for path, signature in explicit.items())
                self.last_snapshot.update(explicit)
                if directory_changed:
                    directories = snapshot_watch_directories()
                    previous = {path: signature for path, signature in self.last_snapshot.items()
                                if any(path.startswith(str(root) + os.sep)
                                       for root in WATCH_DIRECTORIES)}
                    changed = changed or previous != directories
                    for path in previous:
                        self.last_snapshot.pop(path, None)
                    self.last_snapshot.update(directories)
            else:
                if notifications.failed.is_set():
                    notifications.available = False
                if self.stop_event.wait(POLL_SECONDS):
                    break
                current = snapshot_inputs()
                changed = current != self.last_snapshot
                self.last_snapshot = current

            if changed:
                record_source_change()
                changed = False
        notifications.close()

    def start_build_worker(self) -> None:
        with self.lock:
            if self.active_build_generation is not None:
                return
            requested_generation = self.generation
            request_ids: list[str] = []
            if self.pending_build_request_ids:
                requested_mode = self.pending_build_request_modes.get(
                    self.pending_build_request_ids[0], "debug")
                request_ids = [request_id for request_id in self.pending_build_request_ids
                               if self.pending_build_request_modes.get(request_id, "debug")
                               == requested_mode]
                self.pending_build_request_ids = [request_id for request_id in self.pending_build_request_ids
                                                  if request_id not in request_ids]
                for request_id in request_ids:
                    self.pending_build_request_modes.pop(request_id, None)
            else:
                requested_mode = self.pending_build_mode
            self.active_build_request_ids = request_ids
            self.active_build_mode = requested_mode
            self.active_build_generation = requested_generation
            self.active_build_started_utc = utc_now()
            self.active_build_started_monotonic = time.monotonic()
            self.active_build_detail = "starting compiler"
            self.build_output_tail.clear()
            self.build_pending = False
            self.build_status = "building"
            self.last_message = f"building source generation {requested_generation}"
            for request_id in request_ids:
                self.build_requests[request_id] = "building"
        self.save_state("building")
        self.build_worker = threading.Thread(
            target=self.run_build,
            args=(requested_generation, request_ids, requested_mode),
            name=f"dev-loop-build-{requested_generation}",
            daemon=False,
        )
        self.build_worker.start()

    def _record_build_output(self, line: str) -> None:
        detail = line.rstrip()[-240:]
        now = time.monotonic()
        with self.lock:
            if detail:
                self.build_output_tail.append(detail)
                self.active_build_detail = detail
                self.last_message = detail
            save = now - self.build_last_progress_save >= 0.75
            if save:
                self.build_last_progress_save = now
        if save:
            self.save_state("building")

    def run_build(self, requested_generation: int, request_ids: list[str], mode: str) -> None:
        run_build_worker(
            self, requested_generation, request_ids, mode,
            root=ROOT,
            make_artifact_id=build_artifact_id,
            publish_build=publish_build,
            find_ccache=find_ccache,
            creationflags=_CREATE_NO_WINDOW,
            hidden_startupinfo=_hidden_startupinfo,
        )

    def drain_build_completions(self) -> None:
        while True:
            try:
                result = self.build_completions.get_nowait()
            except Empty:
                return

            with self.lock:
                generation = int(result["generation"])
                request_ids = list(dict.fromkeys(
                    [*result.get("request_ids", []), *self.active_build_request_ids]
                ))
                success = bool(result.get("success"))
                self.active_build_request_ids = []
                self.active_build_generation = None
                self.active_build_started_utc = None
                self.active_build_started_monotonic = None
                self.active_build_detail = ""
                self.last_build_duration_seconds = float(result.get("duration_seconds", 0))
                self.last_build_result = "success" if success else "failed"
                if success:
                    self.latest_build_mode = str(result.get("mode") or "debug")
                for request_id in request_ids:
                    self.build_requests[request_id] = self.last_build_result

                if success:
                    self.latest_build = str(result["artifact_id"])
                    self.latest_build_folder = str(result["folder_name"])
                    self.latest_generation = generation
                    self.latest_stale = self.generation != generation
                    self.last_message = (
                        f"published build {self.latest_build} at {result['destination']}"
                    )
                    self.build_status = (
                        "pending" if self.build_pending
                        else "published_stale" if self.latest_stale
                        else "published_current"
                    )
                else:
                    self.last_message = str(result.get("error") or
                                            f"build failed with exit code {result.get('return_code')}")
                    self.build_status = "pending" if self.build_pending else "failed"
                    if self.latest_build is None and self.launch_after_build:
                        self.launch_after_build = False

                self.build_requests = dict(list(self.build_requests.items())[-32:])
                self.build_pending = self.build_pending or bool(self.pending_build_request_ids)
                pending_launch = success and self.launch_after_build and not self.shutting_down
                auto_replace = bool(
                    success and not self.shutting_down and not pending_launch and self.auto_restart
                    and any(process.poll() is None for process in self.processes)
                    and not self.latest_stale
                )
                if pending_launch:
                    self.launch_after_build = False

            self.build_worker = None
            self.save_state("build_succeeded" if success else "build_failed")
            if success:
                print(f"[DEV] published build {self.latest_build}: {result['destination']}")
            else:
                print(f"[DEV] {self.last_message}; the last successful build is retained")

            if pending_launch or auto_replace:
                self.last_message = (
                    f"launching build {self.latest_build} after successful publication"
                )
                self.save_state("launch_after_build")
                self.launch_latest()
            else:
                self.print_status(force=True)

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

        # A launch mode can request the gamemode's own weapon-set default.
        # This matters when the selected dev profile is for another mode and
        # carries an explicit --weapon-set that would otherwise override the
        # launch mode's gamemode/role loadout.
        weapon_set = self.override_weapon_set or self.launch_mode.get(
            "weapon_set", profile.get("weapon_set", 1))
        use_weapon_set_override = str(weapon_set).strip().lower() != "auto"

        args = [
            str(exe), "--server", "--bind", server_bind,
            "--name", str(profile.get("server_name", "MiMITA Dev Server")),
            "--map", map_name,
            "--mode", str(profile.get("mode", "sandbox")),
            "--max-players", str(max_players),
            "--map-rotation-minutes", str(rotation_minutes),
            "--password-protected", password_protected,
            "--password", str(profile.get("password", "")),
            "--room-file", str(room_file_path),
        ]
        if use_weapon_set_override:
            args.extend(["--weapon-set", str(weapon_set)])
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

    def launch_identity_error(self, server_args: list[str]) -> tuple[str, dict, dict]:
        """Compare effective launch values with the launch mode's expectations."""
        expected_keys = {
            "map": ("expect_map", "map"),
            "mode": ("expect_mode", "mode"),
            "gamemode": ("expect_gamemode", "gamemode"),
            "weapon_set": ("expect_weapon_set", "weapon_set"),
            "npcs": ("expect_npcs", "npcs"),
        }
        expected = {}
        for key, (launch_key, _) in expected_keys.items():
            if launch_key in self.launch_mode:
                expected[key] = self.launch_mode.get(launch_key)
        if not expected:
            return "", {}, {}
        actual = _effective_launch_identity(server_args)
        problems = []
        for key, want in expected.items():
            got = actual.get(key)
            if str(got) != str(want):
                problems.append(f"{key}: expected {want!r} got {got!r}")
        if not problems:
            return "", expected, actual
        return ("launch identity mismatch: " + "; ".join(problems),
                expected, actual)

    def validate_firewall_policy(self) -> bool:
        """Prevent dev servers from requesting a public inbound firewall rule."""
        bind = str(self.profile.get("server_bind", "127.0.0.1:1357")).strip()
        host = bind.rsplit(":", 1)[0].strip("[]").lower()
        loopback = host in {"127.0.0.1", "localhost", "::1"}
        if not loopback and not bool(self.profile.get("allow_external_bind", False)):
            self.firewall_result = "FIREWALL_POLICY_BLOCKED"
            self.last_message = (
                f"refusing non-loopback dev server bind {bind}; "
                "set allow_external_bind only for an intentional network test"
            )
            self.record_runtime_event(
                "dev-loop.firewall.preflight",
                {"bind": bind, "result": self.firewall_result},
                self.last_message,
            )
            self.save_state("firewall_policy_blocked")
            return False
        if not loopback:
            self.firewall_result = "EXTERNAL_BIND_REQUIRES_FIREWALL_REVIEW"
            self.record_runtime_event(
                "dev-loop.firewall.preflight",
                {"bind": bind, "result": self.firewall_result},
                "explicit external bind requested; firewall permission requires review",
            )
            return True
        self.firewall_result = "LOOPBACK_ONLY"
        self.record_runtime_event(
            "dev-loop.firewall.preflight",
            {"bind": bind, "result": self.firewall_result},
            "dev server restricted to loopback; no public inbound rule requested",
        )
        return True

    def server_health(self, fresh: bool = False) -> bool:
        """True when the durable external server process is still alive.

        ``fresh=True`` bypasses the short identity cache; use it right after a
        stop attempt so a just-killed process is not reported alive from a
        stale cached identity.
        """
        process = self.server_process
        if process is not None:
            return process.poll() is None
        if self.server_adopted and self.server_pid:
            identity = windows_process_identity(self.server_pid, fresh=fresh)
            return bool(
                identity
                and os.path.normcase(str(identity.get("executable", "")))
                == os.path.normcase(str(self.server_exe or ""))
                and "--server" in str(identity.get("command_line", "")).lower()
            )
        return False

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
        elif self.server_adopted and self.server_pid and self.server_health():
            identity = windows_process_identity(self.server_pid)
            owned = bool(
                identity
                and os.path.normcase(str(identity.get("executable", "")))
                == os.path.normcase(str(self.server_exe or ""))
                and "--server" in str(identity.get("command_line", "")).lower()
            )
            if not owned:
                stopped = False
                self.last_message = (
                    f"refused to stop unverified server pid={self.server_pid}"
                )
            else:
                result = run_hidden(
                    ["taskkill", "/PID", str(self.server_pid), "/T", "/F"],
                    capture_output=True, text=True, timeout=5, check=False,
                )
                stopped = result.returncode == 0
                # Drop the cached identity so the post-stop liveness check is
                # not answered from the pre-kill cache entry.
                _PROCESS_IDENTITY_CACHE.pop(self.server_pid, None)
        if process is not None and process.poll() is None:
            stopped = False
        if stopped and self.server_pid and self.server_health(fresh=True):
            stopped = False
        self.record_runtime_event(
            "dev-loop.process.stop_result",
            {
                "role": "server",
                "pid": self.server_pid,
                "executable": self.server_exe,
                "owned": bool(self.server_process is not None or self.server_adopted),
                "stopped": stopped,
            },
            "server replacement or explicit server-stop completed",
        )
        if stopped:
            try:
                marker = read_json(SERVER_MARKER_PATH)
            except (OSError, json.JSONDecodeError):
                marker = None
            try:
                marker_pid = int(marker.get("pid", 0) or 0) if isinstance(marker, dict) else 0
            except (TypeError, ValueError):
                marker_pid = 0
            if isinstance(marker, dict) and marker_pid == int(self.server_pid or 0):
                try:
                    SERVER_MARKER_PATH.unlink()
                except FileNotFoundError:
                    pass
        self.server_process = None
        self.server_pid = None
        self.server_exe = None
        self.server_build_id = ""
        self.server_local_port = 0
        self.server_args = []
        self.server_unavailable = True
        self.server_ready = False
        self.builds_match = False
        self.server_adopted = False
        # Keep the run layout and journals after an explicit server stop so the
        # dashboard can still show the stopped instance and its evidence.
        self.cleanup_room_file()
        if not stopped:
            self.last_message = f"refused to replace server pid={process.pid if process else '(none)'}; process remains alive"
            print(f"[DEV SERVER] {self.last_message}")
        return stopped

    def wait_for_logger_started(self, role: str, pid: int, timeout: float = 20.0,
                                process: subprocess.Popen | None = None) -> bool:
        """Confirm the exact child wrote its own logger.started event."""
        if not self.events_path:
            return False
        deadline = time.time() + timeout
        while time.time() < deadline:
            if process is not None and process.poll() is not None:
                self.last_message = (
                    f"{role} pid={pid} exited before logger.started "
                    f"(exit_code={process.returncode})"
                )
                return False
            for journal_path in self._journal_read_paths():
                try:
                    with open(journal_path, "r", encoding="utf-8") as journal:
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
                    pass
            time.sleep(0.1)
        return False

    def wait_for_server_local_gameplay(self, pid: int, timeout: float = 60.0) -> int:
        """Return the bound UDP port as soon as authoritative local play is ready."""
        if not self.events_path:
            return 0
        deadline = time.time() + timeout
        while time.time() < deadline:
            process = self.server_process
            if process is not None and process.poll() is not None:
                return 0
            for journal_path in self._journal_read_paths():
                try:
                    with open(journal_path, "r", encoding="utf-8") as journal:
                        for line in journal:
                            try:
                                event = json.loads(line)
                            except json.JSONDecodeError:
                                continue
                            if (event.get("event") != "network.server.local-gameplay-ready" or
                                    event.get("pid") != pid):
                                continue
                            port = int(event.get("fields", {}).get("port", 0) or 0)
                            if 0 < port <= 65535:
                                return port
                except OSError:
                    pass
            time.sleep(0.05)
        return 0

    def server_matches_executable(self, exe: Path) -> bool:
        if not self.server_health():
            self.build_mismatch_reason = "server_not_alive"
            return False
        if not self.server_exe:
            self.build_mismatch_reason = "server_executable_unknown"
            return False
        try:
            matches = Path(self.server_exe).resolve() == exe.resolve()
        except OSError:
            matches = os.path.normcase(self.server_exe) == os.path.normcase(str(exe))
        if not matches:
            self.build_mismatch_reason = "server_executable_build_number_or_hash"
        return matches

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

    def server_matches_launch_configuration(
        self, exe: Path, map_name: str, expected_build_id: str = ""
    ) -> bool:
        """Ensure a durable server belongs to the currently selected mode/profile."""
        if not self.server_matches_executable(exe):
            return False
        if expected_build_id and self.server_build_id != expected_build_id:
            self.build_mismatch_reason = "server_build_identity"
            return False
        desired = self.build_server_args(exe, map_name, Path("<room-file>"))
        matches = self._normalized_server_args(self.server_args) == self._normalized_server_args(desired)
        if not matches:
            self.build_mismatch_reason = "server_launch_configuration"
        return matches

    def check_server_after_client(self) -> None:
        """Observe and repair durable-server state after a client exits.

        The server is never terminated here; only its liveness is checked and a
        stale room code/file is cleared if it died unexpectedly.
        """
        if self.server_process is None:
            if self.server_adopted and self.server_health():
                print(f"[DEV SERVER] client exited; adopted server retained pid={self.server_pid} "
                      f"room={self.room_code}")
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
        self.server_build_id = ""
        self.server_local_port = 0
        self.server_args = []
        self.room_code = None
        self.server_unavailable = True
        self.cleanup_room_file()
        self.save_state("server_exited")

    def launch_latest(self, request_id: str = "") -> bool:
        launch_started = time.monotonic()
        self.last_message = ""
        self.reap_stray_mimita_processes()
        # A new launch owns a fresh set of clients. Never leave an old
        # client-02 writing into the previous run after the server is replaced.
        self.stop_processes()
        self.client_instances = {}
        # Every explicit launch starts a fresh run folder with one journal per
        # executable. Reusing a full/capped journal is what made logger.started
        # disappear and readiness time out while the replacement server stayed
        # alive.
        self.run_layout = None
        self._ensure_run_layout()
        self.launch_id = utc_now()
        self.run_started_utc = utc_now()
        self.run_metadata = {}
        self.client_instances = {}
        self.launch_executable = None
        launch_journal_path = self.events_path
        launch_run_id = self.run_id
        build_id = ""
        artifact_id = ""
        map_name = ""

        def stage(name: str, result: str, reason: str = "",
                  stage_started: float | None = None, **details) -> None:
            now = time.monotonic()
            self.record_runtime_event(
                "dev-loop.launch.stage",
                {
                    "stage": name,
                    "result": result,
                    "request_id": request_id,
                    "run_id": launch_run_id,
                    "build_number": (int(selected_build)
                                     if str(selected_build or "").isdigit() else None),
                    "artifact_id": artifact_id,
                    "build_id": build_id,
                    "launch_mode": self.launch_mode_id,
                    "map": map_name,
                    "elapsed_ms": round((now - launch_started) * 1000.0, 1),
                    "stage_elapsed_ms": round(
                        (now - (stage_started if stage_started is not None else launch_started))
                        * 1000.0, 1),
                    **details,
                },
                reason or f"dev-loop launch stage {name} {result}",
                events_path=launch_journal_path,
                run_id=launch_run_id,
            )

        def stop_server_for_launch() -> bool:
            stopped = self.stop_server()
            # stop_server resets its active-run state for standalone stop
            # requests. Keep this attempt's journal alive through replacement.
            self.events_path = launch_journal_path
            self.run_id = launch_run_id
            return stopped

        selection = select_launch_artifact(
            self.latest_build_folder, BUILD_ROOT, executable_identity)
        selected_build = selection["folder"]
        identity = selection["identity"]
        fell_back = bool(selection["fallback"])
        newest_validation_error = str(selection["validation_error"])
        if selected_build is None or not identity or not identity.get("valid"):
            self.last_message = (
                f"no valid published build; newest artifact "
                f"{self.latest_build or self.latest_build_folder or '(none)'} failed validation: "
                f"{newest_validation_error}"
                if newest_validation_error else
                "no successfully published build is available to launch"
            )
            stage("artifact_selection", "failed", self.last_message)
            stage("complete", "failed", self.last_message)
            return False
        build_dir = BUILD_ROOT / selected_build
        exe = build_dir / "mimita.exe"
        artifact_id = str(identity.get("artifact_id") or selected_build)
        if fell_back:
            self.last_message = (
                f"newest published build {self.latest_build} failed validation "
                f"({newest_validation_error or 'artifact identity invalid'}); "
                f"using previous valid build {artifact_id}"
            )
        if not exe.is_file():
            self.last_message = f"published executable is missing: {exe}"
            stage("artifact_validation", "failed", self.last_message,
                  artifact_executable=str(exe))
            stage("complete", "failed", self.last_message)
            self.save_state("launch_artifact_missing")
            print(f"[DEV] {self.last_message}")
            return False
        build_id = self.build_id(identity)
        launch_exe = STABLE_LAUNCH_EXE
        self.launch_executable = str(launch_exe)
        stage("artifact_selection", "selected",
              "newest valid published artifact selected" if not fell_back
              else "newest published artifact invalid; previous valid artifact selected",
              artifact_executable=str(exe), launch_executable=str(launch_exe),
              artifact_id=artifact_id,
              fallback=fell_back,
              source_generation=identity.get("source_generation", self.latest_generation),
              source_current=self.latest_generation == self.generation,
              build_pending=bool(self.build_pending or self.active_build_generation is not None))
        stage("artifact_validation", "succeeded", "published artifact identity is valid",
              artifact_executable=str(exe), sha256=identity.get("sha256", ""))

        map_override = self.override_map
        weapon_override = self.override_weapon_set
        map_name = select_dev_map(
            self.profile,
            map_override or str(self.launch_mode.get("map", "")),
        )
        self.override_map = ""
        stage("launch_configuration", "resolved", "launch mode and map resolved",
              mode_name=self.launch_mode.get("name", "unnamed"))
        if (
            not map_override and not weapon_override
            and self.client_args and self.client_args[0] == str(launch_exe)
            and any(process.poll() is None for process in self.processes)
            and self.client_ready and self.builds_match
            and self.server_build_id == build_id
            and stable_launch_matches(exe)
            and self.server_health()
            and self.server_matches_launch_configuration(launch_exe, map_name, build_id)
        ):
            self.last_message = f"last successful build {selected_build} is already running"
            stage("process_reuse", "reused", self.last_message,
                  server_pid=self.server_pid,
                  client_pids=[p.pid for p in self.processes if p.poll() is None])
            stage("complete", "succeeded", self.last_message)
            self.save_state("launch_reused")
            self.print_status()
            return True

        print(f"[DEV] selected allowed map: {map_name}")
        stage_started = time.monotonic()
        if not self.validate_firewall_policy():
            stage("firewall_preflight", "failed", self.last_message or self.firewall_result,
                  stage_started, firewall_result=self.firewall_result)
            return False
        stage("firewall_preflight", "passed", self.firewall_result, stage_started,
              firewall_result=self.firewall_result)
        stage_started = time.monotonic()
        try:
            staged_launch = stage_stable_launch(exe, str(identity.get("sha256", "")))
        except (OSError, RuntimeError) as error:
            self.last_message = f"could not stage stable launch path: {error}"
            stage("stable_launch_stage", "failed", self.last_message, stage_started)
            self.save_state("stable_launch_stage_failed")
            print(f"[DEV] {self.last_message}")
            return False
        stage("stable_launch_stage", "succeeded", "stable launch bundle staged",
              stage_started, staged_path=str(staged_launch))
        # Reject an unsafe launch configuration before stopping the current
        # client. A failed preflight must not turn a launch request into a game
        # shutdown with no replacement.
        # Capture one immutable published artifact. A worker may publish a newer
        # build concurrently, but this launch keeps one server/client identity.
        self.stop_processes()
        stage("client_reconciliation", "completed", "previous dev-loop clients stopped")
        self.client_ready = False
        self.builds_match = False
        running_map = self.server_map_name()
        self.observe_launch_windows("before_server_launch")
        if self.server_health() and not self.server_matches_launch_configuration(
                launch_exe, map_name, build_id):
            print(f"[DEV SERVER] launch configuration changed; replacing old server "
                  f"pid={self.server_pid} old_map={running_map} new_map={map_name}")
            self.record_runtime_event(
                "dev-loop.process.replace_requested",
                {
                    "role": "server",
                    "pid": self.server_pid,
                    "old_executable": self.server_exe,
                    "new_executable": str(launch_exe),
                    "new_build_id": build_id,
                    "reason": self.build_mismatch_reason,
                },
                "owned server does not match the selected build or launch configuration",
            )
            if not stop_server_for_launch():
                self.last_message = self.last_message or "could not stop the previous owned server"
                stage("server_reconciliation", "failed", self.last_message,
                      server_pid=self.server_pid)
                shutil.rmtree(staged_launch, ignore_errors=True)
                self.save_state("server_stop_failed")
                return False
            stage("server_reconciliation", "completed", "previous owned server stopped")
        if not self.server_health() and not stable_launch_matches(exe):
            try:
                activate_stable_launch(staged_launch)
            except (OSError, RuntimeError) as error:
                shutil.rmtree(staged_launch, ignore_errors=True)
                self.last_message = f"could not activate stable launch path: {error}"
                stage("stable_launch_activation", "failed", self.last_message)
                self.save_state("stable_launch_activate_failed")
                print(f"[DEV] {self.last_message}")
                return False
            stage("stable_launch_activation", "succeeded", "stable launch path activated")
        else:
            if stable_launch_matches(exe):
                shutil.rmtree(staged_launch, ignore_errors=True)
        if self.server_health() and not self.room_code and not self.server_local_port:
            print(f"[DEV SERVER] live server has no saved room or local port; replacing pid={self.server_pid}")
            if not stop_server_for_launch():
                self.last_message = self.last_message or "could not stop server without a saved room or local port"
                stage("server_reconciliation", "failed", self.last_message,
                      server_pid=self.server_pid)
                shutil.rmtree(staged_launch, ignore_errors=True)
                self.save_state("server_stop_failed")
                return False
            if not stable_launch_matches(exe):
                try:
                    activate_stable_launch(staged_launch)
                except (OSError, RuntimeError) as error:
                    shutil.rmtree(staged_launch, ignore_errors=True)
                    self.last_message = f"could not activate stable launch path: {error}"
                    stage("stable_launch_activation", "failed", self.last_message)
                    self.save_state("stable_launch_activate_failed")
                    print(f"[DEV] {self.last_message}")
                    return False
                stage("stable_launch_activation", "succeeded", "stable launch path activated")
        if self.server_health() and (bool(self.room_code) or self.server_local_port):
            shutil.rmtree(staged_launch, ignore_errors=True)
            room_code = self.room_code
            local_server_port = self.server_local_port
            stage("server_reuse", "reused", "owned server already matches launch configuration",
                  server_pid=self.server_pid, local_port=local_server_port,
                  room_code_available=bool(room_code))
            print(
                f"[DEV SERVER] reusing pid={self.server_pid} room={room_code} alive=1"
            )
        else:
            # A dead server's room code must never be reused.
            self.clear_server_marker()
            if self.server_process is not None:
                code = self.server_process.poll()
                print(f"[DEV SERVER] unexpected exit code={code} pid={self.server_pid} "
                      f"room={self.room_code}")
            self.server_process = None
            self.server_pid = None
            self.server_exe = None
            self.server_build_id = ""
            self.server_local_port = 0
            self.server_args = []
            self.server_adopted = False
            self.room_code = None
            self.server_unavailable = True
            self.cleanup_room_file()
            room_fd, room_file_name = tempfile.mkstemp(prefix="mimita-dev-room-", suffix=".txt")
            os.close(room_fd)
            self.room_file_path = Path(room_file_name)
            self.room_file_path.write_text("", encoding="utf-8")
            server_args = self.build_server_args(launch_exe, map_name, self.room_file_path)
            identity_error, expected_identity, actual_identity = self.launch_identity_error(server_args)
            if identity_error:
                self.last_message = identity_error
                self.record_runtime_event(
                    "dev-loop.launch.identity-mismatch",
                    {"expected": expected_identity, "actual": actual_identity},
                    identity_error,
                )
                self.save_state("launch_identity_mismatch")
                print(f"[DEV LAUNCH] BLOCKED {identity_error}", flush=True)
                return False
            if not self.events_path:
                self._ensure_run_layout()
            launch_journal_path = self.events_path
            launch_run_id = self.run_id
            self.command_queue_path = None
            # The first prelaunch snapshot above may predate creation of the
            # canonical journal. Record the same state again once the journal
            # exists so every fresh launch has a durable before-launch record.
            prelaunch_windows = self.observe_launch_windows("before_server_launch")
            self.run_started_utc = utc_now()
            self.current_map = map_name
            self.server_ready = False
            self.client_ready = False
            self.builds_match = False
            metadata = {
                "run_id": self.run_id,
                "build_number": int(selected_build) if selected_build.isdigit() else None,
                "artifact_id": artifact_id,
                "artifact_folder": selected_build,
                "build_id": build_id,
                "server_build_id": build_id,
                "client_build_id": build_id,
                "build_identity": identity,
                "source_generation": identity.get("source_generation", self.latest_generation),
                "source_current": self.latest_generation == self.generation,
                "launch_mode": self.launch_mode_id,
                "launch_mode_name": self.launch_mode.get("name", "unnamed"),
                "map": map_name,
                "events_path": self.events_path,
                "run_dir": str(self.run_layout.run_dir) if self.run_layout else None,
                "manifest_path": self.manifest_path,
                "server_journal_path": self.server_journal_path,
                "client_journal_path": self.client_journal_path,
                "executable": str(launch_exe),
                "artifact_executable": str(exe),
                "started_utc": self.run_started_utc,
            }
            self.run_metadata = metadata
            common_env = {
                **os.environ,
                "MIMITA_EVENTS_FILE": self.server_journal_path or self.events_path,
                "MIMITA_INSTANCE_ID": "server-01",
                "MIMITA_LAUNCH_ID": self.launch_id or "",
                "MIMITA_RUN_ID": self.run_id,
                "MIMITA_BUILD_ID": build_id,
                "MIMITA_RUN_METADATA": json.dumps(metadata, separators=(",", ":")),
            }
            if (self.profile.get("ice_force_relay", False) or
                    self.launch_mode.get("ice_force_relay", False)):
                common_env["MIMITA_ICE_FORCE_RELAY"] = "1"
            server_env = {**common_env, "MIMITA_PROCESS_ROLE": "server"}
            self.save_state("launching_server")
            version_env = {**common_env, "MIMITA_PROCESS_ROLE": "preflight"}
            try:
                stage_started = time.monotonic()
                version = run_hidden(
                    [str(launch_exe), "--versioninfo"], cwd=ROOT, env=version_env,
                    capture_output=True, text=True, timeout=20, check=False,
                )
                print(f"[DEV] versioninfo exit={version.returncode}")
                stage("versioninfo", "succeeded" if version.returncode == 0 else "failed",
                      f"versioninfo exited with code {version.returncode}", stage_started,
                      return_code=version.returncode, journal_path=self.events_path)
                if version.stdout:
                    print(version.stdout, end="")
                if version.stderr:
                    print(version.stderr, end="", file=sys.stderr)
            except (OSError, subprocess.TimeoutExpired) as error:
                self.last_message = f"versioninfo failed: {error}"
                stage("versioninfo", "failed", self.last_message,
                      journal_path=self.events_path)
                stage("complete", "failed", self.last_message)
                self.save_state("versioninfo_failed")
                print(f"[DEV] {self.last_message}")
                return False
            print(f"[DEV] launching build {selected_build} server")
            print(f"[DEV] server journal: {self.server_journal_path or self.events_path}")
            self.record_runtime_event(
                "dev-loop.launch.args",
                {
                    "role": "server",
                    "executable": str(launch_exe),
                    "args": list(server_args),
                    "console_policy": "CREATE_NO_WINDOW",
                    "server_bind": next(
                        (server_args[index + 1] for index, value in enumerate(server_args[:-1])
                         if value == "--bind"),
                        "",
                    ),
                },
                "dev-loop launching dedicated server with hidden console policy",
            )
            stage_started = time.monotonic()
            try:
                server = subprocess.Popen(
                    server_args,
                    cwd=ROOT,
                    env=server_env,
                    creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
                )
            except OSError as error:
                self.last_message = f"could not start server process: {error}"
                stage("server_process_start", "failed", self.last_message, stage_started)
                stage("complete", "failed", self.last_message)
                self.save_state("server_process_start_failed")
                return False
            self.server_process = server
            self.server_pid = server.pid
            self.server_exe = str(launch_exe)
            self.server_build_id = build_id
            self.server_args = list(server_args)
            self.server_adopted = False
            self.server_launch_ms = time.time() * 1000.0
            self.server_unavailable = False
            stage("server_process_start", "started", "server process created",
                  stage_started, server_pid=server.pid)
            stage_started = time.monotonic()
            self.server_ready = self.wait_for_logger_started(
                "server", server.pid, process=server)
            stage("server_logger_readiness", "ready" if self.server_ready else "failed",
                  self.last_message if not self.server_ready else "server logger.started observed",
                  stage_started, server_pid=server.pid, exit_code=server.returncode,
                  timeout_seconds=20)
            self.observe_launch_windows("server_ready" if self.server_ready else "server_logger_missing")
            if not self.server_ready:
                if not self.last_message or not self.last_message.startswith(
                        f"server pid={server.pid} exited before logger.started"):
                    self.last_message = (
                        f"server pid={server.pid} stayed alive but did not write "
                        "logger.started before the readiness timeout"
                    )
                self.record_runtime_event(
                    "dev-loop.server.startup_failed",
                    {
                        "pid": server.pid,
                        "executable": str(launch_exe),
                        "artifact_executable": str(exe),
                        "exit_code": server.returncode,
                        "failure": self.last_message,
                        "journal_path": self.events_path,
                    },
                    "server failed the logger.started readiness check",
                )
                stage("complete", "failed", self.last_message, server_pid=server.pid)
                self.save_state("server_logger_missing")
                if not stop_server_for_launch():
                    print(f"[DEV SERVER] {self.last_message}; server could not be stopped")
                return False
            print(f"[DEV SERVER] launched pid={server.pid} args={' '.join(server_args[1:])}")
            self.write_server_marker()
            self.server_ready = True
            local_server_port = self.wait_for_server_local_gameplay(server.pid)
            if not local_server_port:
                self.last_message = "server did not become locally playable before the readiness timeout"
                stage("server_gameplay_readiness", "failed", self.last_message,
                      server_pid=server.pid, timeout_seconds=60)
                stage("complete", "failed", self.last_message, server_pid=server.pid)
                self.save_state("server_local_gameplay_timeout")
                print(f"[DEV] {self.last_message}")
                stop_server_for_launch()
                return False
            self.server_local_port = local_server_port
            stage("server_gameplay_readiness", "ready", "server bound a local gameplay port",
                  server_pid=server.pid, local_port=local_server_port)
            self.write_server_marker()
            room_code = ""

        join_via_room_code = bool(self.profile.get("join_via_room_code", False) or
                                  self.launch_mode.get("join_via_room_code", False))
        if join_via_room_code and not room_code:
            stage_started = time.monotonic()
            stage("server_room_registration", "waiting",
                  "waiting for coordinator room code before client launch",
                  stage_started, server_pid=self.server_pid,
                  room_file=str(self.room_file_path or ""))
            room_code = self.wait_for_room_code(self.server_pid,
                                                self.room_file_path,
                                                timeout=60.0)
            if room_code:
                self.record_runtime_event(
                    "dev-loop.room-code.published",
                    {"server_pid": self.server_pid, "room_code": room_code,
                     "room_file": str(self.room_file_path)},
                    "coordinator room code received before room-code client launch",
                )
                print(f"[DEV] ROOM CODE: {room_code}")
            stage("server_room_registration",
                  "ready" if room_code else "failed",
                  "coordinator room code ready" if room_code
                  else "coordinator room code did not arrive before the timeout",
                  stage_started, server_pid=self.server_pid,
                  timeout_seconds=60)
            if not room_code:
                self.last_message = "room-code join requested but no room code arrived"
                stage("complete", "failed", self.last_message,
                      server_pid=self.server_pid)
                self.save_state("room_code_timeout")
                stop_server_for_launch()
                return False

        client_args = [str(launch_exe)]
        if room_code:
            client_args.extend(["--room", room_code])
        else:
            client_args.extend(["--connect", f"127.0.0.1:{local_server_port}"])
            if self.room_file_path is not None:
                client_args.extend(["--room-code-file", str(self.room_file_path)])
        configured_clients = list(self.launch_mode.get("clients") or [])
        primary_client_cfg = configured_clients[0] if configured_clients else {}
        primary_client_name = str(
            primary_client_cfg.get("name")
            or self.profile.get("client_name", "NPC Dev"))
        client_args.extend([
            "--map", map_name,
            "--name", primary_client_name,
            "--unlock-cursor",
        ])
        primary_badconn = str(primary_client_cfg.get("badconn_preset", "") or "")
        if primary_badconn:
            client_args.extend(["--badconn-preset", primary_badconn])
        client_args.extend(str(value) for value in self.launch_mode.get("client_args", []))
        if not self.events_path:
            self._ensure_run_layout()
        if not self.run_id:
            self.run_id = self.run_layout.run_id if self.run_layout else None
        if self.run_metadata.get("build_id") != build_id:
            self.run_started_utc = self.run_started_utc or utc_now()
            self.run_metadata = {
                "run_id": self.run_id,
                "build_number": int(selected_build) if selected_build.isdigit() else None,
                "artifact_id": artifact_id,
                "artifact_folder": selected_build,
                "build_id": build_id,
                "server_build_id": build_id,
                "client_build_id": build_id,
                "build_identity": identity,
                "source_generation": self.latest_generation,
                "source_current": self.latest_generation == self.generation,
                "launch_mode": self.launch_mode_id,
                "launch_mode_name": self.launch_mode.get("name", "unnamed"),
                "map": map_name,
                "events_path": self.events_path,
                "executable": str(launch_exe),
                "artifact_executable": str(exe),
                "started_utc": self.run_started_utc,
            }
        else:
            self.run_metadata["server_build_id"] = build_id
            self.run_metadata["client_build_id"] = build_id
            self.run_metadata["artifact_id"] = artifact_id
            self.run_metadata["artifact_folder"] = selected_build
            self.run_metadata["build_identity"] = identity
            self.run_metadata["executable"] = str(launch_exe)
            self.run_metadata["artifact_executable"] = str(exe)
        if not self.command_queue_path:
            run_key = self.run_id or Path(self.events_path).parent.name
            self.command_queue_path = str(DEV_ROOT / f"agent-command-{run_key}.jsonl")
            Path(self.command_queue_path).write_text("", encoding="utf-8")
        client_args.extend(["--agent-command-file", self.command_queue_path])
        # The client may run generic leave/queue cleanup that is allowed to
        # stop a server it launched through the GUI. This server belongs to
        # the dev-loop instead, so the client must never claim ownership of it.
        client_env = {
            **os.environ,
            "MIMITA_EVENTS_FILE": self.client_journal_path or self.events_path,
            "MIMITA_INSTANCE_ID": "client-01",
            "MIMITA_LAUNCH_ID": self.launch_id or "",
            "MIMITA_DEV_LOOP_SERVER": "1",
            "MIMITA_RUN_ID": self.run_id or Path(self.events_path).parent.name,
            "MIMITA_EXPECTED_RUN_ID": self.run_id or Path(self.events_path).parent.name,
            "MIMITA_EXPECTED_BUILD_ID": build_id,
            "MIMITA_BUILD_ID": build_id,
            "MIMITA_RUN_METADATA": json.dumps(self.run_metadata, separators=(",", ":")),
            "MIMITA_PROCESS_ROLE": "client",
        }
        print(f"[DEV] launching build {selected_build} client")
        print(f"[DEV] client journal: {self.client_journal_path or self.events_path}")
        self.record_runtime_event(
            "dev-loop.launch.args",
            {
                "role": "client",
                "executable": str(launch_exe),
                "artifact_executable": str(exe),
                "args": list(client_args),
                "console_policy": "CREATE_NO_WINDOW",
                "window_policy": self.launch_visibility,
                "unlock_cursor_requested": "--unlock-cursor" in client_args,
            },
            "dev-loop launching client with hidden console and unlocked-cursor policy",
        )
        stage_started = time.monotonic()
        try:
            client = subprocess.Popen(
                client_args,
                cwd=ROOT,
                env=client_env,
                creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
            )
        except OSError as error:
            self.last_message = f"could not start client process: {error}"
            stage("client_process_start", "failed", self.last_message, stage_started)
            stage("complete", "failed", self.last_message, server_pid=self.server_pid)
            self.save_state("client_process_start_failed")
            return False
        self.processes.append(client)
        self.client_instances["client-01"] = {
            "instance_id": "client-01",
            "name": primary_client_name,
            "process": client,
            "pid": client.pid,
            "journal_path": self.client_journal_path,
            "command_queue": self.command_queue_path,
            "badconn_preset": primary_badconn,
            "server_instance_id": "server-01",
            "started_ms": time.time() * 1000.0,
        }
        self.client_args = list(client_args)
        self.client_launch_ms = time.time() * 1000.0
        self.running_build = selected_build
        self.run_metadata["client_build_id"] = build_id
        stage("client_process_start", "started", "client process created",
              stage_started, client_pid=client.pid)
        stage_started = time.monotonic()
        self.client_ready = self.wait_for_logger_started("client", client.pid,
                                                         process=client)
        if not self.client_ready and not self.last_message:
            self.last_message = (
                f"client pid={client.pid} stayed alive but did not write logger.started "
                "before the readiness timeout"
            )
        stage("client_logger_readiness", "ready" if self.client_ready else "failed",
              self.last_message if not self.client_ready else "client logger.started observed",
              stage_started, client_pid=client.pid, exit_code=client.returncode,
              timeout_seconds=20, journal_path=self.events_path)
        # Start extra configured clients after the primary logger is ready but
        # before connection/mode readiness waits. The server/room is known by
        # then, and both humans can still join before the round roster locks.
        if len(configured_clients) > 1:
            for index, client_cfg in enumerate(configured_clients[1:], start=2):
                extra_name = str(client_cfg.get("name") or f"Client {index}")
                extra_preset = str(client_cfg.get("badconn_preset", "") or "")
                started = self.start_client_instance(extra_name, extra_preset)
                if started is None:
                    print(f"[DEV] additional client {extra_name} failed: {self.last_message}")
        self.builds_match = bool(self.server_ready and self.client_ready and
                                 self.server_exe == str(launch_exe) and
                                 identity.get("valid") and
                                 self.server_build_id == build_id and
                                 self.run_metadata.get("server_build_id") == build_id)
        if self.builds_match:
            self.build_mismatch_reason = ""
        if not self.builds_match and self.client_ready:
            self.build_mismatch_reason = self.build_mismatch_reason or (
                "server/client readiness or build identity did not match"
            )
            self.last_message = self.build_mismatch_reason

        connection_ready = True
        if self.client_ready and bool(
                self.profile.get("require_client_connection", False) or
                self.launch_mode.get("require_client_connection", False)):
            stage_started = time.monotonic()
            connected_event = self.wait_for_journal_event(
                "network.connection-state.changed", {client.pid},
                timeout=45.0, field_key="to", field_value="Connected")
            connection_ready = connected_event is not None
            if not connection_ready:
                self.last_message = (
                    f"client pid={client.pid} did not reach Connected before the timeout")
            stage("client_connection", "ready" if connection_ready else "failed",
                  self.last_message if not connection_ready else "client reached Connected",
                  stage_started, client_pid=client.pid, timeout_seconds=45)
        if connection_ready:
            self.apply_configured_client_teams(configured_clients)

        mode_ready = True
        if connection_ready and bool(
                self.profile.get("require_mode_start", False) or
                self.launch_mode.get("require_mode_start", False)):
            stage_started = time.monotonic()
            mode_event = self.wait_for_journal_event(
                "network.mode.start", {client.pid, self.server_pid},
                timeout=45.0, field_key="stage", field_value="active")
            mode_ready = mode_event is not None
            if not mode_ready:
                self.last_message = "managed mode did not become active before the timeout"
            stage("mode_start", "ready" if mode_ready else "failed",
                  self.last_message if not mode_ready else "managed mode reached ACTIVE",
                  stage_started, server_pid=self.server_pid, client_pid=client.pid,
                  timeout_seconds=45)

        npcwaves_ready = True
        if mode_ready and bool(
                self.profile.get("require_npcwaves_start", False) or
                self.launch_mode.get("require_npcwaves_start", False)):
            stage_started = time.monotonic()
            wave_event = self.wait_for_journal_event(
                "network.npcwaves.start", {client.pid, self.server_pid},
                timeout=45.0, field_key="stage", field_value="begin")
            npcwaves_ready = wave_event is not None
            if not npcwaves_ready:
                self.last_message = "NPC Waves did not begin before the timeout"
            stage("npcwaves_start", "ready" if npcwaves_ready else "failed",
                  self.last_message if not npcwaves_ready else "NPC Waves wave 1 began",
                  stage_started, server_pid=self.server_pid, client_pid=client.pid,
                  timeout_seconds=45)

        self.observe_launch_windows("client_ready" if self.client_ready else "client_logger_missing")
        if self.client_ready:
            self.apply_launch_visibility()
            self.restore_prelaunch_foreground(prelaunch_windows)
            self.monitor_launch_windows()
        launch_succeeded = bool(self.client_ready and self.builds_match and
                                connection_ready and mode_ready and npcwaves_ready)
        if launch_succeeded:
            self.last_message = (
                f"launched fallback build {artifact_id} (SHA-256 {identity.get('sha256', '')})"
                if fell_back else
                f"launched build {artifact_id} (SHA-256 {identity.get('sha256', '')})"
            )
        self.save_state("running" if launch_succeeded else
                        ("client_ready" if self.client_ready else "client_logger_missing"))
        self.print_status()
        stage("build_identity_match", "matched" if self.builds_match else "mismatch",
              self.build_mismatch_reason or "server and client use the selected build",
              server_pid=self.server_pid, client_pid=client.pid,
              server_build_id=self.server_build_id, selected_build_id=build_id,
              client_ready=self.client_ready, server_ready=self.server_ready)
        stage("complete", "succeeded" if launch_succeeded else "failed",
              "launch completed with matching ready server/client" if launch_succeeded
              else self.last_message or "launch did not reach a matching ready server/client",
              client_pid=client.pid, server_pid=self.server_pid)
        if self.client_ready and not room_code and self.room_file_path is not None:
            threading.Thread(
                target=self.wait_for_registered_room_code,
                args=(self.server_pid, self.room_file_path),
                daemon=True,
                name="dev-loop-room-code",
            ).start()
        return launch_succeeded

    def wait_for_room_code(self, server_pid: int | None,
                           room_file_path: Path | None, timeout: float = 60.0) -> str:
        """Wait for the server-written coordinator room code, bounded."""
        if room_file_path is None:
            return ""
        deadline = time.time() + timeout
        while time.time() < deadline:
            if server_pid is not None and self.server_pid != server_pid:
                return ""
            if server_pid is not None and not self.server_health():
                return ""
            try:
                room_code = room_file_path.read_text(encoding="utf-8").strip()
            except OSError:
                room_code = ""
            if room_code:
                self.room_code = room_code
                self.write_server_marker()
                return room_code
            time.sleep(0.1)
        return ""

    def wait_for_journal_event(self, event_name: str, pids: set[int],
                               timeout: float = 45.0,
                               field_key: str = "",
                               field_value: str = "") -> dict | None:
        """Wait for one matching event in any journal owned by this run."""
        if not self.events_path:
            return None
        deadline = time.time() + timeout
        while time.time() < deadline:
            for journal_path in self._journal_read_paths():
                for segment in _journal_segment_paths(journal_path):
                    try:
                        with open(segment, "r", encoding="utf-8", errors="replace") as journal:
                            for line in journal:
                                try:
                                    event = json.loads(line)
                                except json.JSONDecodeError:
                                    continue
                                if event.get("event") != event_name:
                                    continue
                                if pids and event.get("pid") not in pids:
                                    continue
                                fields = event.get("fields", {})
                                if field_key and fields.get(field_key) != field_value:
                                    continue
                                return event
                    except OSError:
                        pass
            time.sleep(0.1)
        return None

    def wait_for_registered_room_code(self, server_pid: int | None,
                                      room_file_path: Path) -> None:
        """Publish the coordinator code after the client is already playing."""
        deadline = time.time() + 120.0
        while time.time() < deadline:
            if self.server_pid != server_pid or not self.server_health():
                return
            try:
                room_code = room_file_path.read_text(encoding="utf-8").strip()
            except OSError:
                room_code = ""
            if room_code:
                self.room_code = room_code
                self.server_ready = True
                self.write_server_marker()
                self.record_runtime_event(
                    "dev-loop.room-code.published",
                    {"server_pid": server_pid, "room_code": room_code,
                     "room_file": str(room_file_path)},
                    "coordinator room code arrived after immediate local client launch",
                )
                print(f"[DEV SERVER] room={room_code} alive=1")
                print(f"[DEV] ROOM CODE: {room_code}")
                self.save_state("running")
                return
            time.sleep(0.1)

        self.record_runtime_event(
            "dev-loop.room-code.timeout",
            {"server_pid": server_pid, "room_file": str(room_file_path),
             "timeout_ms": 120000},
            "local gameplay continued but the coordinator room code did not arrive",
        )

    def _checkout_mimita_processes(self, *, strict: bool = False) -> list[dict]:
        """List checkout-owned MiMITA processes, optionally failing on scan errors."""
        return checkout_mimita_processes(strict=strict)

    def stop_external_mimita_process(self, target: dict) -> bool:
        """Stop one external checkout MiMITA process after revalidating identity."""
        try:
            pid = int(target.get("pid", 0) or 0)
        except (TypeError, ValueError):
            pid = 0
        expected_started = str(target.get("process_started_utc") or "")
        expected_executable = str(target.get("executable") or "")
        expected_role = str(target.get("role") or "")
        current = None
        if pid > 0:
            current = next((item for item in self._checkout_mimita_processes(strict=True)
                            if int(item.get("pid", 0) or 0) == pid), None)
        if current is None:
            self.last_message = f"MiMITA process pid={pid or '?'} already exited"
            return True

        command_line = str(current.get("command_line") or "")
        current_role = "server" if "--server" in command_line.lower().split() else "client"
        same_identity = (
            expected_role in ("server", "client")
            and expected_started
            and expected_started == str(current.get("process_started_utc") or "")
            and os.path.normcase(expected_executable)
            == os.path.normcase(str(current.get("executable") or ""))
            and current_role == expected_role
        )
        if not same_identity:
            self.last_message = f"refused to stop pid={pid}: process identity changed or is unverified"
            self.record_runtime_event(
                "dev-loop.process.external_stop_result",
                {"pid": pid, "executable": expected_executable,
                 "process_started_utc": expected_started, "role": expected_role,
                 "stopped": False}, self.last_message)
            return False

        result = run_hidden(
            ["taskkill", "/PID", str(pid), "/F"],
            capture_output=True, text=True, timeout=8, check=False,
        )
        _PROCESS_IDENTITY_CACHE.pop(pid, None)
        remaining = next((item for item in self._checkout_mimita_processes(strict=True)
                          if int(item.get("pid", 0) or 0) == pid
                          and str(item.get("process_started_utc") or "") == expected_started), None)
        stopped = remaining is None
        self.last_message = (f"stopped external {expected_role} pid={pid}" if stopped else
                             f"could not stop external {expected_role} pid={pid}: "
                             f"{(result.stderr or result.stdout or 'process remains alive').strip()[:180]}")
        self.record_runtime_event(
            "dev-loop.process.external_stop_result",
            {"pid": pid, "executable": expected_executable,
             "process_started_utc": expected_started, "role": expected_role,
             "taskkill_exit_code": result.returncode, "stopped": stopped},
            self.last_message)
        return stopped

    def reap_stray_mimita_processes(self) -> None:
        """Enforce the profile's process cap for this checkout.

        With the default cap of 1 the launch is a clean replacement: every
        previous mimita.exe for this checkout is terminated before the new
        server/client pair starts. Raising the cap is reserved for explicit
        multi-client testing.
        """
        if self.max_mimita_processes != 1:
            return
        processes = self._checkout_mimita_processes()
        killed = []
        for item in processes:
            try:
                pid = int(item.get("pid", 0) or 0)
            except (TypeError, ValueError):
                continue
            if pid <= 0 or pid == os.getpid():
                continue
            result = run_hidden(
                ["taskkill", "/PID", str(pid), "/T", "/F"],
                capture_output=True, text=True, timeout=8, check=False,
            )
            _PROCESS_IDENTITY_CACHE.pop(pid, None)
            killed.append({
                "pid": pid,
                "executable": item.get("executable", ""),
                "command_line": item.get("command_line", ""),
                "taskkill_rc": result.returncode,
            })
        self.record_runtime_event(
            "dev-loop.process.audit",
            {"max_mimita_processes": self.max_mimita_processes,
             "found": len(processes), "killed": killed},
            "enforced max mimita.exe process count for this checkout",
        )

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
        self.client_ready = False
        self.builds_match = False

    def maintain_process(self) -> None:
        had_clients = bool(self.processes)
        previous_client_pids = tuple(process.pid for process in self.processes)
        live = [process for process in self.processes if process.poll() is None]
        self.processes = live
        if previous_client_pids != tuple(process.pid for process in live):
            if not live:
                self.client_ready = False
                self.builds_match = False
            self.save_state("running" if live or self.server_health() else "stopped_waiting")
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

    def print_status(self, force: bool = False) -> None:
        snapshot = self.state(self.last_status_value)
        status_line = (
            snapshot.get("build_status"),
            self.running_build,
            self.latest_build,
            self.latest_stale,
            snapshot.get("run_alive"),
            tuple(snapshot.get("client_pids", [])),
            snapshot.get("server_pid"),
            int(snapshot.get("run_uptime_seconds") or 0),
            self.events_path,
            self.launch_visibility,
        )
        if not force and status_line == self.last_status_line:
            return
        self.last_status_line = status_line
        self._render_status(snapshot)

    @staticmethod
    def _duration_text(seconds: float | None) -> str:
        if seconds is None:
            return "-"
        total = max(0, int(seconds))
        hours, remainder = divmod(total, 3600)
        minutes, secs = divmod(remainder, 60)
        return f"{hours:02d}:{minutes:02d}:{secs:02d}"

    def _status_lines(self, snapshot: dict) -> list[str]:
        running = bool(snapshot.get("run_alive"))
        client_pids = snapshot.get("client_pids", [])
        lines = [
            f"[DEV STATUS] MODE: {snapshot.get('launch_mode')} ({self.launch_mode.get('name', 'unnamed')})",
            f"[DEV STATUS] BUILD: phase={snapshot.get('build_status')} active-source={snapshot.get('active_build_generation') or '-'} latest-id={snapshot.get('latest_successful_build_id') or '-'} stale={snapshot.get('latest_build_stale')} elapsed={self._duration_text(snapshot.get('active_build_elapsed_seconds'))}",
            f"[DEV STATUS] BUILD DETAIL: {snapshot.get('active_build_detail') or snapshot.get('last_build_result') or '-'}",
            f"[DEV STATUS] RUNTIME BUILD: {snapshot.get('running_build_id') or snapshot.get('running_build') or '-'} match={snapshot.get('builds_match')}",
            f"[DEV STATUS] RUN: {'RUNNING' if running else 'NOT RUNNING'} uptime={self._duration_text(snapshot.get('run_uptime_seconds'))} visibility={snapshot.get('launch_visibility')}",
            f"[DEV STATUS] PIDS: server={snapshot.get('server_pid') or '-'} client={client_pids or '-'}",
            f"[DEV STATUS] ROOM CODE: {snapshot.get('room_code') or 'waiting for coordinator'}",
            f"[DEV STATUS] MANIFEST: {snapshot.get('manifest_path') or '-'}",
        ]
        for inst in snapshot.get("instances", []):
            snapshot_age = inst.get("snapshot_age_ms")
            age_text = f"{snapshot_age / 1000.0:.1f}s" if snapshot_age is not None else "-"
            journal = str(inst.get("journal_path") or "-")
            lines.append(
                f"[DEV STATUS] {inst.get('instance_id')} {inst.get('role')} "
                f"{inst.get('state')} pid={inst.get('pid') or '-'} "
                f"snap={age_text} journal={journal}")
        lines.append(
            "[DEV STATUS] CONTROLS: [1] launch latest  [2] mode  [V] visible/headless  [A] auto-start  [Q] quit")
        return lines

    def _render_status(self, snapshot: dict | None = None) -> None:
        global _status_rendering
        if self.status_visible and _rainbow_enabled():
            sys.stdout.write(f"\x1b[{self.status_line_count}A\x1b[0J")
        lines = self._status_lines(snapshot or self.state())
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
                self.process_control_requests()
                self.drain_build_completions()
                self.maintain_process()
                if self.build_pending and self.active_build_generation is None:
                    self.change_event.clear()
                    time.sleep(DEBOUNCE_SECONDS)
                    self.start_build_worker()
                    continue
                if self.change_event.wait(0.20):
                    with self.lock:
                        self.build_pending = True
                        if self.active_build_generation is None:
                            self.build_status = "pending"
                self.print_status()
                if time.monotonic() - self.last_state_save_monotonic >= 1.0:
                    self.last_state_save_monotonic = time.monotonic()
                    self.save_state(self.last_status_value)
                self.refresh_status_animation()
        finally:
            self.stop_event.set()
            self.shutting_down = True
            if self.build_worker is not None and self.build_worker.is_alive():
                print("[DEV] waiting for the active build to finish before daemon shutdown")
                self.build_worker.join()
            self.drain_build_completions()
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


def write_stop_intent(reason: str) -> None:
    """Record that the dev-loop daemon was stopped on purpose."""
    try:
        DEV_ROOT.mkdir(parents=True, exist_ok=True)
        STOP_INTENT_PATH.write_text(json.dumps({
            "reason": reason,
            "requested_at": utc_now(),
            "pid": os.getpid(),
        }), encoding="utf-8")
    except OSError:
        pass


def clear_stop_intent() -> None:
    try:
        STOP_INTENT_PATH.unlink()
    except OSError:
        pass


def read_stop_intent() -> dict | None:
    try:
        value = json.loads(STOP_INTENT_PATH.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return None
    return value if isinstance(value, dict) else None


def _scenario_load(path_text: str) -> dict:
    path = Path(path_text).expanduser()
    if not path.is_absolute():
        path = ROOT / path
    text = path.read_text(encoding="utf-8")
    if "```json" in text:
        text = text.split("```json", 1)[1].split("```", 1)[0]
    value = json.loads(text.strip())
    if not isinstance(value, dict) or not value.get("id"):
        raise ValueError("scenario must be an object with a non-empty id")
    return value


def _scenario_input_path(scenario: dict) -> str:
    value = scenario.get("playback")
    if isinstance(value, str) and value.strip():
        return value.strip()
    for step in scenario.get("setup", []) + scenario.get("steps", []):
        if not isinstance(step, dict):
            continue
        if step.get("type") == "input_playback" or step.get("playback"):
            path = step.get("path", step.get("playback", ""))
            if isinstance(path, str) and path.strip():
                return path.strip()
    return ""


def _read_input_header(path_text: str) -> dict:
    path = Path(path_text).expanduser()
    if not path.is_absolute():
        path = ROOT / path
    try:
        with path.open("r", encoding="utf-8", errors="replace") as handle:
            for line in handle:
                line = line.strip()
                if not line:
                    continue
                value = json.loads(line)
                if isinstance(value, dict) and value.get("kind") == "header":
                    return value
                break
    except (OSError, json.JSONDecodeError):
        return {}
    return {}


def _map_basename(value: str) -> str:
    text = str(value or "").strip().replace("\\", "/")
    if not text:
        return ""
    name = text.rsplit("/", 1)[-1]
    if name.lower().endswith(".glb"):
        name = name[:-4]
    return name


def _server_weapon_set(current: dict):
    args = current.get("server_args")
    if not isinstance(args, list):
        return None
    value = None
    for index, arg in enumerate(args):
        text = str(arg)
        if text == "--weapon-set" and index + 1 < len(args):
            value = str(args[index + 1])
        elif text.startswith("--weapon-set="):
            value = text.split("=", 1)[1]
    return value


def _journal_segment_paths(events_path: str) -> list[Path]:
    if not events_path:
        return []
    active = Path(events_path)
    run_dir = active.parent
    segments: list[Path] = []
    try:
        segments = sorted(run_dir.glob("events-*.jsonl"),
                          key=lambda path: path.name)
    except OSError:
        segments = []
    if active.exists():
        segments.append(active)
    return segments


def _journal_record_key(record: dict) -> tuple:
    return (
        record.get("pid"),
        record.get("process"),
        record.get("event_id"),
        record.get("seq"),
        record.get("event"),
        record.get("request_id"),
    )


class _JournalCursor:
    """Incremental reader across sealed journal segments plus the active file."""

    def __init__(self, events_path: str, start_at_end: bool = False):
        self.events_path = events_path
        self.offsets: dict[str, int] = {}
        self.seen: set[tuple] = set()
        if start_at_end:
            for segment in _journal_segment_paths(events_path):
                try:
                    self.offsets[str(segment)] = segment.stat().st_size
                except OSError:
                    self.offsets[str(segment)] = 0
            # Mark existing records so a later seal/rename cannot replay them.
            for record in self._read_segments():
                self.seen.add(_journal_record_key(record))

    def _read_segments(self) -> list[dict]:
        records: list[dict] = []
        for segment in _journal_segment_paths(self.events_path):
            name = str(segment)
            try:
                size = segment.stat().st_size
            except OSError:
                continue
            offset = self.offsets.get(name, 0)
            if offset > size:
                offset = 0  # the active file was truncated after sealing
            try:
                with open(segment, "r", encoding="utf-8", errors="replace") as journal:
                    journal.seek(offset)
                    while True:
                        line = journal.readline()
                        if not line:
                            break
                        if not line.endswith("\n"):
                            break
                        offset = journal.tell()
                        text = line.strip()
                        if not text:
                            continue
                        try:
                            value = json.loads(text)
                        except json.JSONDecodeError:
                            continue
                        if isinstance(value, dict):
                            records.append(value)
            except OSError:
                continue
            self.offsets[name] = offset
        return records

    def read(self) -> list[dict]:
        fresh: list[dict] = []
        for record in self._read_segments():
            key = _journal_record_key(record)
            if key in self.seen:
                continue
            self.seen.add(key)
            fresh.append(record)
        return fresh


def _journal_read_from(path: str, offset: int) -> tuple[list[dict], int]:
    """Compatibility shim: read all segments from a byte offset in the active file."""
    cursor = _JournalCursor(path)
    cursor.offsets[str(Path(path))] = offset
    return cursor.read(), offset


def _append_journal_event(events_path: str, event: str, fields: dict, reason: str,
                          request_id: str = "") -> None:
    """Append one runner-side record to the same journal the game writes.

    The dev-loop already owns journal appends for its own dev-loop.* records,
    so scenario.* lifecycle records belong here as well. Guard against an
    interleaved partial line by writing a single newline-terminated record.
    """
    if not events_path:
        return
    record = {
        "category": "GENERAL",
        "event": event,
        "event_id": f"SCENARIO_{uuid.uuid4().hex[:12]}",
        "fields": fields,
        "level": "IMPORTANT",
        "process": "dev-loop",
        "process_role": "scenario-runner",
        "request_id": request_id,
        "reason": reason,
        "wall_time": utc_now(),
    }
    try:
        with open(events_path, "a", encoding="utf-8", newline="\n") as journal:
            journal.write(json.dumps(record, separators=(",", ":")) + "\n")
    except OSError as error:
        print(f"[DEV SCENARIO] unable to append journal record: {error}", file=sys.stderr)


def _control_send(port: int, token: str, payload: dict, timeout: float = 10.0) -> dict:
    with socket.create_connection(("127.0.0.1", int(port)), timeout=timeout) as connection:
        connection.sendall((json.dumps(payload) + "\n").encode("utf-8"))
        return json.loads(connection.recv(65536).decode("utf-8"))


def _record_request_id(record: dict) -> str:
    # The game writes the agent request id as correlation_id and inside fields;
    # the runner writes it as a top-level request_id. Accept all three.
    for key in ("request_id", "correlation_id"):
        value = record.get(key)
        if value:
            return str(value)
    fields = record.get("fields")
    if isinstance(fields, dict) and fields.get("request_id"):
        return str(fields["request_id"])
    return ""


def _field_path(record: dict, dotted: str):
    value = record.get("fields", {}) if isinstance(record.get("fields"), dict) else {}
    for part in dotted.split("."):
        if isinstance(value, dict) and part in value:
            value = value[part]
        else:
            return None
    return value


def _evaluate_assertions(assertions: list[dict], records: list[dict],
                         scenario_id: str) -> tuple[bool, list[dict]]:
    results = []
    passed_all = True
    for index, assertion in enumerate(assertions or []):
        a_type = str(assertion.get("type", ""))
        ok = False
        expected = assertion
        actual = None
        if a_type == "event_present":
            name = str(assertion.get("event", ""))
            matches = [r for r in records if r.get("event") == name]
            ok = len(matches) > 0
            actual = {"count": len(matches)}
        elif a_type == "event_absent":
            name = str(assertion.get("event", ""))
            matches = [r for r in records if r.get("event") == name]
            ok = len(matches) == 0
            actual = {"count": len(matches)}
        elif a_type == "event_field_equals":
            name = str(assertion.get("event", ""))
            field = str(assertion.get("field", ""))
            want = assertion.get("equals")
            for record in records:
                if record.get("event") == name:
                    value = _field_path(record, field)
                    if value == want:
                        ok = True
                        actual = {"value": value}
                        break
        elif a_type == "spawn_in_bounds":
            bounds = None
            for record in records:
                if record.get("event") == "map.server-load-success":
                    bounds = record.get("fields", {})
            positions = [r for r in records if r.get("event") == "spawn.server-selected"]
            if bounds and positions:
                lo = bounds.get("bounds_min")
                hi = bounds.get("bounds_max")
                valid = 0
                for record in positions:
                    pos = _field_path(record, "position")
                    if (isinstance(pos, list) and isinstance(lo, list)
                            and isinstance(hi, list) and len(pos) == 3):
                        if all(lo[i] <= pos[i] <= hi[i] for i in range(3)):
                            valid += 1
                ok = valid == len(positions) and valid > 0
                actual = {"in_bounds": valid, "total": len(positions), "bounds": [lo, hi]}
        elif a_type == "map_ids_agree":
            server_map = client_required = client_loaded = None
            for record in records:
                event = record.get("event")
                if event == "map.server-load-success":
                    path = _field_path(record, "resolved_path") or _field_path(record, "requested_path")
                    if path:
                        server_map = str(path)
                elif event == "network.map-required":
                    value = _field_path(record, "required_map_id")
                    if value:
                        client_required = str(value)
                elif event == "map.load-success" and record.get("process") == "client":
                    path = _field_path(record, "resolved_path") or _field_path(record, "requested_path")
                    if path:
                        client_loaded = str(path)

            def _base(value: str) -> str:
                return value.replace("\\", "/").split("/")[-1].replace(".glb", "").lower()

            ok = bool(server_map and client_required and client_loaded) and (
                _base(server_map) == _base(client_required) == _base(client_loaded))
            actual = {"server_map": server_map, "client_required": client_required,
                      "client_loaded": client_loaded}
        elif a_type == "event_count_at_least":
            name = str(assertion.get("event", ""))
            minimum = int(assertion.get("minimum", 1))
            count = sum(1 for r in records if r.get("event") == name)
            ok = count >= minimum
            actual = {"count": count, "minimum": minimum}
        elif a_type == "event_count_at_most":
            name = str(assertion.get("event", ""))
            maximum = int(assertion.get("maximum", 0))
            count = sum(1 for r in records if r.get("event") == name)
            ok = count <= maximum
            actual = {"count": count, "maximum": maximum}
        elif a_type == "event_field_count_at_least":
            name = str(assertion.get("event", ""))
            field = str(assertion.get("field", ""))
            want = assertion.get("equals")
            minimum = int(assertion.get("minimum", 1))
            count = sum(1 for r in records
                        if r.get("event") == name and _field_path(r, field) == want)
            ok = count >= minimum
            actual = {"count": count, "minimum": minimum, "field": field, "equals": want}
        elif a_type in ("event_field_at_least", "event_field_at_most"):
            name = str(assertion.get("event", ""))
            field = str(assertion.get("field", ""))
            want = float(assertion.get("value", 0.0))
            seen = []
            for record in records:
                if record.get("event") != name:
                    continue
                value = _field_path(record, field)
                if isinstance(value, (int, float)):
                    seen.append(float(value))
            if a_type == "event_field_at_least":
                ok = any(value >= want for value in seen)
            else:
                ok = any(value <= want for value in seen)
            actual = {"values": seen[-5:], "value": want,
                      "samples": len(seen)}
        else:
            ok = False
            actual = {"error": f"unknown assertion type '{a_type}'"}
        if not ok:
            passed_all = False
        results.append({
            "type": a_type,
            "passed": ok,
            "expected": expected,
            "actual": actual,
        })
    return passed_all, results


def run_scenario_file(args: argparse.Namespace, scenario_override: dict | None = None) -> int:
    """Ensure a live run, execute steps in order (commands and event waits),
    evaluate assertions from the live journal, and return a verdict."""
    try:
        scenario = scenario_override if scenario_override is not None else _scenario_load(args.scenario_file)
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"[DEV SCENARIO] invalid scenario: {error}", file=sys.stderr)
        return 2

    scenario_id = str(scenario["id"])
    steps = list(scenario.get("setup", [])) + list(scenario.get("steps", []))
    if not steps:
        print("[DEV SCENARIO] scenario has no steps", file=sys.stderr)
        return 2
    timeout = float(scenario.get("timeout_seconds", 180) or 180)
    request_id = f"scenario-{scenario_id}-{uuid.uuid4().hex[:8]}"

    launch_cfg = scenario.get("launch") if isinstance(scenario.get("launch"), dict) else {}
    print(f"[DEV SCENARIO] id={scenario_id}")
    print(f"[DEV SCENARIO] purpose={scenario.get('description', '')}")
    print(f"[DEV SCENARIO] launch map={launch_cfg.get('map', '-')} "
          f"mode={launch_cfg.get('launch_mode', '-')} "
          f"weapon_set={launch_cfg.get('weapon_set', 'profile-default')}")
    for index, step in enumerate(steps):
        operation = "wait_for_event" if "wait_for_event" in step else (
            "wait_seconds" if "wait_seconds" in step else
            f"command:{step.get('command', '-')}")
        detail = step.get("wait_for_event") or step.get("wait_seconds") or step.get("args", "")
        print(f"[DEV SCENARIO] step[{index}] {step.get('id', '-')} {operation} {detail}")
    for index, assertion in enumerate(scenario.get("assertions", []) or []):
        print(f"[DEV SCENARIO] assert[{index}] {assertion}")
    if getattr(args, "scenario_explain", False):
        return 0

    current = read_current_run() or {}
    port = current.get("control_port")
    token = current.get("control_token")
    if not port or not token:
        print("[DEV CONTROL] no running daemon control endpoint; start the dev loop once", file=sys.stderr)
        return 2

    # 1. Ensure a live, logger-ready client on the scenario's map/set. Reuse a
    #    matching run when present; otherwise ask the daemon to launch one
    #    (never kill unrelated processes).
    launch_cfg = scenario.get("launch") if isinstance(scenario.get("launch"), dict) else {}
    desired_map = str(launch_cfg.get("map", "") or "").strip()
    desired_mode = launch_cfg.get("launch_mode")
    desired_set = launch_cfg.get("weapon_set")
    playback_path = _scenario_input_path(scenario)
    if playback_path:
        recorded_header = _read_input_header(playback_path)
        recorded_map = _map_basename(str(recorded_header.get("map", "")))
        if recorded_map:
            desired_map = recorded_map
        recorded_set = recorded_header.get("weapon_set")
        if recorded_set:
            desired_set = recorded_set
    need_launch = (
        not current.get("run_alive")
        or not current.get("events_path")
        or not current.get("client_ready")
        or not current.get("client_pids")
    )
    if desired_map and str(current.get("map", "")) != desired_map:
        need_launch = True
    if desired_mode is not None and str(current.get("launch_mode", "")) != str(desired_mode):
        need_launch = True
    # A scenario must not run against a stale binary or an older published build.
    if current.get("latest_build_stale"):
        need_launch = True
    if (current.get("latest_successful_build")
            and current.get("running_build") != current.get("latest_successful_build")):
        need_launch = True
    # A requested weapon set must match the live server. Reuse the current run
    # when it already has that set; otherwise relaunch.
    if desired_set is not None:
        current_set = _server_weapon_set(current)
        if current_set is None or str(current_set) != str(desired_set):
            need_launch = True
    if launch_cfg.get("reuse_matching_run") is False:
        need_launch = True
    if need_launch:
        previous_run_id = str(current.get("run_id", ""))
        payload = {"token": token, "action": "launch", "request_id": request_id + "-launch"}
        if desired_mode is not None:
            payload["launch_mode"] = desired_mode
        if desired_map:
            payload["map"] = desired_map
        if desired_set is not None:
            payload["weapon_set"] = str(desired_set)
        try:
            _control_send(port, token, payload)
        except (OSError, ValueError, json.JSONDecodeError) as error:
            print(f"[DEV SCENARIO] unable to request launch: {error}", file=sys.stderr)
            return 2
        # Wait for the freshly launched run, not the still-live previous run
        # (the daemon replaces the client asynchronously). Require a new run id
        # so commands are queued against the relaunched client, not the old one.
        deadline = time.time() + timeout
        while time.time() < deadline:
            current = read_current_run() or {}
            new_run = str(current.get("run_id", "")) != previous_run_id or not previous_run_id
            if (new_run and current.get("run_alive") and current.get("client_ready")
                    and current.get("client_pids") and current.get("events_path")
                    and (not desired_map or str(current.get("map", "")) == desired_map)):
                break
            time.sleep(0.5)
        else:
            print(f"[DEV SCENARIO] timed out waiting for a live run (map={desired_map or 'any'})",
                  file=sys.stderr)
            return 1

    events_path = str(current.get("events_path") or "")
    if not events_path:
        print("[DEV SCENARIO] run has no journal path", file=sys.stderr)
        return 1

    # If the runner launched a fresh run, read its whole journal (server start,
    # join, initial spawn). If it reused a live run, read only new records.
    launched_fresh = need_launch
    try:
        start_offset = 0 if launched_fresh else Path(events_path).stat().st_size
    except OSError:
        start_offset = 0

    _append_journal_event(events_path, "scenario.started", {
        "scenario_id": scenario_id,
        "request_id": request_id,
        "step_count": len(steps),
        "build": current.get("running_build"),
        "client_pids": current.get("client_pids"),
        "map": current.get("map"),
        "weapon_set": desired_set,
    }, "scenario started by control client", request_id)

    records: list[dict] = []
    cursor = _JournalCursor(events_path, start_at_end=not launched_fresh)

    def pump() -> None:
        records.extend(cursor.read())

    def wait_for_event(spec: dict, deadline: float) -> tuple[bool, dict | None]:
        name = str(spec.get("wait_for_event", ""))
        where = spec.get("where") if isinstance(spec.get("where"), dict) else {}
        start = len(records)
        while time.time() < deadline:
            pump()
            for record in records[start:]:
                if record.get("event") != name:
                    continue
                if all(_field_path(record, key) == value for key, value in where.items()):
                    return True, record
            time.sleep(0.25)
        pump()
        # A reused run may already have passed the requested lifecycle event
        # before this scenario started. Accept it from the full run journal.
        for record in _JournalCursor(events_path).read():
            if record.get("event") != name:
                continue
            if all(_field_path(record, key) == value for key, value in where.items()):
                return True, record
        return False, None

    def wait_for_playback_terminal(path: str, deadline: float) -> tuple[bool, str]:
        start = len(records)
        while time.time() < deadline:
            pump()
            for record in records[start:]:
                event_name = str(record.get("event", ""))
                if event_name not in ("scenario.playback.completed",
                                      "scenario.playback.failed"):
                    continue
                fields = record.get("fields") if isinstance(record.get("fields"), dict) else {}
                if fields.get("path") not in (None, path):
                    continue
                if event_name == "scenario.playback.failed":
                    return False, "playback failed: " + str(
                        fields.get("reason", "unknown_reason"))
                if fields.get("completed") is True and fields.get("valid", True) is True:
                    return True, "playback completed"
                return False, "playback completed invalid: " + str(
                    fields.get("failure_reason", fields.get("end_reason", "unknown")))
            time.sleep(0.25)
        pump()
        return False, "playback timeout"

    def queue_command(command_text: str, rid: str, deadline: float) -> tuple[bool, str]:
        try:
            response = _control_send(port, token, {
                "token": token, "action": "command",
                "command": command_text, "request_id": rid,
            })
        except (OSError, ValueError, json.JSONDecodeError) as error:
            return False, str(error)
        if not response.get("accepted"):
            return False, str(response.get("error", "rejected"))
        delivered = False
        while time.time() < deadline:
            pump()
            for record in records:
                # The daemon appends its own per-entry index to the request id
                # (for example "<rid>:0"), so match by prefix.
                if not _record_request_id(record).startswith(rid):
                    continue
                event_name = str(record.get("event", ""))
                fields = record.get("fields") if isinstance(record.get("fields"), dict) else {}
                if event_name == "dev-loop.command.delivered":
                    delivered = True
                elif event_name == "terminal.command.unknown":
                    return False, f"unknown command: {fields.get('command', command_text)}"
                elif event_name == "terminal.command.completed":
                    if fields.get("success") is True:
                        return True, ""
                    return False, f"command handler failed: {fields.get('command', command_text)}"
            time.sleep(0.2)
        if not delivered:
            return False, "delivery timeout"
        return False, "command delivered but no terminal.command.completed result was recorded"

    step_failures: list[dict] = []
    execution_ok = True
    for index, step in enumerate(steps):
        step_id = str(step.get("id", index))
        if "wait_for_event" in step:
            step_timeout = float(step.get("timeout_seconds", timeout) or timeout)
            ok, record = wait_for_event(step, time.time() + step_timeout)
            _append_journal_event(events_path, "scenario.step", {
                "scenario_id": scenario_id, "step_id": step_id, "step_index": index,
                "operation": "wait_for_event",
                "event": str(step.get("wait_for_event", "")),
                "where": step.get("where", {}),
                "result": "waited" if ok else "timeout",
                "observed": (record.get("fields") if ok and record else None),
            }, "scenario wait recorded", f"{request_id}:{index}")
            if not ok:
                execution_ok = False
                step_failures.append({"step_id": step_id, "kind": "wait_timeout",
                                      "event": str(step.get("wait_for_event", ""))})
                break
        elif "wait_seconds" in step:
            seconds = float(step.get("wait_seconds", 0) or 0)
            time.sleep(seconds)
            _append_journal_event(events_path, "scenario.step", {
                "scenario_id": scenario_id, "step_id": step_id, "step_index": index,
                "operation": "wait_seconds", "seconds": seconds, "result": "waited",
            }, "scenario pacing recorded", f"{request_id}:{index}")
        elif step.get("type") == "input_playback" or step.get("playback"):
            path = str(step.get("path", step.get("playback", ""))).strip()
            rid = f"{request_id}:{index}"
            ok, detail = queue_command("scenario_play " + path, rid, time.time() + 15.0)
            if ok:
                step_timeout = float(step.get("timeout_seconds", timeout) or timeout)
                ok, detail = wait_for_playback_terminal(
                    path, time.time() + step_timeout)
                if not ok:
                    pump()
                    rejected = next(
                        (r for r in records if r.get("event") == "scenario.playback.rejected"),
                        None)
                    if rejected:
                        detail = ("playback rejected: " + json.dumps(
                            rejected.get("fields", {}), separators=(",", ":")))
            _append_journal_event(events_path, "scenario.step", {
                "scenario_id": scenario_id, "step_id": step_id, "step_index": index,
                "operation": "input_playback", "path": path,
                "result": "executed" if ok else "failed", "detail": detail,
            }, "scenario playback recorded", rid)
            if not ok:
                execution_ok = False
                step_failures.append({"step_id": step_id, "kind": "playback_failed",
                                      "path": path, "detail": detail})
                break
        elif step.get("command"):
            command_text = " ".join(
                [str(step["command"]), *(str(value) for value in step.get("args", []))]).strip()
            rid = f"{request_id}:{index}"
            ok, detail = queue_command(command_text, rid, time.time() + 15.0)
            delay = float(step.get("delay_ms_after", 0) or 0) / 1000.0
            if delay > 0:
                time.sleep(delay)
            _append_journal_event(events_path, "scenario.step", {
                "scenario_id": scenario_id, "step_id": step_id, "step_index": index,
                "operation": "command", "command": command_text,
                "result": "executed" if ok else "failed", "detail": detail,
            }, "scenario command recorded", rid)
            if not ok:
                execution_ok = False
                step_failures.append({"step_id": step_id, "kind": "command_failed",
                                      "command": command_text, "detail": detail})
                break
        else:
            _append_journal_event(events_path, "scenario.step", {
                "scenario_id": scenario_id, "step_id": step_id, "step_index": index,
                "operation": "noop", "result": "skipped",
            }, "scenario step recorded", f"{request_id}:{index}")

    pump()
    passed, results = _evaluate_assertions(scenario.get("assertions", []), records, scenario_id)
    for assertion_result in results:
        _append_journal_event(events_path, "scenario.assert", {
            "scenario_id": scenario_id,
            **assertion_result,
        }, "scenario assertion evaluated", request_id)

    final_passed = bool(passed and execution_ok and not step_failures)
    first_assert_failure = next((r for r in results if not r["passed"]), None)
    _append_journal_event(events_path, "scenario.finished", {
        "scenario_id": scenario_id,
        "request_id": request_id,
        "passed": final_passed,
        "execution_complete": execution_ok,
        "assertions_evaluated": True,
        "failure_kind": "" if final_passed else ("step" if step_failures else "assertion"),
        "first_failed_step": step_failures[0] if step_failures else None,
        "first_failed_assertion": first_assert_failure,
        "assertion_count": len(results),
        "events_path": events_path,
    }, "scenario terminal verdict", request_id)

    print(f"[DEV SCENARIO] id={scenario_id} passed={final_passed} "
          f"execution_complete={execution_ok} assertions={len(results)}")
    print(f"[DEV SCENARIO] events={_terminal_file_link(events_path)}")
    if args.json:
        print(json.dumps({"scenario_id": scenario_id, "request_id": request_id,
                          "passed": final_passed, "results": results,
                          "step_failures": step_failures, "events_path": events_path}, indent=2))
    return 0 if final_passed else 1


# ── Process identity, watchdog/daemon ensure, and the visible dashboard ───────


def pid_alive(pid: int) -> bool:
    """Cross-platform liveness that never signals the target process."""
    if not pid or pid <= 0:
        return False
    if os.name == "nt":
        import ctypes

        kernel32 = ctypes.windll.kernel32
        handle = kernel32.OpenProcess(0x1000 | 0x00100000, False, int(pid))
        if not handle:
            return False
        try:
            code = ctypes.c_ulong()
            if kernel32.GetExitCodeProcess(handle, ctypes.byref(code)):
                return code.value == 259  # STILL_ACTIVE
            return False
        finally:
            kernel32.CloseHandle(handle)
    try:
        os.kill(pid, 0)
        return True
    except OSError:
        return False


def _lock_pid(path: Path) -> int:
    try:
        return int(path.read_text(encoding="ascii").strip() or "0")
    except (OSError, ValueError):
        return 0


def watcher_pid() -> int:
    return _lock_pid(WATCH_LOCK_PATH)


def daemon_lock_pid() -> int:
    return _lock_pid(LOCK_PATH)


def dashboard_pid() -> int:
    pid = _lock_pid(DASHBOARD_LOCK_PATH)
    if pid and pid_alive(pid):
        return pid
    return existing_dashboard_pid()


def process_image_name() -> str:
    return os.path.basename(sys.executable) or "python.exe"


def read_watch_state() -> dict:
    try:
        value = read_json(WATCH_STATE_PATH)
    except (OSError, json.JSONDecodeError):
        value = None
    return value if isinstance(value, dict) else {}


def send_control_request(port, token: str, payload: dict, timeout: float = 5.0):
    if not port or not token:
        return None
    try:
        with socket.create_connection(("127.0.0.1", int(port)), timeout=timeout) as connection:
            connection.sendall((json.dumps(payload) + "\n").encode("utf-8"))
            data = connection.recv(65536)
        return json.loads(data.decode("utf-8"))
    except (OSError, ValueError, json.JSONDecodeError):
        return None


def request_daemon_status(current: dict, timeout: float = 2.0):
    port = current.get("control_port")
    token = current.get("control_token")
    if not port or not token:
        return None
    response = send_control_request(port, token, {
        "token": token,
        "action": "status",
        "request_id": f"status-{uuid.uuid4().hex[:8]}",
    }, timeout=timeout)
    if response and response.get("accepted") and isinstance(response.get("state"), dict):
        return response["state"]
    return None


def _detached_flags() -> int:
    if os.name != "nt":
        return 0
    return (getattr(subprocess, "DETACHED_PROCESS", 0)
            | getattr(subprocess, "CREATE_NEW_PROCESS_GROUP", 0))


def _spawn_detached(args: list[str], log_path: Path) -> int:
    DEV_ROOT.mkdir(parents=True, exist_ok=True)
    try:
        with open(log_path, "a", encoding="utf-8") as sink:
            process = subprocess.Popen(
                args, cwd=str(ROOT), stdout=sink, stderr=subprocess.STDOUT,
                creationflags=_detached_flags(), close_fds=True)
    except OSError:
        return 0
    return process.pid


def start_watchdog_detached(profile: str, launch_mode, visibility: str) -> int:
    args = [sys.executable, str(WATCH_SCRIPT), "--dev-loop-role=watchdog",
            "--profile", str(profile)]
    if launch_mode is not None:
        args += ["--launch-mode", str(launch_mode)]
    args.append("--headless" if visibility == "headless" else "--visible")
    return _spawn_detached(args, WATCH_LOG_PATH)


def start_daemon_detached(profile: str, launch_mode, visibility: str) -> int:
    args = [sys.executable, str(DEV_LOOP_SCRIPT), "--daemon",
            "--dev-loop-role=daemon", "--profile", str(profile)]
    if launch_mode is not None:
        args += ["--launch-mode", str(launch_mode)]
    args.append("--headless" if visibility == "headless" else "--visible")
    return _spawn_detached(args, DAEMON_LOG_PATH)


def start_dashboard_detached(profile: str, launch_mode, visibility: str) -> int:
    args = [sys.executable, str(DEV_LOOP_SCRIPT),
            "--dev-loop-role=dashboard", "--profile", str(profile)]
    if launch_mode is not None:
        args += ["--launch-mode", str(launch_mode)]
    args.append("--headless" if visibility == "headless" else "--visible")
    return _spawn_detached(args, DASHBOARD_LOG_PATH)


def wait_for_daemon_ready(timeout: float):
    deadline = time.time() + timeout
    while time.time() < deadline:
        current = read_current_run() or {}
        if request_daemon_status(
                current, timeout=DAEMON_READY_STATUS_TIMEOUT_SECONDS) is not None:
            return current
        time.sleep(0.5)
    return None


def ensure_running(profile: str, launch_mode, visibility: str,
                   open_dashboard: bool = True):
    """Ensure exactly one watchdog and one daemon, then return the ready state.

    Duplicates are impossible: the watchdog and daemon each hold a lock, and
    this only starts a process when the corresponding owner PID is absent.

    An explicit start also clears any prior explicit-shutdown intent so the
    watchdog resumes keeping a daemon alive (spec section 19.4).
    """
    events: list[dict] = []
    clear_stop_intent()
    wpid = watcher_pid()
    if not (wpid and pid_alive(wpid)):
        started = start_watchdog_detached(profile, launch_mode, visibility)
        events.append({"action": "watchdog_start", "pid": started, "ok": bool(started)})
    # Give the watchdog a short chance to start the daemon, then use the
    # daemon lock as the idempotency gate for a direct fallback start. If the
    # watchdog races us, only one process can own that lock.
    current = wait_for_daemon_ready(min(3.0, DAEMON_READY_TIMEOUT_SECONDS))
    if current is None:
        dpid = daemon_lock_pid()
        if not (dpid and pid_alive(dpid)):
            started = start_daemon_detached(profile, launch_mode, visibility)
            events.append({"action": "daemon_start", "pid": started, "ok": bool(started)})
        current = wait_for_daemon_ready(DAEMON_READY_TIMEOUT_SECONDS)
    if open_dashboard and not dashboard_pid():
        started = start_dashboard_detached(profile, launch_mode, visibility)
        events.append({"action": "dashboard_start", "pid": started, "ok": bool(started)})
    return current, events


_COMMAND_DONE_EVENTS = (
    "terminal.command.completed",
    "terminal.command.unknown",
    "dev-loop.command.invalid",
    "scenario.step",
)


def wait_for_command_result(events_path, request_id: str, timeout: float,
                            start_offset: int | None = None) -> dict:
    """Wait for the client to execute a queued command; return a summary.

    `events_path` may be one journal or a list. Delivery is recorded in the
    dev-loop journal while execution is recorded in the client journal, so the
    caller normally passes both.
    """
    result = {"delivered": False, "completed": False, "event": "", "tick": None,
              "reason": ""}
    paths = events_path if isinstance(events_path, (list, tuple)) else [events_path]
    paths = [str(path) for path in paths if path]
    if not paths or not request_id:
        return result
    cursors = [_JournalCursor(path) for path in paths]
    deadline = time.time() + timeout
    while time.time() < deadline:
        for cursor in cursors:
            for record in cursor.read():
                if not _record_request_id(record).startswith(request_id):
                    continue
                name = str(record.get("event", ""))
                if name == "dev-loop.command.delivered":
                    result["delivered"] = True
                if name in _COMMAND_DONE_EVENTS:
                    result["completed"] = True
                    result["event"] = name
                    result["tick"] = record.get("tick")
                elif name == "dev-loop.control":
                    fields = record.get("fields") if isinstance(record.get("fields"), dict) else {}
                    status = str(fields.get("status", ""))
                    if status in ("rejected", "queue_rejected", "scenario_rejected"):
                        result["completed"] = True
                        result["event"] = name
                        result["reason"] = str(fields.get("message", ""))
        if result["completed"]:
            return result
        time.sleep(0.2)
    return result


def _duration_text(seconds) -> str:
    return format_duration(seconds)


def _build_label(executable, artifact_id=None) -> str:
    """Show an artifact ID even when launched through a stable path."""
    if artifact_id is not None:
        return str(artifact_id)
    if not executable:
        return "-"
    parts = Path(str(executable)).parts
    if "builds" in parts:
        index = parts.index("builds")
        if index + 1 < len(parts):
            return parts[index + 1]
    return os.path.basename(str(executable)) or "-"


def _last_arg_value(args: list[str], name: str) -> str:
    value = ""
    for index, arg in enumerate(args[:-1]):
        if arg == name:
            value = args[index + 1]
    return value


def _effective_launch_identity(args: list[str]) -> dict:
    """Resolve the values the server will actually use, last flag wins."""
    npcs = 0
    for index, arg in enumerate(args):
        if arg == "--no-npcs":
            npcs = 0
        elif arg == "--npcs" and index + 1 < len(args):
            try:
                npcs = int(args[index + 1])
            except ValueError:
                npcs = 0
    return {
        "map": _last_arg_value(args, "--map"),
        "mode": _last_arg_value(args, "--mode"),
        "gamemode": _last_arg_value(args, "--gamemode"),
        "weapon_set": _last_arg_value(args, "--weapon-set"),
        "npcs": npcs,
    }


def _recent_activity(events_path: str, limit: int = 10) -> list[str]:
    if not events_path:
        return []
    try:
        with open(events_path, "rb") as handle:
            handle.seek(0, os.SEEK_END)
            size = handle.tell()
            tail_bytes = min(size, 64 * 1024)
            handle.seek(size - tail_bytes)
            tail = handle.read(tail_bytes).decode("utf-8", errors="replace")
        lines = tail.splitlines()
        if size > tail_bytes and lines:
            lines = lines[1:]
        lines = lines[-300:]
    except OSError:
        return []
    found: list[str] = []
    for line in reversed(lines):
        line = line.strip()
        if not line:
            continue
        try:
            record = json.loads(line)
        except json.JSONDecodeError:
            continue
        name = str(record.get("event", ""))
        lowered_name = name.lower()
        if not (name.startswith("dev-loop.") or name.startswith("scenario.")
                or "hot" in lowered_name or "reload" in lowered_name):
            continue
        fields = record.get("fields") if isinstance(record.get("fields"), dict) else {}
        reason = str(fields.get("reason") or fields.get("message") or "")
        command = str(fields.get("command") or "")
        client_id = str(fields.get("client_id") or "")
        source = str(record.get("source") or "runtime")
        detail = reason
        if command:
            detail = f"AI command client={client_id or 'primary'} cmd=\"{command}\" result={reason}"
        found.append(f"{record.get('wall_time', '')} [{source}] {name} {detail}".strip()[:240])
        if len(found) >= limit:
            break
    return list(reversed(found))


DASHBOARD_CONTROLS_LINE = (
    "Controls : [1] launch latest  [2] launch mode  [V] client visible/headless  "
    "[A] auto-restart  [Q] stop daemon  [X] close view  [P] freeze  [R] resume"
)


def _storage_limit_text() -> str:
    try:
        raw = json.loads((ROOT / "config" / "debuglogger.json").read_text(encoding="utf-8"))
        limits = raw.get("storage_limits") or {}
        per_segment = int(limits.get("per_segment_bytes", 100000000))
        total = int(limits.get("total_log_tree_bytes", 1000000000))
        return f"segment<={per_segment} tree<={total}"
    except (OSError, ValueError, TypeError):
        return "segment<=100000000 tree<=1000000000"


def _journal_usage_text(events_path: str) -> str:
    try:
        size = Path(events_path).stat().st_size
        return f"active journal {size:,} bytes; {_storage_limit_text()}"
    except OSError:
        return f"active journal unavailable; {_storage_limit_text()}"


def _scenario_state(events_path: str) -> dict:
    if not events_path:
        return {"id": "-", "state": "No scenario recorded", "verdict": "—",
                "frame": "—", "tick": "—", "checkpoint": "—", "build_id": "—",
                "complete": "unknown", "journal": "(none)"}
    try:
        active = Path(events_path)
        size = active.stat().st_size
        with open(active, "rb") as journal:
            journal.seek(max(0, size - 262144))
            lines = journal.read().decode("utf-8", errors="replace").splitlines()
    except OSError:
        return {"id": "-", "state": "Journal unavailable", "verdict": "UNKNOWN",
                "frame": "—", "tick": "—", "checkpoint": "—", "build_id": "—",
                "complete": "unknown", "journal": events_path}
    result = {"id": "-", "state": "No scenario recorded", "verdict": "—",
              "frame": "—", "tick": "—", "checkpoint": "—", "build_id": "—",
              "checkpoint_progress": "—", "map": "—", "complete": "unknown",
              "journal": events_path}
    for line in reversed(lines):
        try:
            record = json.loads(line)
        except (ValueError, TypeError):
            continue
        event = str(record.get("event") or "")
        fields = record.get("fields") if isinstance(record.get("fields"), dict) else {}
        if not (event.startswith("scenario.") or event == "bookmark.created"):
            continue
        scenario_id = str(fields.get("scenario_id") or record.get("correlation_id") or "-")
        if result["id"] != "-" and scenario_id != "-" and scenario_id != result["id"]:
            continue
        if result["id"] == "-" or fields.get("scenario_id") or record.get("correlation_id"):
            result["id"] = scenario_id
        if result["state"] == "No scenario recorded":
            result["state"] = event
        for key, candidates in {
            "frame": ("frame", "frame_index", "frame_count", "completed_frames"),
            "tick": ("server_tick", "client_tick", "tick"),
            "checkpoint": ("checkpoint_id", "bookmark_number", "checkpoint"),
            "build_id": ("build_id",),
            "checkpoint_progress": ("checkpoint_index", "checkpoint_count"),
            "map": ("map", "map_name"),
        }.items():
            value = next((fields[name] for name in candidates if fields.get(name) is not None), None)
            if value is not None and result[key] == "—":
                result[key] = str(value)
        if event in ("scenario.record.started", "scenario.playback.started"):
            result["verdict"] = "RUNNING"
            result["complete"] = "in progress"
        elif event.endswith((".completed", ".failed", ".invalid")) or event == "scenario.playback.stopped":
            failed = any(word in event for word in ("failed", "invalid")) or event == "scenario.playback.stopped"
            complete = fields.get("valid", fields.get("complete", not failed))
            result["verdict"] = "FAIL" if failed or complete is False else "PASS"
            result["complete"] = "incomplete" if failed or complete is False else "complete"
            break
        elif result["verdict"] == "—" and event not in (
                "scenario.record.stopped", "bookmark.created"):
            result["verdict"] = "RUNNING"
            result["complete"] = "in progress"
        elif fields.get("valid") is False or fields.get("complete") is False:
            result["verdict"] = "INCOMPLETE"
            result["complete"] = "incomplete"
            break
    return result


def _scenario_state_text(events_path: str) -> str:
    scenario = _scenario_state(events_path)
    return (f"scenario={scenario['id']} state={scenario['state']} "
            f"verdict={scenario['verdict']} frame={scenario['frame']} "
            f"tick={scenario['tick']} build={scenario['build_id']} "
            f"journal={scenario['journal']}")


def _dashboard_lines(state: dict, watch: dict, role_pids: dict, view_status: str,
                     recent: list[str], picker: dict | None = None) -> list[str]:
    client_pids = state.get("client_pids") or []
    server_alive = state.get("server_alive")
    run_id = state.get("run_id") or "-"
    build_phase = str(state.get("build_status") or "-")
    build_failed = any(
        token in build_phase
        for token in ("fail", "reject", "blocked", "missing", "invalid", "error")
    )
    auto_restart = "ON" if state.get("auto_restart") else "OFF"
    lines = [
        f"MiMITA Dev Loop Dashboard — {ROOT}",
        f"Watchdog : {ROLE_NAMES['watchdog']}  pid={role_pids.get('watchdog') or '-'}  "
        f"img={process_image_name()}  role=watchdog  restarts={watch.get('restarts', 0)}  {WATCH_SCRIPT}",
        f"Daemon   : {ROLE_NAMES['daemon']}  pid={role_pids.get('daemon') or '-'}  "
        f"img={process_image_name()}  role=daemon  {DEV_LOOP_SCRIPT}",
        f"Dashboard: {ROLE_NAMES['dashboard']}  pid={os.getpid()}  img={process_image_name()}  role=dashboard",
        f"Settings : auto-restart={auto_restart}  next-launch={state.get('launch_visibility') or '-'}  "
        f"mode={state.get('launch_mode') or '-'}({state.get('launch_mode_name') or '-'})",
        f"Heartbeat: updated={state.get('updated_at') or '-'}",
        f"Build    : phase={build_phase} {'FAIL' if build_failed else ''}  "
        f"active-source={state.get('active_build_generation') if state.get('active_build_generation') is not None else '-'}  "
        f"queued={'yes' if state.get('build_pending') else 'no'}  "
        f"latest={state.get('latest_successful_build_id') or state.get('latest_successful_build') or '-'}@{state.get('latest_successful_source_generation', '-')}  "
        f"stale={state.get('latest_build_stale')}  "
        f"elapsed={DevLoop._duration_text(state.get('active_build_elapsed_seconds'))}",
        f"Build detail: {state.get('active_build_detail') or state.get('last_build_result') or '-'}",
        *[f"Build output: {line}" for line in (state.get('build_output_tail') or [])[-2:]],
        f"Runtime  : build={state.get('running_build_id') or state.get('running_build') or '-'}  match={state.get('builds_match')}",
        f"Server   : {'alive' if server_alive else 'dead'}  pid={state.get('server_pid') or '-'}  "
        f"uptime={_duration_text(state.get('server_uptime_seconds'))}  "
        f"build={_build_label(state.get('server_executable'), (state.get('run_metadata') or {}).get('build_id') or (state.get('run_metadata') or {}).get('artifact_id') or (state.get('run_metadata') or {}).get('build_number'))}  "
        f"room={state.get('room_code') or 'waiting'}",
        f"Client   : {'alive' if client_pids else 'dead'}  pids={client_pids or '-'}  "
        f"uptime={_duration_text(state.get('client_uptime_seconds'))}",
        f"Run      : id={run_id}  mode={state.get('launch_mode')}({state.get('launch_mode_name') or '-'})  "
        f"visibility={state.get('launch_visibility') or '-'}  map={state.get('map') or '-'}",
        f"Last     : {state.get('message') or '-'}",
        f"Journal  : {state.get('events_path') or '(none)'}",
        f"Limits   : {_storage_limit_text()}",
        f"Scenario : {_scenario_state_text(str(state.get('events_path') or ''))}",
        f"Log use  : {_journal_usage_text(str(state.get('events_path') or ''))}",
        f"View     : {view_status}",
        DASHBOARD_CONTROLS_LINE,
        "Recent   :",
    ]
    lines += [f"           {item}" for item in (recent or ["(no recent dev-loop activity)"])]
    if picker and picker.get("open"):
        lines.append("Picker   : type a mode number, Enter selects, Esc cancels")
        for mode_id, name in picker.get("modes", []):
            selected = " (selected)" if mode_id == str(state.get("launch_mode")) else ""
            lines.append(f"           {mode_id} = {name}{selected}")
        lines.append(f"           input: {picker.get('input', '')}_")
    return lines


def _plain_signature(lines: list[str]) -> tuple[str, ...]:
    """Signature that ignores volatile uptime/heartbeat churn for the fallback."""
    signature = []
    for line in lines:
        if line.startswith("Heartbeat"):
            continue
        parts = line.split("uptime=")
        if len(parts) > 1:
            tail = parts[1]
            tail = tail.split(" ", 1)[1] if " " in tail else ""
            line = parts[0] + "uptime=-" + ((" " + tail) if tail else "")
        signature.append(line)
    return tuple(signature)


def _dashboard_control(current: dict, action: str, **fields):
    """Send one authenticated dashboard control request to the daemon."""
    port = current.get("control_port")
    token = current.get("control_token")
    if not port or not token:
        return None
    payload = {
        "token": token,
        "action": action,
        "requested_by": "dashboard",
        "request_id": f"dashboard-{uuid.uuid4().hex[:8]}",
    }
    payload.update(fields)
    return send_control_request(port, token, payload, timeout=5.0)


def _read_dashboard_key():
    if os.name == "nt":
        try:
            import msvcrt

            if msvcrt.kbhit():
                return msvcrt.getwch().lower()
        except (ImportError, OSError):
            return None
        return None
    try:
        import select

        ready, _, _ = select.select([sys.stdin], [], [], 0)
        if ready:
            return sys.stdin.read(1).lower()
    except (OSError, ValueError):
        return None
    return None


def run_dashboard(args: argparse.Namespace) -> int:
    try:
        from dev_loop import terminal as term
    except ImportError:
        term = None

    title = f"MiMITA Dev Loop Dashboard — {ROOT.name}"
    if term is not None and not args.no_terminal_relaunch:
        if term.relaunch_in_windows_terminal(title):
            return 0
    if term is not None:
        term.enable_virtual_terminal()
        term.set_title(title)
    ansi = bool(term is not None and term.supports_ansi())

    current, ensure_events = ensure_running(
        args.profile, args.launch_mode, args.launch_visibility or "visible",
        open_dashboard=False)
    try:
        DASHBOARD_LOCK_PATH.parent.mkdir(parents=True, exist_ok=True)
        DASHBOARD_LOCK_PATH.write_text(str(os.getpid()), encoding="ascii")
    except OSError:
        pass
    role_pids = {"watchdog": watcher_pid(), "daemon": daemon_lock_pid(),
                 "dashboard": os.getpid()}
    events_path = str((current or {}).get("events_path") or "")
    _append_journal_event(events_path, "dev-loop.dashboard.attach", {
        "dashboard_pid": os.getpid(),
        "watchdog_pid": role_pids["watchdog"],
        "daemon_pid": role_pids["daemon"],
        "ansi": ansi,
        "ensure": ensure_events,
    }, "dashboard attached to dev-loop daemon status", "dashboard")

    frozen = False
    frozen_since = "-"
    view_status = "live"
    prev_count = 0
    frames = 0
    last_plain_signature = None
    picker = {"open": False, "input": "", "modes": []}
    # Optional scripted key source for automated validation
    # (MIMITA_DASHBOARD_KEYS="1,v,a,2,3,ENTER,q"). Each frame consumes one key
    # through the same handler as a real keypress.
    key_script = os.environ.get("MIMITA_DASHBOARD_KEYS", "")
    scripted_keys: list[str] | None = None
    if key_script:
        scripted_keys = [
            {"ENTER": "\r", "ESC": "\x1b", "BS": "\b"}.get(token.strip().upper(), token.strip())
            for token in key_script.split(",") if token.strip()
        ]

    def render(state: dict) -> None:
        nonlocal prev_count, last_plain_signature
        recent = _recent_activity(events_path)
        lines = _dashboard_lines(state, read_watch_state(), role_pids, view_status,
                                 recent, picker)
        if ansi:
            if prev_count:
                sys.stdout.write(f"\x1b[{prev_count}A")
            for line in lines:
                sys.stdout.write("\x1b[2K" + _rainbowize(line) + "\n")
            sys.stdout.flush()
            prev_count = len(lines)
        else:
            signature = _plain_signature(lines)
            if last_plain_signature is None or signature != last_plain_signature:
                for line in lines:
                    sys.stdout.write(line + "\n")
                sys.stdout.flush()
            last_plain_signature = signature

    def next_key():
        if scripted_keys is None:
            return _read_dashboard_key(), False
        if scripted_keys:
            return scripted_keys.pop(0), False
        return None, True

    def control(state: dict, action: str, **fields) -> dict | None:
        nonlocal current, view_status
        current = read_current_run() or current or {}
        view_status = f"{action} sending"
        render(current)
        response = _dashboard_control(current, action, **fields)
        ok = bool(response and response.get("accepted"))
        if not ok:
            problem = (response or {}).get("error") or "daemon unavailable"
            view_status = f"{action} not accepted: {problem}"
        else:
            if isinstance(response.get("state"), dict):
                current = response["state"]
            view_status = f"{action} accepted"
        return response

    def snapshot() -> dict:
        return request_daemon_status(current, timeout=0.25) if current else None

    if not args.no_tk:
        try:
            from dev_loop import gui as dev_loop_gui
            if dev_loop_gui.available():
                def gui_status():
                    latest = snapshot() or current or {}
                    identity = latest.get("build_identity") or {}
                    scenario = _scenario_state(str(latest.get("events_path") or ""))
                    scenario["build_id"] = scenario.get("build_id") or identity.get("build_id") or "—"
                    scenario["map"] = scenario.get("map") if scenario.get("map") != "—" else latest.get("map") or "—"
                    scenario["log_usage"] = _journal_usage_text(str(latest.get("events_path") or ""))
                    runtime_alive = bool(latest.get("run_alive"))
                    run_metadata = latest.get("run_metadata") or {}
                    runtime_identity = run_metadata.get("build_identity") or {}
                    runtime_build_id = (latest.get("running_build_id")
                                        or run_metadata.get("build_id")
                                        or latest.get("server_build_id")
                                        or latest.get("running_build") or "—")
                    runtime_artifact = (run_metadata.get("artifact_id")
                                        or runtime_identity.get("artifact_id")
                                        or latest.get("running_build") or "—")
                    runtime_sha256 = str(runtime_identity.get("sha256") or "—")
                    if runtime_sha256 == "—" and ":" in str(runtime_build_id):
                        runtime_sha256 = str(runtime_build_id).rsplit(":", 1)[-1]
                    sha256 = str(identity.get("sha256") or "—")
                    build_display = identity.get("build_id")
                    if not build_display and identity.get("build_number") is not None:
                        build_display = f"LEGACY #{identity['build_number']}:{sha256}"
                    build_display = build_display or latest.get("latest_successful_build") or "—"
                    artifact_display = identity.get("artifact_id")
                    if not artifact_display and identity.get("build_number") is not None:
                        artifact_display = f"LEGACY #{identity['build_number']}"
                    return {
                        "checkout": str(ROOT),
                        "daemon_pid": latest.get("daemon_pid") or role_pids.get("daemon"),
                        "updated_at": latest.get("updated_at") or "—",
                        "build_id": build_display,
                        "artifact_id": artifact_display or latest.get("latest_successful_build") or "—",
                        "sha256": sha256,
                        "identity_valid": bool(identity.get("valid")),
                        "running_build_id": runtime_build_id,
                        "running_artifact_id": runtime_artifact,
                        "running_sha256": runtime_sha256,
                        "stale": bool(latest.get("latest_build_stale")),
                        "source_generation": latest.get("latest_successful_source_generation", "—"),
                        "phase": latest.get("build_status", "unknown"),
                        "message": latest.get("message") or "",
                        "last_control": latest.get("last_control") or {},
                        "elapsed": _duration_text(latest.get("active_build_elapsed_seconds")),
                        "phase_detail": (
                            f"active source generation {latest.get('active_build_generation') or '—'}  ·  "
                            f"current source generation {latest.get('source_generation', '—')}\n"
                            f"{latest.get('active_build_detail') or latest.get('message') or ''}"
                        ),
                        "build_output": latest.get("build_output_tail") or [],
                        "runtime": "RUNNING" if runtime_alive else "NOT RUNNING",
                        "server_status": ("server ready" if latest.get("server_ready")
                                          else "server stopped" if not latest.get("server_alive")
                                          else "server starting"),
                        "client_status": ("client ready" if latest.get("client_ready")
                                          else "client stopped" if not (latest.get("client_pids") or [])
                                          else "client starting"),
                        "launch_mode": latest.get("launch_mode") or "",
                        "mode": latest.get("launch_mode_name") or latest.get("launch_mode") or "—",
                        "map": latest.get("map") or "—",
                        "runtime_detail": (
                            f"build {runtime_build_id}  ·  "
                            f"server {'ready' if latest.get('server_ready') else 'not ready'} "
                            f"(PID {latest.get('server_pid') or '—'})  ·  "
                            f"client {'ready' if latest.get('client_ready') else 'not ready'} "
                            f"(PID {', '.join(map(str, latest.get('client_pids') or [])) or '—'})\n"
                            f"map {latest.get('effective_map') or latest.get('map') or '—'}  ·  "
                            f"mode {latest.get('effective_mode') or '—'}  ·  "
                            f"gamemode {latest.get('effective_gamemode') or '—'}  ·  "
                            f"weapon_set {latest.get('effective_weapon_set') or '—'}  ·  "
                            f"npcs {latest.get('effective_npcs', '—')}  ·  "
                            f"build match {'yes' if latest.get('builds_match') else 'no'}"
                        ),
                        "scenario": scenario,
                        "instances": latest.get("instances") or [],
                        "manifest_path": latest.get("manifest_path") or "",
                        "journal": latest.get("events_path") or "(none)",
                        "recent": _recent_activity(str(latest.get("events_path") or "")),
                        "source_activity": (
                            str(latest.get("message") or "")
                            if "source changed" in str(latest.get("message") or "").lower()
                            else (f"source generation {latest.get('source_generation')} is newer than "
                                  f"published generation {latest.get('latest_successful_source_generation')}"
                                  if latest.get("latest_build_stale") else "")
                        ),
                    }

                def gui_processes():
                    return checkout_mimita_processes(strict=True)

                def gui_control(action: str, **fields):
                    if action == "stop":
                        fields["explicit_stop"] = True
                    response = control(current, action, **fields)
                    return response or {"accepted": False, "message": view_status}

                return dev_loop_gui.run(
                    gui_status,
                    gui_control,
                    title,
                    gui_processes)
        except Exception:
            # Fall through to the terminal dashboard; a presentation failure
            # must not stop the daemon or hide status.
            pass

    try:
        last_status_refresh = 0.0
        while True:
            now = time.monotonic()
            if not frozen and now - last_status_refresh >= DASHBOARD_STATUS_REFRESH_SECONDS:
                role_pids["watchdog"] = watcher_pid()
                role_pids["daemon"] = daemon_lock_pid()
                current = read_current_run() or current or {}
                render(snapshot() or current)
                last_status_refresh = now
            key, exhausted = next_key()
            if exhausted:
                break
            if key is not None:
                if picker["open"]:
                    if key in ("\r", "\n"):
                        mode_id = picker["input"].strip()
                        picker["open"] = False
                        picker["input"] = ""
                        if mode_id:
                            control(current, "set-launch-mode", launch_mode=mode_id)
                            view_status = f"launch mode {mode_id} requested"
                    elif key == "\x1b":
                        picker["open"] = False
                        picker["input"] = ""
                        view_status = "launch-mode picker cancelled"
                    elif key in ("\b", "\x7f"):
                        picker["input"] = picker["input"][:-1]
                    elif key.isdigit():
                        picker["input"] += key
                elif key == "1":
                    control(current, "build-or-launch")
                elif key == "2":
                    picker.update({"open": True, "input": "", "modes": launch_mode_table()})
                    view_status = "launch-mode picker open"
                elif key == "v":
                    control(current, "toggle-visibility")
                elif key == "a":
                    control(current, "toggle-auto-restart")
                elif key == "q":
                    control(current, "stop", explicit_stop=True)
                    view_status = "explicit daemon stop requested; watchdog will not restart it"
                    render(snapshot() or current)
                    break
                elif key == "x":
                    break
                elif key == "p" and not frozen:
                    frozen = True
                    frozen_since = utc_now()
                    view_status = f"FROZEN since {frozen_since} — press R to resume"
                    render(snapshot() or current)
                elif key == "r" and frozen:
                    frozen = False
                    view_status = f"refreshed at {utc_now()} (resumed from freeze)"
                if key not in ("x", "q"):
                    render(current or {})
            frames += 1
            if args.dashboard_frames and frames >= args.dashboard_frames:
                break
            time.sleep(DASHBOARD_FRAME_SECONDS)
    finally:
        try:
            if _lock_pid(DASHBOARD_LOCK_PATH) == os.getpid():
                DASHBOARD_LOCK_PATH.unlink(missing_ok=True)
        except OSError:
            pass
        _append_journal_event(events_path, "dev-loop.dashboard.detach", {
            "dashboard_pid": os.getpid(),
            "frames": frames,
        }, "dashboard detached (watchdog/daemon left running)", "dashboard")
    return 0


def _print_read_only_status(current: dict, as_json: bool) -> None:
    if as_json:
        print(json.dumps(current, indent=2))
        return
    print(f"[DEV STATUS] lifecycle={current.get('status')} build={current.get('build_status')} run={current.get('run_id') or '(none)'}")
    print(
        f"[DEV STATUS] mode={current.get('launch_mode')} "
        f"active_source={current.get('active_build_generation')} "
        f"elapsed={DevLoop._duration_text(current.get('active_build_elapsed_seconds'))} "
        f"runtime_build_id={current.get('running_build_id')} "
        f"latest_build_id={current.get('latest_successful_build_id')} "
        f"latest_artifact_id={current.get('latest_artifact_id')} "
        f"latest_source={current.get('latest_successful_source_generation')} "
        f"stale={current.get('latest_build_stale')} detail={current.get('active_build_detail') or current.get('last_build_result')}"
    )
    print(
        f"[DEV STATUS] run={'RUNNING' if current.get('run_alive') else 'NOT RUNNING'} "
        f"uptime={DevLoop._duration_text(current.get('run_uptime_seconds'))} "
        f"visibility={current.get('launch_visibility', 'unknown')}"
    )
    print(f"[DEV STATUS] events={_terminal_file_link(current.get('events_path'))}")
    effective = _effective_launch_identity(list(current.get("server_args") or []))
    print(
        f"[DEV STATUS] LAUNCH IDENTITY map={current.get('effective_map') or effective['map'] or '-'} "
        f"mode={current.get('effective_mode') or effective['mode'] or '-'} "
        f"gamemode={current.get('effective_gamemode') or effective['gamemode'] or '-'} "
        f"weapon_set={current.get('effective_weapon_set') or effective['weapon_set'] or '-'} "
        f"npcs={current.get('effective_npcs', effective['npcs'])}"
    )
    print(
        f"[DEV STATUS] server pid={current.get('server_pid')} "
        f"alive={current.get('server_alive')} "
        f"uptime={DevLoop._duration_text(current.get('server_uptime_seconds'))}"
    )
    print(
        f"[DEV STATUS] client pids={current.get('client_pids', [])} "
        f"uptime={DevLoop._duration_text(current.get('client_uptime_seconds'))}"
    )


def _control_action(args: argparse.Namespace) -> str:
    if getattr(args, "ai_match", False):
        return "scenario-file"
    if getattr(args, "inspect", ""):
        return "inspect"
    if getattr(args, "list_instances", False):
        return "list-instances"
    if getattr(args, "start_server", False):
        return "start-server"
    if getattr(args, "start_client", False):
        return "start-client"
    if getattr(args, "stop_instance", None):
        return "stop-instance"
    if getattr(args, "restart_instance", None):
        return "restart-instance"
    if args.command:
        return "command"
    if args.scenario_file:
        return "scenario-file"
    return args.request or "status"


def wait_for_build_request(request_id: str, timeout: float) -> int:
    deadline = time.monotonic() + max(1.0, timeout)
    while time.monotonic() < deadline:
        current = read_current_run() or {}
        status = (current.get("build_requests") or {}).get(request_id)
        if status == "success":
            print(f"[DEV CONTROL] build request={request_id} result=success "
                  f"build={current.get('latest_successful_build')}")
            return 0
        if status == "failed":
            print(f"[DEV CONTROL] build request={request_id} result=failed "
                  f"detail={current.get('message')}", file=sys.stderr)
            return 1
        time.sleep(0.5)
    print(f"[DEV CONTROL] build request={request_id} timed out waiting for completion",
          file=sys.stderr)
    return 2


def run_ai_match(args: argparse.Namespace) -> int:
    """Bounded unattended TDM match on the real dev-loop client.

    The player stays the same authoritative actor; only its local input source
    changes through `player_control ai`. The scenario is in-memory because this
    is live gameplay, not a recorded-input replay.
    """
    seconds = max(5.0, float(args.ai_seconds))
    scenario_id = f"ai_match_tdm_{datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%d_%H%M%S')}"
    scenario = {
        "id": scenario_id,
        "launch": {
            "profile": args.profile,
            "launch_mode": args.launch_mode or args.ai_launch_mode,
            "map": args.ai_map,
            "weapon_set": 29,
        },
        "setup": [
            {
                "id": "wait_active",
                "wait_for_event": "network.mode.start",
                "where": {"stage": "active"},
                "timeout_seconds": 120,
            },
            {"id": "enable_ai", "command": "player_control", "args": ["ai"]},
        ],
        "steps": [
            {"id": "ai_live_match", "wait_seconds": seconds},
            {"id": "restore_human", "command": "player_control", "args": ["human"]},
        ],
        "assertions": [
            {"type": "event_count_at_least", "event": "player.control.mode", "minimum": 2},
            {"type": "event_count_at_least", "event": "actor.collision.contact", "minimum": 1},
            {"type": "event_field_count_at_least", "event": "npc.shot",
             "field": "accepted", "equals": True, "minimum": 1},
            {"type": "event_count_at_least", "event": "npc.damage", "minimum": 1},
            {"type": "event_count_at_least", "event": "server.damage.applied", "minimum": 1},
            {"type": "event_count_at_least", "event": "player.ai.state", "minimum": 1},
            {"type": "event_count_at_least", "event": "npc.focus-changed", "minimum": 1},
            {"type": "event_count_at_least", "event": "npc.squad-sector-assigned", "minimum": 1},
        ],
        "timeout_seconds": seconds + 150.0,
    }
    print(f"[DEV AI MATCH] id={scenario_id} map={args.ai_map} seconds={seconds:.0f} "
          f"launch_mode={scenario['launch']['launch_mode']}")
    return run_scenario_file(args, scenario)


def run_control_client(args: argparse.Namespace) -> int:
    action = _control_action(args)

    if action == "list-instances":
        current = read_current_run()
        if current is None:
            print(f"[DEV CONTROL] no current run state: {CURRENT_RUN_PATH}", file=sys.stderr)
            return 2
        instances = current.get("instances") or []
        if args.json:
            print(json.dumps({"run_id": current.get("run_id"),
                              "manifest_path": current.get("manifest_path"),
                              "instances": instances}, indent=2))
            return 0
        print(f"[DEV INSTANCES] run={current.get('run_id') or '(none)'} "
              f"manifest={current.get('manifest_path') or '-'}")
        for inst in instances:
            age = inst.get("snapshot_age_ms")
            age_text = f"{age / 1000.0:.1f}s" if age is not None else "-"
            print(f"[DEV INSTANCES] {inst.get('instance_id')} "
                  f"{inst.get('role')} {inst.get('state')} "
                  f"pid={inst.get('pid') or '-'} "
                  f"snap={age_text} journal={inst.get('journal_path') or '-'}")
        return 0

    # Preserve read-only status behavior: never start or wake the daemon for a
    # pure status read. `--ensure-running` is explicitly not a read-only read.
    if (action == "status" and not args.command and not args.scenario_file
            and not getattr(args, "ai_match", False)
            and not args.request and not args.ensure_running):
        current = read_current_run()
        if current is None:
            print(f"[DEV CONTROL] no current run state: {CURRENT_RUN_PATH}", file=sys.stderr)
            return 2
        _print_read_only_status(current, args.json)
        return 0

    visibility = args.launch_visibility or "visible"
    current = read_current_run()
    # Mutating/command entry points ensure one watchdog and one daemon before
    # sending work. Stop requests target whatever daemon already exists.
    if args.scenario_file and getattr(args, "scenario_explain", False):
        return run_scenario_file(args)

    if action in ("command", "scenario-file", "launch", "build", "start-server",
                  "start-client", "stop-instance", "restart-instance") or args.ensure_running:
        current, ensure_events = ensure_running(args.profile, args.launch_mode, visibility)
        for event in ensure_events:
            print(f"[DEV CONTROL] recovery action={event.get('action')} "
                  f"pid={event.get('pid')} ok={event.get('ok')}")
        if current is None:
            print("[DEV CONTROL] daemon/watchdog recovery failed; no control endpoint",
                  file=sys.stderr)
            return 2
    if current is None:
        print(f"[DEV CONTROL] no current run state: {CURRENT_RUN_PATH}", file=sys.stderr)
        return 2

    if action == "status" and args.ensure_running and not args.command \
            and not args.scenario_file and not getattr(args, "ai_match", False) \
            and not args.request:
        print("[DEV CONTROL] watchdog and daemon are running")
        _print_read_only_status(current, args.json)
        return 0

    if action == "scenario-file":
        if getattr(args, "ai_match", False):
            return run_ai_match(args)
        return run_scenario_file(args)

    port = current.get("control_port")
    token = current.get("control_token")
    if not port or not token:
        print("[DEV CONTROL] no running daemon control endpoint; start the dev loop once", file=sys.stderr)
        return 2

    request = {
        "token": token,
        "action": action,
        "launch_mode": args.launch_mode,
        "request_id": f"{utc_now()}-{uuid.uuid4().hex[:8]}",
    }
    if action == "build":
        request["build_mode"] = args.build_mode
    if action == "stop":
        # A CLI stop is an explicit shutdown; the watchdog must honor it.
        request["explicit_stop"] = True
        request["requested_by"] = "cli"
    if args.command:
        request["command"] = args.command
        if getattr(args, "client", ""):
            request["client_id"] = args.client
    if action == "start-client":
        request["name"] = getattr(args, "name", "") or ""
        request["server_instance_id"] = getattr(args, "server", "") or "server-01"
        request["badconn_preset"] = getattr(args, "badconn_preset", "") or ""
    if action in ("stop-instance", "restart-instance"):
        request["instance_id"] = (getattr(args, "stop_instance", None)
                                  or getattr(args, "restart_instance", None) or "")
    # Capture the journal offset before sending so a fast command execution is
    # not missed while the daemon builds its synchronous control reply.
    pre_offset = None
    if action == "command":
        try:
            pre_offset = Path(str(current.get("events_path") or "")).stat().st_size
        except OSError:
            pre_offset = 0
    response = send_control_request(port, token, request, timeout=5.0)
    if response is None:
        print("[DEV CONTROL] unable to contact daemon", file=sys.stderr)
        return 2

    if args.json:
        print(json.dumps(response, indent=2))
    else:
        print(f"[DEV CONTROL] action={action} accepted={response.get('accepted')} "
              f"queued={response.get('queued', False)}")
        if response.get("error"):
            print(f"[DEV CONTROL] error={response['error']}", file=sys.stderr)

    if action == "command" and response.get("accepted"):
        requested_client = str(request.get("client_id", "") or "")
        chosen = None
        for inst in (current.get("instances") or []):
            if inst.get("role") == "client" and (
                    not requested_client or inst.get("instance_id") == requested_client):
                chosen = inst
                break
        events_path = str((chosen or {}).get("journal_path") or
                          current.get("client_journal_path") or
                          current.get("events_path") or "")
        control_journal = str(current.get("events_path") or "")
        result = wait_for_command_result(
            [events_path, control_journal], request["request_id"],
            timeout=float(args.command_timeout), start_offset=pre_offset)
        print(f"[DEV CONTROL] command=\"{args.command}\" delivered={result['delivered']} "
              f"completed={result['completed']} event={result['event'] or '-'} "
              f"tick={result['tick'] if result['tick'] is not None else '-'}"
              + (f" reason=\"{result['reason']}\"" if result["reason"] else ""))
        print(f"[DEV CONTROL] events={_terminal_file_link(events_path)}")
        return 0 if result["completed"] else 1

    if action == "build" and response.get("accepted") and args.wait_for_build:
        return wait_for_build_request(
            request["request_id"], timeout=float(args.build_timeout)
        )

    return 0 if response.get("accepted") else 1


def main() -> int:
    from dev_loop.cli import create_parser
    args = create_parser().parse_args()

    control = bool(args.status or args.request or args.command or args.scenario_file
                   or args.list_instances or args.start_server or args.start_client
                   or args.stop_instance or args.restart_instance or args.ai_match)

    if args.ai_match:
        args.profile = args.ai_profile
        if not args.launch_mode:
            args.launch_mode = args.ai_launch_mode

    if (args.daemon or args.dev_loop_role == "daemon") and not control:
        owner_pid = acquire_dev_loop_lock()
        profile = load_profile(args.profile)
        try:
            DevLoop(
                profile,
                args.auto_restart,
                args.launch_mode,
                args.launch_visibility,
            ).run()
        finally:
            release_dev_loop_lock(owner_pid)
        return 0

    if control or args.ensure_running:
        return run_control_client(args)

    # Default entry (including the desktop shortcut): the visible dashboard.
    return run_dashboard(args)


if __name__ == "__main__":
    raise SystemExit(main())
