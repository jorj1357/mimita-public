#!/usr/bin/env python3
# 2026-11-12
# purpose
# Background whole-game candidate builder. Consumes one coherent source-manifest
# snapshot, compiles the sources owned by "game", links an immutable
# game-g<N>.dll generation, and publishes build/game/current.json.
#
# Fast path:
# - persistent object cache shared across generations (objects/<flags-hash>/)
# - parallel compilation across all cores by default
# - high process priority by default
# - live progress.json + progress.log with per-file timings
#
# It never blocks the simulation thread and never requests an EXE build. The
# live coordinator (devscripts/live.py) spawns this worker in the background.
#
# Coherence: the manifest revision is the identity. Every game source hash is
# verified before compiling and again after linking. A source edit during the
# build is rejected, not silently mixed into the DLL.

from __future__ import annotations

import argparse
from collections import Counter
import ctypes
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import threading
import time
from concurrent.futures import CancelledError, ThreadPoolExecutor, as_completed
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SOURCE_ROOT = ROOT
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

ABI_VERSION = 1
GAME_OWNER = "game"
IDENTITY_HEADER = "module-identity.h"
IDENTITY_PROVIDER_SOURCE = "module-identity-provider.cpp"

FOLDED_MODULE_SYMBOLS = {
    "src/mapeditor/map-editor-module.cpp": "GetMimitaMapEditorModuleAPI",
    "src/map/map-geometry-module.cpp": "GetMimitaMapGeometryModuleAPI",
    "src/render/actor-rendering-module.cpp": "GetMimitaActorRenderingModuleAPI",
    "src/render/world-rendering-module.cpp": "GetMimitaWorldRenderingModuleAPI",
    "src/worldgen/infinite-worldgen-module.cpp": "GetMimitaInfiniteWorldgenModuleAPI",
    "src/world/chunk-collision-module.cpp": "GetMimitaChunkCollisionModuleAPI",
    "src/network/network-policy-module.cpp": "GetMimitaNetworkPolicyModuleAPI",
    "src/navigation/navigation-policy-module.cpp": "GetMimitaNavigationPolicyModuleAPI",
    "src/npc/npc-perception-module.cpp": "GetMimitaNpcPerceptionModuleAPI",
    "src/npc/npc-behavior-module.cpp": "GetMimitaNpcBehaviorModuleAPI",
}
IDENTITY_ONLY_SOURCES = {
    "src/physics/movement/actor-contact-policy-module.cpp",
}
IDENTITY_SOURCE_PATHS = set(FOLDED_MODULE_SYMBOLS) | IDENTITY_ONLY_SOURCES

_GCC_LOCATION_ERROR = re.compile(
    r"^\s*(?:[A-Za-z]:)?(?:[^:\r\n]+[\\/])*[^:\r\n]+:"
    r"\d+(?::\d+)?:\s*(?:fatal\s+)?error:")
_GCC_FATAL_ERROR = re.compile(r"^\s*[^\r\n]+:\s*fatal\s+error:")


def _is_gcc_compile_error_diagnostic(line: str) -> bool:
    """Return true only for a compiler diagnostic that makes this TU fail.

    Matching is deliberately limited to GCC/MinGW's location-prefixed errors
    (or location-free fatal errors) so notes and source snippets mentioning
    the word "error" do not cancel a valid compile wave.
    """
    return bool(_GCC_LOCATION_ERROR.match(line) or _GCC_FATAL_ERROR.match(line))


def _run_tracked_compile_process(command, kwargs, *, active_processes,
                                active_lock, stop_event, terminate_process):
    """Run a tracked compiler and stop it as soon as GCC reports a hard error."""
    process = subprocess.Popen(command, **kwargs)
    with active_lock:
        already_stopping = stop_event.is_set()
        if not already_stopping:
            active_processes[process.pid] = process
    if already_stopping:
        terminate_process(process.pid, process)

    output_lines: list[str] = []
    diagnostic_abort = False
    try:
        if process.stdout is None:
            output, _ = process.communicate()
            output_lines.append(output or "")
        else:
            for line in process.stdout:
                output_lines.append(line)
                if (not diagnostic_abort
                        and _is_gcc_compile_error_diagnostic(line)):
                    # GCC has established that this translation unit cannot
                    # compile. Stop remaining parse/codegen work now;
                    # compile_one will cancel sibling TUs on the nonzero exit.
                    diagnostic_abort = True
                    terminate_process(process.pid, process)
            process.wait()
    finally:
        if process.stdout is not None:
            process.stdout.close()
        with active_lock:
            active_processes.pop(process.pid, None)
    return_code = process.returncode
    if diagnostic_abort and return_code == 0:
        return_code = 1
    return subprocess.CompletedProcess(command, return_code,
                                        "".join(output_lines), None)

# This narrow adapter has a deliberately small include surface. Loading the
# full game PCH costs more than parsing its own headers, so benchmark it without
# the PCH while keeping the opt-out isolated to this source and cache key.
PCH_EXCLUDED_SOURCES = {
    "src/npc/npc-world-query.cpp",
    # PCH-off A/B on 2026-11-23: each of these 15 sources compiled faster
    # without the shared PCH; retain these source-specific cache-key variants.
    "src/gui/ui-system.cpp",
    "src/gui/ui-system-buttons.cpp",
    "src/gui/ui-text-input.cpp",
    "src/worldgen/infinite-worldgen-module.cpp",
    "src/render/world-rendering-module.cpp",
    "src/npc/npc-perception-module.cpp",
    "src/map/map-geometry-module.cpp",
    "src/physics/movement/actor-contact-policy-module.cpp",
    "src/world/chunk-collision-module.cpp",
    "src/navigation/navigation-policy-module.cpp",
    "src/gui/ui-tooltip.cpp",
    "src/mapeditor/map-editor-module.cpp",
    "src/network/network-policy-module.cpp",
    "src/render/actor-rendering-module.cpp",
    "src/npc/npc-behavior-module.cpp",
}

INCLUDE_DIRS = [
    "include",
    "src",
    "external/libjuice/include",
    "external/manifold-prebuilt/include",
    "external/recastnavigation/Recast/Include",
    "external/recastnavigation/Detour/Include",
    "external/recastnavigation/DetourTileCache/Include",
]

DEFINE_FLAGS = [
    "-DGLM_ENABLE_EXPERIMENTAL",
    "-DJUICE_STATIC",
    "-DMANIFOLD_PAR=-1",
    "-DMIMITA_GAME_DLL",
]

LINK_LIBS = [
    "-lglfw3",
    "-lopengl32",
    "-lgdi32",
    "-luser32",
    "-ldwmapi",
    "-lwinhttp",
    "-lws2_32",
    "-ldbghelp",
    "-lpsapi",
    "-lole32",
    "-luuid",
    "-loleaut32",
    "-lmfplat",
    "-lmfreadwrite",
    "-lmfuuid",
    "-lmf",
    "-ld3d11",
    "-ldxgi",
    "-lbcrypt",
    "-lpthread",
]

PRIORITY_CLASSES = {
    "normal": 0x00000020,
    "high": 0x00000080,
    "realtime": 0x00000100,
}


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def sha256_with_stamp(path: Path) -> tuple[str, int, int]:
    for _ in range(3):
        before = path.stat()
        digest = sha256_file(path)
        after = path.stat()
        if (before.st_size, before.st_mtime_ns) == (after.st_size, after.st_mtime_ns):
            return digest, after.st_size, after.st_mtime_ns
    return digest, after.st_size, after.st_mtime_ns


def read_json(path: Path) -> dict:
    with open(path, "r", encoding="utf-8") as handle:
        return json.load(handle)


def write_json_atomic(path: Path, payload: dict, retries: int = 5,
                      delay: float = 0.01) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(f"{path.name}.{os.getpid()}.{threading.get_ident()}.tmp")
    temporary.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    for _ in range(retries):
        try:
            os.replace(temporary, path)
            return
        except PermissionError:
            time.sleep(delay)
    os.replace(temporary, path)


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="milliseconds").replace("+00:00", "Z")


def parse_gcc_time_report(output: str, *, max_rows: int = 48) -> dict:
    """Parse a bounded GCC ``-ftime-report`` block for timing JSONL.

    Keep the compiler's phase names and wall-time columns instead of retaining
    all successful compiler output. GCC versions vary in their exact padding,
    so each row also keeps a short raw form for later interpretation.
    """
    lines = str(output or "").splitlines()
    start = next((index for index, line in enumerate(lines)
                  if line.strip().lower().startswith("time variable")), None)
    if start is None:
        return {"format": "gcc-ftime-report", "status": "missing", "rows": []}

    headers = [name.lower() for name in re.findall(
        r"\b(?:usr|sys|wall|GGC)\b", lines[start], re.IGNORECASE)]
    if not headers:
        return {
            "format": "gcc-ftime-report",
            "status": "unknown_header",
            "rows": [],
            "header": lines[start].strip()[:240],
        }

    number_pattern = re.compile(
        r"(?P<value>(?:\d+(?:\.\d*)?|\.\d+))\s*"
        r"(?P<unit>[kKmMgG]?)\s*"
        r"(?:\(\s*(?P<percent>\d+)\s*%\s*\))?")
    limit = max(1, int(max_rows))
    rows: list[dict] = []
    truncated = False
    for line in lines[start + 1:]:
        if ":" not in line:
            continue
        phase, values = line.split(":", 1)
        phase = phase.strip()
        if not phase:
            continue
        matches = list(number_pattern.finditer(values))
        if len(matches) < len(headers):
            continue
        parsed = [{
            "value": float(match.group("value")),
            "unit": match.group("unit") or "",
            "percent": (int(match.group("percent"))
                        if match.group("percent") is not None else None),
        } for match in matches[:4]]
        row = {"phase": phase, "raw": line.strip()[:240]}
        for name, value in zip(headers, parsed):
            if name == "usr":
                row["user_seconds"] = value["value"]
                row["user_percent"] = value["percent"]
            elif name == "sys":
                row["system_seconds"] = value["value"]
                row["system_percent"] = value["percent"]
            elif name == "wall":
                row["wall_seconds"] = value["value"]
                row["wall_percent"] = value["percent"]
            elif name == "ggc":
                row["ggc"] = value
        if len(rows) >= limit:
            truncated = True
            break
        rows.append(row)
    result = {
        "format": "gcc-ftime-report",
        "status": "parsed" if rows else "empty",
        "rows": rows,
        "truncated": truncated,
    }
    if not rows:
        # A successful compile may emit a GCC report layout we do not yet
        # understand. Retain only a short excerpt so the format can be fixed
        # from real build evidence without preserving unbounded compiler output.
        excerpt = "\n".join(lines[start:start + 8]).strip()
        if excerpt:
            result["excerpt"] = excerpt[:1600]
    return result


def is_unsupported_time_report(output: str) -> bool:
    """Return true only when the compiler rejects the optional report switch."""
    text = str(output or "").lower()
    mentions_flag = "-ftime-report" in text
    return mentions_flag and any(phrase in text for phrase in (
        "unrecognized command-line option", "unrecognized option",
        "unknown argument", "unknown option", "unsupported option"))


def set_process_priority(level: str) -> dict:
    if os.name != "nt":
        return {"requested": level, "status": "unsupported"}
    try:
        from ctypes import wintypes

        kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
        kernel32.GetCurrentProcess.argtypes = []
        kernel32.GetCurrentProcess.restype = wintypes.HANDLE
        kernel32.SetPriorityClass.argtypes = [wintypes.HANDLE, wintypes.DWORD]
        kernel32.SetPriorityClass.restype = wintypes.BOOL
        kernel32.GetPriorityClass.argtypes = [wintypes.HANDLE]
        kernel32.GetPriorityClass.restype = wintypes.DWORD
        process = kernel32.GetCurrentProcess()
        requested_class = PRIORITY_CLASSES[level]
        applied = bool(kernel32.SetPriorityClass(process, requested_class))
        effective_class = int(kernel32.GetPriorityClass(process))
        error_code = ctypes.get_last_error() if not applied else 0
        return {
            "requested": level,
            "requested_class": requested_class,
            "applied": applied,
            "effective_class": effective_class,
            "status": ("applied" if applied and effective_class == requested_class
                       else "mismatch" if applied else "failed"),
            "winerror": error_code,
        }
    except Exception as error:  # noqa: BLE001 - priority is best effort
        return {"requested": level, "status": "error",
                "error": str(error)[:240]}


def build_tool_creation_flags(level: str) -> int:
    """Set the configured priority on compiler/linker child processes too.

    Windows defaults new compiler processes to NORMAL unless CreateProcess gets
    a priority-class flag. The parent worker's priority is not sufficient.
    """
    if os.name != "nt":
        return 0
    return (getattr(subprocess, "CREATE_NO_WINDOW", 0)
            | PRIORITY_CLASSES[level])


class Progress:
    """Thread-safe live progress for one build."""

    def __init__(self, out_root: Path, revision: str, total: int, jobs: int,
                 priority: str, cache_dir: Path, seeded: int,
                 context: dict | None = None):
        self.out_root = out_root
        self.lock = threading.Lock()
        self.path = out_root / "progress.json"
        self.log_path = out_root / "progress.log"
        context = context or {}
        attempt_id = str(context.get("build_attempt_id") or "")
        self.attempt_id = attempt_id
        existing = {}
        if self.path.is_file():
            try:
                candidate = read_json(self.path)
            except (OSError, json.JSONDecodeError):
                candidate = {}
            if attempt_id and str(candidate.get("build_attempt_id") or "") == attempt_id:
                existing = candidate
        self.started_monotonic = float(context.get(
            "attempt_started_monotonic", time.monotonic()))
        self.phase_started_monotonic = time.monotonic()
        timing_value = context.get("timing_path")
        self.timing_path = Path(timing_value) if timing_value else None
        self.stop_heartbeat = threading.Event()
        self.dirty = True
        self.worker_started_monotonic: float | None = None
        self.data = {
            "status": "running",
            "phase": "toolchain_setup",
            "build_attempt_id": str(context.get("build_attempt_id") or ""),
            "source_generation": context.get("source_generation"),
            "build_mode": context.get("build_mode", "debug"),
            "generation": None,
            "revision": revision,
            "total": total,
            "completed": 0,
            "compiled": 0,
            "skipped": 0,
            "failed": 0,
            "cancelled": 0,
            "current": "",
            "last_completed": "",
            "active_sources": [],
            "file_events": [],
            "jobs": jobs,
            "priority": priority,
            "object_cache": str(cache_dir.relative_to(ROOT)) if cache_dir.is_relative_to(ROOT) else str(cache_dir),
            "seeded_objects": seeded,
            "started_utc": str(context.get("started_utc") or utc_now()),
            "updated_utc": utc_now(),
            "phase_started_utc": utc_now(),
            "timeline": existing.get("timeline", []),
            "notes": context.get("notes", []),
            "changed_paths": context.get("changed_paths", []),
            "elapsed_s": 0.0,
            "compile_s": 0.0,
            "compile_sum_ms": 0.0,
            "compile_min_ms": None,
            "compile_max_ms": 0.0,
            "link_s": 0.0,
            "total_s": 0.0,
            "slowest": [],
            "errors": [],
        }
        self.log_path.parent.mkdir(parents=True, exist_ok=True)
        self.log_path.write_text("", encoding="utf-8")
        self._write_locked()
        self.heartbeat = threading.Thread(
            target=self._heartbeat_loop, name="game-build-progress", daemon=True)
        self.heartbeat.start()

    def _timing_locked(self, event: dict) -> None:
        if self.timing_path is None:
            return
        try:
            self.timing_path.parent.mkdir(parents=True, exist_ok=True)
            with self.timing_path.open("a", encoding="utf-8", newline="\n") as handle:
                handle.write(json.dumps(event, separators=(",", ":")) + "\n")
                handle.flush()
        except OSError:
            pass

    def _write_locked(self) -> None:
        if self.data.get("status") == "running":
            self.data["elapsed_s"] = round(
                time.monotonic() - self.started_monotonic, 1)
            self.data["phase_elapsed_ms"] = round(
                (time.monotonic() - self.phase_started_monotonic) * 1000, 3)
        self.data["updated_utc"] = utc_now()
        if self.worker_started_monotonic is not None:
            self.data["worker_elapsed_s"] = round(
                time.monotonic() - self.worker_started_monotonic, 3)
        subphase_started = self.data.get("subphase_started_monotonic")
        if subphase_started is not None:
            self.data["subphase_elapsed_ms"] = round(
                (time.monotonic() - float(subphase_started)) * 1000, 3)
        try:
            write_json_atomic(self.path, self.data)
            self.dirty = False
        except OSError:
            self.dirty = True

    def _heartbeat_loop(self) -> None:
        while not self.stop_heartbeat.wait(0.5):
            with self.lock:
                if self.data.get("status") != "running":
                    return
                # Phase and nested-phase elapsed values move even when the
                # worker has no file-level counter to update. Keep the live
                # snapshot fresh at 2 Hz for the dashboard.
                self._write_locked()

    def _append_locked(self, line: str) -> None:
        with open(self.log_path, "a", encoding="utf-8", buffering=1) as handle:
            handle.write(line + "\n")

    def set(self, **kwargs) -> None:
        with self.lock:
            self.data.update(kwargs)
            self._write_locked()

    def log(self, line: str) -> None:
        with self.lock:
            self._append_locked(line)

    def start_source(self, path: str, *, queue_wait_ms: float = 0.0,
                     submitted_utc: str = "") -> tuple[str, float]:
        with self.lock:
            started = time.monotonic()
            started_utc = utc_now()
            active = self.data["active_sources"]
            if path not in active:
                active.append(path)
            self.data["current"] = path
            self.data.setdefault("active_source_started", {})[path] = {
                "started_utc": started_utc,
                "elapsed_ms": round((started - self.started_monotonic) * 1000, 3),
                "queue_wait_ms": round(queue_wait_ms, 3),
                "submitted_utc": submitted_utc,
            }
            self._timing_locked({
                "event": "translation_unit.started", "path": path,
                "time_utc": started_utc,
                "elapsed_ms": round((started - self.started_monotonic) * 1000, 3),
                "queue_wait_ms": round(queue_wait_ms, 3),
                "submitted_utc": submitted_utc,
                "build_attempt_id": self.data.get("build_attempt_id"),
                "revision": self.data.get("revision"),
            })
            self._write_locked()
            return started_utc, round((started - self.started_monotonic) * 1000, 3)

    def record(self, status: str, path: str, ms: float, detail: str = "",
               *, check_started_utc: str = "", check_elapsed_ms: float = 0.0,
               queue_wait_ms: float = 0.0, started_utc: str = "",
               started_elapsed_ms: float = 0.0,
               diagnostics: dict | None = None) -> None:
        with self.lock:
            record_started = time.perf_counter()
            ended_utc = utc_now()
            active_started = self.data.setdefault("active_source_started", {}).pop(path, {})
            active = self.data["active_sources"]
            if path in active:
                active.remove(path)
            self.data["last_completed"] = path
            self.data["current"] = active[-1] if active else ""
            self.data["completed"] += 1
            if status == "compiled":
                self.data["compiled"] += 1
                self.data["compile_sum_ms"] = round(
                    float(self.data.get("compile_sum_ms") or 0) + ms, 3)
                prior_min = self.data.get("compile_min_ms")
                self.data["compile_min_ms"] = round(
                    ms if prior_min is None else min(float(prior_min), ms), 3)
                self.data["compile_max_ms"] = round(
                    max(float(self.data.get("compile_max_ms") or 0), ms), 3)
            elif status == "skipped":
                self.data["skipped"] += 1
            elif status == "failed":
                self.data["failed"] += 1
            elif status == "cancelled":
                self.data["cancelled"] += 1
            if self.data.get("phase") == "compile":
                self.data["pending"] = max(0, int(self.data.get("pending", 0)) - 1)
            if status in ("compiled", "failed"):
                entry = {"path": path, "seconds": round(ms / 1000.0, 3), "status": status}
                slowest = self.data["slowest"]
                slowest.append(entry)
                slowest.sort(key=lambda item: item["seconds"], reverse=True)
                del slowest[10:]
            if status == "failed" and detail:
                self.data["errors"].append({"path": path, "output": detail[:4000]})
            file_event = {
                "sequence": self.data["completed"],
                "path": path,
                "status": status,
                "seconds": round(ms / 1000.0, 3),
                "compile_ms": round(ms, 3),
                "dependency_check_ms": round(check_elapsed_ms, 3),
                "queue_wait_ms": round(queue_wait_ms, 3),
                "check_started_utc": check_started_utc,
                "submitted_utc": active_started.get("submitted_utc", ""),
                "started_utc": started_utc or active_started.get("started_utc", ""),
                "started_elapsed_ms": round(started_elapsed_ms or
                    float(active_started.get("elapsed_ms", 0.0)), 3),
                "wall_time": ended_utc,
                "elapsed_ms": round((time.monotonic() - self.started_monotonic) * 1000, 3),
                "build_attempt_id": self.data.get("build_attempt_id"),
                "source_generation": self.data.get("source_generation"),
                "generation": self.data.get("generation"),
                "revision": self.data.get("revision"),
            }
            if diagnostics is not None:
                file_event["compiler_timing"] = diagnostics
            self.data["file_events"].append(file_event)
            del self.data["file_events"][:-100]
            if status == "skipped":
                self._append_locked(f"[SKIP] {self.data['completed']}/{self.data['total']} {path}")
            elif status == "cancelled":
                self._append_locked(f"[CANCEL] {self.data['completed']}/{self.data['total']} {path}")
            else:
                suffix = " FAIL" if status == "failed" else ""
                self._append_locked(
                    f"[CXX ] {self.data['completed']}/{self.data['total']} "
                    f"{ms / 1000.0:.2f}s {path}{suffix}")
            progress_write_started = time.perf_counter()
            self._write_locked()
            file_event["progress_write_ms"] = round(
                (time.perf_counter() - progress_write_started) * 1000, 3)
            file_event["record_overhead_ms"] = round(
                (time.perf_counter() - record_started) * 1000, 3)
            self._timing_locked({
                "event": "translation_unit.completed", **file_event,
                "build_attempt_id": self.data.get("build_attempt_id"),
                "revision": self.data.get("revision"),
            })
            print("[GAME_FILE] " + json.dumps(file_event, separators=(",", ":")), flush=True)

    def _append_timing_rows_locked(self, rows: list[dict]) -> None:
        if self.timing_path is None or not rows:
            return
        try:
            self.timing_path.parent.mkdir(parents=True, exist_ok=True)
            with self.timing_path.open("a", encoding="utf-8", newline="\n") as handle:
                for row in rows:
                    handle.write(json.dumps(row, separators=(",", ":")) + "\n")
                handle.flush()
        except OSError:
            pass

    def bulk_file_events(self, rows: list[dict]) -> None:
        """Persist cache-scan rows without a progress write per source."""
        if not rows:
            return
        with self.lock:
            self.data["file_events"].extend(rows)
            del self.data["file_events"][:-100]
            self._append_timing_rows_locked(rows)
            self.dirty = True

    def scan_snapshot(self, *, scanned: int, skipped: int, stale: int,
                      completed: int, scan_seconds: float,
                      dependency_check_ms: float, parse_ms: float,
                      stat_ms: float, unique_dependencies: int,
                      scan_mode: str = "dependency_index",
                      cache_marker_reused_count: int = 0) -> None:
        with self.lock:
            self.data.update({
                "scan_scanned": scanned,
                "completed": completed,
                "skipped": skipped,
                "stale_count": stale,
                "object_scan_s": round(scan_seconds, 3),
                "dependency_check_ms": round(dependency_check_ms, 3),
                "scan_parse_ms": round(parse_ms, 3),
                "scan_stat_ms": round(stat_ms, 3),
                "scan_unique_dependencies": unique_dependencies,
                "scan_mode": scan_mode,
                "cache_marker_reused_count": cache_marker_reused_count,
            })
            self._append_locked(
                f"[SCAN] {scanned}/{self.data['total']} "
                f"skipped={skipped} stale={stale} {scan_seconds:.2f}s")
            self._timing_locked({
                "event": "scan.snapshot", "time_utc": utc_now(),
                "scanned": scanned, "skipped": skipped, "stale": stale,
                "scan_seconds": round(scan_seconds, 3),
                "dependency_check_ms": round(dependency_check_ms, 3),
                "parse_ms": round(parse_ms, 3), "stat_ms": round(stat_ms, 3),
                "unique_dependencies": unique_dependencies,
                "scan_mode": scan_mode,
                "cache_marker_reused_count": cache_marker_reused_count,
                "build_attempt_id": self.data.get("build_attempt_id"),
                "revision": self.data.get("revision"),
            })
            self.dirty = True

    def scan_prepass(self, *, parsed: int, total: int, current: str,
                     elapsed_s: float) -> None:
        with self.lock:
            self.data.update({
                "phase": "object_cache_scan",
                "scan_phase": "dependency_prepass",
                "scan_parsed": parsed,
                "scan_total": total,
                "scan_current": current,
                "scan_prepass_elapsed_s": round(elapsed_s, 3),
                "last_progress_utc": utc_now(),
            })
            self.dirty = True

    def subphase_start(self, name: str, **fields) -> float:
        started = time.monotonic()
        utc = utc_now()
        with self.lock:
            phase = str(self.data.get("phase") or "")
            self.data.update({"subphase": name,
                              "subphase_started_monotonic": started,
                              "subphase_started_utc": utc,
                              "subphase_elapsed_ms": 0.0,
                              **fields})
            event = {"event": "subphase.started", "phase": phase,
                     "subphase": name, "time_utc": utc,
                     "elapsed_ms": round((started - self.started_monotonic) * 1000, 3),
                     "build_attempt_id": self.data.get("build_attempt_id"),
                     "revision": self.data.get("revision")}
            self.data.setdefault("timeline", []).append(event)
            del self.data["timeline"][:-128]
            self._timing_locked(event)
            self._write_locked()
        return started

    def subphase_end(self, name: str, started: float, **fields) -> float:
        ended = time.monotonic()
        duration_ms = round((ended - started) * 1000, 3)
        with self.lock:
            event = {"event": "subphase.completed",
                     "phase": str(self.data.get("phase") or ""),
                     "subphase": name, "time_utc": utc_now(),
                     "duration_ms": duration_ms,
                     "elapsed_ms": round((ended - self.started_monotonic) * 1000, 3),
                     **fields,
                     "build_attempt_id": self.data.get("build_attempt_id"),
                     "revision": self.data.get("revision")}
            self.data.setdefault("subphase_durations_ms", {})[name] = duration_ms
            self.data.setdefault("timeline", []).append(event)
            self.data["subphase"] = ""
            self.data.pop("subphase_started_monotonic", None)
            self.data.pop("subphase_started_utc", None)
            self.data["subphase_elapsed_ms"] = 0.0
            self.data.update(fields)
            del self.data["timeline"][:-128]
            self._timing_locked(event)
            self._write_locked()
        return duration_ms

    def record_cache_invalidations(self, rows: list[dict]) -> None:
        if not rows:
            return
        with self.lock:
            self.data["cache_invalidations"] = rows[-32:]
            self._append_timing_rows_locked([
                {"event": "cache.invalidation", "time_utc": utc_now(), **row,
                 "build_attempt_id": self.data.get("build_attempt_id"),
                 "revision": self.data.get("revision")}
                for row in rows])
            self.dirty = True

    def finish(self, status: str, **kwargs) -> None:
        with self.lock:
            now = time.monotonic()
            phase = str(self.data.get("phase") or "")
            phase_duration_ms = round((now - self.phase_started_monotonic) * 1000, 3)
            if phase:
                self.data.setdefault("phase_durations_ms", {})[phase] = phase_duration_ms
                ended = {"event": "phase.completed", "phase": phase,
                         "time_utc": utc_now(), "duration_ms": phase_duration_ms}
                self.data.setdefault("timeline", []).append(ended)
                self._timing_locked({**ended,
                                     "build_attempt_id": self.data.get("build_attempt_id"),
                                     "revision": self.data.get("revision")})
            if status not in ("running", "ok"):
                self.data["last_completed_phase"] = phase
            self.data["status"] = status
            self.data["phase"] = "done" if status == "ok" else "failed"
            self.data["total_s"] = round(now - self.started_monotonic, 3)
            self.data["elapsed_s"] = self.data["total_s"]
            self.data["phase_elapsed_ms"] = phase_duration_ms
            self.data["completed_utc"] = utc_now()
            self.data.update(kwargs)
            self.stop_heartbeat.set()
            self._write_locked()

    def set_phase(self, phase: str, **fields) -> None:
        with self.lock:
            now = time.monotonic()
            utc = utc_now()
            previous = str(self.data.get("phase") or "")
            if previous and previous != phase:
                duration_ms = round((now - self.phase_started_monotonic) * 1000, 3)
                self.data.setdefault("phase_durations_ms", {})[previous] = duration_ms
                ended = {"event": "phase.completed", "phase": previous,
                         "time_utc": utc, "duration_ms": duration_ms}
                self.data.setdefault("timeline", []).append(ended)
                self._timing_locked({**ended,
                                     "build_attempt_id": self.data.get("build_attempt_id"),
                                     "revision": self.data.get("revision")})
            self.phase_started_monotonic = now
            self.data.update(fields)
            self.data["phase"] = phase
            self.data["phase_started_utc"] = utc
            self.data.setdefault("timeline", []).append({
                "event": "phase.started", "phase": phase, "time_utc": utc,
                "elapsed_ms": round((now - self.started_monotonic) * 1000, 3),
            })
            self._timing_locked({
                "event": "phase.started", "phase": phase, "time_utc": utc,
                "elapsed_ms": round((now - self.started_monotonic) * 1000, 3),
                "build_attempt_id": self.data.get("build_attempt_id"),
                "revision": self.data.get("revision"),
            })
            del self.data["timeline"][:-128]
            self._write_locked()


def select_game_sources(manifest: dict) -> list[dict]:
    return sorted(
        (item for item in manifest.get("sources", []) if item.get("owner") == GAME_OWNER),
        key=lambda item: item["path"])


def sources_from_file(path: Path) -> tuple[list[dict], dict]:
    payload = read_json(path)
    entries = payload.get("sources", []) if isinstance(payload, dict) else payload
    build_args = payload.get("build_args", {}) if isinstance(payload, dict) else {}
    result: list[dict] = []
    for entry in entries or []:
        rel = entry.get("path") if isinstance(entry, dict) else entry
        if not isinstance(rel, str) or not rel:
            continue
        source = SOURCE_ROOT / rel
        if not source.is_file():
            raise FileNotFoundError(f"probe source not found: {rel}")
        digest, size, mtime_ns = sha256_with_stamp(source)
        result.append({"path": rel, "sha256": digest,
                       "size": size, "mtime_ns": mtime_ns})
    if not result:
        raise ValueError(f"no sources listed in {path}")
    return sorted(result, key=lambda item: item["path"]), build_args


def verify_coherent(root: Path, sources: list[dict]) -> list[str]:
    errors: list[str] = []
    for item in sources:
        path = root / item["path"]
        if not path.is_file():
            errors.append(f"missing game source: {item['path']}")
            continue
        expected_size = item.get("size")
        expected_mtime = item.get("mtime_ns")
        if expected_size is not None and expected_mtime is not None:
            try:
                stat = path.stat()
            except OSError:
                errors.append(f"missing game source: {item['path']}")
                continue
            if stat.st_size == expected_size and stat.st_mtime_ns == expected_mtime:
                continue
        if sha256_file(path) != item["sha256"]:
            errors.append(f"changed during build: {item['path']}")
    return errors


def object_key(item: dict) -> str:
    path = str(item["path"]).replace("\\", "/")
    identity = f"{path}:{item['sha256']}"
    if path in PCH_EXCLUDED_SOURCES:
        identity += ":pch-off-v1"
    return hashlib.sha256(identity.encode("utf-8")).hexdigest()[:20]


def source_only_changed_paths(change_report: dict | None) -> set[str] | None:
    """Return changed source paths only when the manifest proves no fan-out.

    Any added/removed/renamed input, header change, ownership change, malformed
    report, or non-C++ path must use the full dependency scan.
    """
    if not isinstance(change_report, dict):
        return None
    list_fields = (
        "added", "removed", "renamed", "changed", "added_headers",
        "removed_headers", "changed_headers", "changed_owners",
    )
    if any(not isinstance(change_report.get(key), list) for key in list_fields):
        return None
    if any(change_report[key] for key in list_fields if key != "changed"):
        return None
    if not change_report.get("old_revision") or not change_report.get("new_revision"):
        return None
    paths: set[str] = set()
    for value in change_report["changed"]:
        if not isinstance(value, str):
            return None
        path = value.replace("\\", "/")
        if (not path or path.startswith("/")
                or any(part == ".." for part in path.split("/"))
                or not path.lower().endswith(".cpp")
                or path in paths):
            return None
        paths.add(path)
    return paths or None


def source_only_preflight_eligible(
        change_report: dict | None, changed_cpp_paths: set[str]) -> bool:
    """Keep the early compile wave only when the report proves no fan-out."""
    reported = source_only_changed_paths(change_report)
    normalized_paths = {str(path).replace("\\", "/")
                        for path in changed_cpp_paths}
    return bool(normalized_paths) and reported == normalized_paths


def source_uses_pch(path: str, enabled: bool) -> bool:
    normalized_path = str(path).replace("\\", "/")
    return enabled and normalized_path not in PCH_EXCLUDED_SOURCES


def flags_identity(cxx: str, base_flags: list[str], build_mode: str, pch: bool) -> str:
    text = "\n".join([cxx, f"mode={build_mode}", f"pch={int(pch)}", *base_flags])
    return hashlib.sha256(text.encode("utf-8")).hexdigest()[:16]


def identity_text() -> str:
    """Return the stable wrapper-facing identity declarations.

    Generation and source revision live in one generated object per DLL. Keeping
    these declarations byte-for-byte stable prevents every module wrapper from
    rebuilding just because a new game generation was published.
    """
    return ("#pragma once\n"
            "#include <cstdint>\n"
            "namespace mimita_module_identity {\n"
            "extern const std::uint32_t generation;\n"
            "extern const char source_hash[];\n"
            "extern const std::uint32_t state_schema;\n"
            "}\n"
            "#define MIMITA_MODULE_GENERATION (::mimita_module_identity::generation)\n"
            "#define MIMITA_MODULE_SOURCE_HASH (::mimita_module_identity::source_hash)\n"
            "#define MIMITA_MODULE_STATE_SCHEMA (::mimita_module_identity::state_schema)\n")


def identity_provider_text(generation: int, revision: str) -> str:
    """Generate one tiny TU carrying the values for this DLL generation."""
    return ("#include <cstdint>\n"
            "#include \"module-identity.h\"\n"
            "namespace mimita_module_identity {\n"
            f"const std::uint32_t generation = {int(generation)}u;\n"
            f'const char source_hash[] = "{revision}";\n'
            "const std::uint32_t state_schema = 1u;\n"
            "}\n")


def write_identity_if_changed(path: Path) -> bool:
    """Write stable module identity declarations only when their bytes change.

    Cache freshness uses file mtimes, so rewriting an identical header would
    invalidate every module wrapper for no reason. Per-generation values are
    supplied by identity_provider_text() instead.
    """
    text = identity_text()
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.is_file():
        try:
            if path.read_text(encoding="utf-8") == text:
                return False
        except OSError:
            pass
    path.write_text(text, encoding="utf-8")
    return True


def identity_matches(path: Path) -> bool:
    if not path.is_file():
        return False
    try:
        return path.read_text(encoding="utf-8") == identity_text()
    except OSError:
        return False


def marker_objects(object_dir: Path) -> dict[str, tuple[int, int]] | None:
    try:
        with os.scandir(object_dir) as entries:
            found = {}
            for entry in entries:
                if entry.name.endswith(".o"):
                    stat = entry.stat()
                    found[entry.name] = (stat.st_size, stat.st_mtime_ns)
            return found
    except OSError:
        return None


def cache_marker_valid(marker: dict, object_dir: Path, objects: list[Path],
                       published: dict, revision: str, flags_hash: str,
                       link_identity: str, compiler_stamp: str,
                       use_pch: bool) -> bool:
    """Validate the fast no-op marker without reading depfiles."""
    if not marker:
        return False
    try:
        schema = int(marker.get("schema_version") or 0)
        marker_generation = int(marker.get("generation") or 0)
        published_generation = int(published.get("generation") or 0)
    except (TypeError, ValueError):
        return False
    if schema != 1:
        return False
    if (str(marker.get("revision") or "") != revision
            or str(marker.get("flags_identity") or "") != flags_hash
            or str(marker.get("link_identity") or "") != link_identity
            or str(marker.get("compiler_stamp") or "") != compiler_stamp
            or marker_generation != published_generation
            or str(marker.get("code_hash") or "") != str(published.get("code_hash") or "")):
        return False
    expected = marker.get("objects")
    if not isinstance(expected, dict) or not expected:
        return False
    found = marker_objects(object_dir)
    if found is None:
        return False
    expected_names = {path.name for path in objects}
    if set(expected) != expected_names or not expected_names.issubset(found):
        return False
    for name, value in expected.items():
        try:
            size, mtime_ns = int(value[0]), int(value[1])
        except (TypeError, ValueError, IndexError):
            return False
        if found.get(name) != (size, mtime_ns):
            return False
    try:
        newest_object_mtime = max(int(value[1]) for value in expected.values())
    except (TypeError, ValueError, IndexError):
        return False
    if use_pch:
        pch = ROOT / "src" / "pch.h.gch"
        try:
            if pch.is_file() and pch.stat().st_mtime_ns > newest_object_mtime:
                return False
        except OSError:
            return False
    external = marker.get("external_dependencies")
    if external is not None and not isinstance(external, list):
        return False
    for dependency in external or []:
        path = Path(str(dependency))
        if not path.is_absolute():
            path = ROOT / path
        try:
            if path.stat().st_mtime_ns > newest_object_mtime:
                return False
        except OSError:
            return False
    return True


def source_only_cache_marker_valid(
        change_report: dict | None, current_revision: str,
        changed_paths: set[str], fresh_changed_paths: set[str],
        object_paths_by_source: dict[str, Path], object_dir: Path,
        marker: dict, published: dict, object_cache_rel: str,
        flags_hash: str, link_identity: str, compiler_stamp: str,
        use_pch: bool) -> bool:
    """Reuse a complete published marker for a strictly source-only delta.

    The changed objects are compiled/validated separately. The old marker must
    prove every unchanged object still belongs to the last published generation.
    """
    reported = source_only_changed_paths(change_report)
    normalized_changed = {str(path).replace("\\", "/") for path in changed_paths}
    normalized_fresh = {str(path).replace("\\", "/")
                        for path in fresh_changed_paths}
    normalized_objects = {
        str(path).replace("\\", "/"): Path(obj)
        for path, obj in object_paths_by_source.items()
    }
    if (reported is None or reported != normalized_changed
            or normalized_fresh != normalized_changed
            or not normalized_changed.issubset(normalized_objects)):
        return False
    if (str(change_report.get("old_revision") or "")
            != str(published.get("revision") or "")
            or str(change_report.get("new_revision") or "") != current_revision
            or str(published.get("status") or "") != "ok"
            or str(published.get("object_cache") or "") != object_cache_rel):
        return False
    expected = marker.get("objects") if isinstance(marker, dict) else None
    if not isinstance(expected, dict) or len(expected) != len(normalized_objects):
        return False
    current_names = [path.name for path in normalized_objects.values()]
    if len(set(current_names)) != len(current_names):
        return False
    expected_names = set(expected)
    changed_names = {
        obj.name for path, obj in normalized_objects.items()
        if path in normalized_changed
    }
    unchanged_names = {
        obj.name for path, obj in normalized_objects.items()
        if path not in normalized_changed
    }
    if (not unchanged_names.issubset(expected_names)
            or changed_names.intersection(expected_names)
            or len(expected_names - unchanged_names) != len(normalized_changed)):
        return False
    prior_objects = [object_dir / name for name in expected_names]
    return cache_marker_valid(
        marker, object_dir, prior_objects, published,
        str(published.get("revision") or ""), flags_hash, link_identity,
        compiler_stamp, use_pch)


def write_cache_marker(object_dir: Path, *, revision: str, generation: int,
                       flags_hash: str, link_identity: str,
                       compiler_stamp: str, code_hash: str,
                       objects: list[Path],
                       external_dependencies: list[str]) -> None:
    found = marker_objects(object_dir) or {}
    names = {path.name for path in objects}
    payload = {
        "schema_version": 1,
        "revision": revision,
        "generation": generation,
        "flags_identity": flags_hash,
        "link_identity": link_identity,
        "compiler_stamp": compiler_stamp,
        "code_hash": code_hash,
        "objects": {name: [found[name][0], found[name][1]]
                    for name in names if name in found},
        "external_dependencies": sorted(external_dependencies),
        "generated_utc": utc_now(),
    }
    try:
        write_json_atomic(object_dir / "complete.json", payload)
    except OSError:
        pass


def is_external_dependency(token: str) -> bool:
    text = str(token).replace("\\", "/")
    lowered = text.lower()
    root = str(ROOT).replace("\\", "/").lower().rstrip("/")
    source_root = str(SOURCE_ROOT).replace("\\", "/").lower().rstrip("/")
    if lowered.startswith((root + "/src/", root + "/include/",
                           source_root + "/src/", source_root + "/include/",
                           "src/", "include/", "./src/", "./include/")):
        return False
    return True


def parse_depfile(dep: Path) -> list[str] | None:
    try:
        if dep.stat().st_size == 0:
            return None
        text = dep.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return None
    text = re.sub(r"\\\r?\n", " ", text)
    match = re.match(r"^.*?\.o\s*:\s*(.*)$", text, re.S)
    if not match:
        return None
    return [token for token in match.group(1).split()
            if not token.endswith(":") and not token.endswith("pch.h.gch")]


DEPENDENCY_INDEX_SCHEMA = 1


def _normalized_cache_token(token: str) -> str:
    path = Path(token)
    if path.is_absolute():
        try:
            return str(ROOT / path.relative_to(SOURCE_ROOT))
        except ValueError:
            return str(path)
    return str(ROOT / path)


def _dependency_cache_key(token: str) -> str:
    return os.path.normcase(os.path.normpath(_normalized_cache_token(token)))


def load_dependency_index(path: Path) -> dict:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return {}
    if not isinstance(value, dict) or int(value.get("schema_version") or 0) != DEPENDENCY_INDEX_SCHEMA:
        return {}
    entries = value.get("entries")
    return entries if isinstance(entries, dict) else {}


def save_dependency_index(path: Path, entries: dict) -> None:
    try:
        write_json_atomic(path, {
            "schema_version": DEPENDENCY_INDEX_SCHEMA,
            "entries": entries,
        })
    except OSError:
        pass


def cached_dependency_tokens(entries: dict, source_path: str,
                             source_hash: str, dep: Path) -> list[str] | None:
    entry = entries.get(source_path)
    if not isinstance(entry, dict):
        return None
    if source_hash and str(entry.get("source_hash") or "") != source_hash:
        return None
    try:
        stat = dep.stat()
        dependencies = entry.get("dependencies")
        if (not isinstance(dependencies, list) or
                int(entry.get("dep_size") or -1) != stat.st_size or
                int(entry.get("dep_mtime_ns") or -1) != stat.st_mtime_ns):
            return None
        return [str(item) for item in dependencies]
    except (OSError, TypeError, ValueError):
        return None


def remember_dependency_tokens(entries: dict, source_path: str,
                               source_hash: str, dep: Path,
                               tokens: list[str]) -> bool:
    try:
        stat = dep.stat()
    except OSError:
        return False
    normalized = list(dict.fromkeys(
        _normalized_cache_token(token) for token in tokens))
    entries[source_path] = {
        "source_hash": source_hash,
        "dep_size": stat.st_size,
        "dep_mtime_ns": stat.st_mtime_ns,
        "dependencies": normalized,
    }
    return True


def dependency_relative(token: str) -> str:
    """Return a project-relative header path for fan-out attribution.

    Dependency tokens come from compiler-generated depfiles and the persistent
    dependency index. Resolve them lexically: filesystem ``Path.resolve()`` on
    every unique token was dominating the index prepass despite this result
    being used only for diagnostic header fan-out, not freshness decisions.
    """
    path = Path(token)
    if path.is_absolute():
        relative = None
        for base in (SOURCE_ROOT, ROOT):
            try:
                relative = path.relative_to(base)
                break
            except ValueError:
                continue
        if relative is None:
            return ""
    else:
        relative = path
    relative_text = Path(os.path.normpath(str(relative))).as_posix()
    if relative_text == ".." or relative_text.startswith("../"):
        return ""
    if relative_text.startswith("src/") and relative_text.endswith(
            (".h", ".hpp", ".inl")):
        return relative_text
    return ""


def prioritize_changed_header_dependents(
        work: list[tuple[dict, Path, Path]],
        dependent_headers: dict[str, list[str]],
        changed_headers: set[str],
        changed_sources: set[str] | None = None
        ) -> tuple[list[tuple[dict, Path, Path]], dict]:
    """Prioritize directly edited sources, then changed-header dependents.

    The depfile is the authoritative transitive include list. This is only a
    scheduling hint: it never changes which objects are stale or compiled.
    Directly edited translation units run first so their diagnostics surface
    before unrelated stale work. Changed-header dependents run next. Python's
    stable sort preserves manifest order within each priority group.
    """
    changed_source_paths = {
        str(path).replace("\\", "/") for path in (changed_sources or set())}
    if (not changed_headers and not changed_source_paths) or not work:
        return work, {
            "enabled": False,
            "candidate_count": len(work),
            "prioritized_count": 0,
            "changed_header_count": len(changed_headers),
            "direct_changed_source_count": 0,
            "direct_changed_sources": [],
            "prioritized_sources": [],
        }

    def is_directly_changed(entry: tuple[dict, Path, Path]) -> bool:
        source_path = str(entry[0]["path"]).replace("\\", "/")
        return source_path in changed_source_paths

    def has_changed_header(entry: tuple[dict, Path, Path]) -> bool:
        source_path = str(entry[0]["path"]).replace("\\", "/")
        return any(header in changed_headers
                   for header in dependent_headers.get(source_path, ()))

    direct_matches = [entry for entry in work if is_directly_changed(entry)]
    header_matches = [entry for entry in work if has_changed_header(entry)]
    ordered = sorted(
        work,
        key=lambda entry: (0 if is_directly_changed(entry)
                           else 1 if has_changed_header(entry) else 2))
    return ordered, {
        "enabled": True,
        "candidate_count": len(work),
        "prioritized_count": len(header_matches),
        "changed_header_count": len(changed_headers),
        "direct_changed_source_count": len(direct_matches),
        "direct_changed_sources": [
            entry[0]["path"] for entry in direct_matches[:20]],
        "prioritized_sources": [
            entry[0]["path"] for entry in header_matches[:20]],
    }


def cached_stat(token: str, cache: dict[str, os.stat_result | None]) -> os.stat_result | None:
    path = Path(token)
    relative = None
    if path.is_absolute():
        for base in (SOURCE_ROOT, ROOT):
            try:
                relative = path.relative_to(base)
                break
            except ValueError:
                continue
        if relative is None:
            relative = None
    else:
        relative = path

    if relative is not None:
        relative = Path(os.path.normpath(str(relative)))
        if (relative.parts and relative.parts[0] in {"src", "include", "external"}
                and ".." not in relative.parts):
            # Cache validity must describe the exact inputs this attempt built.
            # A live edit after snapshot creation cannot invalidate an object
            # compiled from the older frozen source. Once snapshots are pruned,
            # fall back to ROOT so the persistent dep index remains usable.
            snapshot_path = SOURCE_ROOT / relative
            live_path = ROOT / relative
            path = snapshot_path if snapshot_path.is_file() else live_path
        else:
            path = ROOT / relative
    # Non-project dependencies (toolchain/system headers, generated paths, etc.)
    # keep their original absolute path and are still checked for freshness.
    key = os.path.normcase(os.path.normpath(str(path)))
    if key not in cache:
        try:
            cache[key] = os.stat(path)
        except OSError:
            cache[key] = None
    return cache[key]


def seed_object_cache(out_root: Path, object_dir: Path) -> int:
    """Reuse objects from an earlier generation when flags match.

    The generation dirs from before this change kept their objects in
    generation-N/obj. Copy missing objects/deps into the persistent cache so a
    diagnostic build does not restart from zero.
    """
    candidates: list[tuple[int, Path]] = []
    for obj_dir in out_root.glob("generation-*/obj"):
        if obj_dir.resolve() == object_dir.resolve():
            continue
        count = sum(1 for _ in obj_dir.glob("*.o"))
        if count:
            candidates.append((count, obj_dir))
    if not candidates:
        return 0
    _, source_dir = max(candidates)
    copied = 0
    for obj in source_dir.glob("*.o"):
        destination = object_dir / obj.name
        if not destination.exists():
            shutil.copy2(obj, destination)
            copied += 1
        dep = obj.with_suffix(".d")
        destination_dep = object_dir / dep.name
        if dep.exists() and not destination_dep.exists():
            shutil.copy2(dep, destination_dep)
    return copied


def dep_is_stale(obj: Path, dep: Path, *, use_pch: bool = True) -> bool:
    if not obj.is_file() or not dep.is_file():
        return True
    try:
        if obj.stat().st_size == 0 or dep.stat().st_size == 0:
            return True
        object_time = obj.stat().st_mtime
        text = dep.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return True
    # GCC .d files are Makefile rules with a Windows drive-letter target, so a
    # naive ":" split breaks on the drive letter. Join continuations, drop the
    # target prefix, then split dependencies on whitespace. Backslashes remain
    # part of the path and must not be replaced with spaces. -MP phony targets
    # end with ':' and are not dependencies.
    text = re.sub(r"\\\r?\n", " ", text)
    match = re.match(r"^.*?\.o\s*:\s*(.*)$", text, re.S)
    if not match:
        return True
    if use_pch:
        pch = ROOT / "src" / "pch.h.gch"
        if pch.exists() and pch.stat().st_mtime >= object_time:
            return True
    for token in match.group(1).split():
        if token.endswith(":") or token.endswith("pch.h.gch"):
            continue
        dependency = Path(token)
        if dependency.is_absolute():
            try:
                dependency = ROOT / dependency.relative_to(SOURCE_ROOT)
            except ValueError:
                dependency = Path(token)
        else:
            dependency = ROOT / dependency
        try:
            if dependency.stat().st_mtime >= object_time:
                return True
        except OSError:
            return True
    return False


def build(args: argparse.Namespace) -> int:
    from build_toolchain import compiler, ccache, glfw_include, glfw_lib, runtime_path, run_tool

    worker_entry_started = time.monotonic()
    worker_entry_utc = utc_now()
    attempt_context = {}
    if args.attempt_context:
        try:
            attempt_context = read_json(Path(args.attempt_context))
        except (OSError, json.JSONDecodeError):
            attempt_context = {}
    out_root = (ROOT / args.out).resolve() if not Path(args.out).is_absolute() else Path(args.out)
    progress_path = out_root / "progress.json"
    last_attempt_path = out_root / "last-attempt.json"
    current_path = out_root / "current.json"
    timing_value = attempt_context.get("timing_path")
    timing_path = Path(timing_value) if timing_value else None
    attempt_started = float(attempt_context.get(
        "attempt_started_monotonic", time.monotonic()))
    attempt_id = str(attempt_context.get("build_attempt_id") or "")
    revision = str(attempt_context.get("revision") or "")
    generation: int | None = None
    early_progress = {}
    same_attempt = False
    if progress_path.is_file():
        try:
            candidate = read_json(progress_path)
        except (OSError, json.JSONDecodeError):
            candidate = {}
        if attempt_id and str(candidate.get("build_attempt_id") or "") == attempt_id:
            early_progress = candidate
            same_attempt = True
    if not same_attempt:
        early_progress["timeline"] = []

    def emit_result(payload: dict) -> None:
        encoded = json.dumps(payload, separators=(",", ":"))
        if payload.get("status") == "failed":
            print("[GAME_ERROR] " + encoded, flush=True)
        print("[GAME_RESULT] " + encoded, flush=True)

    def write_last_attempt(status: str, phase: str, **fields) -> None:
        payload = {
            "schema_version": 1,
            "build_attempt_id": attempt_id,
            "source_generation": attempt_context.get("source_generation"),
            "revision": revision,
            "generation": generation,
            "status": status,
            "phase": phase,
            "generated_utc": utc_now(),
            **fields,
        }
        try:
            write_json_atomic(last_attempt_path, payload)
        except OSError:
            pass

    def early_fail(phase: str, message: str, exit_code: int,
                   **fields) -> int:
        now = time.monotonic()
        early_progress.update({
            "status": "failed", "phase": phase, "failed_phase": phase,
            "error": message, "updated_utc": utc_now(),
            "elapsed_s": round(now - attempt_started, 3),
        })
        early_progress.setdefault("timeline", []).append({
            "event": "phase.failed", "phase": phase, "time_utc": utc_now(),
            "elapsed_ms": round((now - attempt_started) * 1000, 3),
        })
        try:
            write_json_atomic(progress_path, early_progress)
        except OSError:
            pass
        write_last_attempt("failed", phase, error=message, **fields)
        emit_result({"status": "failed", "phase": phase, "error": message,
                     "build_attempt_id": attempt_id, "revision": revision,
                     **fields})
        print(f"[GAME BUILD] {message}", file=sys.stderr, flush=True)
        return exit_code

    def early_phase(name: str, started: float | None = None, *, completed: bool = False,
                    **fields) -> float:
        now = time.monotonic()
        utc = utc_now()
        if not completed:
            started = now
            phase_event = {"event": "phase.started", "phase": name, "time_utc": utc,
                           "elapsed_ms": round((now - attempt_started) * 1000, 3)}
        else:
            duration_ms = round((now - (started or now)) * 1000, 3)
            phase_event = {"event": "phase.completed", "phase": name,
                           "time_utc": utc, "duration_ms": duration_ms}
            fields["phase_duration_ms"] = duration_ms
        phase_event.update(fields)
        early_progress.update({
            "status": "running", "phase": name,
            "phase_started_utc": utc if not completed else early_progress.get("phase_started_utc", utc),
            "updated_utc": utc,
            "elapsed_s": round(now - attempt_started, 3),
        })
        early_progress.setdefault("timeline", []).append(phase_event)
        early_progress["timeline"] = early_progress["timeline"][-128:]
        try:
            write_json_atomic(progress_path, early_progress)
        except OSError:
            pass
        if timing_path is not None:
            try:
                timing_path.parent.mkdir(parents=True, exist_ok=True)
                with timing_path.open("a", encoding="utf-8", newline="\n") as handle:
                    handle.write(json.dumps({**phase_event,
                        "build_attempt_id": attempt_context.get("build_attempt_id"),
                        "revision": attempt_context.get("revision")},
                        separators=(",", ":")) + "\n")
                    handle.flush()
            except OSError:
                pass
        return started if started is not None else now

    input_phase = early_phase("worker_input_validation")

    if args.sources_file:
        sources_path = Path(args.sources_file)
        if not sources_path.is_absolute():
            sources_path = ROOT / sources_path
        try:
            sources, build_args = sources_from_file(sources_path)
        except (OSError, ValueError) as error:
            return early_fail("worker_input_validation", str(error), 3)
        if build_args.get("no_pch"):
            args.pch = False
        revision = hashlib.sha256(
            "\n".join(f"{item['path']}:{item['sha256']}" for item in sources).encode("utf-8")
        ).hexdigest()
        manifest = {"revision": revision, "sources": sources}
    else:
        manifest_path = Path(args.manifest)
        if not manifest_path.is_absolute():
            manifest_path = ROOT / manifest_path
        manifest = read_json(manifest_path)
        revision = str(manifest.get("revision") or "")
        if args.revision and revision != args.revision:
            return early_fail(
                "worker_input_validation",
                f"revision mismatch: {revision} != {args.revision}", 3)
        sources = select_game_sources(manifest)

    if args.revision and args.sources_file and revision != args.revision:
        return early_fail(
            "worker_input_validation",
            f"revision mismatch: {revision} != {args.revision}", 3)

    early_phase("worker_input_validation", input_phase, completed=True,
                source_count=len(sources), revision=str(revision or ""))

    change_report = None
    if args.change_report:
        report_path = Path(args.change_report)
        if not report_path.is_absolute():
            report_path = ROOT / report_path
        change_report = read_json(report_path)
        if not change_report:
            return early_fail("worker_input_validation",
                              f"unreadable change report: {report_path}", 3)
        reported_revision = change_report.get("new_revision", change_report.get("revision"))
        if reported_revision and revision and reported_revision != revision:
            return early_fail(
                "worker_input_validation",
                f"change report revision mismatch: {reported_revision} != {revision}",
                3)
    if not sources:
        return early_fail("worker_input_validation",
                          "no game-owned sources in manifest", 3)
    if args.list:
        print(f"[GAME BUILD] revision={manifest.get('revision')} game_sources={len(sources)}")
        for item in sources:
            print(f"  {item['path']}")
        return 0

    coherence_started = early_phase("initial_source_coherence")
    initial_errors = verify_coherent(SOURCE_ROOT, sources)
    early_phase("initial_source_coherence", coherence_started, completed=True,
                checked_sources=len(sources), error_count=len(initial_errors))
    if initial_errors:
        return early_fail("initial_source_coherence",
                          f"game source changed before build: {initial_errors[0]}",
                          2, errors=initial_errors[:20])

    # The probe module is a separate validation export. The whole-game DLL uses
    # src/game/game-module.cpp as its GameModuleV1 entry point.
    if not args.sources_file:
        sources = [s for s in sources
                   if s.get("path") != "src/game/probe/npc-target-probe-module.cpp"]

    toolchain_started = early_phase("toolchain_setup")
    toolchain_started_utc = str(early_progress.get("phase_started_utc") or utc_now())
    try:
        cxx = compiler()
        os.environ["PATH"] = runtime_path(cxx) + os.pathsep + os.environ.get("PATH", "")
        cache = ccache(cxx)
        glfw_inc = glfw_include()
        glfw_lib_dir = glfw_lib()
    except FileNotFoundError as error:
        return early_fail("toolchain_setup", f"toolchain unavailable: {error}", 4)
    toolchain_seconds = round(time.monotonic() - toolchain_started, 3)

    worker_priority = set_process_priority(args.priority)
    compiler_creationflags = build_tool_creation_flags(args.priority)

    active_compile_processes: dict[int, subprocess.Popen] = {}
    active_compile_lock = threading.Lock()

    def terminate_compile_process(pid: int,
                                  process: subprocess.Popen) -> None:
        if os.name == "nt":
            try:
                subprocess.run(
                    ["taskkill", "/F", "/T", "/PID", str(pid)],
                    stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                    timeout=5.0,
                    creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
            except (OSError, subprocess.TimeoutExpired):
                pass
        if process.poll() is None:
            try:
                process.kill()
            except OSError:
                pass

    def cancel_active_compile_processes() -> None:
        stop_event.set()
        with active_compile_lock:
            active = list(active_compile_processes.items())
        for pid, process in active:
            terminate_compile_process(pid, process)

    def run_build_tool(command, *, tracked_compile: bool = False, **kwargs):
        if os.name == "nt":
            kwargs["creationflags"] = compiler_creationflags
        if not tracked_compile:
            return run_tool(command, **kwargs)
        return _run_tracked_compile_process(
            command, kwargs, active_processes=active_compile_processes,
            active_lock=active_compile_lock, stop_event=stop_event,
            terminate_process=terminate_compile_process)

    setup_started = time.monotonic()
    jobs = args.jobs if args.jobs > 0 else (os.cpu_count() or 1)
    common_flags = [
        "-std=c++17",
        "-O2" if args.build_mode == "release" else "-Og",
        "-g" if args.build_mode == "release" else "-g1",
        "-pipe",
        "-MMD",
        "-MP",
        "-fpch-preprocess",
    ]
    # The actual command reads headers from the frozen snapshot (relative to the
    # compile cwd), while the cache identity is computed from the historical
    # repository paths so an unchanged tree keeps its existing object cache.
    base_flags = [*common_flags,
                  *[f"-I{directory}" for directory in INCLUDE_DIRS],
                  f"-I{glfw_inc}", *DEFINE_FLAGS]
    logical_flags = [*common_flags,
                     *[f"-I{ROOT / directory}" for directory in INCLUDE_DIRS],
                     f"-I{glfw_inc}", *DEFINE_FLAGS]
    if args.pch:
        # Keep the repository PCH: only pch.h and its includes stay live; every
        # other header comes from the snapshot.
        base_flags += ["-include", str(ROOT / "src" / "pch.h")]
        logical_flags += ["-include", "pch.h"]
    if args.sources_file and not args.no_probe_bridge:
        base_flags.append("-DMIMITA_PROBE_BRIDGE")
        logical_flags.append("-DMIMITA_PROBE_BRIDGE")

    flags_hash = flags_identity(cxx, logical_flags, args.build_mode, args.pch)
    if args.linker == "auto":
        args.linker = ("lld" if (Path(cxx).parent / "ld.lld.exe").is_file()
                       else "gnu")
    linker_name = "ld.lld.exe" if args.linker == "lld" else "ld.exe"
    linker_path = Path(cxx).parent / linker_name
    if not linker_path.is_file():
        message = f"requested game DLL linker is missing: {linker_path}"
        return early_fail("linker_selection", message, 1)
    linker_stat = linker_path.stat()
    linker_stamp = f"{linker_stat.st_size}:{linker_stat.st_mtime_ns}"
    link_identity_parts = [
        *LINK_LIBS, "manifold-prebuilt", f"glfw={Path(glfw_lib_dir).as_posix()}",
        f"abi={ABI_VERSION}"]
    if args.linker == "lld":
        link_identity_parts.append(f"linker=lld:{linker_stamp}")
    link_identity = hashlib.sha256(
        "\n".join(link_identity_parts).encode("utf-8")).hexdigest()[:16]
    try:
        compiler_stat = Path(cxx).stat()
        compiler_stamp = f"{compiler_stat.st_size}:{compiler_stat.st_mtime_ns}"
    except OSError:
        compiler_stamp = ""
    object_dir = out_root / "objects" / flags_hash
    object_dir.mkdir(parents=True, exist_ok=True)
    already_seeded = any(object_dir.glob("*.o"))
    seeded = (0 if args.no_seed or already_seeded
              else seed_object_cache(out_root, object_dir))
    try:
        object_cache_rel = str(object_dir.relative_to(ROOT))
    except ValueError:
        object_cache_rel = str(object_dir)
    early_phase("generation_and_cache_setup", setup_started, completed=True,
                object_cache=object_cache_rel, seeded_objects=seeded, jobs=jobs)
    progress = Progress(out_root, revision, len(sources), jobs,
                        args.priority, object_dir, seeded, attempt_context)
    progress.set(worker_priority=worker_priority,
                 compiler_creationflags=compiler_creationflags)
    progress.log(
        f"[PRIORITY] worker requested={args.priority} "
        f"status={worker_priority.get('status')} "
        f"effective_class={worker_priority.get('effective_class', 'unknown')}; "
        f"compiler_child_flags=0x{compiler_creationflags:x}")
    progress.worker_started_monotonic = worker_entry_started
    progress.data["worker_started_utc"] = worker_entry_utc
    progress.data["phase"] = "toolchain_setup"
    progress.phase_started_monotonic = toolchain_started
    progress.data["phase_started_utc"] = toolchain_started_utc
    progress.set_phase("identity_and_cache",
                       compiler=str(cxx), flags_identity=flags_hash,
                       link_identity=link_identity,
                       linker=args.linker, linker_path=str(linker_path),
                       linker_stamp=linker_stamp,
                       compiler_stamp=compiler_stamp,
                       object_cache=object_cache_rel,
                       seeded_objects=seeded, toolchain_seconds=toolchain_seconds)
    progress.log(f"[GAME BUILD] revision={revision} sources={len(sources)} "
                 f"jobs={jobs} priority={args.priority} seed={seeded}")
    print(f"[GAME BUILD] revision={revision} sources={len(sources)} "
          f"jobs={jobs} priority={args.priority} cache={object_cache_rel} "
          f"seeded={seeded}", flush=True)

    entries: list[tuple[dict, Path, Path]] = []
    objects: list[Path] = []
    for item in sources:
        key = object_key(item)
        obj = object_dir / f"{key}.o"
        dep = object_dir / f"{key}.d"
        entries.append((item, obj, dep))
        objects.append(obj)

    ccache_prefix = [cache] if cache and os.path.isfile(cache) else []
    stop_event = threading.Event()
    failures: list[dict] = []

    def normalized(path: str) -> str:
        return str(path).replace("\\", "/")

    changed_header_paths: dict[str, str] = {}
    if change_report:
        for path in ((change_report.get("changed_headers") or []) +
                     (change_report.get("added_headers") or [])):
            changed_header_paths[_dependency_cache_key(path)] = normalized(path)

    timing_report_paths = {
        normalized(str(path))
        for path in progress.data.get("changed_paths", [])
        if str(path).lower().endswith(".cpp")
    }

    def module_extra_flags(source_path: str) -> list[str]:
        if Path(source_path).name == IDENTITY_PROVIDER_SOURCE:
            return [f"-I{identity_dir}"]
        symbol = FOLDED_MODULE_SYMBOLS.get(source_path)
        if symbol:
            return [f"-I{identity_dir}", "-DMIMITA_MODULE_BUILD",
                    f"-DGetMimitaModuleAPI={symbol}"]
        if source_path in IDENTITY_ONLY_SOURCES:
            return [f"-I{identity_dir}", "-DMIMITA_MODULE_BUILD"]
        return []

    def compile_command(item: dict, obj: Path, dep: Path, *,
                        time_report: bool = False) -> list[str]:
        source_path = normalized(item["path"])
        if Path(source_path).name == IDENTITY_PROVIDER_SOURCE:
            # This generated TU only defines three primitive identity values.
            # Loading the full game PCH made this 258-byte source take 5.26s;
            # compile it with the same MinGW C++ compiler but only its own
            # stable header dependency.
            return [*ccache_prefix, cxx, "-std=c++17", "-O0", "-g0",
                    f"-I{identity_dir}", "-c", item["path"], "-o", str(obj)]
        source_flags = base_flags
        if not source_uses_pch(source_path, args.pch):
            source_flags = list(base_flags)
            pch_path = str(ROOT / "src" / "pch.h")
            for flag_index in range(len(source_flags) - 1):
                if (source_flags[flag_index] == "-include"
                        and source_flags[flag_index + 1] == pch_path):
                    del source_flags[flag_index:flag_index + 2]
                    break
        command = [*ccache_prefix, cxx, *source_flags,
                   *module_extra_flags(source_path)]
        if time_report:
            # Diagnostic-only: this flag does not change generated code or the
            # persistent object-cache identity. It is limited to explicitly
            # changed .cpp inputs so a header fan-out does not flood logs.
            command.append("-ftime-report")
        command.extend(["-MF", str(dep), "-c", item["path"], "-o", str(obj)])
        return command

    def compile_one(item: dict, obj: Path, dep: Path, submitted: float,
                    submitted_utc: str
                    ) -> tuple[str, dict, float, str, float, str, float,
                               dict | None]:
        if stop_event.is_set():
            return ("skipped", item, 0.0, "", 0.0, "", 0.0, None)
        queue_wait_ms = (time.monotonic() - submitted) * 1000.0
        started_utc, started_elapsed_ms = progress.start_source(
            item["path"], queue_wait_ms=queue_wait_ms,
            submitted_utc=submitted_utc)
        started = time.perf_counter()
        source_path = normalized(item["path"])
        report_requested = source_path in timing_report_paths
        report_started = time.perf_counter()
        result = run_build_tool(compile_command(
                              item, obj, dep, time_report=report_requested),
                          cwd=str(SOURCE_ROOT),
                          stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                          text=True, tracked_compile=True)
        output = result.stdout or ""
        diagnostics = None
        if report_requested:
            probe_ms = (time.perf_counter() - report_started) * 1000.0
            diagnostics = parse_gcc_time_report(output)
            if result.returncode != 0 and is_unsupported_time_report(output):
                # Optional compiler instrumentation must not turn a valid game
                # compile into a failed candidate. Retry without the flag only
                # when the compiler clearly rejected that diagnostic option.
                first_output = output
                fallback_started = time.perf_counter()
                result = run_build_tool(compile_command(item, obj, dep),
                                  cwd=str(SOURCE_ROOT),
                                  stdout=subprocess.PIPE,
                                  stderr=subprocess.STDOUT, text=True,
                                  tracked_compile=True)
                fallback_ms = (time.perf_counter() - fallback_started) * 1000.0
                output = result.stdout or ""
                diagnostics = {
                    "format": "gcc-ftime-report",
                    "status": "unsupported_fallback",
                    "unsupported_probe_ms": round(probe_ms, 3),
                    "fallback_compile_ms": round(fallback_ms, 3),
                    "error": first_output[:600],
                }
                if result.returncode != 0:
                    output = (first_output[:1000]
                              + "\n--- retry without optional timing flag ---\n"
                              + output)
        elapsed_ms = (time.perf_counter() - started) * 1000.0
        if result.returncode != 0:
            if stop_event.is_set():
                return ("cancelled", item, elapsed_ms,
                        "cancelled after another translation unit failed",
                        queue_wait_ms, started_utc, started_elapsed_ms,
                        diagnostics)
            cancel_active_compile_processes()
            return ("failed", item, elapsed_ms, output,
                    queue_wait_ms, started_utc, started_elapsed_ms,
                    diagnostics)
        return ("compiled", item, elapsed_ms, "", queue_wait_ms,
                started_utc, started_elapsed_ms, diagnostics)

    def record_compile_result(result: tuple) -> str:
        (status, item, elapsed_ms, output, queue_wait_ms,
         started_utc, started_elapsed_ms, diagnostics) = result
        if status == "skipped":
            return status
        progress.record(status, item["path"], elapsed_ms, output,
                        queue_wait_ms=queue_wait_ms,
                        started_utc=started_utc,
                        started_elapsed_ms=started_elapsed_ms,
                        diagnostics=diagnostics)
        if status == "failed":
            failures.append({"path": normalized(item["path"]),
                             "output": output[:4000]})
        return status

    def run_compile_batch(batch: list[tuple[dict, Path, Path]],
                          max_workers: int) -> None:
        with ThreadPoolExecutor(max_workers=max_workers) as executor:
            futures = []
            for item, obj, dep in batch:
                if stop_event.is_set():
                    break
                futures.append(executor.submit(
                    compile_one, item, obj, dep, time.monotonic(), utc_now()))
            for future in as_completed(futures):
                try:
                    result = future.result()
                except CancelledError:
                    continue
                if record_compile_result(result) == "failed":
                    stop_event.set()
                    for pending in futures:
                        pending.cancel()
                    if len(failures) >= args.max_errors:
                        break

    def scan_entries(scan_list: list[tuple[dict, Path, Path]], *,
                     handled: set[str], quiet: bool) -> dict:
        base_completed = int(progress.data.get("completed") or 0)
        base_skipped = int(progress.data.get("skipped") or 0)
        scan_started = time.monotonic()
        scan_rows: list[dict] = []
        stale: list[tuple[dict, Path, Path]] = []
        stale_changed_header_dependencies: dict[str, list[str]] = {}
        stat_cache: dict[str, os.stat_result | None] = {}
        external_dependencies: set[str] = set()
        pch = ROOT / "src" / "pch.h.gch"
        try:
            pch_stat = (pch.stat() if args.pch and pch.is_file() else None)
        except OSError:
            pch_stat = None
        scanned = skipped = 0
        dependency_check_ms = parse_ms = stat_ms = 0.0
        dependency_check_rows: list[tuple[str, float]] = []
        dependency_path_and_fanout_ms = 0.0
        dependency_path_normalization_ms = 0.0
        external_dependency_classification_ms = 0.0
        last_snapshot = scan_started
        parsed_tokens: dict[str, list[str] | None] = {}
        stat_targets: set[str] = set()
        header_counts: Counter[str] = Counter()
        prepass_started = time.perf_counter()
        prepass_phase_started = progress.subphase_start(
            "scan.dependency_index_prepass", depfile_total=len(scan_list))
        last_prepass_update = prepass_started
        dep_index_path = object_dir / "dependency-index.json"
        dependency_metadata: dict[str, tuple[bool, str]] = {}
        index_load_started = time.perf_counter()
        dep_index = load_dependency_index(dep_index_path)
        dep_index_load_ms = (time.perf_counter() - index_load_started) * 1000.0
        dep_index_changed = False
        dep_index_seen: set[str] = set()
        progress.scan_prepass(parsed=0, total=len(scan_list), current="",
                              elapsed_s=0.0)
        for index, (item, _obj, dep) in enumerate(scan_list, 1):
            source_path = normalized(item["path"])
            dep_index_seen.add(source_path)
            source_hash = str(item.get("sha256") or "")
            tokens = cached_dependency_tokens(
                dep_index, source_path, source_hash, dep)
            if tokens is None:
                parsed_started = time.perf_counter()
                tokens = parse_depfile(dep)
                parse_ms += (time.perf_counter() - parsed_started) * 1000.0
                if tokens is not None:
                    dep_index_changed = remember_dependency_tokens(
                        dep_index, source_path, source_hash, dep, tokens
                    ) or dep_index_changed
            parsed_tokens[source_path] = tokens
            path_walk_started = time.perf_counter()
            for token in tokens or ():
                metadata = dependency_metadata.get(token)
                if metadata is None:
                    external_started = time.perf_counter()
                    is_external = is_external_dependency(token)
                    external_dependency_classification_ms += (
                        time.perf_counter() - external_started) * 1000.0
                    normalize_started = time.perf_counter()
                    relative = dependency_relative(token)
                    dependency_path_normalization_ms += (
                        time.perf_counter() - normalize_started) * 1000.0
                    metadata = (is_external, relative)
                    dependency_metadata[token] = metadata
                is_external, relative = metadata
                if is_external:
                    external_dependencies.add(token)
                if relative:
                    header_counts[relative] += 1
            dependency_path_and_fanout_ms += (
                time.perf_counter() - path_walk_started) * 1000.0
            if source_path not in handled and tokens:
                stat_targets.update(tokens)
            now = time.perf_counter()
            if index == len(scan_list) or now - last_prepass_update >= 0.5:
                progress.scan_prepass(
                    parsed=index, total=len(scan_list), current=source_path,
                    elapsed_s=now - prepass_started)
                last_prepass_update = now
        index_save_ms = 0.0
        if dep_index_changed or len(dep_index) != len(dep_index_seen):
            index_save_started = time.perf_counter()
            dep_index = {key: value for key, value in dep_index.items()
                         if key in dep_index_seen}
            save_dependency_index(dep_index_path, dep_index)
            index_save_ms = (time.perf_counter() - index_save_started) * 1000.0
        dependency_prepass_ms = (time.perf_counter() - prepass_started) * 1000.0
        dependency_prepass_other_ms = max(
            0.0, dependency_prepass_ms - parse_ms - dependency_path_and_fanout_ms
            - dep_index_load_ms - index_save_ms)
        progress.subphase_end(
            "scan.dependency_index_prepass", prepass_phase_started,
            depfile_count=len(scan_list), parse_ms=round(parse_ms, 3),
            path_and_fanout_ms=round(dependency_path_and_fanout_ms, 3),
            path_normalization_ms=round(dependency_path_normalization_ms, 3),
            external_classification_ms=round(
                external_dependency_classification_ms, 3),
            index_load_ms=round(dep_index_load_ms, 3),
            index_save_ms=round(index_save_ms, 3),
            remaining_loop_work_ms=round(dependency_prepass_other_ms, 3))
        progress.set(scan_phase="object_checks", scan_parsed=len(scan_list),
                     scan_current="", scan_prepass_elapsed_s=round(
                         dependency_prepass_ms / 1000.0),
                     dependency_prepass_ms=round(dependency_prepass_ms, 3),
                     dependency_index_load_ms=round(dep_index_load_ms, 3),
                     dependency_index_save_ms=round(index_save_ms, 3),
                     dependency_path_and_fanout_ms=round(
                         dependency_path_and_fanout_ms, 3))
        stat_prewarm_ms = 0.0
        if stat_targets:
            stat_phase_started = progress.subphase_start(
                "scan.unique_dependency_stat_prewarm",
                unique_path_count=len(stat_targets))
            stat_started = time.perf_counter()
            workers = min(16, max(2, (os.cpu_count() or 4) * 2))
            with ThreadPoolExecutor(max_workers=workers) as executor:
                list(executor.map(
                    lambda token: cached_stat(token, stat_cache),
                    sorted(stat_targets)))
            stat_prewarm_ms = (time.perf_counter() - stat_started) * 1000.0
            progress.subphase_end(
                "scan.unique_dependency_stat_prewarm", stat_phase_started,
                unique_path_count=len(stat_targets))
        progress.set(stat_prewarm_ms=round(stat_prewarm_ms, 3),
                     unique_stat_targets=len(stat_targets))
        object_check_started = time.perf_counter()
        object_check_phase_started = progress.subphase_start(
            "scan.object_freshness_checks", object_count=len(scan_list))
        invalidation_rows: list[dict] = []
        dependency_stats_by_token: dict[str, os.stat_result | None] = {}
        for index, (item, obj, dep) in enumerate(scan_list, 1):
            source_path = normalized(item["path"])
            if source_path in handled:
                continue
            scanned += 1
            check_started = time.perf_counter()
            check_started_utc = utc_now()
            awaiting = (time.monotonic() - progress.started_monotonic) * 1000.0
            dep_tokens = parsed_tokens.get(source_path)
            is_stale = args.force or dep_tokens is None
            stale_reason = ("forced rebuild" if args.force else
                            "missing or invalid dependency file" if dep_tokens is None else "")
            if not is_stale:
                try:
                    object_stat = os.stat(obj)
                except OSError:
                    is_stale = True
                    object_stat = None
                    stale_reason = "object file missing"
                if not is_stale and object_stat.st_size == 0:
                    is_stale = True
                    stale_reason = "object file is empty"
                if (not is_stale and pch_stat is not None
                        and source_uses_pch(source_path, args.pch)
                        and pch_stat.st_mtime >= object_stat.st_mtime):
                    is_stale = True
                    stale_reason = "precompiled header is newer than object"
                if not is_stale:
                    for token in dep_tokens or ():
                        if token not in dependency_stats_by_token:
                            dependency_stats_by_token[token] = cached_stat(
                                token, stat_cache)
                        dependency = dependency_stats_by_token[token]
                        if dependency is None:
                            is_stale = True
                            stale_reason = f"dependency missing or unreadable: {token}"
                            break
                        if dependency.st_mtime >= object_stat.st_mtime:
                            is_stale = True
                            stale_reason = f"dependency newer than object: {token}"
                            break
            entry_ms = (time.perf_counter() - check_started) * 1000.0
            dependency_check_ms += entry_ms
            dependency_check_rows.append((item["path"], entry_ms))
            if is_stale:
                stale.append((item, obj, dep))
                if changed_header_paths and dep_tokens:
                    matching_headers = sorted({
                        changed_header_paths[key] for token in dep_tokens
                        if (key := _dependency_cache_key(token))
                        in changed_header_paths})
                    if matching_headers:
                        stale_changed_header_dependencies[
                            normalized(item["path"])] = matching_headers
                invalidation_rows.append({"path": item["path"],
                                          "reason": stale_reason or "unknown stale condition",
                                          "dependency_check_ms": round(entry_ms, 3)})
                continue
            skipped += 1
            scan_rows.append({
                "sequence": base_completed + skipped,
                "path": item["path"],
                "status": "skipped",
                "seconds": 0.0,
                "compile_ms": 0.0,
                "dependency_check_ms": round(entry_ms, 3),
                "queue_wait_ms": 0.0,
                "check_started_utc": check_started_utc,
                "submitted_utc": "",
                "started_utc": check_started_utc,
                "started_elapsed_ms": round(awaiting, 3),
                "wall_time": utc_now(),
                "elapsed_ms": round((time.monotonic() - progress.started_monotonic) * 1000.0, 3),
                "build_attempt_id": progress.data.get("build_attempt_id"),
                "source_generation": progress.data.get("source_generation"),
                "generation": progress.data.get("generation"),
                "revision": progress.data.get("revision"),
                "progress_write_ms": 0.0,
                "record_overhead_ms": 0.0,
                "scan_phase": "object_cache_scan",
            })
            if not quiet and (time.monotonic() - last_snapshot >= 0.25
                              or index == len(scan_list)):
                progress.bulk_file_events(scan_rows)
                scan_rows = []
                progress.scan_snapshot(
                    scanned=scanned,
                    skipped=base_skipped + skipped,
                    stale=len(stale),
                    completed=base_completed + skipped,
                    scan_seconds=time.monotonic() - scan_started,
                    dependency_check_ms=dependency_check_ms,
                    parse_ms=parse_ms,
                    stat_ms=stat_prewarm_ms,
                    unique_dependencies=len(stat_cache))
                last_snapshot = time.monotonic()
        object_check_wall_ms = (time.perf_counter() - object_check_started) * 1000.0
        object_check_overhead_ms = max(0.0, object_check_wall_ms - dependency_check_ms)
        progress.subphase_end(
            "scan.object_freshness_checks", object_check_phase_started,
            object_count=scanned, dependency_check_sum_ms=round(dependency_check_ms, 3),
            event_and_loop_overhead_ms=round(object_check_overhead_ms, 3),
            stale_count=len(stale), skipped_count=skipped)
        progress.record_cache_invalidations(invalidation_rows)
        sorted_checks = sorted(dependency_check_rows, key=lambda row: row[1], reverse=True)
        progress.set(dependency_prepass_ms=round(dependency_prepass_ms, 3),
                     dependency_index_load_ms=round(dep_index_load_ms, 3),
                     dependency_index_save_ms=round(index_save_ms, 3),
                     dependency_path_and_fanout_ms=round(
                         dependency_path_and_fanout_ms, 3),
                     dependency_path_normalization_ms=round(
                         dependency_path_normalization_ms, 3),
                     external_dependency_classification_ms=round(
                         external_dependency_classification_ms, 3),
                     unique_dependency_metadata=len(dependency_metadata),
                     dependency_prepass_other_ms=round(
                         dependency_prepass_other_ms, 3),
                     stat_prewarm_ms=round(stat_prewarm_ms, 3),
                     unique_stat_targets=len(stat_targets),
                     unique_dependency_token_stat_checks=len(
                         dependency_stats_by_token),
                     object_check_wall_ms=round(object_check_wall_ms, 3),
                     object_check_overhead_ms=round(object_check_overhead_ms, 3),
                     dependency_check_count=len(dependency_check_rows),
                     dependency_check_min_ms=round(min(
                         (row[1] for row in dependency_check_rows), default=0.0), 3),
                     dependency_check_max_ms=round(
                         sorted_checks[0][1] if sorted_checks else 0.0, 3),
                     dependency_check_avg_ms=round(
                         dependency_check_ms / len(dependency_check_rows), 3)
                         if dependency_check_rows else 0.0,
                     dependency_check_slowest=[
                         {"path": path, "milliseconds": round(duration, 3)}
                         for path, duration in sorted_checks[:10]])
        scan_seconds = round(time.monotonic() - scan_started, 3)
        stat_ms = stat_prewarm_ms
        if not quiet:
            progress.bulk_file_events(scan_rows)
            scan_rows = []
            progress.scan_snapshot(
                scanned=scanned, skipped=base_skipped + skipped,
                stale=len(stale), completed=base_completed + skipped,
                scan_seconds=scan_seconds,
                dependency_check_ms=dependency_check_ms, parse_ms=parse_ms,
                stat_ms=stat_ms, unique_dependencies=len(stat_cache))
        return {"stale": stale, "rows": scan_rows, "scanned": scanned,
                "skipped": skipped, "scan_seconds": scan_seconds,
                "dependency_check_ms": dependency_check_ms,
                "parse_ms": parse_ms, "stat_ms": stat_ms,
                "unique_dependencies": len(stat_cache),
                "header_fanout": dict(header_counts),
                "stale_changed_header_dependencies":
                    stale_changed_header_dependencies,
                "external_dependencies": sorted(external_dependencies)}

    identity_dir = out_root / "identity"
    identity_path = identity_dir / IDENTITY_HEADER
    published = read_json(current_path) if current_path.is_file() else {}
    changed_paths = set()
    if change_report:
        for key in ("changed", "added"):
            changed_paths.update(normalized(path)
                                 for path in change_report.get(key, []))
    changed_entries = [entry for entry in entries
                       if normalized(entry[0]["path"]) in changed_paths
                       and str(entry[0]["path"]).endswith(".cpp")]
    changed_cpp_paths = {
        normalized(entry[0]["path"]) for entry in changed_entries}
    preflight_handled: set[str] = set()
    source_only_preflight = source_only_preflight_eligible(
        change_report, changed_cpp_paths)
    preflight_seconds = 0.0

    def next_generation() -> int:
        existing = [int(path.name.split("-")[-1])
                    for path in out_root.glob("generation-*")
                    if path.name.split("-")[-1].isdigit()]
        return (max(existing) + 1) if existing else 1

    published_generation = int(published.get("generation") or 0)
    published_dll_value = str(published.get("dll") or "")
    published_dll = Path(published_dll_value)
    if published_dll_value and not published_dll.is_absolute():
        published_dll = ROOT / published_dll

    def link_inputs_unchanged() -> bool:
        try:
            publish_stamp = published_dll.stat().st_mtime_ns
        except OSError:
            return False
        for path in (ROOT / "src" / "glad.c",
                     ROOT / "external/manifold-prebuilt/lib/libmanifold.a",
                     Path(cxx)):
            try:
                if path.is_file() and path.stat().st_mtime_ns > publish_stamp:
                    return False
            except OSError:
                return False
        return True

    def finish_no_op(*, scan_seconds: float, skipped: int, parse_ms: float,
                     stat_ms: float, unique_dependencies: int,
                     external_dependencies: list[str],
                     write_marker: bool) -> int:
        progress.scan_snapshot(
            scanned=len(entries), skipped=skipped, stale=0,
            completed=skipped, scan_seconds=scan_seconds,
            dependency_check_ms=parse_ms + stat_ms,
            parse_ms=parse_ms, stat_ms=stat_ms,
            unique_dependencies=unique_dependencies)
        if write_marker:
            write_cache_marker(
                object_dir, revision=revision, generation=generation,
                flags_hash=flags_hash, link_identity=link_identity,
                compiler_stamp=compiler_stamp,
                code_hash=str(published.get("code_hash") or ""),
                objects=objects, external_dependencies=external_dependencies)
        total_seconds = round(time.monotonic() - progress.started_monotonic, 3)
        worker_seconds = round(time.monotonic() - worker_entry_started, 3)
        progress.set_phase("no_op", no_op=True, generation=generation)
        progress.finish(
            "ok", compile_s=0.0, link_s=0.0, publish_s=0.0,
            total_s=total_seconds, worker_total_s=worker_seconds,
            no_op=True, generation=generation,
            object_scan_s=scan_seconds,
            dependency_check_ms=round(parse_ms + stat_ms, 3),
            scan_parse_ms=round(parse_ms, 3), scan_stat_ms=round(stat_ms, 3),
            scan_unique_dependencies=unique_dependencies,
            build_cause="no_op",
            phase="no_op")
        write_last_attempt(
            "ok", "no_op", no_op=True, generation=generation,
            dll=str(published.get("dll") or ""),
            code_hash=str(published.get("code_hash") or ""),
            total_seconds=total_seconds, worker_total_seconds=worker_seconds,
            worker_started_utc=worker_entry_utc,
            object_scan_seconds=scan_seconds, skipped=skipped,
            build_cause="no_op")
        emit_result({"status": "ok", "no_op": True,
                     "generation": generation, "revision": revision,
                     "build_attempt_id": attempt_id,
                     "object_scan_seconds": scan_seconds,
                     "total_seconds": total_seconds,
                     "worker_total_seconds": worker_seconds})
        print(f"[GAME BUILD] no_op generation={generation} "
              f"revision={revision} skipped={skipped} "
              f"scan={scan_seconds}s worker={worker_seconds}s "
              f"total={total_seconds}s", flush=True)
        return 0

    candidate = bool(
        not args.force and not args.sources_file and not changed_entries
        and str(published.get("status") or "") == "ok"
        and str(published.get("revision") or "") == revision
        and published_generation > 0
        and str(published.get("build_mode") or "") == args.build_mode
        and (str(published.get("flags_identity") or "") == flags_hash
             or str(published.get("object_cache") or "") == object_cache_rel)
        and str(published.get("link_identity") or "") == link_identity
        and published_dll.is_file()
        and link_inputs_unchanged())

    probe_scan = None
    identity_header_changed = False
    if candidate:
        generation = published_generation
        progress.data["generation"] = generation
        if identity_matches(identity_path):
            marker_started = time.monotonic()
            progress.set_phase("object_cache_scan",
                               cache_candidates=len(entries),
                               scan_mode="complete_marker")
            try:
                marker = read_json(object_dir / "complete.json")
            except (OSError, json.JSONDecodeError):
                marker = {}
            if cache_marker_valid(
                    marker, object_dir, objects, published, revision,
                    flags_hash, link_identity, compiler_stamp, args.pch):
                fast_seconds = round(time.monotonic() - marker_started, 3)
                return finish_no_op(
                    scan_seconds=fast_seconds, skipped=len(entries),
                    parse_ms=0.0, stat_ms=fast_seconds * 1000.0,
                    unique_dependencies=len(marker.get("external_dependencies") or []),
                    external_dependencies=[], write_marker=False)
        identity_header_changed = write_identity_if_changed(identity_path)
        if not identity_header_changed:
            progress.set_phase("object_cache_scan",
                               cache_candidates=len(entries),
                               scan_mode="dependency_probe")
            probe_scan = scan_entries(entries, handled=set(), quiet=True)
            if not probe_scan["stale"]:
                progress.bulk_file_events(probe_scan["rows"])
                return finish_no_op(
                    scan_seconds=probe_scan["scan_seconds"],
                    skipped=probe_scan["skipped"],
                    parse_ms=probe_scan["parse_ms"],
                    stat_ms=probe_scan["stat_ms"],
                    unique_dependencies=probe_scan["unique_dependencies"],
                    external_dependencies=probe_scan["external_dependencies"],
                    write_marker=True)

    generation = args.generation if args.generation > 0 else next_generation()
    progress.data["generation"] = generation
    progress.set(total=len(sources) + 1, generated_compile_units=1)
    identity_header_changed = (write_identity_if_changed(identity_path)
                               or identity_header_changed)
    generation_dir = out_root / f"generation-{generation}"
    generation_dir.mkdir(parents=True, exist_ok=True)
    dll_path = generation_dir / f"game-g{generation}.dll"
    staging = generation_dir / f"game-g{generation}.build.dll"

    progress.set_phase("identity_provider_source_generation")
    identity_source_started = progress.subphase_start(
        "identity.provider_source_generation")
    identity_provider_source = generation_dir / IDENTITY_PROVIDER_SOURCE
    identity_provider_contents = identity_provider_text(generation, revision)
    identity_provider_source.write_text(identity_provider_contents,
                                        encoding="utf-8")
    progress.subphase_end(
        "identity.provider_source_generation", identity_source_started,
        generated_source=str(identity_provider_source.relative_to(ROOT)),
        source_bytes=len(identity_provider_contents.encode("utf-8")),
        generation=generation, revision=revision)

    if source_only_preflight:
        # Keep the fast source-only cache-marker path: these directly changed
        # TUs can be validated before reusing the unchanged object set.
        to_build = [entry for entry in changed_entries
                    if args.force or dep_is_stale(
                        entry[1], entry[2],
                        use_pch=source_uses_pch(entry[0]["path"], args.pch))]
        progress.set_phase("changed_source_preflight",
                           preflight_candidates=len(to_build))
        preflight_started = time.monotonic()
        if to_build:
            run_compile_batch(to_build, min(jobs, len(to_build)))
            preflight_handled = {
                normalized(item["path"]) for item, obj, dep in to_build
                if not dep_is_stale(
                        obj, dep,
                        use_pch=source_uses_pch(item["path"], args.pch))}
        preflight_seconds = round(time.monotonic() - preflight_started, 3)
        if failures:
            failure = failures[0]
            message = (f"changed-source preflight failed: {failure['path']} "
                       f"in {preflight_seconds}s")
            progress.log(f"[GAME BUILD] {message}")
            progress.log(failure["output"])
            progress.finish(
                "compile_failed", compile_s=preflight_seconds,
                changed_source_preflight_s=preflight_seconds,
                failed=len(failures), failed_phase="changed_source_preflight",
                errors=[{"path": item["path"], "output": item["output"][:2000]}
                        for item in failures[:args.max_errors]],
                phase="compile_failed")
            write_last_attempt(
                "failed", "changed_source_preflight", error=message,
                errors=[{"path": item["path"], "output": item["output"][:2000]}
                        for item in failures[:args.max_errors]],
                preflight_seconds=preflight_seconds)
            emit_result({"status": "failed",
                         "phase": "changed_source_preflight",
                         "error": message,
                         "build_attempt_id": attempt_id,
                         "revision": revision,
                         "paths": [failure["path"]]})
            print(message, file=sys.stderr, flush=True)
            return 1
        stop_event.clear()
    # Mixed source/header changes must discover header dependents before
    # compiling. Put directly changed TUs and stale header dependents into the
    # same bounded compile wave to avoid a serial compile barrier. Hard GCC
    # diagnostics cancel the wave immediately.

    if probe_scan is None:
        progress.set_phase("object_cache_scan", cache_candidates=len(entries))
    if probe_scan is not None:
        scan = probe_scan
        stale = list(scan["stale"])
        rows = scan["rows"]
        if identity_header_changed:
            identity_paths = {normalized(path) for path in IDENTITY_SOURCE_PATHS}
            stale.extend(entry for entry in entries
                         if normalized(entry[0]["path"]) in identity_paths
                         and entry not in stale)
            rows = [row for row in rows
                    if normalized(row["path"]) not in identity_paths]
        for row in rows:
            row["generation"] = generation
        progress.bulk_file_events(rows)
        completed = (int(progress.data.get("completed") or 0)
                     + len(rows))
        progress.scan_snapshot(
            scanned=scan["scanned"], skipped=len(rows), stale=len(stale),
            completed=completed, scan_seconds=scan["scan_seconds"],
            dependency_check_ms=scan["dependency_check_ms"],
            parse_ms=scan["parse_ms"], stat_ms=scan["stat_ms"],
            unique_dependencies=scan["unique_dependencies"])
        dependency_scan_seconds = scan["scan_seconds"]
        dependency_check_ms = scan["dependency_check_ms"]
        scan_parse_ms = scan["parse_ms"]
        scan_stat_ms = scan["stat_ms"]
        scan_unique_dependencies = scan["unique_dependencies"]
    else:
        scan = None
        source_only_paths = source_only_changed_paths(change_report)
        current_changed_paths = {
            normalized(item["path"]) for item, _obj, _dep in changed_entries
        }
        object_paths_by_source = {
            normalized(item["path"]): obj for item, obj, _dep in entries
        }
        source_only_scan_started = time.perf_counter()
        if (source_only_paths == current_changed_paths
                and not args.force and not args.sources_file
                and not identity_header_changed
                and len(object_paths_by_source) == len(entries)):
            fresh_changed_paths = set(preflight_handled)
            for item, obj, dep in changed_entries:
                source_path = normalized(item["path"])
                if (source_path not in fresh_changed_paths
                        and not dep_is_stale(
                            obj, dep,
                            use_pch=source_uses_pch(
                                item["path"], args.pch))):
                    fresh_changed_paths.add(source_path)
            marker_path = object_dir / "complete.json"
            try:
                marker = read_json(marker_path)
            except (OSError, json.JSONDecodeError):
                marker = {}
            marker_phase_started = progress.subphase_start(
                "scan.source_only_cache_marker",
                cache_candidates=len(entries),
                changed_source_count=len(current_changed_paths))
            marker_valid = source_only_cache_marker_valid(
                change_report, revision, current_changed_paths,
                fresh_changed_paths, object_paths_by_source, object_dir,
                marker, published, object_cache_rel, flags_hash,
                link_identity, compiler_stamp, args.pch)
            marker_validation_ms = progress.subphase_end(
                "scan.source_only_cache_marker", marker_phase_started,
                outcome="reused" if marker_valid else "fallback_full_scan")
            if marker_valid:
                progress.set_phase(
                    "object_cache_scan", cache_candidates=len(entries),
                    scan_mode="source_only_complete_marker")
                base_completed = int(progress.data.get("completed") or 0)
                base_skipped = int(progress.data.get("skipped") or 0)
                skipped = len(entries) - len(preflight_handled)
                scan_seconds = time.perf_counter() - source_only_scan_started
                external_dependencies = marker.get(
                    "external_dependencies") or []
                if not isinstance(external_dependencies, list):
                    external_dependencies = []
                progress.scan_snapshot(
                    scanned=len(entries), skipped=base_skipped + skipped,
                    stale=0, completed=base_completed + skipped,
                    scan_seconds=scan_seconds,
                    dependency_check_ms=0.0, parse_ms=0.0,
                    stat_ms=marker_validation_ms,
                    unique_dependencies=len(external_dependencies),
                    scan_mode="source_only_complete_marker",
                    cache_marker_reused_count=skipped)
                scan = {
                    "stale": [], "rows": [],
                    "scanned": len(entries), "skipped": skipped,
                    "scan_seconds": scan_seconds,
                    "dependency_check_ms": 0.0, "parse_ms": 0.0,
                    "stat_ms": marker_validation_ms,
                    "unique_dependencies": len(external_dependencies),
                    "header_fanout": {},
                    "external_dependencies": external_dependencies,
                }
        if scan is None:
            scan = scan_entries(entries, handled=preflight_handled,
                                quiet=False)
        stale = scan["stale"]
        dependency_scan_seconds = scan["scan_seconds"]
        dependency_check_ms = scan["dependency_check_ms"]
        scan_parse_ms = scan["parse_ms"]
        scan_stat_ms = scan["stat_ms"]
        scan_unique_dependencies = scan["unique_dependencies"]

    header_fanout = scan.get("header_fanout", {})
    changed_headers = []
    if change_report:
        changed_headers = [
            normalized(path) for path in
            (change_report.get("changed_headers") or []) +
            (change_report.get("added_headers") or [])]
    changed_header_fanout = {
        path: int(header_fanout.get(path, 0)) for path in changed_headers}
    changed_sources = []
    if change_report:
        changed_sources = [
            normalized(path) for path in
            (change_report.get("changed") or []) +
            (change_report.get("added") or [])
            if str(path).endswith(".cpp")]
    if changed_headers and changed_sources:
        build_cause = "mixed"
    elif changed_headers:
        build_cause = "header_fanout"
    elif changed_sources:
        build_cause = "changed_sources"
    elif identity_header_changed:
        build_cause = "identity"
    else:
        build_cause = "rebuild"
    fanout_top = [
        {"path": path, "dependents": count}
        for path, count in sorted(
            header_fanout.items(), key=lambda item: (-item[1], item[0]))[:20]]

    work = [entry for entry in stale
            if normalized(entry[0]["path"]) not in preflight_handled]
    priority_started = progress.subphase_start(
        "compile.changed_header_priority",
        stale_candidate_count=len(work),
        changed_header_count=len(changed_header_paths))
    work, compile_priority = prioritize_changed_header_dependents(
        work,
        scan.get("stale_changed_header_dependencies", {}),
        set(changed_header_paths.values()),
        set(changed_sources))
    compile_priority["ordering_ms"] = round(
        (time.perf_counter() - priority_started) * 1000.0, 3)
    progress.subphase_end(
        "compile.changed_header_priority", priority_started,
        **compile_priority)
    identity_provider_object = generation_dir / "module-identity-provider.o"
    identity_provider_dep = generation_dir / "module-identity-provider.d"
    identity_provider_entry = (
        {"path": str(identity_provider_source),
         "sha256": hashlib.sha256(
             identity_provider_contents.encode("utf-8")).hexdigest()},
        identity_provider_object,
        identity_provider_dep,
    )
    progress.set_phase("vendor_compile",
                       dependency_scan_seconds=dependency_scan_seconds,
                       object_scan_s=dependency_scan_seconds,
                       dependency_check_ms=round(dependency_check_ms, 3),
                       scan_parse_ms=round(scan_parse_ms, 3),
                       scan_stat_ms=round(scan_stat_ms, 3),
                       scan_unique_dependencies=scan_unique_dependencies,
                       pending_compile_count=len(work) + 1,
                       identity_provider_pending=1,
                        compile_priority=compile_priority,
                       changed_source_preflight_s=preflight_seconds)

    # Transitional diagnostic link input: the game DLL calls GLAD directly
    # today. Build the resident C dispatcher into the candidate so the whole-game
    # link can be measured; this is not a cutover-safe render boundary and is
    # replaced by CAP_RENDER before cutover.
    vendor_objects: list[Path] = [identity_provider_object]
    vendor_dir = out_root / "vendor"
    vendor_dir.mkdir(parents=True, exist_ok=True)
    glad_source = SOURCE_ROOT / "src" / "glad.c"
    glad_object = vendor_dir / "glad.o"
    vendor_compile_seconds = 0.0
    if glad_source.is_file() and (
            not glad_object.is_file() or
            glad_source.stat().st_mtime > glad_object.stat().st_mtime):
        progress.log("[VENDOR] compiling src/glad.c for diagnostic link")
        vendor_started = time.monotonic()
        result = run_build_tool(
            ["gcc", "-O2", "-g", "-c", "src/glad.c", "-o", str(glad_object),
             "-Iinclude", "-Isrc"],
            cwd=str(SOURCE_ROOT), stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, text=True)
        if result.returncode != 0:
            output = result.stdout or "glad compile failed"
            message = "vendor glad compile failed"
            progress.log(output)
            progress.finish("vendor_compile_failed",
                            failed_phase="vendor_compile",
                            error=message, errors=[output[:2000]],
                            phase="vendor_compile_failed")
            write_last_attempt("failed", "vendor_compile", error=message,
                               errors=[output[:2000]])
            emit_result({"status": "failed", "phase": "vendor_compile",
                         "error": message, "build_attempt_id": attempt_id,
                         "revision": revision})
            print(f"[GAME BUILD] {message}", file=sys.stderr, flush=True)
            return 1
        vendor_compile_seconds = round(time.monotonic() - vendor_started, 3)
    if glad_object.is_file():
        vendor_objects.append(glad_object)
    progress.set_phase(
        "compile", vendor_compile_seconds=vendor_compile_seconds,
        pending=len(work) + 1, total=len(sources) + 1,
        generated_compile_units=1,
        identity_provider_source=str(identity_provider_source.relative_to(ROOT)))

    compile_start = time.monotonic()
    identity_compile_started = progress.subphase_start(
        "compile.module_identity_provider",
        generated_source=str(identity_provider_source.relative_to(ROOT)))
    run_compile_batch([identity_provider_entry], 1)
    identity_provider_compile_ms = progress.subphase_end(
        "compile.module_identity_provider", identity_compile_started,
        generated_source=str(identity_provider_source.relative_to(ROOT)),
        generated_object=str(identity_provider_object.relative_to(ROOT)),
        outcome="failed" if failures else "ok")
    if work and not failures:
        stop_event.clear()
        run_compile_batch(work, jobs)
    compile_seconds = round(preflight_seconds + time.monotonic() - compile_start, 3)

    if failures:
        message = (f"compile failed: {len(failures)} file(s) in {compile_seconds}s")
        for failure in failures[:args.max_errors]:
            progress.log(f"[GAME BUILD] FAILED {failure['path']}")
            progress.log(failure["output"])
        progress.finish(
            "compile_failed", compile_s=compile_seconds,
            failed=len(failures), failed_phase="compile",
            errors=[{"path": item["path"], "output": item["output"][:2000]}
                    for item in failures[:args.max_errors]],
            phase="compile_failed")
        write_last_attempt(
            "failed", "compile", error=message,
            errors=[{"path": item["path"], "output": item["output"][:2000]}
                    for item in failures[:args.max_errors]],
            compile_seconds=compile_seconds)
        emit_result({"status": "failed", "phase": "compile", "error": message,
                     "build_attempt_id": attempt_id, "revision": revision,
                     "paths": [failure["path"] for failure in failures[:args.max_errors]]})
        print(message, file=sys.stderr, flush=True)
        return 1

    progress.set_phase("link", compile_s=compile_seconds)
    response = generation_dir / "link.rsp"
    link_lines = [path.as_posix() for path in objects]
    link_lines.extend(path.as_posix() for path in vendor_objects)
    link_lines.append((ROOT / "external/manifold-prebuilt/lib/libmanifold.a").as_posix())
    link_lines.append(f"-L{Path(glfw_lib_dir).as_posix()}")
    link_lines.extend(LINK_LIBS)
    link_prepare_started = progress.subphase_start("link.response_file_prepare")
    response.write_text("\n".join(link_lines) + "\n", encoding="utf-8")
    object_input_bytes = 0
    for path in objects:
        try:
            object_input_bytes += path.stat().st_size
        except OSError:
            pass
    extra_object_input_bytes = 0
    for path in vendor_objects:
        try:
            extra_object_input_bytes += path.stat().st_size
        except OSError:
            pass
    progress.subphase_end("link.response_file_prepare", link_prepare_started,
                          response_file_bytes=response.stat().st_size,
                          object_input_count=len(objects),
                          extra_object_input_count=len(vendor_objects),
                          total_object_input_count=(len(objects) +
                                                    len(vendor_objects)),
                          identity_provider_object=(
                              str((generation_dir / "module-identity-provider.o")
                                  .relative_to(ROOT))),
                          linker_argument_count=len(link_lines),
                          object_input_bytes=object_input_bytes,
                          extra_object_input_bytes=extra_object_input_bytes,
                          total_object_input_bytes=(object_input_bytes +
                                                    extra_object_input_bytes))

    link_command = [cxx]
    if args.linker == "lld":
        link_command.extend(["-fuse-ld=lld", f"-B{linker_path.parent}"])
    link_command.extend(["-shared", "-o", str(staging), "@" + str(response)])
    if os.path.isfile(staging):
        os.remove(staging)
    link_start = time.monotonic()
    link_command_started = progress.subphase_start(
        f"link.{args.linker}_linker", linker=str(linker_path),
        object_input_count=len(objects),
        linker_argument_count=len(link_lines),
        response_file=str(response.relative_to(ROOT)))
    link_result = run_build_tool(link_command, cwd=str(ROOT), stdout=subprocess.PIPE,
                           stderr=subprocess.STDOUT, text=True)
    link_seconds = round(time.monotonic() - link_start, 3)
    progress.subphase_end(f"link.{args.linker}_linker", link_command_started,
                          outcome="ok" if link_result.returncode == 0 else "failed",
                          exit_code=link_result.returncode,
                          staged_dll_bytes=(staging.stat().st_size
                                            if staging.is_file() else 0))
    if link_result.returncode != 0:
        output = link_result.stdout or ""
        message = f"link failed in {link_seconds}s"
        progress.log(f"[GAME BUILD] {message}")
        progress.log(output)
        progress.finish("link_failed", compile_s=compile_seconds,
                        link_s=link_seconds,
                        failed_phase="link", error=message,
                        errors=[output[-4000:]], phase="link_failed")
        write_last_attempt("failed", "link", error=message,
                           errors=[output[-4000:]],
                           compile_seconds=compile_seconds,
                           link_seconds=link_seconds)
        emit_result({"status": "failed", "phase": "link", "error": message,
                     "build_attempt_id": attempt_id, "revision": revision})
        print(f"[GAME BUILD] {message}; see {progress.log_path}",
              file=sys.stderr, flush=True)
        return link_result.returncode or 1

    progress.set_phase("coherence_validation")
    validation_started = time.monotonic()
    final_errors = verify_coherent(SOURCE_ROOT, sources)
    if final_errors:
        message = f"coherence rejected: {final_errors[0]}"
        for error in final_errors:
            progress.log(f"[GAME BUILD] coherence rejected: {error}")
        try:
            staging.unlink()
        except OSError:
            pass
        progress.finish("coherence_failed", compile_s=compile_seconds,
                        link_s=link_seconds,
                        validation_seconds=round(time.monotonic() - validation_started, 3),
                        failed_phase="coherence_validation", error=message,
                        errors=final_errors[:20])
        write_last_attempt("failed", "coherence_validation", error=message,
                           errors=final_errors[:20],
                           compile_seconds=compile_seconds,
                           link_seconds=link_seconds)
        emit_result({"status": "failed", "phase": "coherence_validation",
                     "error": message, "build_attempt_id": attempt_id,
                     "revision": revision, "paths": final_errors[:20]})
        print(f"[GAME BUILD] {message}", file=sys.stderr, flush=True)
        return 2

    progress.set_phase("publish")
    publish_started = time.monotonic()
    publish_hash_started = progress.subphase_start("publish.rename_and_hash")
    try:
        os.replace(staging, dll_path)
        code_hash = sha256_file(dll_path)
    except OSError as error:
        progress.subphase_end("publish.rename_and_hash", publish_hash_started,
                              outcome="failed", error=str(error)[:300])
        message = f"publish failed: {error}"
        progress.finish("publish_failed", compile_s=compile_seconds,
                        link_s=link_seconds, failed_phase="publish",
                        error=message, errors=[message])
        write_last_attempt("failed", "publish", error=message,
                           compile_seconds=compile_seconds,
                           link_seconds=link_seconds)
        emit_result({"status": "failed", "phase": "publish", "error": message,
                     "build_attempt_id": attempt_id, "revision": revision})
        print(f"[GAME BUILD] {message}", file=sys.stderr, flush=True)
        return 1
    progress.subphase_end("publish.rename_and_hash", publish_hash_started,
                          outcome="ok", dll_size_bytes=dll_path.stat().st_size)
    publish_seconds = round(time.monotonic() - publish_started, 3)
    total_seconds = round(time.monotonic() - progress.started_monotonic, 3)
    worker_seconds = round(time.monotonic() - worker_entry_started, 3)
    progress.data["worker_started_utc"] = worker_entry_utc
    current = {
        "status": "ok",
        "no_op": False,
        "abi_version": ABI_VERSION,
        "generation": generation,
        "revision": revision,
        "code_hash": code_hash,
        "dll": str(dll_path.relative_to(ROOT)) if dll_path.is_relative_to(ROOT) else str(dll_path),
        "source_count": len(sources),
        "source_root": (str(SOURCE_ROOT.relative_to(ROOT))
                        if SOURCE_ROOT.is_relative_to(ROOT) else str(SOURCE_ROOT)),
        "compiled": progress.data["compiled"],
        "skipped": progress.data["skipped"],
        "build_mode": args.build_mode,
        "flags_identity": flags_hash,
        "link_identity": link_identity,
        "linker": args.linker,
        "linker_path": str(linker_path),
        "compiler_stamp": compiler_stamp,
        "jobs": jobs,
        "priority": args.priority,
        "compile_seconds": compile_seconds,
        "link_seconds": link_seconds,
        "publish_seconds": publish_seconds,
        "object_scan_seconds": dependency_scan_seconds,
        "dependency_check_ms": round(dependency_check_ms, 3),
        "scan_parse_ms": round(scan_parse_ms, 3),
        "scan_stat_ms": round(scan_stat_ms, 3),
        "scan_unique_dependencies": scan_unique_dependencies,
        "build_cause": build_cause,
        "changed_sources": changed_sources,
        "changed_headers": changed_headers,
        "changed_header_fanout": changed_header_fanout,
        "compile_priority": compile_priority,
        "header_fanout_top": fanout_top,
        "identity_wrapper_count": (len(IDENTITY_SOURCE_PATHS)
                                   if identity_header_changed else 0),
        "generated_compile_units": 1,
        "identity_provider_source": str(
            identity_provider_source.relative_to(ROOT)),
        "identity_provider_object": str(
            identity_provider_object.relative_to(ROOT)),
        "identity_provider_compile_ms": identity_provider_compile_ms,
        "changed_source_preflight_seconds": preflight_seconds,
        "total_seconds": total_seconds,
        "worker_total_seconds": worker_seconds,
        "worker_started_utc": worker_entry_utc,
        "object_cache": object_cache_rel,
        "seeded_objects": seeded,
        "slowest_compiles": progress.data["slowest"],
        "generated_utc": utc_now(),
    }
    if change_report is not None:
        current["change_report"] = change_report
    current_write_started = progress.subphase_start("publish.current_metadata")
    write_json_atomic(current_path, current, retries=40, delay=0.05)
    progress.subphase_end("publish.current_metadata", current_write_started,
                          path=str(current_path.relative_to(ROOT)))
    generation_write_started = progress.subphase_start("publish.generation_metadata")
    write_json_atomic(generation_dir / "generation.json", current,
                      retries=40, delay=0.05)
    progress.subphase_end("publish.generation_metadata", generation_write_started,
                          path=str((generation_dir / "generation.json").relative_to(ROOT)))
    dependency_manifest_started = progress.subphase_start(
        "publish.external_dependency_manifest")
    # scan_entries already parsed the depfiles and classified their tokens.
    # Reuse that complete result instead of rereading all depfiles after link.
    external_dependencies = list(scan.get("external_dependencies") or ())
    progress.subphase_end("publish.external_dependency_manifest",
                          dependency_manifest_started,
                          dependency_count=len(external_dependencies),
                          depfile_count=len(entries),
                          source="object_cache_scan")
    cache_marker_started = progress.subphase_start("publish.cache_marker")
    write_cache_marker(
        object_dir, revision=revision, generation=generation,
        flags_hash=flags_hash, link_identity=link_identity,
        compiler_stamp=compiler_stamp, code_hash=code_hash,
        objects=objects,
        external_dependencies=external_dependencies)
    progress.subphase_end("publish.cache_marker", cache_marker_started,
                          object_count=len(objects),
                          dependency_count=len(external_dependencies))
    progress.finish("ok", compile_s=compile_seconds, link_s=link_seconds,
                    publish_s=publish_seconds, total_s=total_seconds,
                    worker_total_s=worker_seconds, no_op=False,
                    generation=generation,
                    object_scan_s=dependency_scan_seconds,
                    dependency_check_ms=round(dependency_check_ms, 3),
                    scan_parse_ms=round(scan_parse_ms, 3),
                    scan_stat_ms=round(scan_stat_ms, 3),
                    scan_unique_dependencies=scan_unique_dependencies,
                    build_cause=build_cause,
                    changed_headers=changed_headers,
                    changed_header_fanout=changed_header_fanout,
                    header_fanout_top=fanout_top,
                    changed_source_preflight_s=preflight_seconds,
                    phase="done")
    write_last_attempt(
        "ok", "done", no_op=False, generation=generation,
        dll=current["dll"], code_hash=code_hash,
        compile_seconds=compile_seconds, link_seconds=link_seconds,
        publish_seconds=publish_seconds, total_seconds=total_seconds,
        worker_total_seconds=worker_seconds,
        worker_started_utc=worker_entry_utc,
        object_scan_seconds=dependency_scan_seconds,
        dependency_check_ms=round(dependency_check_ms, 3),
        build_cause=build_cause,
        changed_headers=changed_headers,
        changed_header_fanout=changed_header_fanout,
        header_fanout_top=fanout_top,
        changed_source_preflight_seconds=preflight_seconds)
    emit_result({"status": "ok", "no_op": False, "generation": generation,
                 "revision": revision, "build_attempt_id": attempt_id,
                 "dll": current["dll"], "code_hash": code_hash,
                 "compiled": current["compiled"],
                 "skipped": current["skipped"],
                 "object_scan_seconds": dependency_scan_seconds,
                 "compile_seconds": compile_seconds,
                 "link_seconds": link_seconds,
                 "total_seconds": total_seconds,
                 "worker_total_seconds": worker_seconds})
    print(f"[GAME BUILD] generation={generation} revision={revision} "
          f"compiled={current['compiled']} skipped={current['skipped']} "
          f"compile={compile_seconds}s link={link_seconds}s total={total_seconds}s "
          f"worker={worker_seconds}s dll={current['dll']}", flush=True)
    for entry in current["slowest_compiles"][:5]:
        print(f"[GAME BUILD] slow {entry['seconds']:.2f}s {entry['path']}", flush=True)
    if change_report is not None:
        print(f"[GAME BUILD] change {change_report.get('old_revision')} -> "
              f"{change_report.get('new_revision')} "
              f"added={len(change_report.get('added', []))} "
              f"removed={len(change_report.get('removed', []))} "
              f"changed={len(change_report.get('changed', []))} "
              f"headers={len(change_report.get('changed_headers', []))}", flush=True)
    return 0


def main(argv: list[str] | None = None) -> int:
    global SOURCE_ROOT
    parser = argparse.ArgumentParser(description="Whole-game candidate DLL builder")
    parser.add_argument("--manifest", default="build/source-manifest.json")
    parser.add_argument("--source-root", default="",
                        help="frozen source tree to compile (defaults to the repository root)")
    parser.add_argument("--sources-file", default="",
                        help="explicit probe source set (JSON list or {\"sources\": [...]})")
    parser.add_argument("--out", default="build/game")
    parser.add_argument("--generation", type=int, default=0)
    parser.add_argument("--build-mode", choices=("debug", "release"), default="debug")
    parser.add_argument("--linker", choices=("auto", "gnu", "lld"), default="auto",
                        help="linker for this candidate generation; auto prefers LLD and uses GNU ld if LLD is absent")
    parser.add_argument("--revision", default="", help="required manifest revision")
    parser.add_argument("--change-report", default="",
                        help="source diff report to attach to current/generation metadata")
    parser.add_argument("--attempt-context", default="",
                        help="dev-loop build attempt timing and annotation context")
    parser.add_argument("--jobs", type=int, default=0, help="compile workers; 0 = all cores")
    parser.add_argument("--priority", choices=tuple(PRIORITY_CLASSES), default="high")
    parser.add_argument("--max-errors", type=int, default=20)
    parser.add_argument("--no-seed", action="store_true", help="do not reuse older generation objects")
    parser.add_argument("--no-probe-bridge", action="store_true",
                        help="explicit whole-game source set, not a probe bridge build")
    parser.add_argument("--no-pch", dest="pch", action="store_false", default=True)
    parser.add_argument("--force", action="store_true")
    parser.add_argument("--list", action="store_true")
    args = parser.parse_args(argv)
    if args.source_root:
        SOURCE_ROOT = Path(args.source_root).resolve()

    try:
        return build(args)
    except OSError as error:
        print(f"[GAME BUILD] {error}", file=sys.stderr)
        return 4


if __name__ == "__main__":
    raise SystemExit(main())
