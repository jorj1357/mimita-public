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
from concurrent.futures import ThreadPoolExecutor, as_completed
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

ABI_VERSION = 1
GAME_OWNER = "game"

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


def read_json(path: Path) -> dict:
    with open(path, "r", encoding="utf-8") as handle:
        return json.load(handle)


def write_json_atomic(path: Path, payload: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(f"{path.name}.{os.getpid()}.{threading.get_ident()}.tmp")
    temporary.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    for _ in range(40):
        try:
            os.replace(temporary, path)
            return
        except PermissionError:
            time.sleep(0.05)
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
        existing = read_json(self.path) if self.path.is_file() else {}
        self.started_monotonic = float(context.get(
            "attempt_started_monotonic", time.monotonic()))
        self.phase_started_monotonic = time.monotonic()
        self.timing_path = Path(context.get("timing_path") or "")
        self.stop_heartbeat = threading.Event()
        self.data = {
            "status": "running",
            "phase": "toolchain_setup",
            "build_attempt_id": str(context.get("build_attempt_id") or ""),
            "source_generation": context.get("source_generation"),
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
        if not str(self.timing_path):
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
        write_json_atomic(self.path, self.data)

    def _heartbeat_loop(self) -> None:
        while not self.stop_heartbeat.wait(0.5):
            with self.lock:
                if self.data.get("status") != "running":
                    return
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

    def start_source(self, path: str, *, queue_wait_ms: float = 0.0) -> tuple[str, float]:
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
            }
            self._timing_locked({
                "event": "translation_unit.started", "path": path,
                "time_utc": started_utc,
                "elapsed_ms": round((started - self.started_monotonic) * 1000, 3),
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
                "started_utc": started_utc or active_started.get("started_utc", ""),
                "started_elapsed_ms": round(started_elapsed_ms or
                    float(active_started.get("elapsed_ms", 0.0)), 3),
                "wall_time": ended_utc,
                "elapsed_ms": round((time.monotonic() - self.started_monotonic) * 1000, 3),
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
        source = ROOT / rel
        if not source.is_file():
            raise FileNotFoundError(f"probe source not found: {rel}")
        result.append({"path": rel, "sha256": sha256_file(source)})
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
        if sha256_file(path) != item["sha256"]:
            errors.append(f"changed during build: {item['path']}")
    return errors


def object_key(item: dict) -> str:
    return hashlib.sha256(f"{item['path']}:{item['sha256']}".encode("utf-8")).hexdigest()[:20]


def flags_identity(cxx: str, base_flags: list[str], build_mode: str, pch: bool) -> str:
    text = "\n".join([cxx, f"mode={build_mode}", f"pch={int(pch)}", *base_flags])
    return hashlib.sha256(text.encode("utf-8")).hexdigest()[:16]


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
        if not dependency.is_absolute():
            dependency = ROOT / dependency
        try:
            if dependency.stat().st_mtime >= object_time:
                return True
        except OSError:
            return True
    return False


def build(args: argparse.Namespace) -> int:
    from build_toolchain import compiler, ccache, glfw_include, glfw_lib, runtime_path, run_tool

    attempt_context = {}
    if args.attempt_context:
        try:
            attempt_context = read_json(Path(args.attempt_context))
        except (OSError, json.JSONDecodeError):
            attempt_context = {}

    if args.sources_file:
        sources_path = Path(args.sources_file)
        if not sources_path.is_absolute():
            sources_path = ROOT / sources_path
        try:
            sources, build_args = sources_from_file(sources_path)
        except (OSError, ValueError) as error:
            print(f"[GAME BUILD] {error}", file=sys.stderr)
            return 3
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
        revision = manifest.get("revision")
        if args.revision and revision != args.revision:
            print(f"[GAME BUILD] revision mismatch: {revision} != {args.revision}",
                  file=sys.stderr)
            return 3
        sources = select_game_sources(manifest)

    if args.revision and args.sources_file and revision != args.revision:
        print(f"[GAME BUILD] revision mismatch: {revision} != {args.revision}", file=sys.stderr)
        return 3

    change_report = None
    if args.change_report:
        report_path = Path(args.change_report)
        if not report_path.is_absolute():
            report_path = ROOT / report_path
        change_report = read_json(report_path)
        if not change_report:
            print(f"[GAME BUILD] unreadable change report: {report_path}", file=sys.stderr)
            return 3
        reported_revision = change_report.get("new_revision", change_report.get("revision"))
        if reported_revision and revision and reported_revision != revision:
            print(f"[GAME BUILD] change report revision mismatch: "
                  f"{reported_revision} != {revision}", file=sys.stderr)
            return 3
    if not sources:
        print("[GAME BUILD] no game-owned sources in manifest", file=sys.stderr)
        return 3
    if args.list:
        print(f"[GAME BUILD] revision={manifest.get('revision')} game_sources={len(sources)}")
        for item in sources:
            print(f"  {item['path']}")
        return 0

    initial_errors = verify_coherent(ROOT, sources)
    if initial_errors:
        for error in initial_errors:
            print(f"[GAME BUILD] ERROR {error}", file=sys.stderr)
        return 2

    # The probe module is a separate validation export. The whole-game DLL uses
    # src/game/game-module.cpp as its GameModuleV1 entry point.
    if not args.sources_file:
        sources = [s for s in sources
                   if s.get("path") != "src/game/probe/npc-target-probe-module.cpp"]

    toolchain_started = time.monotonic()
    try:
        cxx = compiler()
        os.environ["PATH"] = runtime_path(cxx) + os.pathsep + os.environ.get("PATH", "")
        cache = ccache(cxx)
        glfw_inc = glfw_include()
        glfw_lib_dir = glfw_lib()
    except FileNotFoundError as error:
        print(f"[GAME BUILD] toolchain unavailable: {error}", file=sys.stderr)
        return 4
    toolchain_seconds = round(time.monotonic() - toolchain_started, 3)

    set_process_priority(args.priority)

    out_root = (ROOT / args.out).resolve() if not Path(args.out).is_absolute() else Path(args.out)
    generation = args.generation
    if generation <= 0:
        existing = [int(path.name.split("-")[-1]) for path in out_root.glob("generation-*")
                    if path.name.split("-")[-1].isdigit()]
        generation = (max(existing) + 1) if existing else 1

    generation_dir = out_root / f"generation-{generation}"
    generation_dir.mkdir(parents=True, exist_ok=True)
    dll_path = generation_dir / f"game-g{generation}.dll"
    staging = generation_dir / f"game-g{generation}.build.dll"

    identity_dir = out_root / "identity"
    identity_dir.mkdir(parents=True, exist_ok=True)
    identity_revision = str(manifest.get("revision") or ("0" * 64))
    (identity_dir / "module-identity.h").write_text(
        "#pragma once\n"
        f"#define MIMITA_MODULE_GENERATION {generation}u\n"
        f'#define MIMITA_MODULE_SOURCE_HASH "{identity_revision}"\n'
        "#define MIMITA_MODULE_STATE_SCHEMA 1\n",
        encoding="utf-8")

    base_flags = [
        "-std=c++17",
        "-O2" if args.build_mode == "release" else "-Og",
        "-g" if args.build_mode == "release" else "-g1",
        "-pipe",
        "-MMD",
        "-MP",
        "-fpch-preprocess",
        *[f"-I{ROOT / directory}" if not Path(directory).is_absolute() else f"-I{directory}"
          for directory in INCLUDE_DIRS],
        f"-I{glfw_inc}",
        *DEFINE_FLAGS,
    ]
    if args.pch:
        base_flags += ["-include", "pch.h"]
    if args.sources_file and not args.no_probe_bridge:
        base_flags.append("-DMIMITA_PROBE_BRIDGE")

    flags_hash = flags_identity(cxx, base_flags, args.build_mode, args.pch)
    object_dir = out_root / "objects" / flags_hash
    object_dir.mkdir(parents=True, exist_ok=True)
    seeded = 0 if args.no_seed else seed_object_cache(out_root, object_dir)

    jobs = args.jobs if args.jobs > 0 else (os.cpu_count() or 1)
    progress = Progress(out_root, manifest.get("revision", ""), len(sources), jobs,
                        args.priority, object_dir, seeded, attempt_context)
    progress.set_phase("identity_and_cache", generation=generation,
                       compiler=str(cxx), flags_identity=flags_hash,
                       object_cache=str(object_dir.relative_to(ROOT)),
                       seeded_objects=seeded, toolchain_seconds=toolchain_seconds)
    progress.log(f"[GAME BUILD] revision={manifest.get('revision')} sources={len(sources)} "
                 f"jobs={jobs} priority={args.priority} seed={seeded}")
    print(f"[GAME BUILD] revision={manifest.get('revision')} sources={len(sources)} "
          f"jobs={jobs} priority={args.priority} cache={object_dir.relative_to(ROOT)} "
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
    work: list[tuple[dict, Path, Path, float]] = []
    dependency_scan_started = time.monotonic()
    dependency_check_ms = 0.0
    cache_record_overhead_ms = 0.0
    progress.set_phase("object_cache_scan", cache_candidates=len(entries))
    for item, obj, dep in entries:
        check_started = time.monotonic()
        check_started_utc = utc_now()
        stale = args.force or dep_is_stale(obj, dep)
        check_elapsed_ms = (time.monotonic() - check_started) * 1000.0
        dependency_check_ms += check_elapsed_ms
        if not stale:
            record_started = time.perf_counter()
            progress.record("skipped", item["path"], 0.0,
                            check_started_utc=check_started_utc,
                            started_utc=check_started_utc,
                            check_elapsed_ms=check_elapsed_ms,
                            started_elapsed_ms=(check_started - progress.started_monotonic) * 1000.0)
            cache_record_overhead_ms += (time.perf_counter() - record_started) * 1000.0
            continue
        work.append((item, obj, dep, time.monotonic()))

    dependency_scan_seconds = round(time.monotonic() - dependency_scan_started, 3)
    progress.set_phase("vendor_compile", dependency_scan_seconds=dependency_scan_seconds,
                       dependency_check_ms=round(dependency_check_ms, 3),
                       cache_record_overhead_ms=round(cache_record_overhead_ms, 3),
                       pending_compile_count=len(work))

    # Transitional diagnostic link input: the game DLL calls GLAD directly
    # today. Build the resident C dispatcher into the candidate so the whole-game
    # link can be measured; this is not a cutover-safe render boundary and is
    # replaced by CAP_RENDER before cutover.
    vendor_objects: list[Path] = []
    vendor_dir = out_root / "vendor"
    vendor_dir.mkdir(parents=True, exist_ok=True)
    glad_source = ROOT / "src" / "glad.c"
    glad_object = vendor_dir / "glad.o"
    vendor_compile_seconds = 0.0
    if glad_source.is_file() and (
            not glad_object.is_file() or
            glad_source.stat().st_mtime > glad_object.stat().st_mtime):
        progress.log("[VENDOR] compiling src/glad.c for diagnostic link")
        vendor_started = time.monotonic()
        result = run_tool(
            ["gcc", "-O2", "-g", "-c", str(glad_source), "-o", str(glad_object),
             f"-I{ROOT / 'include'}", f"-I{ROOT / 'src'}"],
            cwd=str(ROOT), stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            text=True)
        if result.returncode != 0:
            progress.log(result.stdout or "glad compile failed")
            progress.finish("vendor_compile_failed", phase="vendor_compile_failed")
            return 1
        vendor_compile_seconds = round(time.monotonic() - vendor_started, 3)
    if glad_object.is_file():
        vendor_objects.append(glad_object)
    progress.set_phase("compile", vendor_compile_seconds=vendor_compile_seconds,
                       pending=len(work))

    def compile_one(item: dict, obj: Path, dep: Path, submitted: float
                    ) -> tuple[str, dict, float, str, float, str, float]:
        if stop_event.is_set():
            return ("skipped", item, 0.0, "", 0.0, "", 0.0)
        queue_wait_ms = (time.monotonic() - submitted) * 1000.0
        started_utc, started_elapsed_ms = progress.start_source(
            item["path"], queue_wait_ms=queue_wait_ms)
        source_path = str(item["path"]).replace("\\", "/")
        folded_module_symbols = {
            "src/mapeditor/map-editor-module.cpp":
                "GetMimitaMapEditorModuleAPI",
            "src/map/map-geometry-module.cpp":
                "GetMimitaMapGeometryModuleAPI",
            "src/render/actor-rendering-module.cpp":
                "GetMimitaActorRenderingModuleAPI",
            "src/render/world-rendering-module.cpp":
                "GetMimitaWorldRenderingModuleAPI",
            "src/worldgen/infinite-worldgen-module.cpp":
                "GetMimitaInfiniteWorldgenModuleAPI",
            "src/world/chunk-collision-module.cpp":
                "GetMimitaChunkCollisionModuleAPI",
            "src/network/network-policy-module.cpp":
                "GetMimitaNetworkPolicyModuleAPI",
            "src/navigation/navigation-policy-module.cpp":
                "GetMimitaNavigationPolicyModuleAPI",
            "src/npc/npc-perception-module.cpp":
                "GetMimitaNpcPerceptionModuleAPI",
            "src/npc/npc-behavior-module.cpp":
                "GetMimitaNpcBehaviorModuleAPI",
        }
        identity_only_sources = {
            "src/physics/movement/actor-contact-policy-module.cpp",
        }
        extra_flags = []
        if source_path in folded_module_symbols:
            extra_flags.extend([
                f"-I{identity_dir}",
                "-DMIMITA_MODULE_BUILD",
                f"-DGetMimitaModuleAPI={folded_module_symbols[source_path]}",
            ])
        elif source_path in identity_only_sources:
            extra_flags.extend([
                f"-I{identity_dir}",
                "-DMIMITA_MODULE_BUILD",
            ])
        command = [
            *ccache_prefix,
            cxx,
            *base_flags,
            *extra_flags,
            "-MF", str(dep),
            "-c", str(ROOT / item["path"]),
            "-o", str(obj),
        ]
        started = time.perf_counter()
        result = run_tool(command, cwd=str(ROOT), stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT, text=True)
        elapsed_ms = (time.perf_counter() - started) * 1000.0
        if result.returncode != 0:
            return ("failed", item, elapsed_ms, result.stdout or "",
                    queue_wait_ms, started_utc, started_elapsed_ms)
        return ("compiled", item, elapsed_ms, "", queue_wait_ms,
                started_utc, started_elapsed_ms)

    failures: list[dict] = []
    compile_start = time.monotonic()
    if work:
        with ThreadPoolExecutor(max_workers=jobs) as executor:
            futures = [executor.submit(compile_one, item, obj, dep, submitted)
                       for item, obj, dep, submitted in work]
            for future in as_completed(futures):
                (status, item, elapsed_ms, output, queue_wait_ms,
                 started_utc, started_elapsed_ms) = future.result()
                progress.record(status, item["path"], elapsed_ms, output,
                                queue_wait_ms=queue_wait_ms,
                                started_utc=started_utc,
                                started_elapsed_ms=started_elapsed_ms)
                if status == "failed":
                    failures.append({"path": item["path"], "output": output[:4000]})
                    if len(failures) >= args.max_errors:
                        stop_event.set()

    compile_seconds = round(time.monotonic() - compile_start, 3)

    if failures:
        progress.log(f"[GAME BUILD] compile failed count={len(failures)} in {compile_seconds}s")
        for failure in failures[:args.max_errors]:
            progress.log(f"[GAME BUILD] FAILED {failure['path']}")
            progress.log(failure["output"])
        progress.finish("compile_failed", compile_s=compile_seconds,
                        failed=len(failures), phase="compile_failed")
        print(f"[GAME BUILD] compile failed: {len(failures)} file(s); see {progress.log_path}",
              file=sys.stderr, flush=True)
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
        progress.log(f"[GAME BUILD] link failed in {link_seconds}s")
        progress.log(output)
        progress.finish("link_failed", compile_s=compile_seconds, link_s=link_seconds,
                        phase="link_failed")
        print(f"[GAME BUILD] link failed; see {progress.log_path}", file=sys.stderr, flush=True)
        return link_result.returncode or 1

    progress.set_phase("coherence_validation")
    validation_started = time.monotonic()
    final_errors = verify_coherent(ROOT, sources)
    if final_errors:
        for error in final_errors:
            print(f"[GAME BUILD] coherence rejected: {error}", file=sys.stderr)
        try:
            staging.unlink()
        except OSError:
            pass
        progress.finish("coherence_failed", compile_s=compile_seconds, link_s=link_seconds,
                        validation_seconds=round(time.monotonic() - validation_started, 3))
        return 2

    progress.set_phase("publish")
    publish_started = time.monotonic()
    os.replace(staging, dll_path)
    code_hash = sha256_file(dll_path)
    publish_seconds = round(time.monotonic() - publish_started, 3)
    total_seconds = round(time.monotonic() - progress.started_monotonic, 3)
    current = {
        "status": "ok",
        "abi_version": ABI_VERSION,
        "generation": generation,
        "revision": manifest.get("revision"),
        "code_hash": code_hash,
        "dll": str(dll_path.relative_to(ROOT)) if dll_path.is_relative_to(ROOT) else str(dll_path),
        "source_count": len(sources),
        "compiled": progress.data["compiled"],
        "skipped": progress.data["skipped"],
        "build_mode": args.build_mode,
        "jobs": jobs,
        "priority": args.priority,
        "compile_seconds": compile_seconds,
        "link_seconds": link_seconds,
        "publish_seconds": publish_seconds,
        "object_scan_seconds": dependency_scan_seconds,
        "dependency_check_ms": round(dependency_check_ms, 3),
        "cache_record_overhead_ms": round(cache_record_overhead_ms, 3),
        "total_seconds": total_seconds,
        "object_cache": str(object_dir.relative_to(ROOT)),
        "seeded_objects": seeded,
        "slowest_compiles": progress.data["slowest"],
        "generated_utc": utc_now(),
    }
    if change_report is not None:
        current["change_report"] = change_report
    write_json_atomic(out_root / "current.json", current)
    write_json_atomic(generation_dir / "generation.json", current)
    progress.finish("ok", compile_s=compile_seconds, link_s=link_seconds,
                    publish_s=publish_seconds, total_s=total_seconds,
                    object_scan_s=dependency_scan_seconds,
                    dependency_check_ms=round(dependency_check_ms, 3),
                    cache_record_overhead_ms=round(cache_record_overhead_ms, 3),
                    phase="done")
    print(f"[GAME BUILD] generation={generation} revision={manifest.get('revision')} "
          f"compiled={current['compiled']} skipped={current['skipped']} "
          f"compile={compile_seconds}s link={link_seconds}s total={total_seconds}s "
          f"dll={current['dll']}", flush=True)
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
    parser = argparse.ArgumentParser(description="Whole-game candidate DLL builder")
    parser.add_argument("--manifest", default="build/source-manifest.json")
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

    try:
        return build(args)
    except OSError as error:
        print(f"[GAME BUILD] {error}", file=sys.stderr)
        return 4


if __name__ == "__main__":
    raise SystemExit(main())
