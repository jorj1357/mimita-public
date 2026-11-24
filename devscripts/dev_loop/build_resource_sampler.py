"""Low-overhead, best-effort resource sampling for root dev-loop builds."""

from __future__ import annotations

import importlib
import json
import os
import threading
import time
import ctypes
from ctypes import wintypes
from datetime import datetime, timezone
from pathlib import Path


def _utc_now() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="milliseconds").replace(
        "+00:00", "Z")


def _parse_utc(value: object) -> float | None:
    if not isinstance(value, str) or not value:
        return None
    try:
        return datetime.fromisoformat(value.replace("Z", "+00:00")).timestamp()
    except ValueError:
        return None


def _tail_jsonl(path: Path, limit_bytes: int = 256 * 1024) -> list[dict]:
    """Read only a bounded tail so journal correlation stays cheap during builds."""
    try:
        size = path.stat().st_size
        with path.open("rb") as source:
            source.seek(max(0, size - limit_bytes))
            data = source.read(limit_bytes).decode("utf-8", "replace")
    except OSError:
        return []
    rows = []
    for line in data.splitlines():
        try:
            row = json.loads(line)
        except (json.JSONDecodeError, TypeError):
            continue
        if isinstance(row, dict):
            rows.append(row)
    return rows


def _role(name: str, pid: int, parent_pid: int | None,
          own_pid: int, build_descendants: set[int]) -> str:
    lower_name = name.lower()
    if lower_name in {"mimita.exe", "mimita"}:
        return "mimita"
    if lower_name in {"g++.exe", "gcc.exe", "cc1plus.exe", "clang++.exe",
                      "clang.exe", "ld.exe", "lld.exe", "collect2.exe"}:
        return "compiler_linker"
    if pid == own_pid:
        return "build_orchestrator"
    if pid in build_descendants:
        return "build_worker"
    return "other"


class _WindowsHostMetrics:
    """Read host CPU and memory through kernel32 without third-party packages."""

    class _FileTime(ctypes.Structure):
        _fields_ = [("low", wintypes.DWORD), ("high", wintypes.DWORD)]

    class _MemoryStatus(ctypes.Structure):
        _fields_ = [
            ("length", wintypes.DWORD), ("load", wintypes.DWORD),
            ("total_physical", ctypes.c_ulonglong),
            ("available_physical", ctypes.c_ulonglong),
            ("total_pagefile", ctypes.c_ulonglong),
            ("available_pagefile", ctypes.c_ulonglong),
            ("total_virtual", ctypes.c_ulonglong),
            ("available_virtual", ctypes.c_ulonglong),
            ("available_extended_virtual", ctypes.c_ulonglong),
        ]

    def __init__(self):
        if os.name != "nt":
            raise OSError("Windows host metrics unavailable on this platform")
        self._kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
        self._kernel32.GetSystemTimes.argtypes = [
            ctypes.POINTER(self._FileTime), ctypes.POINTER(self._FileTime),
            ctypes.POINTER(self._FileTime)]
        self._kernel32.GetSystemTimes.restype = wintypes.BOOL
        self._kernel32.GlobalMemoryStatusEx.argtypes = [
            ctypes.POINTER(self._MemoryStatus)]
        self._kernel32.GlobalMemoryStatusEx.restype = wintypes.BOOL
        self._previous_cpu: tuple[int, int, int] | None = None

    @staticmethod
    def _filetime(value) -> int:
        return (int(value.high) << 32) | int(value.low)

    def sample(self) -> dict:
        idle, kernel, user = self._FileTime(), self._FileTime(), self._FileTime()
        if not self._kernel32.GetSystemTimes(
                ctypes.byref(idle), ctypes.byref(kernel), ctypes.byref(user)):
            raise ctypes.WinError(ctypes.get_last_error())
        current = (self._filetime(idle), self._filetime(kernel),
                   self._filetime(user))
        cpu_percent = None
        if self._previous_cpu is not None:
            idle_delta = current[0] - self._previous_cpu[0]
            kernel_delta = current[1] - self._previous_cpu[1]
            user_delta = current[2] - self._previous_cpu[2]
            total = kernel_delta + user_delta
            if total > 0:
                cpu_percent = max(0.0, min(
                    100.0, 100.0 * (total - idle_delta) / total))
        self._previous_cpu = current

        memory = self._MemoryStatus()
        memory.length = ctypes.sizeof(memory)
        if not self._kernel32.GlobalMemoryStatusEx(ctypes.byref(memory)):
            raise ctypes.WinError(ctypes.get_last_error())
        return {
            "logical_cpu_count": max(1, os.cpu_count() or 1),
            "cpu_percent": cpu_percent,
            "memory_total_bytes": int(memory.total_physical),
            "memory_available_bytes": int(memory.available_physical),
            "memory_used_percent": float(memory.load),
            "physical_used_bytes": int(memory.total_physical -
                                       memory.available_physical),
            "pagefile_total_bytes": int(memory.total_pagefile),
            "pagefile_used_bytes": int(memory.total_pagefile -
                                       memory.available_pagefile),
            "disk_read_bytes_total": None,
            "disk_write_bytes_total": None,
        }


class _WindowsProcessMetrics:
    """Enumerate process CPU and memory without requiring psutil."""

    _SNAPSHOT_PROCESS = 0x00000002
    _PROCESS_QUERY_LIMITED_INFORMATION = 0x1000
    _PROCESS_QUERY_INFORMATION = 0x0400
    _PROCESS_VM_READ = 0x0010
    _INVALID_HANDLE_VALUE = ctypes.c_void_p(-1).value

    class _ProcessEntry(ctypes.Structure):
        _fields_ = [
            ("dwSize", wintypes.DWORD),
            ("cntUsage", wintypes.DWORD),
            ("th32ProcessID", wintypes.DWORD),
            ("th32DefaultHeapID", ctypes.c_size_t),
            ("th32ModuleID", wintypes.DWORD),
            ("cntThreads", wintypes.DWORD),
            ("th32ParentProcessID", wintypes.DWORD),
            ("pcPriClassBase", wintypes.LONG),
            ("dwFlags", wintypes.DWORD),
            ("szExeFile", wintypes.WCHAR * 260),
        ]

    class _FileTime(ctypes.Structure):
        _fields_ = [("low", wintypes.DWORD), ("high", wintypes.DWORD)]

    class _ProcessMemoryCountersEx(ctypes.Structure):
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

    class _IoCounters(ctypes.Structure):
        _fields_ = [
            ("ReadOperationCount", ctypes.c_ulonglong),
            ("WriteOperationCount", ctypes.c_ulonglong),
            ("OtherOperationCount", ctypes.c_ulonglong),
            ("ReadTransferCount", ctypes.c_ulonglong),
            ("WriteTransferCount", ctypes.c_ulonglong),
            ("OtherTransferCount", ctypes.c_ulonglong),
        ]

    def __init__(self):
        if os.name != "nt":
            raise OSError("Windows process metrics unavailable on this platform")
        self._kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
        self._psapi = ctypes.WinDLL("psapi", use_last_error=True)
        self._kernel32.CreateToolhelp32Snapshot.argtypes = [
            wintypes.DWORD, wintypes.DWORD]
        self._kernel32.CreateToolhelp32Snapshot.restype = wintypes.HANDLE
        self._kernel32.Process32FirstW.argtypes = [
            wintypes.HANDLE, ctypes.POINTER(self._ProcessEntry)]
        self._kernel32.Process32FirstW.restype = wintypes.BOOL
        self._kernel32.Process32NextW.argtypes = [
            wintypes.HANDLE, ctypes.POINTER(self._ProcessEntry)]
        self._kernel32.Process32NextW.restype = wintypes.BOOL
        self._kernel32.OpenProcess.argtypes = [
            wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
        self._kernel32.OpenProcess.restype = wintypes.HANDLE
        self._kernel32.GetProcessTimes.argtypes = [
            wintypes.HANDLE, ctypes.POINTER(self._FileTime),
            ctypes.POINTER(self._FileTime), ctypes.POINTER(self._FileTime),
            ctypes.POINTER(self._FileTime)]
        self._kernel32.GetProcessTimes.restype = wintypes.BOOL
        self._kernel32.CloseHandle.argtypes = [wintypes.HANDLE]
        self._kernel32.CloseHandle.restype = wintypes.BOOL
        self._psapi.GetProcessMemoryInfo.argtypes = [
            wintypes.HANDLE, ctypes.POINTER(self._ProcessMemoryCountersEx),
            wintypes.DWORD]
        self._psapi.GetProcessMemoryInfo.restype = wintypes.BOOL
        self._kernel32.GetProcessIoCounters.argtypes = [
            wintypes.HANDLE, ctypes.POINTER(self._IoCounters)]
        self._kernel32.GetProcessIoCounters.restype = wintypes.BOOL

    @staticmethod
    def _filetime_value(value) -> int:
        return (int(value.high) << 32) | int(value.low)

    def memory(self, pid: int) -> dict:
        process = self._kernel32.OpenProcess(
            self._PROCESS_QUERY_INFORMATION | self._PROCESS_VM_READ,
            False, int(pid))
        if not process:
            return {}
        try:
            memory = self._ProcessMemoryCountersEx()
            memory.cb = ctypes.sizeof(memory)
            if not self._psapi.GetProcessMemoryInfo(
                    process, ctypes.byref(memory), memory.cb):
                return {}
            result = {
                "rss_bytes": int(memory.WorkingSetSize),
                "private_bytes": int(memory.PrivateUsage),
                "pagefile_usage_bytes": int(memory.PagefileUsage),
            }
            io = self._IoCounters()
            io_available = bool(self._kernel32.GetProcessIoCounters(
                process, ctypes.byref(io)))
            result["process_io_counters_available"] = io_available
            for name, value in (
                    ("read_bytes_total", io.ReadTransferCount),
                    ("write_bytes_total", io.WriteTransferCount),
                    ("other_bytes_total", io.OtherTransferCount),
                    ("read_operations_total", io.ReadOperationCount),
                    ("write_operations_total", io.WriteOperationCount),
                    ("other_operations_total", io.OtherOperationCount)):
                result[f"process_io_{name}"] = int(value) if io_available else None
            return result
        finally:
            self._kernel32.CloseHandle(process)

    def sample(self) -> list[dict]:
        snapshot = self._kernel32.CreateToolhelp32Snapshot(
            self._SNAPSHOT_PROCESS, 0)
        if not snapshot or snapshot == self._INVALID_HANDLE_VALUE:
            raise ctypes.WinError(ctypes.get_last_error())
        rows: list[dict] = []
        try:
            entry = self._ProcessEntry()
            entry.dwSize = ctypes.sizeof(entry)
            has_entry = self._kernel32.Process32FirstW(snapshot,
                                                       ctypes.byref(entry))
            while has_entry:
                pid = int(entry.th32ProcessID)
                row = {
                    "pid": pid,
                    "parent_pid": int(entry.th32ParentProcessID),
                    "executable": str(entry.szExeFile),
                    "thread_count": int(entry.cntThreads),
                    "create_time": None,
                    "cpu_time_100ns": None,
                }
                process = self._kernel32.OpenProcess(
                    self._PROCESS_QUERY_LIMITED_INFORMATION,
                    False, pid)
                if process:
                    try:
                        created = self._FileTime()
                        exited = self._FileTime()
                        kernel = self._FileTime()
                        user = self._FileTime()
                        if self._kernel32.GetProcessTimes(
                                process, ctypes.byref(created),
                                ctypes.byref(exited), ctypes.byref(kernel),
                                ctypes.byref(user)):
                            row["create_time"] = self._filetime_value(created)
                            row["cpu_time_100ns"] = (
                                self._filetime_value(kernel) +
                                self._filetime_value(user))
                    finally:
                        self._kernel32.CloseHandle(process)
                rows.append(row)
                entry.dwSize = ctypes.sizeof(entry)
                has_entry = self._kernel32.Process32NextW(
                    snapshot, ctypes.byref(entry))
        finally:
            self._kernel32.CloseHandle(snapshot)
        return rows


class BuildResourceSampler:
    """Append host/build/process samples without allowing telemetry to fail a build."""

    def __init__(self, root: Path, attempt_id: str,
                 progress_path: Path | None = None,
                 interval_seconds: float = 1.0,
                 process_discovery_seconds: float = 5.0):
        self.root = Path(root)
        self.attempt_id = attempt_id
        self.progress_path = progress_path or self.root / "build" / "game" / "progress.json"
        self.interval_seconds = max(0.5, min(float(interval_seconds), 10.0))
        self.process_discovery_seconds = max(2.0, float(process_discovery_seconds))
        timing_root = self.root / ".dev" / "build-timings"
        self.jsonl_path = timing_root / f"{attempt_id}.resource.jsonl"
        self.summary_path = timing_root / f"{attempt_id}.resource.json"
        self._stop = threading.Event()
        self._thread: threading.Thread | None = None
        self._psutil = None
        self._native_host = None
        self._native_processes = None
        self._processes: dict[int, tuple[object, dict]] = {}
        self._native_process_history: dict[int, tuple[int, int, float]] = {}
        self._native_tracked_processes: dict[int, dict] = {}
        self._native_io_history: dict[int, tuple] = {}
        self._top_processes: list[dict] = []
        self._started = 0.0
        self._started_utc = ""
        self._setup_wall_ms = 0.0
        self._setup_cpu_ms = 0.0
        self._last_discovery = 0.0
        self._last_discovery_wall_ms = 0.0
        self._process_discovery_wall_ms_total = 0.0
        self._process_cpu_observed = False
        self._runtime_context_refreshed = 0.0
        self._runtime_context_by_pid: dict[int, dict] = {}
        self._runtime_manifest_by_pid: dict[int, dict] = {}
        self._runtime_manifest_refreshed = 0.0
        self._runtime_manifest_pid_set: set[int] = set()
        self._runtime_context_wall_ms = 0.0
        self._runtime_context_wall_ms_total = 0.0
        self._last_sample_monotonic = 0.0
        self._sample_count = 0
        self._sampler_thread_cpu_ms = 0.0
        self._host_cpu: list[float] = []
        self._available_memory: list[int] = []
        self._errors: list[str] = []
        self._status = "starting"
        self._write_lock = threading.Lock()
        self._summary: dict | None = None

    def _append(self, event: dict) -> None:
        event.update({"build_attempt_id": self.attempt_id,
                      "time_utc": _utc_now()})
        with self._write_lock:
            self.jsonl_path.parent.mkdir(parents=True, exist_ok=True)
            with self.jsonl_path.open("a", encoding="utf-8", newline="\n") as output:
                output.write(json.dumps(event, separators=(",", ":")) + "\n")

    def _discover_processes(self) -> None:
        psutil = self._psutil
        if psutil is None:
            self._discover_native_processes()
            return
        own_pid = os.getpid()
        discovered = []
        for process in psutil.process_iter(("pid", "ppid", "name", "create_time")):
            try:
                info = process.info
                pid = int(info.get("pid") or 0)
                discovered.append((process, info, pid, int(info.get("ppid") or 0)))
            except (psutil.NoSuchProcess, psutil.AccessDenied, psutil.ZombieProcess):
                continue
        descendants = {own_pid}
        changed = True
        while changed:
            changed = False
            for _process, _info, pid, parent_pid in discovered:
                if parent_pid in descendants and pid not in descendants:
                    descendants.add(pid)
                    changed = True
        current: dict[int, tuple[object, dict]] = {}
        ranked: list[dict] = []
        logical = max(1, int(psutil.cpu_count() or 1))
        for process, info, pid, parent_pid in discovered:
            try:
                name = str(info.get("name") or "")
                metadata = {
                    "pid": pid,
                    "parent_pid": parent_pid,
                    "executable": name,
                    "role": _role(name, pid, parent_pid, own_pid, descendants),
                    "create_time": info.get("create_time"),
                }
                previous = self._processes.get(pid)
                if previous is not None:
                    process = previous[0]
                if metadata["role"] == "other":
                    # Top competitors are refreshed at the slower discovery
                    # cadence; do not reset MiMITA/compiler counters here.
                    cpu_one_core = float(process.cpu_percent(interval=None))
                elif previous is None:
                    # Prime only newly discovered target processes. The first
                    # one-second sample will then measure their CPU delta.
                    process.cpu_percent(interval=None)
                    cpu_one_core = 0.0
                else:
                    cpu_one_core = 0.0
                current[pid] = (process, metadata)
                if metadata["role"] == "other":
                    ranked.append({**metadata,
                                   "cpu_percent_one_logical_cpu": cpu_one_core,
                                   "cpu_percent_whole_host": cpu_one_core / logical,
                                   "rss_bytes": None, "private_bytes": None,
                                   "pagefile_usage_bytes": None})
            except (psutil.NoSuchProcess, psutil.AccessDenied, psutil.ZombieProcess):
                continue
        self._processes = current
        # Only memory-sample the bounded top list; full process scans are limited
        # to the configured discovery cadence to keep observer overhead low.
        for row in sorted(ranked,
                          key=lambda item: item["cpu_percent_one_logical_cpu"],
                          reverse=True)[:8]:
            pair = current.get(row["pid"])
            if pair is not None:
                try:
                    memory = pair[0].memory_info()
                    row["rss_bytes"] = int(memory.rss)
                    row["private_bytes"] = getattr(memory, "private", None)
                    row["pagefile_usage_bytes"] = getattr(memory, "pagefile", None)
                except (psutil.NoSuchProcess, psutil.AccessDenied, psutil.ZombieProcess):
                    pass
        self._top_processes = sorted(
            ranked, key=lambda item: item["cpu_percent_one_logical_cpu"],
            reverse=True)[:8]
        self._last_discovery = time.monotonic()

    def _discover_native_processes(self) -> None:
        if self._native_processes is None:
            return
        now = time.monotonic()
        snapshots = self._native_processes.sample()
        own_pid = os.getpid()
        descendants = {own_pid}
        changed = True
        while changed:
            changed = False
            for row in snapshots:
                pid = int(row["pid"])
                if int(row["parent_pid"]) in descendants and pid not in descendants:
                    descendants.add(pid)
                    changed = True
        current_history: dict[int, tuple[int, int, float]] = {}
        ranked: list[dict] = []
        logical = max(1, int(os.cpu_count() or 1))
        for row in snapshots:
            pid = int(row["pid"])
            create_time = row.get("create_time")
            cpu_time = row.get("cpu_time_100ns")
            previous = self._native_process_history.get(pid)
            cpu_one = None
            if (create_time is not None and cpu_time is not None and
                    previous is not None and previous[0] == create_time):
                elapsed = now - previous[2]
                if elapsed > 0:
                    cpu_one = max(0.0, (cpu_time - previous[1]) /
                                  10_000_000.0 / elapsed * 100.0)
            if create_time is not None and cpu_time is not None:
                current_history[pid] = (create_time, cpu_time, now)
            metadata = {
                "pid": pid,
                "parent_pid": int(row["parent_pid"]),
                "executable": row["executable"],
                "role": _role(row["executable"], pid,
                              int(row["parent_pid"]), own_pid, descendants),
                "create_time": create_time,
                "thread_count": row.get("thread_count"),
                "cpu_percent_one_logical_cpu": (round(cpu_one, 3)
                                                 if cpu_one is not None else None),
                "cpu_percent_whole_host": (round(cpu_one / logical, 3)
                                           if cpu_one is not None else None),
                "rss_bytes": row.get("rss_bytes"),
                "private_bytes": row.get("private_bytes"),
                "pagefile_usage_bytes": row.get("pagefile_usage_bytes"),
            }
            ranked.append(metadata)
        self._native_process_history = current_history
        top = sorted(
            (row for row in ranked
             if row["cpu_percent_one_logical_cpu"] is not None),
            key=lambda item: item["cpu_percent_one_logical_cpu"] or 0.0,
            reverse=True)[:8]
        relevant_roles = {"mimita", "compiler_linker", "build_worker",
                          "build_orchestrator"}
        tracked = {row["pid"]: row for row in ranked
                   if row["role"] in relevant_roles}
        selected = {row["pid"]: row for row in top}
        selected.update(tracked)
        for pid, row in selected.items():
            try:
                row.update(self._native_processes.memory(pid))
                self._update_native_process_io_rates(row, now)
            except (OSError, AttributeError, ctypes.ArgumentError):
                pass
        self._top_processes = top
        self._native_tracked_processes = tracked
        self._last_discovery = now

    def _update_native_process_io_rates(self, row: dict, now: float) -> None:
        """Add bounded process-I/O deltas; these are not physical-disk rates."""
        row["process_io_sample_interval_seconds"] = None
        pid = int(row["pid"])
        create_time = row.get("create_time")
        names = ("read_bytes_total", "write_bytes_total", "other_bytes_total",
                 "read_operations_total", "write_operations_total",
                 "other_operations_total")
        values = [row.get(f"process_io_{name}") for name in names]
        if create_time is None or any(not isinstance(value, int) for value in values):
            row["process_io_sample_interval_seconds"] = None
            return
        previous = self._native_io_history.get(pid)
        if previous is not None and previous[0] == create_time:
            elapsed = now - previous[-1]
            if elapsed > 0:
                row["process_io_sample_interval_seconds"] = round(elapsed, 3)
                for index, name in enumerate(names):
                    delta = max(0, values[index] - previous[index + 1])
                    rate_name = name.removesuffix("_total") + "_per_second"
                    row[f"process_io_{rate_name}"] = round(delta / elapsed, 3)
        self._native_io_history[pid] = (create_time, *values, now)

    def _refresh_processes(self) -> None:
        if self._psutil is None and self._native_processes is None:
            return
        started = time.perf_counter()
        try:
            self._discover_processes()
        except Exception as error:
            # Keep host sampling alive when a process snapshot or a process
            # handle is temporarily unavailable.
            self._last_discovery = time.monotonic()
            message = f"process discovery: {type(error).__name__}: {error}"
            self._errors.append(message[:500])
            try:
                self._append({"event": "resource.process_discovery_error",
                              "error": message[:500]})
            except OSError:
                pass
        finally:
            self._last_discovery_wall_ms = max(
                0.0, time.perf_counter() - started) * 1000.0
            self._process_discovery_wall_ms_total += self._last_discovery_wall_ms

    def _build_state(self) -> dict:
        runtime = {}
        try:
            with self.progress_path.open("r", encoding="utf-8") as source:
                progress = json.load(source)
            active = progress.get("active_source_started")
            active_rows = []
            if isinstance(active, dict):
                for path, values in list(active.items())[:20]:
                    if isinstance(values, dict):
                        active_rows.append({
                            "path": path,
                            "queue_wait_ms": values.get("queue_wait_ms"),
                            "elapsed_ms": values.get("elapsed_ms"),
                            "started_utc": values.get("started_utc"),
                        })
            state_path = self.root / ".dev" / "state.json"
            try:
                state = json.loads(state_path.read_text(encoding="utf-8"))
                metadata = state.get("run_metadata")
                metadata = metadata if isinstance(metadata, dict) else {}
                runtime = {
                    "run_id": state.get("run_id"),
                    "reported_mode": state.get("effective_mode"),
                    "reported_map": state.get("effective_map"),
                    "reported_npcs": state.get("effective_npcs"),
                    "state_updated_utc": state.get("updated_at"),
                    "server_journal_path": metadata.get("server_journal_path"),
                    "client_journal_path": metadata.get("client_journal_path"),
                    "mode_source": "dev-loop state; verify against active journal",
                }
            except (OSError, json.JSONDecodeError, TypeError):
                runtime = {"status": "unknown", "reason": "run_state_unavailable"}
            return {
                "phase": progress.get("phase"),
                "elapsed_seconds": progress.get("elapsed_s"),
                "completed": progress.get("completed"),
                "total": progress.get("total"),
                "compiled": progress.get("compiled"),
                "skipped": progress.get("skipped"),
                "active_sources": active_rows,
                "runtime_context": runtime,
            }
        except (OSError, json.JSONDecodeError, TypeError):
            return {"status": "unknown", "reason": "progress_unavailable"}

    def _process_start_times(self) -> dict[int, float]:
        starts = {}
        if self._psutil is not None:
            for pid, (_process, metadata) in self._processes.items():
                try:
                    starts[int(pid)] = float(metadata["create_time"])
                except (KeyError, TypeError, ValueError):
                    continue
        else:
            # Native FILETIME values count 100 ns intervals from 1601-01-01.
            for pid, (created, _cpu, _when) in self._native_process_history.items():
                starts[int(pid)] = created / 10_000_000.0 - 11_644_473_600.0
        return starts

    def _read_runtime_journal(self, journal_path: str, pid: int,
                              run_id: str) -> dict:
        path = Path(journal_path)
        if not path.is_absolute():
            path = self.root / path
        candidates = [path]
        try:
            siblings = sorted(path.parent.glob("events*.jsonl"),
                              key=lambda item: item.stat().st_mtime,
                              reverse=True)
            candidates = list(dict.fromkeys([*siblings[:3], path]))
        except OSError:
            pass
        rows = []
        for candidate in candidates:
            rows.extend(_tail_jsonl(candidate))
        matching = []
        for row in rows:
            if (row.get("pid") != int(pid) or
                    str(row.get("run_id") or "") != str(run_id)):
                continue
            fields = row.get("fields") if isinstance(row.get("fields"), dict) else {}
            if (row.get("event") in {
                    "game.hotness.summary", "npc.server-phase-window",
                    "npc.phase-window", "performance.fps",
                    "client.npc.viewport-window"} or
                    fields.get("mode") or fields.get("active_mode")):
                matching.append(row)
        matching.sort(key=lambda row: _parse_utc(row.get("wall_time")) or 0.0)
        observations = {}
        field_sets = {
            "game.hotness.summary": ("generation", "dll", "npcs", "players",
                                     "server_path"),
            "npc.server-phase-window": ("npc_count", "ticks", "total_ms",
                                        "ai_ms", "pair_ms", "target_scans"),
            "npc.phase-window": ("module_generation", "updates", "total_ms",
                                 "physics_ms", "collision_ms", "decision_ms",
                                 "animation_ms"),
            "performance.fps": ("fps", "window_s", "frames",
                                "p95_frame_ms", "max_frame_ms",
                                "frame_budget_ms", "frames_over_budget"),
            "client.npc.viewport-window": ("remote_npcs", "alive", "in_view",
                                           "active_map"),
        }
        for row in matching:
            event = row["event"]
            fields = row.get("fields") if isinstance(row.get("fields"), dict) else {}
            mode = fields.get("mode") or fields.get("active_mode")
            if mode and event in {
                    "game.hotness.summary", "npc.server-phase-window",
                    "performance.server-tick-window", "server.process.heartbeat",
                    "performance.fps", "network.mode.active",
                    "client.gamemode.local-actor-identity"}:
                observations["journal_mode"] = {
                    "event": event, "observed_utc": row.get("wall_time"),
                    "mode": mode,
                }
            if event not in field_sets:
                continue
            if event == "game.hotness.summary":
                key = "generation_npc_player"
            elif event == "npc.server-phase-window":
                key = "server_tick_window"
            elif event == "npc.phase-window":
                key = "npc_phase_window"
            elif event == "performance.fps":
                key = "client_frame_window"
            else:
                key = "client_npc_view_window"
            observations[key] = {
                "event": event,
                "observed_utc": row.get("wall_time"),
                "fields": {name: fields[name] for name in field_sets[event]
                           if name in fields},
            }
        if observations:
            observed = [(_parse_utc(item.get("observed_utc")) or 0.0)
                        for item in observations.values()]
            newest = max(observed, default=0.0)
            observations["newest_observation_age_seconds"] = (
                round(max(0.0, time.time() - newest), 3) if newest else None)
            observations["actual_mode"] = (
                observations.get("journal_mode", {}).get("mode"))
        else:
            observations = {"status": "no_matching_recent_events"}
        return observations

    def _refresh_runtime_context(self) -> None:
        started = time.perf_counter()
        contexts: dict[int, dict] = {}
        starts = self._process_start_times()
        if self._psutil is not None:
            observed_pids = {int(pid) for pid, (_process, metadata) in
                             self._processes.items()
                             if metadata.get("role") == "mimita"}
        else:
            observed_pids = {int(pid) for pid, metadata in
                             self._native_tracked_processes.items()
                             if metadata.get("role") == "mimita"}
        if (time.monotonic() - self._runtime_manifest_refreshed >= 60.0 or
                observed_pids != self._runtime_manifest_pid_set):
            manifest_contexts: dict[int, dict] = {}

            def index_instance(run: dict, instance: dict, source: str) -> None:
                run_id = str(run.get("run_id") or "")
                runtime_run_id = str(instance.get("runtime_run_id") or "")
                if (runtime_run_id and run_id and
                        runtime_run_id != run_id):
                    # A root state snapshot can adopt a process that still
                    # belongs to an isolated runtime. Its copied journal path
                    # is not authoritative; let that runtime's own manifest
                    # provide the PID/run/journal tuple.
                    return
                try:
                    pid = int(instance.get("pid") or 0)
                except (TypeError, ValueError):
                    return
                if (pid not in starts or pid not in observed_pids or
                        not instance.get("journal_path")):
                    return
                exact_start = _parse_utc(
                    instance.get("process_started_utc") or
                    instance.get("started_utc"))
                declared_start = exact_start
                identity_basis = "instance_process_start"
                if declared_start is None:
                    # Older run manifests can omit per-instance process start
                    # fields. Use the instance launch token first, then the run
                    # creation time, and fail closed outside a short launch
                    # window so a reused PID cannot inherit an old run's role.
                    declared_start = _parse_utc(instance.get("launch_id"))
                    identity_basis = "instance_launch_id"
                    if declared_start is None:
                        declared_start = _parse_utc(run.get("created_utc"))
                        identity_basis = "run_created_utc"
                if declared_start is None:
                    return
                start_delta = starts[pid] - declared_start
                if exact_start is not None:
                    if abs(start_delta) > 5.0:
                        return
                elif start_delta < 0.0 or start_delta > 120.0:
                    return
                process_executable = None
                if self._psutil is not None:
                    pair = self._processes.get(pid)
                    metadata = pair[1] if pair else {}
                    process_executable = metadata.get("executable")
                else:
                    metadata = self._native_tracked_processes.get(pid, {})
                    process_executable = metadata.get("executable")
                manifest_executable = instance.get("executable")
                if manifest_executable and process_executable:
                    manifest_name = (str(manifest_executable)
                                     .replace("\\", "/").rsplit("/", 1)[-1]
                                     .casefold())
                    process_name = (str(process_executable)
                                    .replace("\\", "/").rsplit("/", 1)[-1]
                                    .casefold())
                    if manifest_name != process_name:
                        return
                run_created = _parse_utc(run.get("created_utc"))
                run_created_delta = (abs(starts[pid] - run_created)
                                     if run_created is not None else float("inf"))
                existing = manifest_contexts.get(pid)
                candidate_rank = (0 if exact_start is not None else 1,
                                  abs(start_delta), run_created_delta)
                if existing is not None:
                    existing_rank = (
                        0 if existing.get("identity_match_basis") ==
                        "instance_process_start" else 1,
                        abs(float(existing.get(
                            "identity_start_delta_seconds") or 0.0)),
                        float(existing.get(
                            "identity_run_created_delta_seconds") or
                            float("inf")))
                    if candidate_rank >= existing_rank:
                        return
                manifest_contexts[pid] = {
                    "run_id": runtime_run_id or run_id,
                    "instance_id": instance.get("instance_id"),
                    "role": instance.get("role"),
                    "pid": pid,
                    "manifest_source": source,
                    "manifest_mode": instance.get("mode") or run.get("mode"),
                    "manifest_map": instance.get("map") or run.get("map"),
                    "game_generation": ((run.get("build_identity") or {}).get(
                        "game_generation") or run.get("game_generation")),
                    "journal_path": str(instance.get("journal_path")),
                    "process_started_utc": instance.get("process_started_utc") or
                                           instance.get("started_utc"),
                    "identity_match_basis": identity_basis,
                    "identity_start_delta_seconds": round(start_delta, 3),
                    "identity_run_created_delta_seconds": (
                        round(run_created_delta, 3)
                        if run_created_delta != float("inf") else None),
                }

            state_path = self.root / ".dev" / "state.json"
            try:
                state = json.loads(state_path.read_text(encoding="utf-8"))
                run = {"run_id": state.get("run_id"),
                       "mode": state.get("effective_mode"),
                       "map": state.get("effective_map"),
                       "game_generation": state.get("game_generation")}
                for instance in state.get("instances", []):
                    index_instance(run, instance, "dev-loop-state")
            except (OSError, json.JSONDecodeError, TypeError):
                pass

            # Isolated dev-loop runs are absent from the root state file.
            # Cache manifests and rescan only when the PID set changes or once
            # per minute; journal tails are refreshed at the sample cadence.
            logs = self.root / "logs"
            try:
                manifests = sorted(logs.glob("*/*/run.json"),
                                   key=lambda item: item.stat().st_mtime,
                                   reverse=True)[:500]
            except OSError:
                manifests = []
            for manifest_path in manifests:
                try:
                    run = json.loads(manifest_path.read_text(encoding="utf-8"))
                except (OSError, json.JSONDecodeError, TypeError):
                    continue
                for instance in run.get("instances", []):
                    index_instance(run, instance, "run-manifest")
            self._runtime_manifest_by_pid = manifest_contexts
            self._runtime_manifest_refreshed = time.monotonic()
            self._runtime_manifest_pid_set = observed_pids

        for pid, manifest in self._runtime_manifest_by_pid.items():
            if pid not in starts:
                continue
            row = dict(manifest)
            row["journal"] = self._read_runtime_journal(
                row["journal_path"], pid, row["run_id"])
            contexts[pid] = row
        self._runtime_context_by_pid = contexts
        self._runtime_context_refreshed = time.monotonic()
        self._runtime_context_wall_ms = max(
            0.0, time.perf_counter() - started) * 1000.0
        self._runtime_context_wall_ms_total += self._runtime_context_wall_ms

    def _runtime_contexts(self) -> dict[int, dict]:
        if (time.monotonic() - self._runtime_context_refreshed >=
                self.process_discovery_seconds):
            self._refresh_runtime_context()
        return self._runtime_context_by_pid

    def _runtime_alignment_summary(self) -> dict:
        rows = list(self._runtime_context_by_pid.values())

        def fresh(context: dict, *keys: str) -> bool:
            journal = context.get("journal", {})
            limit = max(10.0, self.process_discovery_seconds * 2.0)
            for key in keys:
                item = journal.get(key)
                if not isinstance(item, dict):
                    return False
                observed = _parse_utc(item.get("observed_utc"))
                if observed is None or time.time() - observed > limit:
                    return False
            return True

        servers = [row for row in rows if row.get("role") == "server" and
                   fresh(row, "generation_npc_player", "server_tick_window")]
        clients = [row for row in rows if row.get("role") == "client" and
                   fresh(row, "client_frame_window", "client_npc_view_window")]
        matched = next(((server, client) for server in servers for client in clients
                        if server.get("run_id") == client.get("run_id")), None)
        if matched is None:
            return {"status": "unavailable",
                    "pair_alignment_status": "unavailable",
                    "mode_verification_status": "unverified",
                    "same_run_server_client": False,
                    "mode_match": False,
                    "reason": "fresh_server_and_client_journal_evidence_not_matched"}
        server, client = matched
        server_journal = server.get("journal", {})
        client_journal = client.get("journal", {})
        server_mode_row = server_journal.get("journal_mode")
        client_mode_row = client_journal.get("journal_mode")
        server_mode_fresh = fresh(server, "journal_mode")
        client_mode_fresh = fresh(client, "journal_mode")
        server_mode = (server_journal.get("actual_mode")
                       if server_mode_fresh else None)
        client_mode = (client_journal.get("actual_mode")
                       if client_mode_fresh else None)
        mode_match = bool(server_mode and client_mode and
                          str(server_mode).strip().casefold() ==
                          str(client_mode).strip().casefold())
        status = ("observed" if mode_match else
                  "mode_unverified" if not server_mode or not client_mode else
                  "mode_mismatch")
        reason = (None if mode_match else
                  "fresh_authoritative_mode_missing_or_stale"
                  if not server_mode or not client_mode else
                  "fresh_authoritative_modes_disagree")
        mode_verification_status = (
            "matched" if mode_match else
            "mode_mismatch" if server_mode and client_mode else
            "client_mode_unverified" if server_mode else
            "server_mode_unverified")
        return {
            "status": status,
            "pair_alignment_status": "observed",
            "mode_verification_status": mode_verification_status,
            "same_run_server_client": True,
            "mode_match": mode_match,
            "reason": reason,
            "run_id": server.get("run_id"),
            "server_pid": server.get("pid"),
            "client_pid": client.get("pid"),
            "server_actual_mode": server_mode,
            "server_authoritative_mode": server_mode,
            "server_mode_source": "server_journal" if server_mode else None,
            "client_actual_mode": client_mode,
            "client_mode_status": ("fresh" if client_mode_fresh else
                                   "stale_or_unreported"),
            "server_mode_observed_utc": (
                server_mode_row.get("observed_utc")
                if isinstance(server_mode_row, dict) else None),
            "client_mode_observed_utc": (
                client_mode_row.get("observed_utc")
                if isinstance(client_mode_row, dict) else None),
            "actual_mode": client_mode if mode_match else None,
            "server_workload": {
                "generation_npc_player": server_journal.get(
                    "generation_npc_player"),
                "server_tick_window": server_journal.get("server_tick_window"),
            },
            "client_workload": {
                "client_frame_window": client_journal.get(
                    "client_frame_window"),
                "client_npc_view_window": client_journal.get(
                    "client_npc_view_window"),
            },
            "server_observation_age_seconds": server_journal.get(
                "newest_observation_age_seconds"),
            "client_observation_age_seconds": client_journal.get(
                "newest_observation_age_seconds"),
        }

    def _sample(self) -> None:
        psutil = self._psutil
        if psutil is None and self._native_host is None:
            return
        now = time.monotonic()
        if (now - self._last_discovery >= self.process_discovery_seconds and
                (psutil is not None or self._native_processes is not None)):
            self._refresh_processes()
        else:
            self._last_discovery_wall_ms = 0.0
        if psutil is not None:
            vm = psutil.virtual_memory()
            swap = psutil.swap_memory()
            disk = psutil.disk_io_counters()
            host = {
                "logical_cpu_count": max(1, int(psutil.cpu_count() or 1)),
                "cpu_percent": float(psutil.cpu_percent(interval=None)),
                "memory_total_bytes": int(vm.total),
                "memory_available_bytes": int(vm.available),
                "memory_used_percent": float(vm.percent),
                "physical_used_bytes": int(vm.total - vm.available),
                "pagefile_total_bytes": int(swap.total),
                "pagefile_used_bytes": int(swap.used),
                "disk_read_bytes_total": getattr(disk, "read_bytes", None),
                "disk_write_bytes_total": getattr(disk, "write_bytes", None),
            }
        else:
            host = self._native_host.sample()
        relevant: dict[int, dict] = {}
        logical = host["logical_cpu_count"]
        for pid, (process, metadata) in (list(self._processes.items()) if psutil else []):
            if metadata["role"] == "other":
                continue
            try:
                cpu_one_core = float(process.cpu_percent(interval=None))
                row = {
                    **metadata,
                    "cpu_percent_one_logical_cpu": cpu_one_core,
                    "cpu_percent_whole_host": cpu_one_core / logical,
                    "rss_bytes": None,
                    "private_bytes": None,
                }
                memory = process.memory_info()
                row["rss_bytes"] = int(memory.rss)
                row["private_bytes"] = getattr(memory, "private", None)
                row["pagefile_usage_bytes"] = getattr(memory, "pagefile", None)
                relevant[pid] = row
                for top_row in self._top_processes:
                    if top_row.get("pid") == pid:
                        top_row.update({key: row[key] for key in (
                            "cpu_percent_one_logical_cpu",
                            "cpu_percent_whole_host", "rss_bytes",
                            "private_bytes", "pagefile_usage_bytes")})
                        break
            except (psutil.NoSuchProcess, psutil.AccessDenied, psutil.ZombieProcess):
                continue
        if psutil is None:
            relevant.update(self._native_tracked_processes)
        top = self._top_processes
        for row in top:
            relevant.setdefault(row["pid"], row)
        runtime_contexts = self._runtime_contexts()
        for pid, row in relevant.items():
            runtime = runtime_contexts.get(int(pid))
            if runtime:
                row["runtime_context"] = runtime
        if any(row.get("cpu_percent_one_logical_cpu") is not None
               for row in [*top, *relevant.values()]):
            self._process_cpu_observed = True
        sample = {
            "event": "resource.sample",
            "elapsed_ms": round((now - self._started) * 1000.0, 3),
            "sample_index": self._sample_count,
            "sample_interval_seconds": self.interval_seconds,
            "actual_sample_interval_seconds": (
                round(now - self._last_sample_monotonic, 3)
                if self._last_sample_monotonic else None),
            "host": host,
            "build": self._build_state(),
            "top_processes": top,
            "tracked_processes": list(relevant.values()),
            "process_discovery_age_seconds": (
                round(now - self._last_discovery, 3)
                if psutil is not None or self._native_processes is not None else None),
            "process_discovery_wall_ms": round(self._last_discovery_wall_ms, 3),
            "runtime_context_discovery_wall_ms": round(
                self._runtime_context_wall_ms, 3),
        }
        self._append(sample)
        self._sample_count += 1
        self._last_sample_monotonic = now
        if host["cpu_percent"] is not None:
            self._host_cpu.append(float(host["cpu_percent"]))
        self._available_memory.append(int(host["memory_available_bytes"]))

    def _run(self) -> None:
        while not self._stop.wait(self.interval_seconds):
            cpu_started = time.thread_time()
            try:
                self._sample()
            except Exception as error:  # telemetry must never fail a build
                message = f"{type(error).__name__}: {error}"
                self._errors.append(message[:500])
                try:
                    self._append({"event": "resource.sample_error",
                                  "error": message[:500]})
                except OSError:
                    pass
            finally:
                self._sampler_thread_cpu_ms += max(
                    0.0, time.thread_time() - cpu_started) * 1000.0

    def start(self) -> None:
        self._started = time.monotonic()
        self._started_utc = _utc_now()
        setup_wall_started = time.monotonic()
        setup_cpu_started = time.thread_time()
        try:
            try:
                self._psutil = importlib.import_module("psutil")
                self._psutil.cpu_percent(interval=None)
                self._last_discovery = self._started
                provider = "psutil_host_and_process"
            except ImportError:
                self._native_host = _WindowsHostMetrics()
                # Prime GetSystemTimes here so even a build shorter than one
                # polling interval can measure CPU from its final sample.
                self._native_host.sample()
                try:
                    self._native_processes = _WindowsProcessMetrics()
                    provider = "windows_kernel32_host_and_process"
                except (OSError, AttributeError, ctypes.ArgumentError):
                    self._native_processes = None
                    provider = "windows_kernel32_host_only"
            if self._psutil is not None or self._native_processes is not None:
                # Establish a process CPU baseline immediately so short builds
                # still report current process memory and can compute a final
                # CPU delta when they finish.
                self._refresh_processes()
            self._status = "capturing"
            self._append({"event": "resource.capture_started",
                          "sample_interval_seconds": self.interval_seconds,
                          "process_discovery_seconds": self.process_discovery_seconds,
                          "logical_cpu_count": (self._psutil.cpu_count()
                                                if self._psutil else os.cpu_count()),
                          "provider": provider})
            self._thread = threading.Thread(
                target=self._run,
                name=f"build-resource-{self.attempt_id[-12:]}", daemon=True)
            self._thread.start()
        except Exception as error:  # do not turn optional telemetry into a build failure
            self._status = "unavailable"
            self._errors.append(f"{type(error).__name__}: {error}"[:500])
            try:
                self._append({"event": "resource.capture_unavailable",
                              "reason": self._errors[-1]})
            except OSError:
                pass
        finally:
            self._setup_wall_ms = max(
                0.0, time.monotonic() - setup_wall_started) * 1000.0
            self._setup_cpu_ms = max(
                0.0, time.thread_time() - setup_cpu_started) * 1000.0

    def stop(self) -> dict:
        if self._summary is not None:
            return self._summary
        self._stop.set()
        if self._thread is not None:
            self._thread.join(timeout=max(2.0, self.interval_seconds * 2.0))
            if self._thread.is_alive():
                self._errors.append("sampler thread did not stop before timeout")
        if self._status == "capturing":
            try:
                if (self._psutil is not None or self._native_processes is not None) and (
                        time.monotonic() - self._last_discovery >=
                        min(1.0, self.process_discovery_seconds)):
                    self._refresh_processes()
                self._sample()
            except Exception as error:
                self._errors.append(
                    f"final sample failed: {type(error).__name__}: {error}"[:500])
        if self._status == "capturing":
            has_process_provider = bool(
                (self._psutil and self._processes) or
                (self._native_processes and self._native_process_history))
            self._status = ("captured" if self._sample_count and
                            has_process_provider and not self._errors else "partial")
        runtime_alignment = self._runtime_alignment_summary()
        summary = {
            "build_attempt_id": self.attempt_id,
            "status": self._status,
            "sample_count": self._sample_count,
            "sample_interval_seconds": self.interval_seconds,
            "process_discovery_seconds": self.process_discovery_seconds,
            "started_utc": self._started_utc,
            "completed_utc": _utc_now(),
            "duration_seconds": round(time.monotonic() - self._started, 3),
            "sampler_setup_wall_ms": round(self._setup_wall_ms, 3),
            "sampler_setup_cpu_ms": round(self._setup_cpu_ms, 3),
            "sampler_thread_cpu_ms": round(self._sampler_thread_cpu_ms, 3),
            "process_discovery_wall_ms_total": round(
                self._process_discovery_wall_ms_total, 3),
            "runtime_context_discovery_wall_ms_total": round(
                self._runtime_context_wall_ms_total, 3),
            "runtime_alignment": runtime_alignment,
            "provider": ("psutil_host_and_process" if self._psutil else
                         "windows_kernel32_host_and_process"
                         if self._native_processes else
                         "windows_kernel32_host_only" if self._native_host else
                         "unavailable"),
            "mean_host_cpu_percent": (round(sum(self._host_cpu) / len(self._host_cpu), 3)
                                      if self._host_cpu else None),
            "peak_host_cpu_percent": (max(self._host_cpu) if self._host_cpu else None),
            "minimum_available_memory_bytes": (min(self._available_memory)
                                                if self._available_memory else None),
            "jsonl_path": str(self.jsonl_path.relative_to(self.root)),
            "summary_path": str(self.summary_path.relative_to(self.root)),
            "errors": self._errors[-20:],
            "unavailable_fields": [
                "matched_prebuild_resource_baseline",
                "host_commit_charge_and_limit",
            ] + (["journal_aligned_live_mode_workload_and_frame_tick_metrics"]
                 if not (runtime_alignment.get("pair_alignment_status") ==
                         "observed" and
                         runtime_alignment.get("server_authoritative_mode"))
                 else []) +
                 (["client_mode_independent_confirmation"]
                  if (runtime_alignment.get("pair_alignment_status") ==
                      "observed" and
                      runtime_alignment.get("client_mode_status") != "fresh")
                  else []) +
                 (["per_process_private_bytes_when_not_exposed_by_provider"]
                 if not self._psutil and not self._native_processes else []) +
                 (["tracked_process_metrics"]
                  if not self._psutil and not self._native_processes else []) +
                 (["per_process_cpu_rate_not_observed"]
                  if (self._psutil or self._native_processes) and
                  not self._process_cpu_observed else []) +
                 (["host_process_metrics"]
                 if self._status == "unavailable" else []),
        }
        try:
            self.summary_path.parent.mkdir(parents=True, exist_ok=True)
            self.summary_path.write_text(json.dumps(summary, indent=2, sort_keys=True) + "\n",
                                         encoding="utf-8")
            self._append({"event": "resource.capture_completed", **summary})
        except Exception as error:
            summary["status"] = "partial"
            summary.setdefault("errors", []).append(
                f"resource summary write failed: {type(error).__name__}: {error}"[:500])
        self._summary = summary
        return self._summary
