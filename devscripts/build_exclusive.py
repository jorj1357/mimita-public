#!/usr/bin/env python3
# 2026-11-08
# purpose
# enforce a single EXE build across every checkout/lane in the same user session
# use an OS kernel mutex on Windows so a dead builder cannot leave a stale lock
# use a repo-local file lock on non-Windows systems
# Does NOT compile and does NOT replace the build scheduler/memory guard.

from __future__ import annotations

import atexit
import ctypes
import os
import time
from pathlib import Path

MUTEX_NAME = "Local\\MiMITA-EXE-Build"
WAIT_OBJECT_0 = 0
WAIT_ABANDONED = 0x80
WAIT_TIMEOUT = 0x102
INFINITE = 0xFFFFFFFF


class ExclusiveBuildLock:
    def __init__(self):
        self.handle = None
        self.descriptor = None
        self.path: Path | None = None
        self._released = False

    def acquire_windows(self, owner: str) -> bool:
        kernel32 = ctypes.windll.kernel32
        kernel32.CreateMutexW.restype = ctypes.c_void_p
        handle = kernel32.CreateMutexW(None, False, MUTEX_NAME)
        if not handle:
            raise OSError("CreateMutexW failed")
        print(f"[BUILD EXCLUSIVITY] waiting for the single EXE build ({owner})",
              flush=True)
        result = kernel32.WaitForSingleObject(ctypes.c_void_p(handle), INFINITE)
        if result not in (WAIT_OBJECT_0, WAIT_ABANDONED):
            kernel32.CloseHandle(ctypes.c_void_p(handle))
            raise OSError(f"WaitForSingleObject failed: {result}")
        self.handle = handle
        print("[BUILD EXCLUSIVITY] acquired the single EXE build", flush=True)
        return True

    def acquire_unix(self, root: Path, owner: str) -> bool:
        import fcntl
        path = Path(root) / ".dev" / "build-scheduler" / "exe-0.lock"
        path.parent.mkdir(parents=True, exist_ok=True)
        descriptor = os.open(path, os.O_CREAT | os.O_RDWR)
        print(f"[BUILD EXCLUSIVITY] waiting for the single EXE build ({owner})",
              flush=True)
        fcntl.flock(descriptor, fcntl.LOCK_EX)
        os.ftruncate(descriptor, 0)
        os.write(descriptor, json_bytes({
            "owner": owner,
            "pid": os.getpid(),
            "started_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        }))
        self.descriptor = descriptor
        self.path = path
        print("[BUILD EXCLUSIVITY] acquired the single EXE build", flush=True)
        return True

    def release(self) -> None:
        if self._released:
            return
        self._released = True
        if os.name == "nt" and self.handle:
            kernel32 = ctypes.windll.kernel32
            kernel32.ReleaseMutex(ctypes.c_void_p(self.handle))
            kernel32.CloseHandle(ctypes.c_void_p(self.handle))
            self.handle = None
        elif self.descriptor is not None:
            try:
                os.close(self.descriptor)
            except OSError:
                pass
            self.descriptor = None


def json_bytes(value: dict) -> bytes:
    import json
    return (json.dumps(value, separators=(",", ":")) + "\n").encode("utf-8")


def acquire_exclusive_build(root: Path, owner: str = "build.py") -> ExclusiveBuildLock:
    lock = ExclusiveBuildLock()
    if os.name == "nt":
        lock.acquire_windows(owner)
    else:
        lock.acquire_unix(root, owner)
    atexit.register(lock.release)
    return lock
