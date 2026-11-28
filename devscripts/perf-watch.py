"""Sample a Windows game process outside the game while a dev-loop run is active.

Two outputs are supported:

* the original per-process resource CSV (``--output``), unchanged column set;
* an optional per-thread CPU CSV (``--threads-csv`` or ``--threads``) with the
  OS thread name, thread creation time, user/kernel CPU split, and normalized
  core/host percentages. This is the Phase 0 CPU accounting collector: it reads
  native Windows counters only (no PowerShell/WMI), and stays well under the
  0.25% whole-host collector budget from
  ``docs/specs/dev-loop/system-resource-measurement.md``.

Thread names come from ``SetThreadDescription`` (``src/utils/thread-name.h``).
When a process has no names (older builds) the name column is empty and the row
still carries the thread id and creation timestamp, so creation-order gaps
remain visible.
"""

from __future__ import annotations

import argparse
import csv
import ctypes
from ctypes import wintypes
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import time


PROCESS_QUERY_LIMITED_INFORMATION = 0x1000
PROCESS_VM_READ = 0x0010
THREAD_QUERY_LIMITED_INFORMATION = 0x0800
TH32CS_SNAPTHREAD = 0x00000004
STILL_ACTIVE = 259


class FileTime(ctypes.Structure):
    _fields_ = [("low", wintypes.DWORD), ("high", wintypes.DWORD)]


class ThreadEntry32(ctypes.Structure):
    _fields_ = [
        ("dwSize", wintypes.DWORD),
        ("cntUsage", wintypes.DWORD),
        ("th32ThreadID", wintypes.DWORD),
        ("th32OwnerProcessID", wintypes.DWORD),
        ("tpBasePri", ctypes.c_long),
        ("tpDeltaPri", ctypes.c_long),
        ("dwFlags", wintypes.DWORD),
    ]


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


KERNEL32 = ctypes.WinDLL("kernel32", use_last_error=True)
PSAPI = ctypes.WinDLL("psapi", use_last_error=True)

PROCESS_CSV_FIELDS = [
    "utc", "pid", "target_alive", "target_exit_code", "working_set_bytes",
    "private_bytes", "pagefile_usage_bytes", "peak_working_set_bytes",
    "system_available_physical_bytes", "system_available_pagefile_bytes",
    "system_memory_load_percent", "process_cpu_one_core_percent", "sample_error",
]

THREAD_CSV_FIELDS = [
    "utc", "sample_index", "interval_s", "pid", "tid", "thread_name",
    "thread_start_utc", "cpu_user_s", "cpu_kernel_s", "cpu_total_s",
    "delta_cpu_s", "core_pct", "host_pct", "host_logical_cpus",
    "host_cpu_percent", "thread_alive",
]


def filetime_value(value: FileTime) -> int:
    return (int(value.high) << 32) | int(value.low)


def filetime_to_iso(value: FileTime) -> str:
    ticks = filetime_value(value)
    if ticks <= 0:
        return ""
    # FILETIME epoch is 1601-01-01; 11644473600 seconds to the Unix epoch.
    seconds = ticks / 10_000_000.0 - 11_644_473_600.0
    return datetime.fromtimestamp(seconds, timezone.utc).isoformat(
        timespec="milliseconds").replace("+00:00", "Z")


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="milliseconds").replace("+00:00", "Z")


def logical_cpu_count() -> int:
    try:
        return max(1, int(os.cpu_count() or 1))
    except Exception:
        return 1


class HostCpuSampler:
    """Whole-host CPU from GetSystemTimes deltas (no WMI, no PowerShell)."""

    def __init__(self) -> None:
        self.previous: tuple[int, int, int] | None = None

    def sample_percent(self) -> float | None:
        idle, kernel, user = FileTime(), FileTime(), FileTime()
        if not KERNEL32.GetSystemTimes(
                ctypes.byref(idle), ctypes.byref(kernel), ctypes.byref(user)):
            return None
        current = (filetime_value(idle), filetime_value(kernel),
                   filetime_value(user))
        if self.previous is None:
            self.previous = current
            return None
        idle_delta = current[0] - self.previous[0]
        total_delta = (current[1] - self.previous[1]) + (current[2] - self.previous[2])
        self.previous = current
        if total_delta <= 0:
            return None
        busy = max(0, total_delta - idle_delta)
        return 100.0 * busy / total_delta


def sample_process(handle: int, pid: int, previous_cpu: int | None,
                   previous_time: float | None) -> tuple[dict[str, object], int | None, float]:
    api = KERNEL32
    exit_code = wintypes.DWORD(0)
    alive = bool(api.GetExitCodeProcess(handle, ctypes.byref(exit_code)) and
                 exit_code.value == STILL_ACTIVE)

    memory = ProcessMemoryCountersEx()
    memory.cb = ctypes.sizeof(memory)
    memory_ok = bool(PSAPI.GetProcessMemoryInfo(
        handle, ctypes.byref(memory), ctypes.sizeof(memory)))

    system = MemoryStatusEx()
    system.dwLength = ctypes.sizeof(system)
    system_ok = bool(api.GlobalMemoryStatusEx(ctypes.byref(system)))

    cpu_100ns = None
    cpu_percent = None
    if alive:
        created, exited, kernel, user = FileTime(), FileTime(), FileTime(), FileTime()
        if api.GetProcessTimes(handle, ctypes.byref(created), ctypes.byref(exited),
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


def enumerate_threads(pid: int) -> list[int]:
    snapshot = KERNEL32.CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0)
    if snapshot in (0, -1):
        return []
    entry = ThreadEntry32()
    entry.dwSize = ctypes.sizeof(entry)
    threads: list[int] = []
    try:
        if KERNEL32.Thread32First(snapshot, ctypes.byref(entry)):
            while True:
                if entry.th32OwnerProcessID == pid:
                    threads.append(int(entry.th32ThreadID))
                if not KERNEL32.Thread32Next(snapshot, ctypes.byref(entry)):
                    break
    finally:
        KERNEL32.CloseHandle(snapshot)
    return threads


def thread_name(handle: int) -> str:
    getter = getattr(KERNEL32, "GetThreadDescription", None)
    if getter is None:
        return ""
    pointer = ctypes.c_wchar_p()
    try:
        # Some Windows builds return a non-S_OK HRESULT (observed 0x10000000
        # from the kernel32 forwarder) while still filling the string, so the
        # name is accepted whenever it is non-empty.
        getter(handle, ctypes.byref(pointer))
        return pointer.value or ""
    finally:
        if pointer:
            KERNEL32.LocalFree(pointer)


def thread_times(handle: int) -> tuple[FileTime, int, int, int] | None:
    created, exited, kernel, user = FileTime(), FileTime(), FileTime(), FileTime()
    if not KERNEL32.GetThreadTimes(handle, ctypes.byref(created), ctypes.byref(exited),
                                   ctypes.byref(kernel), ctypes.byref(user)):
        return None
    return created, filetime_value(kernel), filetime_value(user), filetime_value(exited)


def sample_threads(pid: int, previous: dict[int, int], interval_s: float,
                   sample_index: int, host_cpu: float | None) -> list[dict[str, object]]:
    """Return one row per thread for this sample and update ``previous``."""
    rows: list[dict[str, object]] = []
    cpus = host_cpu if host_cpu is not None else 0.0
    cpu_count = logical_cpu_count()
    seen: set[int] = set()
    for tid in enumerate_threads(pid):
        seen.add(tid)
        handle = KERNEL32.OpenThread(THREAD_QUERY_LIMITED_INFORMATION, False, tid)
        if not handle:
            continue
        try:
            times = thread_times(handle)
            if times is None:
                continue
            created, kernel, user, exited = times
            total = kernel + user
            delta = None
            if tid in previous and total >= previous[tid]:
                delta = total - previous[tid]
            previous[tid] = total
            core_pct = ""
            host_pct = ""
            if delta is not None and interval_s > 0:
                core_pct = round(delta / 10_000_000.0 / interval_s * 100.0, 2)
                host_pct = round(core_pct / cpu_count, 2)
            rows.append({
                "utc": utc_now(),
                "sample_index": sample_index,
                "interval_s": round(interval_s, 3),
                "pid": pid,
                "tid": tid,
                "thread_name": thread_name(handle),
                "thread_start_utc": filetime_to_iso(created),
                "cpu_user_s": round(user / 10_000_000.0, 3),
                "cpu_kernel_s": round(kernel / 10_000_000.0, 3),
                "cpu_total_s": round(total / 10_000_000.0, 3),
                "delta_cpu_s": "" if delta is None else round(delta / 10_000_000.0, 4),
                "core_pct": core_pct,
                "host_pct": host_pct,
                "host_logical_cpus": cpu_count,
                "host_cpu_percent": "" if host_cpu is None else round(host_cpu, 2),
                "thread_alive": 0 if exited else 1,
            })
        finally:
            KERNEL32.CloseHandle(handle)
    for tid in list(previous.keys()):
        if tid not in seen:
            previous.pop(tid, None)
    return rows


def discover_run_pids(run_id: str) -> list[tuple[int, str]]:
    """Find (pid, role) pairs for a run from its run.json instances."""
    matches = sorted(Path("logs").glob(f"*/{run_id}/run.json"))
    if not matches:
        return []
    try:
        payload = json.loads(matches[-1].read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return []
    pids: list[tuple[int, str]] = []
    for instance in payload.get("instances") or []:
        pid = instance.get("pid")
        if isinstance(pid, int) and pid > 0:
            pids.append((pid, str(instance.get("role") or instance.get("instance_id") or "")))
    return pids


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pid", type=int, nargs="+", help="target process id(s)")
    parser.add_argument("--run", help="dev-loop run id; discovers server/client pids")
    parser.add_argument("--output", type=Path,
                        help="process resource CSV path (default .dev/perf-captures/perf-watch-<utc>.csv)")
    parser.add_argument("--threads", action="store_true",
                        help="also write per-thread CPU rows next to --output")
    parser.add_argument("--threads-csv", type=Path,
                        help="explicit per-thread CPU CSV path")
    parser.add_argument("--interval", type=float, default=1.0)
    parser.add_argument("--duration", type=float, default=120.0)
    parser.add_argument("--until-exit", action="store_true",
                        help="stop when every target process has exited")
    parser.add_argument("--summary", type=Path,
                        help="optional JSON summary path")
    args = parser.parse_args()
    if os.name != "nt":
        parser.error("this recorder currently supports Windows only")

    targets: list[tuple[int, str]] = []
    if args.pid:
        targets.extend((pid, "") for pid in args.pid)
    if args.run:
        discovered = discover_run_pids(args.run)
        for pid, role in discovered:
            if (pid, role) not in targets:
                targets.append((pid, role))
    if not targets:
        parser.error("provide --pid and/or --run with at least one live process")
    if args.interval < 0.25 or args.duration <= 0:
        parser.error("interval must be >= 0.25s and duration positive")

    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    output = args.output or Path(".dev/perf-captures") / f"perf-watch-{stamp}.csv"
    threads_csv = args.threads_csv
    if threads_csv is None and args.threads:
        threads_csv = output.with_name(output.stem + ".threads.csv")
    summary_path = args.summary or output.with_name(output.stem + ".summary.json")

    handles: dict[int, int] = {}
    for pid, _role in targets:
        handle = KERNEL32.OpenProcess(
            PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, False, pid)
        if handle:
            handles[pid] = handle

    output.parent.mkdir(parents=True, exist_ok=True)
    thread_previous: dict[int, dict[int, int]] = {pid: {} for pid in handles}
    host = HostCpuSampler()
    previous_cpu: dict[int, int] = {}
    previous_time: dict[int, float] = {}
    sample_index = 0
    host_samples: list[float] = []
    deadline = time.monotonic() + args.duration
    next_sample = time.monotonic()
    last_sample_monotonic: float | None = None
    started_utc = utc_now()

    thread_stream = None
    thread_writer = None
    if threads_csv is not None:
        threads_csv.parent.mkdir(parents=True, exist_ok=True)
        thread_stream = threads_csv.open("a", newline="", encoding="utf-8")
        thread_writer = csv.DictWriter(thread_stream, fieldnames=THREAD_CSV_FIELDS)
        if thread_stream.tell() == 0:
            thread_writer.writeheader()

    try:
        with output.open("a", newline="", encoding="utf-8") as stream:
            writer = csv.DictWriter(stream, fieldnames=PROCESS_CSV_FIELDS)
            if stream.tell() == 0:
                writer.writeheader()
            while True:
                sample_index += 1
                now_monotonic = time.monotonic()
                actual_interval = (args.interval if last_sample_monotonic is None
                                   else max(0.001, now_monotonic - last_sample_monotonic))
                last_sample_monotonic = now_monotonic
                host_cpu = host.sample_percent()
                if host_cpu is not None:
                    host_samples.append(host_cpu)
                alive_any = False
                for pid, _role in targets:
                    handle = handles.get(pid)
                    if not handle:
                        continue
                    row, cpu, now = sample_process(
                        handle, pid, previous_cpu.get(pid), previous_time.get(pid))
                    if cpu is not None:
                        previous_cpu[pid] = cpu
                        previous_time[pid] = now
                    writer.writerow(row)
                    if row["target_alive"]:
                        alive_any = True
                    if thread_writer is not None:
                        for thread_row in sample_threads(
                                pid, thread_previous[pid], actual_interval,
                                sample_index, host_cpu):
                            thread_writer.writerow(thread_row)
                stream.flush()
                if thread_stream is not None:
                    thread_stream.flush()
                if not alive_any or time.monotonic() >= deadline:
                    break
                if args.until_exit and not alive_any:
                    break
                next_sample += args.interval
                time.sleep(max(0.0, next_sample - time.monotonic()))
    finally:
        for handle in handles.values():
            KERNEL32.CloseHandle(handle)
        if thread_stream is not None:
            thread_stream.close()

    summary = {
        "schema_version": 1,
        "collector": "devscripts/perf-watch.py",
        "collector_pid": os.getpid(),
        "started_utc": started_utc,
        "ended_utc": utc_now(),
        "sample_count": sample_index,
        "interval_seconds": args.interval,
        "logical_cpus": logical_cpu_count(),
        "host_cpu_percent_mean": round(sum(host_samples) / len(host_samples), 2)
        if host_samples else None,
        "host_cpu_percent_max": round(max(host_samples), 2) if host_samples else None,
        "targets": [{"pid": pid, "role": role} for pid, role in targets],
        "process_csv": str(output),
        "threads_csv": str(threads_csv) if threads_csv else "",
    }
    summary_path.parent.mkdir(parents=True, exist_ok=True)
    summary_path.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(summary, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
