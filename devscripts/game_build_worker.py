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


def set_process_priority(level: str) -> None:
    if os.name != "nt":
        return
    try:
        kernel32 = ctypes.windll.kernel32
        kernel32.SetPriorityClass(kernel32.GetCurrentProcess(), PRIORITY_CLASSES[level])
    except Exception:  # noqa: BLE001 - priority is best effort
        pass


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
                if not self.dirty:
                    continue
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
               started_elapsed_ms: float = 0.0) -> None:
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
            elif status == "skipped":
                self.data["skipped"] += 1
            elif status == "failed":
                self.data["failed"] += 1
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
            self.data["file_events"].append(file_event)
            del self.data["file_events"][:-100]
            if status == "skipped":
                self._append_locked(f"[SKIP] {self.data['completed']}/{self.data['total']} {path}")
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
                      stat_ms: float, unique_dependencies: int) -> None:
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
    return hashlib.sha256(f"{item['path']}:{item['sha256']}".encode("utf-8")).hexdigest()[:20]


def flags_identity(cxx: str, base_flags: list[str], build_mode: str, pch: bool) -> str:
    text = "\n".join([cxx, f"mode={build_mode}", f"pch={int(pch)}", *base_flags])
    return hashlib.sha256(text.encode("utf-8")).hexdigest()[:16]


def identity_text(generation: int, revision: str) -> str:
    return ("#pragma once\n"
            f"#define MIMITA_MODULE_GENERATION {generation}u\n"
            f'#define MIMITA_MODULE_SOURCE_HASH "{revision}"\n'
            "#define MIMITA_MODULE_STATE_SCHEMA 1\n")


def write_identity_if_changed(path: Path, generation: int, revision: str) -> bool:
    """Write the generated identity header only when its bytes change.

    Cache freshness uses file mtimes, so rewriting an identical header would
    invalidate every module wrapper for no reason.
    """
    text = identity_text(generation, revision)
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.is_file():
        try:
            if path.read_text(encoding="utf-8") == text:
                return False
        except OSError:
            pass
    path.write_text(text, encoding="utf-8")
    return True


def identity_matches(path: Path, generation: int, revision: str) -> bool:
    if not path.is_file():
        return False
    try:
        return path.read_text(encoding="utf-8") == identity_text(generation, revision)
    except OSError:
        return False


def marker_objects(object_dir: Path) -> dict[str, tuple[int, int]] | None:
    try:
        with os.scandir(object_dir) as entries:
            return {
                entry.name: (entry.stat().st_size, entry.stat().st_mtime_ns)
                for entry in entries if entry.name.endswith(".o")
            }
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


def collect_external_dependencies(entries: list[tuple[dict, Path, Path]]) -> list[str]:
    found: set[str] = set()
    for _item, _obj, dep in entries:
        for token in parse_depfile(dep) or ():
            if is_external_dependency(token):
                found.add(token)
    return sorted(found)


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


def dependency_relative(token: str) -> str:
    """Return a project-relative header path for fan-out attribution."""
    path = Path(token)
    if path.is_absolute():
        try:
            path = ROOT / path.relative_to(SOURCE_ROOT)
        except ValueError:
            pass
    else:
        path = ROOT / path
    try:
        relative = path.resolve().relative_to(ROOT).as_posix()
    except (OSError, ValueError):
        return ""
    if relative.startswith("src/") and relative.endswith(
            (".h", ".hpp", ".inl")):
        return relative
    return ""


def cached_stat(token: str, cache: dict[str, os.stat_result | None]) -> os.stat_result | None:
    path = Path(token)
    if path.is_absolute():
        # Dependency files produced from a frozen source snapshot keep absolute
        # snapshot paths. Resolve them against the live tree so the cache stays
        # usable after the snapshot is removed.
        try:
            path = ROOT / path.relative_to(SOURCE_ROOT)
        except ValueError:
            path = Path(token)
    else:
        path = ROOT / path
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


def dep_is_stale(obj: Path, dep: Path) -> bool:
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

    set_process_priority(args.priority)

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
    link_identity = hashlib.sha256("\n".join([
        *LINK_LIBS, "manifold-prebuilt", f"glfw={Path(glfw_lib_dir).as_posix()}",
        f"abi={ABI_VERSION}"]).encode("utf-8")
    ).hexdigest()[:16]
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
    progress.worker_started_monotonic = worker_entry_started
    progress.data["worker_started_utc"] = worker_entry_utc
    progress.data["phase"] = "toolchain_setup"
    progress.phase_started_monotonic = toolchain_started
    progress.data["phase_started_utc"] = toolchain_started_utc
    progress.set_phase("identity_and_cache",
                       compiler=str(cxx), flags_identity=flags_hash,
                       link_identity=link_identity,
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

    def module_extra_flags(source_path: str) -> list[str]:
        symbol = FOLDED_MODULE_SYMBOLS.get(source_path)
        if symbol:
            return [f"-I{identity_dir}", "-DMIMITA_MODULE_BUILD",
                    f"-DGetMimitaModuleAPI={symbol}"]
        if source_path in IDENTITY_ONLY_SOURCES:
            return [f"-I{identity_dir}", "-DMIMITA_MODULE_BUILD"]
        return []

    def compile_command(item: dict, obj: Path, dep: Path) -> list[str]:
        source_path = normalized(item["path"])
        return [*ccache_prefix, cxx, *base_flags, *module_extra_flags(source_path),
                "-MF", str(dep), "-c", item["path"], "-o", str(obj)]

    def compile_one(item: dict, obj: Path, dep: Path, submitted: float,
                    submitted_utc: str
                    ) -> tuple[str, dict, float, str, float, str, float]:
        if stop_event.is_set():
            return ("skipped", item, 0.0, "", 0.0, "", 0.0)
        queue_wait_ms = (time.monotonic() - submitted) * 1000.0
        started_utc, started_elapsed_ms = progress.start_source(
            item["path"], queue_wait_ms=queue_wait_ms,
            submitted_utc=submitted_utc)
        started = time.perf_counter()
        result = run_tool(compile_command(item, obj, dep), cwd=str(SOURCE_ROOT),
                          stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                          text=True)
        elapsed_ms = (time.perf_counter() - started) * 1000.0
        if result.returncode != 0:
            return ("failed", item, elapsed_ms, result.stdout or "",
                    queue_wait_ms, started_utc, started_elapsed_ms)
        return ("compiled", item, elapsed_ms, "", queue_wait_ms,
                started_utc, started_elapsed_ms)

    def record_compile_result(result: tuple) -> str:
        (status, item, elapsed_ms, output, queue_wait_ms,
         started_utc, started_elapsed_ms) = result
        if status == "skipped":
            return status
        progress.record(status, item["path"], elapsed_ms, output,
                        queue_wait_ms=queue_wait_ms,
                        started_utc=started_utc,
                        started_elapsed_ms=started_elapsed_ms)
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
        stat_cache: dict[str, os.stat_result | None] = {}
        external_dependencies: set[str] = set()
        pch = ROOT / "src" / "pch.h.gch"
        try:
            pch_stat = (pch.stat() if args.pch and pch.is_file() else None)
        except OSError:
            pch_stat = None
        scanned = skipped = 0
        dependency_check_ms = parse_ms = stat_ms = 0.0
        last_snapshot = scan_started
        parsed_tokens: dict[str, list[str] | None] = {}
        stat_targets: set[str] = set()
        header_counts: Counter[str] = Counter()
        prepass_started = time.perf_counter()
        last_prepass_update = prepass_started
        progress.scan_prepass(parsed=0, total=len(scan_list), current="",
                              elapsed_s=0.0)
        for index, (item, _obj, dep) in enumerate(scan_list, 1):
            source_path = normalized(item["path"])
            tokens = parse_depfile(dep)
            parsed_tokens[source_path] = tokens
            for token in tokens or ():
                if is_external_dependency(token):
                    external_dependencies.add(token)
                relative = dependency_relative(token)
                if relative:
                    header_counts[relative] += 1
            if source_path not in handled and tokens:
                stat_targets.update(tokens)
            now = time.perf_counter()
            if index == len(scan_list) or now - last_prepass_update >= 0.5:
                progress.scan_prepass(
                    parsed=index, total=len(scan_list), current=source_path,
                    elapsed_s=now - prepass_started)
                last_prepass_update = now
        parse_ms = (time.perf_counter() - prepass_started) * 1000.0
        progress.set(scan_phase="object_checks", scan_parsed=len(scan_list),
                     scan_current="", scan_prepass_elapsed_s=round(
                         (time.perf_counter() - prepass_started), 3))
        stat_prewarm_ms = 0.0
        if stat_targets:
            stat_started = time.perf_counter()
            workers = min(16, max(2, (os.cpu_count() or 4) * 2))
            with ThreadPoolExecutor(max_workers=workers) as executor:
                list(executor.map(
                    lambda token: cached_stat(token, stat_cache),
                    sorted(stat_targets)))
            stat_prewarm_ms = (time.perf_counter() - stat_started) * 1000.0
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
            if not is_stale:
                try:
                    object_stat = os.stat(obj)
                except OSError:
                    is_stale = True
                    object_stat = None
                if not is_stale and (object_stat.st_size == 0 or (
                        pch_stat is not None and
                        pch_stat.st_mtime >= object_stat.st_mtime)):
                    is_stale = True
                if not is_stale:
                    for token in dep_tokens or ():
                        dependency = cached_stat(token, stat_cache)
                        if dependency is None or dependency.st_mtime >= object_stat.st_mtime:
                            is_stale = True
                            break
            entry_ms = (time.perf_counter() - check_started) * 1000.0
            dependency_check_ms += entry_ms
            if is_stale:
                stale.append((item, obj, dep))
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
    preflight_seconds = 0.0
    preflight_handled: set[str] = set()

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
        and published_dll.is_file()
        and link_inputs_unchanged())

    probe_scan = None
    identity_changed = False
    if candidate:
        generation = published_generation
        progress.data["generation"] = generation
        if identity_matches(identity_path, generation, revision):
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
        identity_changed = write_identity_if_changed(identity_path, generation, revision)
        if not identity_changed:
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
    identity_changed = write_identity_if_changed(identity_path, generation, revision)
    generation_dir = out_root / f"generation-{generation}"
    generation_dir.mkdir(parents=True, exist_ok=True)
    dll_path = generation_dir / f"game-g{generation}.dll"
    staging = generation_dir / f"game-g{generation}.build.dll"

    if changed_entries:
        to_build = [entry for entry in changed_entries
                    if args.force or dep_is_stale(entry[1], entry[2])]
        progress.set_phase("changed_source_preflight",
                           preflight_candidates=len(to_build))
        preflight_started = time.monotonic()
        if to_build:
            run_compile_batch(to_build, min(jobs, len(to_build)))
            preflight_handled = {
                normalized(item["path"]) for item, obj, dep in to_build
                if not dep_is_stale(obj, dep)}
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

    if probe_scan is None:
        progress.set_phase("object_cache_scan", cache_candidates=len(entries))
    if probe_scan is not None:
        scan = probe_scan
        stale = list(scan["stale"])
        rows = scan["rows"]
        if identity_changed:
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
        scan = scan_entries(entries, handled=preflight_handled, quiet=False)
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
    elif identity_changed:
        build_cause = "identity"
    else:
        build_cause = "rebuild"
    fanout_top = [
        {"path": path, "dependents": count}
        for path, count in sorted(
            header_fanout.items(), key=lambda item: (-item[1], item[0]))[:20]]

    work = [entry for entry in stale
            if normalized(entry[0]["path"]) not in preflight_handled]
    progress.set_phase("vendor_compile",
                       dependency_scan_seconds=dependency_scan_seconds,
                       object_scan_s=dependency_scan_seconds,
                       dependency_check_ms=round(dependency_check_ms, 3),
                       scan_parse_ms=round(scan_parse_ms, 3),
                       scan_stat_ms=round(scan_stat_ms, 3),
                       scan_unique_dependencies=scan_unique_dependencies,
                       pending_compile_count=len(work),
                       changed_source_preflight_s=preflight_seconds)

    # Transitional diagnostic link input: the game DLL calls GLAD directly
    # today. Build the resident C dispatcher into the candidate so the whole-game
    # link can be measured; this is not a cutover-safe render boundary and is
    # replaced by CAP_RENDER before cutover.
    vendor_objects: list[Path] = []
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
        result = run_tool(
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
    progress.set_phase("compile", vendor_compile_seconds=vendor_compile_seconds,
                       pending=len(work))

    compile_start = time.monotonic()
    if work:
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
    response.write_text("\n".join(link_lines) + "\n", encoding="utf-8")

    link_command = [cxx, "-shared", "-o", str(staging), "@" + str(response)]
    if os.path.isfile(staging):
        os.remove(staging)
    link_start = time.monotonic()
    link_result = run_tool(link_command, cwd=str(ROOT), stdout=subprocess.PIPE,
                           stderr=subprocess.STDOUT, text=True)
    link_seconds = round(time.monotonic() - link_start, 3)
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
    try:
        os.replace(staging, dll_path)
        code_hash = sha256_file(dll_path)
    except OSError as error:
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
        "header_fanout_top": fanout_top,
        "identity_wrapper_count": (len(IDENTITY_SOURCE_PATHS)
                                   if identity_changed else 0),
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
    write_json_atomic(current_path, current, retries=40, delay=0.05)
    write_json_atomic(generation_dir / "generation.json", current,
                      retries=40, delay=0.05)
    write_cache_marker(
        object_dir, revision=revision, generation=generation,
        flags_hash=flags_hash, link_identity=link_identity,
        compiler_stamp=compiler_stamp, code_hash=code_hash,
        objects=objects,
        external_dependencies=collect_external_dependencies(entries))
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
