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
                 priority: str, cache_dir: Path, seeded: int):
        self.out_root = out_root
        self.lock = threading.Lock()
        self.path = out_root / "progress.json"
        self.log_path = out_root / "progress.log"
        self.started_monotonic = time.monotonic()
        self.data = {
            "status": "running",
            "phase": "start",
            "revision": revision,
            "total": total,
            "completed": 0,
            "compiled": 0,
            "skipped": 0,
            "failed": 0,
            "current": "",
            "jobs": jobs,
            "priority": priority,
            "object_cache": str(cache_dir.relative_to(ROOT)) if cache_dir.is_relative_to(ROOT) else str(cache_dir),
            "seeded_objects": seeded,
            "started_utc": utc_now(),
            "updated_utc": utc_now(),
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

    def _write_locked(self) -> None:
        self.data["updated_utc"] = utc_now()
        write_json_atomic(self.path, self.data)

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

    def record(self, status: str, path: str, ms: float, detail: str = "") -> None:
        with self.lock:
            self.data["completed"] += 1
            if status == "compiled":
                self.data["compiled"] += 1
            elif status == "skipped":
                self.data["skipped"] += 1
            elif status == "failed":
                self.data["failed"] += 1
            self.data["current"] = path
            self.data["elapsed_s"] = round(time.monotonic() - self.started_monotonic, 1)
            if status in ("compiled", "failed"):
                entry = {"path": path, "seconds": round(ms / 1000.0, 3), "status": status}
                slowest = self.data["slowest"]
                slowest.append(entry)
                slowest.sort(key=lambda item: item["seconds"], reverse=True)
                del slowest[10:]
            if status == "failed" and detail:
                self.data["errors"].append({"path": path, "output": detail[:4000]})
            if status == "skipped":
                self._append_locked(f"[SKIP] {self.data['completed']}/{self.data['total']} {path}")
            else:
                suffix = " FAIL" if status == "failed" else ""
                self._append_locked(
                    f"[CXX ] {self.data['completed']}/{self.data['total']} "
                    f"{ms / 1000.0:.2f}s {path}{suffix}")
            self._write_locked()

    def finish(self, status: str, **kwargs) -> None:
        with self.lock:
            self.data["status"] = status
            self.data["phase"] = "done" if status == "ok" else "failed"
            self.data["total_s"] = round(time.monotonic() - self.started_monotonic, 1)
            self.data.update(kwargs)
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

    try:
        cxx = compiler()
        os.environ["PATH"] = runtime_path(cxx) + os.pathsep + os.environ.get("PATH", "")
        cache = ccache(cxx)
        glfw_inc = glfw_include()
        glfw_lib_dir = glfw_lib()
    except FileNotFoundError as error:
        print(f"[GAME BUILD] toolchain unavailable: {error}", file=sys.stderr)
        return 4

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

    flags_hash = flags_identity(cxx, base_flags, args.build_mode, args.pch)
    object_dir = out_root / "objects" / flags_hash
    object_dir.mkdir(parents=True, exist_ok=True)
    seeded = 0 if args.no_seed else seed_object_cache(out_root, object_dir)

    jobs = args.jobs if args.jobs > 0 else (os.cpu_count() or 1)
    progress = Progress(out_root, manifest.get("revision", ""), len(sources), jobs,
                        args.priority, object_dir, seeded)
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
    work: list[tuple[dict, Path, Path]] = []
    for item, obj, dep in entries:
        if not args.force and not dep_is_stale(obj, dep):
            progress.record("skipped", item["path"], 0.0)
            continue
        work.append((item, obj, dep))

    compile_start = time.monotonic()
    progress.set(phase="compile", pending=len(work))

    def compile_one(item: dict, obj: Path, dep: Path) -> tuple[str, dict, float, str]:
        if stop_event.is_set():
            return ("skipped", item, 0.0, "")
        command = [
            *ccache_prefix,
            cxx,
            *base_flags,
            "-MF", str(dep),
            "-c", str(ROOT / item["path"]),
            "-o", str(obj),
        ]
        started = time.perf_counter()
        result = run_tool(command, cwd=str(ROOT), stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT, text=True)
        elapsed_ms = (time.perf_counter() - started) * 1000.0
        if result.returncode != 0:
            return ("failed", item, elapsed_ms, result.stdout or "")
        return ("compiled", item, elapsed_ms, "")

    failures: list[dict] = []
    if work:
        with ThreadPoolExecutor(max_workers=jobs) as executor:
            futures = [executor.submit(compile_one, item, obj, dep) for item, obj, dep in work]
            for future in as_completed(futures):
                status, item, elapsed_ms, output = future.result()
                progress.record(status, item["path"], elapsed_ms, output)
                if status == "failed":
                    failures.append({"path": item["path"], "output": output[:4000]})
                    if len(failures) >= args.max_errors:
                        stop_event.set()

    compile_seconds = round(time.monotonic() - compile_start, 1)

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

    progress.set(phase="link", compile_s=compile_seconds)
    response = generation_dir / "link.rsp"
    link_lines = [path.as_posix() for path in objects]
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
    link_seconds = round(time.monotonic() - link_start, 1)
    if link_result.returncode != 0:
        output = link_result.stdout or ""
        progress.log(f"[GAME BUILD] link failed in {link_seconds}s")
        progress.log(output)
        progress.finish("link_failed", compile_s=compile_seconds, link_s=link_seconds,
                        phase="link_failed")
        print(f"[GAME BUILD] link failed; see {progress.log_path}", file=sys.stderr, flush=True)
        return link_result.returncode or 1

    final_errors = verify_coherent(ROOT, sources)
    if final_errors:
        for error in final_errors:
            print(f"[GAME BUILD] coherence rejected: {error}", file=sys.stderr)
        try:
            staging.unlink()
        except OSError:
            pass
        progress.finish("coherence_failed", compile_s=compile_seconds, link_s=link_seconds)
        return 2

    os.replace(staging, dll_path)
    code_hash = sha256_file(dll_path)
    total_seconds = round(compile_seconds + link_seconds, 1)
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
        "total_seconds": total_seconds,
        "object_cache": str(object_dir.relative_to(ROOT)),
        "seeded_objects": seeded,
        "slowest_compiles": progress.data["slowest"],
        "generated_utc": utc_now(),
    }
    write_json_atomic(out_root / "current.json", current)
    write_json_atomic(generation_dir / "generation.json", current)
    progress.finish("ok", compile_s=compile_seconds, link_s=link_seconds,
                    total_s=total_seconds, phase="done")
    print(f"[GAME BUILD] generation={generation} revision={manifest.get('revision')} "
          f"compiled={current['compiled']} skipped={current['skipped']} "
          f"compile={compile_seconds}s link={link_seconds}s total={total_seconds}s "
          f"dll={current['dll']}", flush=True)
    for entry in current["slowest_compiles"][:5]:
        print(f"[GAME BUILD] slow {entry['seconds']:.2f}s {entry['path']}", flush=True)
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
    parser.add_argument("--jobs", type=int, default=0, help="compile workers; 0 = all cores")
    parser.add_argument("--priority", choices=tuple(PRIORITY_CLASSES), default="high")
    parser.add_argument("--max-errors", type=int, default=20)
    parser.add_argument("--no-seed", action="store_true", help="do not reuse older generation objects")
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
