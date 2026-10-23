"""Sample a Windows game process outside the game while a dev-loop run is active."""

from __future__ import annotations

import argparse
import csv
import ctypes
from ctypes import wintypes
from datetime import datetime, timezone
import os
from pathlib import Path
import time


PROCESS_QUERY_LIMITED_INFORMATION = 0x1000
PROCESS_VM_READ = 0x0010
STILL_ACTIVE = 259


class FileTime(ctypes.Structure):
    _fields_ = [("low", wintypes.DWORD), ("high", wintypes.DWORD)]


class ProcessMemoryCountersEx(ctypes.Structure):
    _fields_ = [
        ("cb", wintypes.DWORD),
        ("PageFaultCount", wintypes.DWORD),
        ("PeakWorkingSetSize", ctypes.c_size_t),
        ("WorkingSetSize", ctypes.c_size_t),
        ("QuotaPeakPagedPoolUsage", ctypes.c_size_t),
        ("QuotaPagedPoolUsage", ctypes.c_size_t),
        ("QuotaPeakNonPagedPoolUsage", ctypes.c_size_t),
        ("QuotaNonPagedPoolUsage", ctypes.c_size_t),
        ("PagefileUsage", ctypes.c_size_t),
        ("PeakPagefileUsage", ctypes.c_size_t),
        ("PrivateUsage", ctypes.c_size_t),
    ]


class MemoryStatusEx(ctypes.Structure):
    _fields_ = [
        ("dwLength", wintypes.DWORD),
        ("dwMemoryLoad", wintypes.DWORD),
        ("ullTotalPhys", ctypes.c_ulonglong),
        ("ullAvailPhys", ctypes.c_ulonglong),
        ("ullTotalPageFile", ctypes.c_ulonglong),
        ("ullAvailPageFile", ctypes.c_ulonglong),
        ("ullTotalVirtual", ctypes.c_ulonglong),
        ("ullAvailVirtual", ctypes.c_ulonglong),
        ("ullAvailExtendedVirtual", ctypes.c_ulonglong),
    ]


def filetime_value(value: FileTime) -> int:
    return (int(value.high) << 32) | int(value.low)


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="milliseconds").replace("+00:00", "Z")


def sample_process(handle: int, pid: int, previous_cpu: int | None,
                   previous_time: float | None) -> tuple[dict[str, object], int | None, float]:
    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    psapi = ctypes.WinDLL("psapi", use_last_error=True)
    exit_code = wintypes.DWORD(0)
    alive = bool(kernel32.GetExitCodeProcess(handle, ctypes.byref(exit_code)) and
                 exit_code.value == STILL_ACTIVE)

    memory = ProcessMemoryCountersEx()
    memory.cb = ctypes.sizeof(memory)
    memory_ok = bool(psapi.GetProcessMemoryInfo(
        handle, ctypes.byref(memory), ctypes.sizeof(memory)))

    system = MemoryStatusEx()
    system.dwLength = ctypes.sizeof(system)
    system_ok = bool(kernel32.GlobalMemoryStatusEx(ctypes.byref(system)))

    cpu_100ns = None
    cpu_percent = None
    if alive:
        created, exited, kernel, user = FileTime(), FileTime(), FileTime(), FileTime()
        if kernel32.GetProcessTimes(handle, ctypes.byref(created), ctypes.byref(exited),
                                    ctypes.byref(kernel), ctypes.byref(user)):
            cpu_100ns = filetime_value(kernel) + filetime_value(user)
            now = time.monotonic()
            if previous_cpu is not None and previous_time is not None and now > previous_time:
                cpu_percent = max(0.0, (cpu_100ns - previous_cpu) /
                                  10_000_000.0 / (now - previous_time) * 100.0)
        else:
            now = time.monotonic()
    else:
        now = time.monotonic()

    row: dict[str, object] = {
        "utc": utc_now(),
        "pid": pid,
        "target_alive": int(alive),
        "target_exit_code": int(exit_code.value),
        "working_set_bytes": int(memory.WorkingSetSize) if memory_ok else "",
        "private_bytes": int(memory.PrivateUsage) if memory_ok else "",
        "pagefile_usage_bytes": int(memory.PagefileUsage) if memory_ok else "",
        "peak_working_set_bytes": int(memory.PeakWorkingSetSize) if memory_ok else "",
        "system_available_physical_bytes": int(system.ullAvailPhys) if system_ok else "",
        "system_available_pagefile_bytes": int(system.ullAvailPageFile) if system_ok else "",
        "system_memory_load_percent": int(system.dwMemoryLoad) if system_ok else "",
        "process_cpu_one_core_percent": round(cpu_percent, 2) if cpu_percent is not None else "",
        "sample_error": "" if memory_ok and system_ok else "windows_memory_query_failed",
    }
    return row, cpu_100ns, now


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pid", type=int, required=True)
    parser.add_argument("--output", type=Path, required=True,
                        help="run-specific host-resource-samples.csv path")
    parser.add_argument("--interval", type=float, default=1.0)
    parser.add_argument("--duration", type=float, default=120.0)
    args = parser.parse_args()
    if os.name != "nt":
        parser.error("this recorder currently supports Windows only")
    if args.pid <= 0 or args.interval < 0.25 or args.duration <= 0:
        parser.error("pid must be positive, interval >= 0.25s, and duration positive")

    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel32.OpenProcess.restype = wintypes.HANDLE
    kernel32.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
    handle = kernel32.OpenProcess(
        PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, False, args.pid)
    if not handle:
        raise OSError(ctypes.get_last_error(), f"cannot open process {args.pid}")

    args.output.parent.mkdir(parents=True, exist_ok=True)
    fields = [
        "utc", "pid", "target_alive", "target_exit_code", "working_set_bytes",
        "private_bytes", "pagefile_usage_bytes", "peak_working_set_bytes",
        "system_available_physical_bytes", "system_available_pagefile_bytes",
        "system_memory_load_percent", "process_cpu_one_core_percent", "sample_error",
    ]
    previous_cpu = None
    previous_time = None
    deadline = time.monotonic() + args.duration
    next_sample = time.monotonic()
    try:
        with args.output.open("a", newline="", encoding="utf-8") as stream:
            writer = csv.DictWriter(stream, fieldnames=fields)
            if stream.tell() == 0:
                writer.writeheader()
            while True:
                row, previous_cpu, previous_time = sample_process(
                    handle, args.pid, previous_cpu, previous_time)
                writer.writerow(row)
                stream.flush()
                if not row["target_alive"] or time.monotonic() >= deadline:
                    break
                next_sample += args.interval
                time.sleep(max(0.0, next_sample - time.monotonic()))
    finally:
        kernel32.CloseHandle(handle)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
