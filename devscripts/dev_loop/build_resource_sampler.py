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
        self._processes: dict[int, tuple[object, dict]] = {}
        self._top_processes: list[dict] = []
        self._started = 0.0
        self._started_utc = ""
        self._setup_wall_ms = 0.0
        self._setup_cpu_ms = 0.0
        self._last_discovery = 0.0
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
        if psutil is None and self._native_host is None:
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

    def _sample(self) -> None:
        psutil = self._psutil
        if psutil is None and self._native_host is None:
            return
        now = time.monotonic()
        if psutil is not None and now - self._last_discovery >= self.process_discovery_seconds:
            self._discover_processes()
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
        top = self._top_processes
        for row in top:
            relevant.setdefault(row["pid"], row)
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
            "process_discovery_age_seconds": round(now - self._last_discovery, 3),
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
                provider = "windows_kernel32_host_only"
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
        if self._status == "capturing" and self._sample_count == 0:
            try:
                self._sample()
            except Exception as error:
                self._errors.append(
                    f"final sample failed: {type(error).__name__}: {error}"[:500])
        if self._status == "capturing":
            self._status = "captured" if self._sample_count else "partial"
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
            "provider": ("psutil_host_and_process" if self._psutil else
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
                "journal_aligned_live_mode_workload_and_frame_tick_metrics",
                "host_commit_charge_and_limit",
                "per_process_private_bytes_when_not_exposed_by_psutil",
            ] + (["tracked_process_metrics"] if not self._psutil else []) +
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
