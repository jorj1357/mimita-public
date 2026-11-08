#!/usr/bin/env python3
# 2026-11-08
# purpose
# Build-authority guard shared by the EXE builder and module worker.
# Only the dev-loop (EXE) or the running game's ModuleHost (modules) may build.
# Agents use status/ensure commands; they never acquire this authority.
# Does NOT compile and does NOT decide what to build.

from __future__ import annotations

import ctypes
import datetime
import json
import os
import secrets
from pathlib import Path

EXE_KIND = "exe"
MODULE_KIND = "module"
ENV_KEYS = {
    EXE_KIND: "MIMITA_BUILD_AUTHORITY",
    MODULE_KIND: "MIMITA_MODULE_BUILD_AUTHORITY",
}
MARKER_KEYS = {
    EXE_KIND: "MIMITA_DEV_LOOP_BUILD",
    MODULE_KIND: "MIMITA_GAME_MODULE_BUILD",
}
FILE_NAMES = {
    EXE_KIND: "build-authority.json",
    MODULE_KIND: "module-build-authority.json",
}
PARENT_ALLOWLIST = {
    # Modules are spawned by the running game's ModuleHost, whose process is
    # mimita*.exe. The EXE builder uses the dev-loop lock PID instead of a
    # process name so a random python wrapper cannot impersonate it.
    EXE_KIND: set(),
    MODULE_KIND: {"mimita.exe"},
}


def _now_utc() -> str:
    return datetime.datetime.now(datetime.timezone.utc).isoformat(
        timespec="seconds").replace("+00:00", "Z")


def authority_path(root: Path, kind: str) -> Path:
    return Path(root) / ".dev" / FILE_NAMES[kind]


def parent_process_name() -> str:
    if os.name != "nt":
        return ""
    kernel32 = ctypes.windll.kernel32
    snapshot = kernel32.CreateToolhelp32Snapshot(0x00000002, 0)
    if snapshot in (0, ctypes.c_void_p(-1).value):
        return ""
    try:
        class PROCESSENTRY32(ctypes.Structure):
            _fields_ = [
                ("dwSize", ctypes.c_ulong),
                ("cntUsage", ctypes.c_ulong),
                ("th32ProcessID", ctypes.c_ulong),
                ("th32DefaultHeapID", ctypes.c_void_p),
                ("th32ModuleID", ctypes.c_ulong),
                ("cntThreads", ctypes.c_ulong),
                ("th32ParentProcessID", ctypes.c_ulong),
                ("pcPriClassBase", ctypes.c_long),
                ("dwFlags", ctypes.c_ulong),
                ("szExeFile", ctypes.c_char * 260),
            ]

        entry = PROCESSENTRY32()
        entry.dwSize = ctypes.sizeof(PROCESSENTRY32)
        current = os.getpid()
        parent_pid = 0
        names: dict[int, str] = {}
        if not kernel32.Process32First(snapshot, ctypes.byref(entry)):
            return ""
        while True:
            names[entry.th32ProcessID] = entry.szExeFile.decode(
                "utf-8", errors="replace").lower()
            if entry.th32ProcessID == current:
                parent_pid = entry.th32ParentProcessID
            if not kernel32.Process32Next(snapshot, ctypes.byref(entry)):
                break
        return names.get(parent_pid, "")
    finally:
        kernel32.CloseHandle(snapshot)


def write_authority(root: Path, kind: str, owner: str,
                    metadata: dict | None = None) -> str:
    token = secrets.token_hex(16)
    path = authority_path(root, kind)
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(".json.tmp")
    payload = {
        "kind": kind,
        "owner": owner,
        "pid": os.getpid(),
        "token": token,
        "started_utc": _now_utc(),
        **(metadata or {}),
    }
    temporary.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
    os.replace(temporary, path)
    return token


def _read(path: Path) -> dict:
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return {}
    return data if isinstance(data, dict) else {}


def check_authority(root: Path, kind: str, owner: str) -> tuple[bool, str]:
    if os.environ.get("MIMITA_BUILD_OVERRIDE") == "1":
        print(f"[BUILD AUTHORITY] WARNING: human override active for {owner}; "
              "record this as an intentional cold build", flush=True)
        return True, "human_override"

    marker = MARKER_KEYS[kind]
    env_token = os.environ.get(ENV_KEYS[kind], "").strip()
    authority = _read(authority_path(root, kind))
    expected = str(authority.get("token", ""))
    if env_token and expected and secrets.compare_digest(env_token, expected):
        return True, "authority_token"

    if kind == EXE_KIND:
        try:
            lock_pid = int((Path(root) / ".dev" / "dev-loop.lock")
                           .read_text(encoding="utf-8").strip())
        except (OSError, ValueError):
            lock_pid = 0
        if lock_pid and os.getppid() == lock_pid:
            return True, "dev_loop_process"

    parent = parent_process_name()
    if parent in PARENT_ALLOWLIST[kind]:
        return True, f"parent:{parent}"
    if kind == MODULE_KIND and parent.startswith("mimita"):
        return True, f"parent:{parent}"

    if os.environ.get(marker) == "1" and authority:
        return True, "dev_loop_marker"

    return False, "missing_authority"


def require_authority(root: Path, kind: str, owner: str) -> None:
    allowed, reason = check_authority(root, kind, owner)
    if allowed:
        return
    raise SystemExit(
        f"[BUILD AUTHORITY] refusing to build for {owner}: {reason}.\n"
        "Builds are owned by the dev-loop (EXE) and the running game's "
        "ModuleHost (modules). Agents must run the lane dev-loop and poll "
        "status/journal. Humans may set MIMITA_BUILD_OVERRIDE=1.")
