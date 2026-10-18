"""Checkout-wide advisory lock shared by every compiler/linker entry point."""

from __future__ import annotations

import datetime
import json
import os
import socket
from pathlib import Path


class BuildLockBusy(RuntimeError):
    pass


class CheckoutBuildLock:
    def __init__(self, handle, path: Path, platform_lock: str):
        self.handle = handle
        self.path = path
        self.platform_lock = platform_lock

    def release(self) -> None:
        if self.handle is None:
            return
        try:
            self.handle.seek(0)
            if self.platform_lock == "windows":
                import msvcrt
                msvcrt.locking(self.handle.fileno(), msvcrt.LK_UNLCK, 1)
            else:
                import fcntl
                fcntl.flock(self.handle.fileno(), fcntl.LOCK_UN)
        except OSError:
            pass
        try:
            self.handle.close()
        finally:
            self.handle = None


def acquire_checkout_build_lock(root: Path, owner: str) -> CheckoutBuildLock:
    path = Path(root) / ".dev" / "build-serialization.lock"
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor = os.open(str(path), os.O_CREAT | os.O_RDWR, 0o666)
    handle = os.fdopen(descriptor, "r+b")
    platform_lock = "windows" if os.name == "nt" else "posix"

    try:
        handle.seek(0, os.SEEK_END)
        if handle.tell() == 0:
            handle.write(b"\0")
            handle.flush()
        handle.seek(0)
        if platform_lock == "windows":
            import msvcrt
            msvcrt.locking(handle.fileno(), msvcrt.LK_NBLCK, 1)
        else:
            import fcntl
            fcntl.flock(handle.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
    except OSError as error:
        handle.seek(0)
        active_owner = handle.read(2048).decode("utf-8", errors="replace").strip()
        handle.close()
        detail = f" ({active_owner})" if active_owner else ""
        raise BuildLockBusy(f"another MiMITA build owns {path}{detail}") from error

    metadata = {
        "pid": os.getpid(),
        "host": socket.gethostname(),
        "owner": owner,
        "started_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(
            timespec="seconds").replace("+00:00", "Z"),
    }
    handle.seek(0)
    handle.truncate()
    handle.write(json.dumps(metadata, separators=(",", ":")).encode("utf-8"))
    handle.flush()
    return CheckoutBuildLock(handle, path, platform_lock)


def inherited_lock_parent_pid() -> int | None:
    """Return the build.py parent marker for its nested DLL build, if valid."""
    try:
        parent_pid = int(os.environ.get("MIMITA_BUILD_LOCK_PARENT_PID", ""))
    except ValueError:
        return None
    return parent_pid if parent_pid == os.getppid() else None
