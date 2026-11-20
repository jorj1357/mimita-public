#!/usr/bin/env python3
"""Keep the dev-loop dashboard process and window alive and restorable."""

from __future__ import annotations

import argparse
import ctypes
import ctypes.wintypes as wintypes
import datetime
import json
import os
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEV_ROOT = ROOT / ".dev"
STATE_ROOT = DEV_ROOT / "dashboard-keepalive"
EVENTS_PATH = STATE_ROOT / "events.jsonl"
DASHBOARD_TITLE = "MiMITA Coordinator Dashboard"
WINDOW_WIDTH = 880
WINDOW_HEIGHT = 760

if os.name == "nt":
    user32 = ctypes.WinDLL("user32", use_last_error=True)
    shell32 = ctypes.WinDLL("shell32", use_last_error=True)
    user32.EnumWindows.argtypes = [ctypes.WINFUNCTYPE(
        wintypes.BOOL, wintypes.HWND, wintypes.LPARAM), wintypes.LPARAM]
    user32.EnumWindows.restype = wintypes.BOOL
    user32.GetWindowThreadProcessId.argtypes = [wintypes.HWND,
                                                ctypes.POINTER(wintypes.DWORD)]
    user32.GetWindowTextLengthW.argtypes = [wintypes.HWND]
    user32.GetWindowTextW.argtypes = [wintypes.HWND, wintypes.LPWSTR, ctypes.c_int]
    user32.IsWindowVisible.argtypes = [wintypes.HWND]
    user32.IsIconic.argtypes = [wintypes.HWND]
    user32.ShowWindow.argtypes = [wintypes.HWND, ctypes.c_int]
    user32.MonitorFromWindow.argtypes = [wintypes.HWND, wintypes.DWORD]
    user32.MonitorFromWindow.restype = ctypes.c_void_p
    user32.MoveWindow.argtypes = [wintypes.HWND, ctypes.c_int, ctypes.c_int,
                                  ctypes.c_int, ctypes.c_int, wintypes.BOOL]
    user32.SetForegroundWindow.argtypes = [wintypes.HWND]
    user32.GetWindowRect.argtypes = [wintypes.HWND,
                                     ctypes.POINTER(wintypes.RECT)]
    USER32 = user32
else:
    USER32 = None


def utc_now() -> str:
    return datetime.datetime.now(datetime.timezone.utc).isoformat(
        timespec="milliseconds").replace("+00:00", "Z")


def emit(event: str, fields: dict, reason: str = "") -> None:
    STATE_ROOT.mkdir(parents=True, exist_ok=True)
    record = {
        "category": "DEV_DASHBOARD",
        "event": event,
        "event_id": f"DASHBOARD_{int(time.time() * 1000)}",
        "fields": fields,
        "level": "IMPORTANT",
        "pid": os.getpid(),
        "process": "dashboard-keepalive",
        "process_role": "dashboard-keepalive",
        "reason": reason,
        "wall_time": utc_now(),
    }
    with EVENTS_PATH.open("a", encoding="utf-8", newline="\n") as handle:
        handle.write(json.dumps(record, separators=(",", ":")) + "\n")


def _pythonw() -> str:
    candidate = Path(sys.executable).with_name("pythonw.exe")
    return str(candidate if candidate.is_file() else Path(sys.executable))


def dashboard_processes() -> list[dict]:
    if os.name != "nt":
        return []
    query = (
        "Get-CimInstance Win32_Process | Where-Object { "
        "$_.CommandLine -match 'dev-loop.py' -and "
        "$_.CommandLine -match '--dashboard' } | "
        "Select-Object ProcessId,CommandLine | ConvertTo-Json -Compress"
    )
    try:
        result = subprocess.run(
            ["powershell", "-NoProfile", "-Command", query], cwd=ROOT,
            capture_output=True, text=True, timeout=8, check=False,
            creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
    except (OSError, subprocess.SubprocessError):
        return []
    text = result.stdout.strip()
    if not text:
        return []
    try:
        value = json.loads(text)
    except json.JSONDecodeError:
        return []
    rows = value if isinstance(value, list) else [value]
    return [row for row in rows if isinstance(row, dict) and row.get("ProcessId")]


def find_window(pid: int) -> int | None:
    if USER32 is None:
        return None
    found: list[int] = []

    @ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    def callback(hwnd, _lparam):
        process_id = wintypes.DWORD()
        USER32.GetWindowThreadProcessId(hwnd, ctypes.byref(process_id))
        if process_id.value != pid:
            return True
        length = USER32.GetWindowTextLengthW(hwnd)
        if length <= 0:
            return True
        buffer = ctypes.create_unicode_buffer(length + 1)
        USER32.GetWindowTextW(hwnd, buffer, length + 1)
        if DASHBOARD_TITLE in buffer.value:
            found.append(int(hwnd))
        return True

    USER32.EnumWindows(callback, 0)
    return found[0] if found else None


def window_state(hwnd: int) -> dict:
    if USER32 is None or not hwnd:
        return {}
    rect = wintypes.RECT()
    USER32.GetWindowRect(hwnd, ctypes.byref(rect))
    return {
        "hwnd": hwnd,
        "visible": bool(USER32.IsWindowVisible(hwnd)),
        "minimized": bool(USER32.IsIconic(hwnd)),
        "rect": [rect.left, rect.top, rect.right, rect.bottom],
    }


def restore_window(hwnd: int, *, focus: bool) -> dict:
    if USER32 is None or not hwnd:
        return {}
    state = window_state(hwnd)
    if state.get("minimized"):
        USER32.ShowWindow(hwnd, 9)
    rect = state.get("rect") or [0, 0, 0, 0]
    width = max(1, rect[2] - rect[0])
    height = max(1, rect[3] - rect[1])
    # The primary-screen metrics do not include secondary monitors. Move the
    # window only when it intersects no connected display at all.
    offscreen = not bool(USER32.MonitorFromWindow(hwnd, 0))
    if offscreen or width < 200 or height < 200:
        USER32.MoveWindow(hwnd, 20, 20, WINDOW_WIDTH, WINDOW_HEIGHT, True)
    USER32.ShowWindow(hwnd, 5)
    if focus:
        USER32.SetForegroundWindow(hwnd)
    return window_state(hwnd)


def start_dashboard() -> int:
    command = [_pythonw(), str(ROOT / "devscripts" / "dev-loop.py"),
               "--dashboard", "--no-terminal-relaunch"]
    process = subprocess.Popen(
        command, cwd=ROOT,
        creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
    emit("dashboard.start", {"pid": process.pid, "command": command},
         "no dashboard process was present")
    return process.pid


def ensure(interval: float = 0.0, focus: bool = False, once: bool = False) -> int:
    started = 0
    restored = 0
    while True:
        processes = dashboard_processes()
        if not processes:
            pid = start_dashboard()
            started += 1
            deadline = time.monotonic() + 10.0
            while time.monotonic() < deadline:
                processes = dashboard_processes()
                if processes:
                    break
                time.sleep(0.25)
        hwnd = None
        pid = 0
        if processes:
            pid = int(processes[0].get("ProcessId") or 0)
            deadline = time.monotonic() + 5.0
            while time.monotonic() < deadline and not hwnd:
                hwnd = find_window(pid)
                if hwnd:
                    break
                time.sleep(0.2)
        if hwnd:
            before = window_state(hwnd)
            after = restore_window(hwnd, focus=focus)
            if before != after:
                restored += 1
                emit("dashboard.restored",
                     {"pid": pid, "before": before, "after": after},
                     "dashboard window was minimized or off-screen")
            else:
                emit("dashboard.healthy", {"pid": pid, "window": after})
        elif processes:
            emit("dashboard.window-missing",
                 {"pid": pid, "processes": processes},
                 "dashboard process exists but no matching window")
        else:
            emit("dashboard.failed", {"started": started},
                 "dashboard process could not be started")
        if once:
            return 1 if not processes else 0
        time.sleep(max(1.0, interval))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--once", action="store_true")
    parser.add_argument("--focus", action="store_true")
    parser.add_argument("--interval", type=float, default=5.0)
    args = parser.parse_args()
    return ensure(interval=args.interval, focus=args.focus, once=args.once)


if __name__ == "__main__":
    raise SystemExit(main())
