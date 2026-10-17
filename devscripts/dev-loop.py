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
DAEMON_LOG_PATH = DEV_ROOT / "dev-loop-console.log"
DAEMON_READY_TIMEOUT_SECONDS = 60.0
DASHBOARD_FRAME_SECONDS = 1.0
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


def script_sha256(path: Path) -> str:
    try:
        return hashlib.sha256(path.read_bytes()).hexdigest()
    except OSError:
        return ""
DEFAULT_MAP_POOL_PATH = ROOT / "config" / "gamemode-good-maps.json"

# Snapshotting the watched source tree takes about 60 ms on this checkout.
# Polling at 150 ms consumed too much background CPU during client startup and
# could briefly steal frame time from the newly launched game.
POLL_SECONDS = 0.50
DEBOUNCE_SECONDS = 0.30
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
        "windows security",
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
        if pid != os.getpid() and script_text in command_line.replace("/", "\\").lower():
            pids.append(pid)
    return pids


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
    def __init__(
        self,
        profile: dict,
        auto_restart: bool,
        launch_mode: str | None = None,
        launch_visibility: str | None = None,
    ):
        self.profile = profile
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
        self.manual_launch_requested = False
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
        self.server_args: list[str] = []
        self.server_adopted = False
        self.build_mismatch_reason = ""
        self.firewall_result = "NOT_CHECKED"
        self.server_launch_ms: float = 0.0
        self.client_launch_ms: float = 0.0
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
        self.server_args = list(marker.get("args", []))
        self.server_adopted = True
        self.server_ready = bool(marker.get("server_ready", False))
        self.server_launch_ms = float(marker.get("launch_ms", 0.0) or 0.0)
        self.run_id = str(marker.get("run_id", "")) or self.run_id
        self.events_path = str(marker.get("events_path", "")) or self.events_path
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
            "run_id": self.run_id,
            "events_path": self.events_path,
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
        mismatch_reason = self.build_mismatch_reason
        if not mismatch_reason and self.server_exe and latest_identity:
            if os.path.normcase(self.server_exe) != os.path.normcase(str(latest_identity.get("path", ""))):
                mismatch_reason = "server_uses_older_published_build"
        if not mismatch_reason and self.server_exe and client_executable:
            if os.path.normcase(self.server_exe) != os.path.normcase(client_executable):
                mismatch_reason = "server_client_executable_path"
        if not mismatch_reason and self.latest_stale:
            mismatch_reason = "latest_build_stale_after_source_change"
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
        return {
            "profile": self.profile.get("name", "unnamed"),
            "launch_mode": self.launch_mode_id,
            "running_build": self.running_build,
            "latest_successful_build": self.latest_build,
            "source_generation": self.generation,
            "latest_successful_source_generation": self.latest_generation,
            "latest_build_stale": self.latest_stale,
            "auto_restart": self.auto_restart,
            "launch_visibility": self.launch_visibility,
            "headless": self.launch_visibility == "headless",
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
            "build_identity": self.latest_build_identity(),
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

    def record_runtime_event(self, event: str, fields: dict, reason: str) -> None:
        """Append bounded dev-loop launch evidence to the canonical journal."""
        if not self.events_path:
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
            "run_id": self.run_id or "",
            "wall_time": utc_now(),
        }
        try:
            with self.control_event_lock:
                with open(self.events_path, "a", encoding="utf-8", newline="\n") as journal:
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
        live_clients = [process for process in self.processes if process.poll() is None]
        if not live_clients or not self.client_ready:
            message = "client is not alive and logger-ready"
            self.record_control_event(request, "rejected", message)
            self.save_state("command_rejected")
            return {"accepted": False, "queued": False, "error": message}
        if not self.command_queue_path:
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

        with open(self.command_queue_path, "a", encoding="utf-8", newline="\n") as queue:
            for record in queue_records:
                queue.write(json.dumps(record, separators=(",", ":")) + "\n")

        self.record_runtime_event(
            "dev-loop.command.request",
            {
                "request_id": request_id,
                "command_count": len(queue_records),
                "scenario_id": scenario_id,
                "client_pid": live_clients[0].pid,
                "build": self.running_build,
                "events_path": self.events_path,
                "queue_path": self.command_queue_path,
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
            "build": self.running_build,
            "events_path": self.events_path,
            "queue_path": self.command_queue_path,
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
                self.override_map = str(request.get("map", "") or "").strip()
                self.override_weapon_set = str(request.get("weapon_set", "") or "").strip()
                self.manual_launch_requested = True
                self.build_pending = True
                self.change_event.clear()
                self.last_message = f"AI launch queued for mode {self.launch_mode_id}"
                self.record_control_event(request, "accepted", self.last_message)
                self.save_state("build_queued")
            elif action == "build-or-launch":
                # Dashboard key 1: build only when the latest artifact is stale
                # or missing; otherwise launch the latest published build.
                if self.latest_stale or self.latest_build is None:
                    self.manual_launch_requested = True
                    self.build_pending = True
                    self.change_event.clear()
                    self.last_message = "dashboard build/launch queued (stale or missing build)"
                    self.record_control_event(request, "accepted", self.last_message)
                    self.save_state("build_queued")
                else:
                    self.launch_latest()
                    self.last_message = "dashboard launched latest build"
                    self.record_control_event(request, "completed", self.last_message)
                    self.save_state("launch_latest")
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
            elif action == "stop-client":
                self.stop_processes()
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
            creationflags=_CREATE_NO_WINDOW,
            **({"startupinfo": _hidden_startupinfo()} if os.name == "nt" else {}),
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
            "--weapon-set", str(self.override_weapon_set or profile.get("weapon_set", 1)),
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
        self.server_args = []
        self.server_unavailable = True
        self.server_ready = False
        self.builds_match = False
        self.server_adopted = False
        self.events_path = None
        self.run_metadata = {}
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
            try:
                with open(self.events_path, "r", encoding="utf-8") as journal:
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

    def server_matches_launch_configuration(self, exe: Path, map_name: str) -> bool:
        """Ensure a durable server belongs to the currently selected mode/profile."""
        if not self.server_matches_executable(exe):
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
        self.server_args = []
        self.room_code = None
        self.server_unavailable = True
        self.cleanup_room_file()
        self.save_state("server_exited")

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
            self.override_map or str(self.launch_mode.get("map", "")),
        )
        self.override_map = ""
        print(f"[DEV] selected allowed map: {map_name}")
        if not self.validate_firewall_policy():
            return
        running_map = self.server_map_name()
        prelaunch_windows = self.observe_launch_windows("before_server_launch")
        if self.server_health() and not self.server_matches_launch_configuration(exe, map_name):
            print(f"[DEV SERVER] launch configuration changed; replacing old server "
                  f"pid={self.server_pid} old_map={running_map} new_map={map_name}")
            self.record_runtime_event(
                "dev-loop.process.replace_requested",
                {
                    "role": "server",
                    "pid": self.server_pid,
                    "old_executable": self.server_exe,
                    "new_executable": str(exe),
                    "reason": self.build_mismatch_reason,
                },
                "owned server does not match the selected build or launch configuration",
            )
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
            self.clear_server_marker()
            if self.server_process is not None:
                code = self.server_process.poll()
                print(f"[DEV SERVER] unexpected exit code={code} pid={self.server_pid} "
                      f"room={self.room_code}")
            self.server_process = None
            self.server_pid = None
            self.server_exe = None
            self.server_args = []
            self.server_adopted = False
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
            self.command_queue_path = None
            self.run_id = Path(self.events_path).parent.name
            # The first prelaunch snapshot above may predate creation of the
            # canonical journal. Record the same state again once the journal
            # exists so every fresh launch has a durable before-launch record.
            prelaunch_windows = self.observe_launch_windows("before_server_launch")
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
                version = run_hidden(
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
            self.record_runtime_event(
                "dev-loop.launch.args",
                {
                    "role": "server",
                    "executable": str(exe),
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
            server = subprocess.Popen(
                server_args,
                cwd=ROOT,
                env=server_env,
                creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
            )
            self.server_process = server
            self.server_pid = server.pid
            self.server_exe = str(exe)
            self.server_args = list(server_args)
            self.server_adopted = False
            self.server_launch_ms = time.time() * 1000.0
            self.server_unavailable = False
            self.server_ready = self.wait_for_logger_started(
                "server", server.pid, process=server)
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
                        "executable": str(exe),
                        "exit_code": server.returncode,
                        "failure": self.last_message,
                        "journal_path": self.events_path,
                    },
                    "server failed the logger.started readiness check",
                )
                self.save_state("server_logger_missing")
                if not self.stop_server():
                    print(f"[DEV SERVER] {self.last_message}; server could not be stopped")
                return
            print(f"[DEV SERVER] launched pid={server.pid} args={' '.join(server_args[1:])}")
            self.write_server_marker()

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
            "--unlock-cursor",
        ]
        client_args.extend(str(value) for value in self.launch_mode.get("client_args", []))
        if not self.events_path:
            self.events_path = create_shared_events_path()
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
        self.record_runtime_event(
            "dev-loop.launch.args",
            {
                "role": "client",
                "executable": str(exe),
                "args": list(client_args),
                "console_policy": "CREATE_NO_WINDOW",
                "window_policy": self.launch_visibility,
                "unlock_cursor_requested": "--unlock-cursor" in client_args,
            },
            "dev-loop launching client with hidden console and unlocked-cursor policy",
        )
        client = subprocess.Popen(
            client_args,
            cwd=ROOT,
            env=client_env,
            creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
        )
        self.processes.append(client)
        self.client_args = list(client_args)
        self.client_launch_ms = time.time() * 1000.0
        self.running_build = self.latest_build
        self.client_ready = self.wait_for_logger_started("client", client.pid)
        self.builds_match = bool(self.server_ready and self.client_ready and
                                 self.server_exe == str(exe) and
                                 self.latest_build_identity() and
                                 self.latest_build_identity().get("valid"))
        self.observe_launch_windows("client_ready" if self.client_ready else "client_logger_missing")
        if self.client_ready:
            self.apply_launch_visibility()
            self.restore_prelaunch_foreground(prelaunch_windows)
            self.monitor_launch_windows()
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
        previous_client_pids = tuple(process.pid for process in self.processes)
        live = [process for process in self.processes if process.poll() is None]
        self.processes = live
        if previous_client_pids != tuple(process.pid for process in live):
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
        return [
            f"[DEV STATUS] MODE: {snapshot.get('launch_mode')} ({self.launch_mode.get('name', 'unnamed')})",
            f"[DEV STATUS] BUILD: running={snapshot.get('running_build') or '-'} latest={snapshot.get('latest_successful_build') or '-'} match={snapshot.get('builds_match')}",
            f"[DEV STATUS] RUN: {'RUNNING' if running else 'NOT RUNNING'} uptime={self._duration_text(snapshot.get('run_uptime_seconds'))} visibility={snapshot.get('launch_visibility')}",
            f"[DEV STATUS] PIDS: server={snapshot.get('server_pid') or '-'} client={client_pids or '-'}",
            f"[DEV STATUS] EVENTS: {_terminal_file_link(snapshot.get('events_path'))}",
            "[DEV STATUS] CONTROLS: [1] build/launch  [2] mode  [V] visible/headless  [A] auto-start  [Q] quit",
        ]

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
                self.maintain_process()
                if self.build_pending:
                    self.change_event.clear()
                    time.sleep(DEBOUNCE_SECONDS)
                    self.run_build()
                    continue
                if self.change_event.wait(0.20):
                    self.build_pending = True
                self.print_status()
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


def _journal_read_from(path: str, offset: int) -> tuple[list[dict], int]:
    records: list[dict] = []
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as journal:
            journal.seek(offset)
            for line in journal:
                line = line.strip()
                if not line:
                    continue
                try:
                    value = json.loads(line)
                except json.JSONDecodeError:
                    continue
                if isinstance(value, dict):
                    records.append(value)
            offset = journal.tell()
    except OSError:
        pass
    return records, offset


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


def run_scenario_file(args: argparse.Namespace) -> int:
    """Ensure a live run, execute steps in order (commands and event waits),
    evaluate assertions from the live journal, and return a verdict."""
    try:
        scenario = _scenario_load(args.scenario_file)
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
    need_launch = not current.get("run_alive") or not current.get("events_path")
    if desired_map and str(current.get("map", "")) != desired_map:
        need_launch = True
    # A weapon-set override cannot be verified from the status record, so a
    # scenario that requests one always (re)launches to guarantee the set.
    if desired_set is not None:
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
    offset = start_offset

    def pump() -> None:
        nonlocal offset
        batch, new_offset = _journal_read_from(events_path, offset)
        records.extend(batch)
        offset = new_offset

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
        return False, None

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
        while time.time() < deadline:
            pump()
            for record in records:
                # The daemon appends its own per-entry index to the request id
                # (for example "<rid>:0"), so match by prefix.
                if (record.get("event") == "dev-loop.command.delivered" and
                        _record_request_id(record).startswith(rid)):
                    return True, ""
            time.sleep(0.2)
        return False, "delivery timeout"

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
                "observed": _field_path(record, "fields") if ok else None,
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


def wait_for_daemon_ready(timeout: float):
    deadline = time.time() + timeout
    while time.time() < deadline:
        current = read_current_run() or {}
        if request_daemon_status(current, timeout=2.0) is not None:
            return current
        time.sleep(0.5)
    return None


def ensure_running(profile: str, launch_mode, visibility: str):
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
    current = wait_for_daemon_ready(DAEMON_READY_TIMEOUT_SECONDS)
    if current is None:
        dpid = daemon_lock_pid()
        if not (dpid and pid_alive(dpid)):
            started = start_daemon_detached(profile, launch_mode, visibility)
            events.append({"action": "daemon_start", "pid": started, "ok": bool(started)})
        current = wait_for_daemon_ready(DAEMON_READY_TIMEOUT_SECONDS)
    return current, events


_COMMAND_DONE_EVENTS = (
    "terminal.command.completed",
    "terminal.command.unknown",
    "dev-loop.command.invalid",
    "scenario.step",
)


def wait_for_command_result(events_path: str, request_id: str, timeout: float,
                            start_offset: int | None = None) -> dict:
    """Wait for the client to execute a queued command; return a summary.

    The caller should pass the journal offset captured *before* sending the
    request: the daemon's control reply is built synchronously (including a
    status snapshot) and can arrive after the client already executed the
    command, so stat-ing the file after the reply can miss the events.
    """
    result = {"delivered": False, "completed": False, "event": "", "tick": None,
              "reason": ""}
    if not events_path or not request_id:
        return result
    if start_offset is None:
        try:
            start_offset = Path(events_path).stat().st_size
        except OSError:
            start_offset = 0
    offset = start_offset
    deadline = time.time() + timeout
    while time.time() < deadline:
        batch, offset = _journal_read_from(events_path, offset)
        for record in batch:
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
    if seconds is None:
        return "-"
    total = max(0, int(seconds))
    hours, remainder = divmod(total, 3600)
    minutes, secs = divmod(remainder, 60)
    return f"{hours:02d}:{minutes:02d}:{secs:02d}"


def _build_label(executable) -> str:
    """Show the published build number for a `.dev/builds/<n>/mimita.exe` path."""
    if not executable:
        return "-"
    parts = Path(str(executable)).parts
    if "builds" in parts:
        index = parts.index("builds")
        if index + 1 < len(parts):
            return parts[index + 1]
    return os.path.basename(str(executable)) or "-"


def _recent_activity(events_path: str, limit: int = 3) -> list[str]:
    if not events_path:
        return []
    try:
        with open(events_path, "r", encoding="utf-8", errors="replace") as handle:
            lines = handle.readlines()[-300:]
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
        if not (name.startswith("dev-loop.") or name.startswith("scenario.")):
            continue
        fields = record.get("fields") if isinstance(record.get("fields"), dict) else {}
        reason = str(fields.get("reason") or fields.get("message") or "")
        found.append(f"{record.get('wall_time', '')} {name} {reason}".strip()[:140])
        if len(found) >= limit:
            break
    return list(reversed(found))


DASHBOARD_CONTROLS_LINE = (
    "Controls : [1] build/launch  [2] launch mode  [V] client visible/headless  "
    "[A] auto-restart  [Q] stop daemon  [X] close view  [P] freeze  [R] resume"
)


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
        f"Build    : phase={build_phase} {'FAIL' if build_failed else 'OK'}  "
        f"running={state.get('running_build') or '-'}  latest={state.get('latest_successful_build') or '-'}  "
        f"match={state.get('builds_match')}",
        f"Server   : {'alive' if server_alive else 'dead'}  pid={state.get('server_pid') or '-'}  "
        f"uptime={_duration_text(state.get('server_uptime_seconds'))}  "
        f"build={_build_label(state.get('server_executable'))}",
        f"Client   : {'alive' if client_pids else 'dead'}  pids={client_pids or '-'}  "
        f"uptime={_duration_text(state.get('client_uptime_seconds'))}",
        f"Run      : id={run_id}  mode={state.get('launch_mode')}({state.get('launch_mode_name') or '-'})  "
        f"visibility={state.get('launch_visibility') or '-'}  map={state.get('map') or '-'}",
        f"Last     : {state.get('message') or '-'}",
        f"Journal  : {state.get('events_path') or '(none)'}",
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
        import dev_loop_terminal as term
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
        args.profile, args.launch_mode, args.launch_visibility or "visible")
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

    def control(state: dict, action: str, **fields) -> None:
        nonlocal current
        current = read_current_run() or current or {}
        response = _dashboard_control(current, action, **fields)
        ok = bool(response and response.get("accepted"))
        if not ok:
            problem = (response or {}).get("error") or "daemon unavailable"
            view_status = f"{action} not accepted: {problem}"

    def snapshot() -> dict:
        return request_daemon_status(current) if current else None

    try:
        while True:
            if not frozen:
                role_pids["watchdog"] = watcher_pid()
                role_pids["daemon"] = daemon_lock_pid()
                current = read_current_run() or current or {}
                render(snapshot() or current)
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
                    view_status = "build/launch requested"
                elif key == "2":
                    picker.update({"open": True, "input": "", "modes": launch_mode_table()})
                    view_status = "launch-mode picker open"
                elif key == "v":
                    control(current, "toggle-visibility")
                    view_status = "visibility toggle requested (next launch)"
                elif key == "a":
                    control(current, "toggle-auto-restart")
                    view_status = "auto-restart toggle requested"
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
            frames += 1
            if args.dashboard_frames and frames >= args.dashboard_frames:
                break
            time.sleep(DASHBOARD_FRAME_SECONDS)
    finally:
        _append_journal_event(events_path, "dev-loop.dashboard.detach", {
            "dashboard_pid": os.getpid(),
            "frames": frames,
        }, "dashboard detached (watchdog/daemon left running)", "dashboard")
    return 0


def _print_read_only_status(current: dict, as_json: bool) -> None:
    if as_json:
        print(json.dumps(current, indent=2))
        return
    print(f"[DEV STATUS] status={current.get('build_status')} run={current.get('run_id') or '(none)'}")
    print(
        f"[DEV STATUS] mode={current.get('launch_mode')} "
        f"running_build={current.get('running_build')} "
        f"latest_build={current.get('latest_successful_build')} "
        f"stale={current.get('latest_build_stale')}"
    )
    print(
        f"[DEV STATUS] run={'RUNNING' if current.get('run_alive') else 'NOT RUNNING'} "
        f"uptime={DevLoop._duration_text(current.get('run_uptime_seconds'))} "
        f"visibility={current.get('launch_visibility', 'unknown')}"
    )
    print(f"[DEV STATUS] events={_terminal_file_link(current.get('events_path'))}")
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
    if args.command:
        return "command"
    if args.scenario_file:
        return "scenario-file"
    return args.request or "status"


def run_control_client(args: argparse.Namespace) -> int:
    action = _control_action(args)

    # Preserve read-only status behavior: never start or wake the daemon for a
    # pure status read. `--ensure-running` is explicitly not a read-only read.
    if (action == "status" and not args.command and not args.scenario_file
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
    if action in ("command", "scenario-file", "launch") or args.ensure_running:
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
            and not args.scenario_file and not args.request:
        print("[DEV CONTROL] watchdog and daemon are running")
        _print_read_only_status(current, args.json)
        return 0

    if action == "scenario-file":
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
    if action == "stop":
        # A CLI stop is an explicit shutdown; the watchdog must honor it.
        request["explicit_stop"] = True
        request["requested_by"] = "cli"
    if args.command:
        request["command"] = args.command
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
        events_path = str(current.get("events_path") or "")
        result = wait_for_command_result(
            events_path, request["request_id"], timeout=float(args.command_timeout),
            start_offset=pre_offset)
        print(f"[DEV CONTROL] command=\"{args.command}\" delivered={result['delivered']} "
              f"completed={result['completed']} event={result['event'] or '-'} "
              f"tick={result['tick'] if result['tick'] is not None else '-'}"
              + (f" reason=\"{result['reason']}\"" if result["reason"] else ""))
        print(f"[DEV CONTROL] events={_terminal_file_link(events_path)}")
        return 0 if result["completed"] else 1

    return 0 if response.get("accepted") else 1


def main() -> int:
    parser = argparse.ArgumentParser(description="MiMITA incremental development loop")
    parser.add_argument("--profile", default="npc-navigation")
    parser.add_argument("--auto-restart", action="store_true")
    parser.add_argument(
        "--launch-mode",
        help="numeric dev launch mode from devscripts/dev-launch-modes.json",
    )
    visibility = parser.add_mutually_exclusive_group()
    visibility.add_argument(
        "--headless",
        dest="launch_visibility",
        action="store_const",
        const="headless",
        help="hide the game window while keeping the process and journal running",
    )
    visibility.add_argument(
        "--visible",
        dest="launch_visibility",
        action="store_const",
        const="visible",
        help="show the game window without leaving it focused",
    )
    parser.add_argument("--status", action="store_true", help="read the current dev-loop status and exit")
    parser.add_argument("--json", action="store_true", help="print control/status responses as JSON")
    parser.add_argument("--command", "--cmd", dest="command",
                        help="send one terminal command to the running daemon, wait for the result, and exit")
    parser.add_argument("--scenario-file", help="queue a repository-local Markdown/JSON scenario in the running client")
    parser.add_argument(
        "--request",
        choices=("launch", "stop-client", "stop-server", "stop"),
        help="send a command to the already-running dev-loop daemon and exit",
    )
    parser.add_argument("--ensure-running", action="store_true",
                        help="ensure the watchdog and daemon exist and exit")
    parser.add_argument("--daemon", action="store_true",
                        help="run the dev-loop daemon in this process (used by the watchdog/entry point)")
    parser.add_argument("--dashboard", action="store_true",
                        help="open the visible dashboard view attached to the daemon")
    parser.add_argument("--dev-loop-role", default="",
                        choices=("", "watchdog", "daemon", "dashboard"),
                        help="stable role marker recorded in the process command line")
    parser.add_argument("--no-terminal-relaunch", action="store_true",
                        help="do not prefer Windows Terminal; render in the current console")
    parser.add_argument("--dashboard-frames", type=int, default=0,
                        help="render this many dashboard frames then exit (0 = run until quit)")
    parser.add_argument("--command-timeout", type=float, default=15.0,
                        help="seconds to wait for a command result from the client")
    args = parser.parse_args()

    control = bool(args.status or args.request or args.command or args.scenario_file)

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
