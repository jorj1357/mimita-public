#!/usr/bin/env python3
# 2026-11-08
# purpose
# Machine-wide build slots shared by every MiMITA checkout and agent lane.
# Exactly one EXE build and two module builds may compile at once.
# Agents never call this directly; build.py and the module worker acquire it.
# Does NOT compile anything and does NOT own source/config state.

from __future__ import annotations

import ctypes
import datetime
import json
import os
import time
from pathlib import Path

try:
    from devscripts.path_containment import (
        resolve_coordinator_root,
        resolve_within,
    )
except ModuleNotFoundError:
    from path_containment import resolve_coordinator_root, resolve_within

SLOT_LIMITS = {"exe": 1, "module": 2}
DEFAULT_MIN_FREE_MB = 1024
STALE_SECONDS = 60.0


def scheduler_root() -> Path:
    coordinator = resolve_coordinator_root(Path(__file__).resolve().parents[1])
    configured = os.environ.get("MIMITA_BUILD_SCHEDULER_DIR", "").strip()
    root = Path(configured) if configured else coordinator / ".dev" / "build-scheduler"
    return resolve_within(coordinator, root, label="build scheduler path")


def _now_utc() -> str:
    return datetime.datetime.now(datetime.timezone.utc).isoformat(
        timespec="seconds").replace("+00:00", "Z")


def _pid_alive(pid: int) -> bool:
    if pid <= 0:
        return False
    if os.name != "nt":
        try:
            os.kill(pid, 0)
            return True
        except OSError:
            return False
    kernel32 = ctypes.windll.kernel32
    handle = kernel32.OpenProcess(0x00100000, False, pid)
    if not handle:
        return False
    try:
        return kernel32.WaitForSingleObject(handle, 0) == 258
    finally:
        kernel32.CloseHandle(handle)


def available_memory_bytes() -> int | None:
    if os.name != "nt":
        return None

    class MEMORYSTATUSEX(ctypes.Structure):
        _fields_ = [
            ("dwLength", ctypes.c_ulong),
            ("dwMemoryLoad", ctypes.c_ulong),
            ("ullTotalPhys", ctypes.c_ulonglong),
            ("ullAvailPhys", ctypes.c_ulonglong),
            ("ullTotalPageFile", ctypes.c_ulonglong),
            ("ullAvailPageFile", ctypes.c_ulonglong),
            ("ullTotalVirtual", ctypes.c_ulonglong),
            ("ullAvailVirtual", ctypes.c_ulonglong),
            ("ullAvailExtendedVirtual", ctypes.c_ulonglong),
        ]

    status = MEMORYSTATUSEX()
    status.dwLength = ctypes.sizeof(MEMORYSTATUSEX)
    if not ctypes.windll.kernel32.GlobalMemoryStatusEx(ctypes.byref(status)):
        return None
    return int(status.ullAvailPhys)


def min_free_bytes() -> int:
    try:
        megabytes = int(os.environ.get("MIMITA_BUILD_MIN_FREE_MB",
                                       str(DEFAULT_MIN_FREE_MB)))
    except ValueError:
        megabytes = DEFAULT_MIN_FREE_MB
    return max(0, megabytes) * 1024 * 1024


class BuildSlot:
    def __init__(self, kind: str, index: int, descriptor: int, path: Path):
        self.kind = kind
        self.index = index
        self.descriptor = descriptor
        self.path = path

    def release(self) -> None:
        if self.descriptor is None:
            return
        try:
            os.close(self.descriptor)
        except OSError:
            pass
        self.descriptor = None
        try:
            self.path.unlink()
        except OSError:
            pass

    def __enter__(self) -> "BuildSlot":
        return self

    def __exit__(self, *_exc) -> None:
        self.release()


def _read_owner(path: Path) -> dict:
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return {}
    return data if isinstance(data, dict) else {}


def _stale(path: Path, owner: dict) -> bool:
    pid = owner.get("pid")
    if isinstance(pid, int) and _pid_alive(pid):
        return False
    try:
        age = time.time() - path.stat().st_mtime
    except OSError:
        return False
    return age > STALE_SECONDS or not (isinstance(pid, int) and pid > 0)


def _claim(kind: str, index: int, owner: str, metadata: dict) -> BuildSlot | None:
    root = scheduler_root()
    root.mkdir(parents=True, exist_ok=True)
    path = root / f"{kind}-{index}.lock"
    try:
        descriptor = os.open(path, os.O_CREAT | os.O_EXCL | os.O_WRONLY)
    except FileExistsError:
        if _stale(path, _read_owner(path)):
            try:
                path.unlink()
            except OSError:
                pass
        return None
    payload = {
        "pid": os.getpid(),
        "owner": owner,
        "slot": f"{kind}-{index}",
        "started_utc": _now_utc(),
        **metadata,
    }
    try:
        os.write(descriptor, json.dumps(payload, separators=(",", ":")).encode("utf-8"))
    except OSError:
        os.close(descriptor)
        try:
            path.unlink()
        except OSError:
            pass
        return None
    return BuildSlot(kind, index, descriptor, path)


def acquire(kind: str, owner: str, *, timeout: float | None = None,
            poll_seconds: float = 0.5, metadata: dict | None = None) -> BuildSlot | None:
    if kind not in SLOT_LIMITS:
        raise ValueError(f"unknown build slot kind: {kind}")
    root = scheduler_root()
    root.mkdir(parents=True, exist_ok=True)
    metadata = metadata or {}
    deadline = None if timeout is None else time.monotonic() + max(0.0, timeout)
    warned = False
    announced_memory = False
    while True:
        free = available_memory_bytes()
        required = min_free_bytes()
        if free is not None and free < required:
            if not announced_memory:
                print(f"[BUILD SCHEDULER] waiting for memory: {free // (1024 * 1024)} MB free, "
                      f"{required // (1024 * 1024)} MB required", flush=True)
                announced_memory = True
            if deadline is not None and time.monotonic() >= deadline:
                return None
            time.sleep(max(0.5, poll_seconds))
            continue
        announced_memory = False
        for index in range(SLOT_LIMITS[kind]):
            slot = _claim(kind, index, owner, metadata)
            if slot is not None:
                if warned:
                    print(f"[BUILD SCHEDULER] acquired {kind}-{index} for {owner}", flush=True)
                return slot
        if not warned:
            holders = ", ".join(
                f"{kind}-{index}" for index in range(SLOT_LIMITS[kind]))
            print(f"[BUILD SCHEDULER] waiting for a free {kind} slot "
                  f"({holders}); other lanes are building", flush=True)
            warned = True
        if deadline is not None and time.monotonic() >= deadline:
            return None
        time.sleep(max(0.1, poll_seconds))


def status() -> dict:
    root = scheduler_root()
    slots = []
    if root.is_dir():
        for kind, limit in SLOT_LIMITS.items():
            for index in range(limit):
                path = root / f"{kind}-{index}.lock"
                owner = _read_owner(path) if path.is_file() else {}
                slots.append({
                    "slot": f"{kind}-{index}",
                    "free": not path.is_file(),
                    "owner": owner.get("owner", ""),
                    "pid": owner.get("pid", 0),
                    "module": owner.get("module", ""),
                    "lane": owner.get("lane", ""),
                })
    return {"root": str(root), "slots": slots,
            "free_memory_mb": (available_memory_bytes() or 0) // (1024 * 1024)}

if __name__ == "__main__":
    print(json.dumps(status(), indent=2))
