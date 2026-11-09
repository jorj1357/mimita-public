# 08 16 2026, 01 35
# purpose
# MiMITA incremental build implementation invoked by the dev-loop worker.
# Compiles only changed cpp files in parallel, uses a ccache wrapper and a
# precompiled header (pch.h), then links mimita.exe.
# Supports debug/release modes, clean, build-only, and auto-close flags.
# Does NOT coordinate launch/run state; user-facing requests go through dev-loop.
# Does NOT rebuild translation units whose objects are up to date.
# Does NOT modify source files.
# Uses a persistent link ledger so a killed/partial build relinks next time.

# usage:
#
# python build.py
# python build.py release
# python build.py build-only
# python build.py clean
# python build.py auto-close
#

import os
import sys
import subprocess
import shutil
import time
import atexit
import hashlib
import json
import datetime
import threading

from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path
from build_toolchain import compiler, ccache, glfw_include, glfw_lib, runtime_path, run_tool
from devscripts.build_authority import require_authority
from devscripts.build_scheduler import acquire as acquire_build_slot
from devscripts.path_containment import (
    apply_temp_environment,
    resolve_coordinator_root,
    resolve_within,
)
from devscripts.build_scheduler import scheduler_root as build_scheduler_root
from devscripts.build_exclusive import acquire_exclusive_build

# ============================================================
# CONFIG
# ============================================================

ROOT = os.path.dirname(os.path.abspath(__file__))

if os.name == "nt":
    try:
        import ctypes
        _kernel32 = ctypes.windll.kernel32
        _kernel32.SetPriorityClass(_kernel32.GetCurrentProcess(), 0x00000080)
    except (AttributeError, OSError):
        pass

from build_lock import BuildLockBusy, acquire_checkout_build_lock

try:
    _BUILD_LOCK = acquire_checkout_build_lock(ROOT, "build.py")
except BuildLockBusy as error:
    print(f"[BUILD LOCK] {error}", file=sys.stderr, flush=True)
    sys.exit(3)
atexit.register(_BUILD_LOCK.release)

def stage_runtime_dlls():
    """Place the shared runtime DLLs beside the executable for local runs."""
    runtime_files = {
        "libgcc_s_seh-1.dll": os.path.join(os.path.dirname(COMPILER), "libgcc_s_seh-1.dll"),
        "libstdc++-6.dll": os.path.join(os.path.dirname(COMPILER), "libstdc++-6.dll"),
        "libwinpthread-1.dll": os.path.join(os.path.dirname(COMPILER), "libwinpthread-1.dll"),
        "glfw3.dll": os.path.join(GLFW_LIB, "glfw3.dll"),
    }
    for name, source in runtime_files.items():
        if not os.path.isfile(source):
            print("[RUNTIME] missing dependency: %s" % source)
            continue
        destination = os.path.join(ROOT, name)
        try:
            shutil.copy2(source, destination)
            print("[RUNTIME] staged %s" % name)
        except PermissionError:
            # A running root-local mimita.exe can keep a DLL mapped. The
            # linker already succeeded, and the dev loop publishes its own
            # DLL copies beside each numbered executable, so do not turn this
            # convenience staging lock into a failed build.
            print("[RUNTIME] destination locked; keeping existing %s" % name)

try:
    COMPILER = compiler()
    CCACHE = ccache(COMPILER)
    GLFW_INCLUDE = glfw_include()
    GLFW_LIB = glfw_lib()
    # Debug builds use MinGW's shared C++ runtime DLLs. Make the project-local
    # toolchain visible to mimita.exe without installing DLLs system-wide.
    os.environ["PATH"] = runtime_path(COMPILER) + os.pathsep + os.environ.get("PATH", "")
except FileNotFoundError as error:
    print(f"[TOOLCHAIN] {error}")
    sys.exit(2)

def ccache_cmd():
    # Fall back to the plain compiler when ccache.exe is not installed (e.g.
    # CI runners), so the build works without ccache.
    if os.environ.get("MIMITA_CCACHE_DISABLE") == "1":
        return [COMPILER]
    if os.path.isfile(CCACHE):
        return [CCACHE, COMPILER]
    return [COMPILER]


SRC_DIR = os.path.join(ROOT, "src")
BUILD_DIR = os.path.join(ROOT, "build")
PROGRESS_PATH = os.path.join(BUILD_DIR, "progress.json")
BUILD_MONOTONIC_START = time.monotonic()
PHASE_TIMINGS = {}
_PROGRESS_LOCK = threading.RLock()
_LAST_CURRENT_PROGRESS = 0.0
# OBJ_DIR and PCH_OUTPUT are set per build mode after MODE is resolved below.

EXE_NAME = os.environ.get("MIMITA_EXE_NAME", "mimita.exe")
# Machine-wide policy serializes EXE builds. Each active build uses all
# logical processors.
_MAX_BUILD_JOBS = max(1, os.cpu_count() or 1)
BUILD_JOBS = _MAX_BUILD_JOBS

PCH_HEADER = os.path.join(SRC_DIR, "pch.h")
BUILD_METRICS_START = time.time()


def write_build_metrics(result, link_reason=""):
    try:
        path = os.path.join(BUILD_DIR, "build-metrics.jsonl")
        cache = ccache_stats()
        payload = {
            "timestamp_utc": datetime.datetime.now(
                datetime.timezone.utc).isoformat(
                    timespec="seconds").replace("+00:00", "Z"),
            "result": result,
            "elapsed_s": round(time.time() - BUILD_METRICS_START, 3),
            "compiled": compiled_count,
            "skipped": skipped_count,
            "cold_build": compiled_count >= 100 or pch_needs_build,
            "jobs": BUILD_JOBS,
            "mode": MODE,
            "lane": os.environ.get("MIMITA_LANE_ID", ""),
            "scheduler_root": str(build_scheduler_root()),
            "link_reason": link_reason,
            "phase_seconds": {
                key: round(value, 3) for key, value in PHASE_TIMINGS.items()
            },
            "ccache_hits": cache.get("direct_cache_hit", 0) -
                CCACHE_START_STATS.get("direct_cache_hit", 0),
            "ccache_misses": cache.get("direct_cache_miss", 0) -
                CCACHE_START_STATS.get("direct_cache_miss", 0),
            "ccache_errors": cache.get("modified_input_file", 0) -
                CCACHE_START_STATS.get("modified_input_file", 0),
            "ccache_total": cache,
        }
        with open(path, "a", encoding="utf-8", newline="\n") as handle:
            handle.write(json.dumps(payload, sort_keys=True) + "\n")
        with open(os.path.join(BUILD_DIR, "build-timing.json"), "w",
                  encoding="utf-8", newline="\n") as handle:
            json.dump(payload, handle, indent=2, sort_keys=True)
            handle.write("\n")
    except (OSError, ValueError):
        pass

# ============================================================
# BUILD MODE
# ============================================================

MODE = "debug"
RUN_AFTER_BUILD = True
AUTO_CLOSE_SECONDS = 0

if len(sys.argv) > 1:
    args = sys.argv[1:]
    for index, arg in enumerate(args):
        a = arg.lower()
        if a in ("build-only", "compile-only", "norun", "no-run"):
            if MODE != "release":
                MODE = "debug"
            RUN_AFTER_BUILD = False
        elif a == "auto-close":
            AUTO_CLOSE_SECONDS = 5
        elif a == "timeout":
            AUTO_CLOSE_SECONDS = 5
            if index + 1 < len(args):
                try:
                    AUTO_CLOSE_SECONDS = max(1, int(args[index + 1]))
                except ValueError:
                    pass
        elif a == "release":
            MODE = "release"
        elif a == "clean":
            MODE = "clean"

if MODE == "release":
    CXX_FLAGS = [
        "-std=c++17",
        "-O2",
        "-march=x86-64-v2",
        "-s",
        # NDEBUG removes assert()/debug-only code and strings from the shipped
        # binary. -fmacro-prefix-map rewrites __FILE__ paths so no local dev
        # path (e.g. C:\mimita-priv-v8\...) leaks into the release exe.
        "-DNDEBUG",
        "-fmacro-prefix-map=" + ROOT.replace("\\", "/") + "=.",
        "-pipe",
        "-MMD",
        "-MP",
        "-fpch-preprocess",
    ]
    LINK_FLAGS = []
else:
    MODE = "debug"
    LINK_FLAGS = []

    # MUCH faster compile times. -g1 keeps line numbers + function names so
    # GDB backtraces/crash triage still work, but drops struct/var debug info.
    # Objects are ~30x smaller than -g2, which makes the 500MB+ exe link in
    # about a second instead of several. Switch to -g2 when you need to inspect
    # variables in GDB (then delete build/obj-debug to force a rebuild).
    CXX_FLAGS = [
        "-std=c++17",
        "-Og",
        "-g1",
        "-pipe",
        "-MMD",
        "-MP",
        "-fpch-preprocess",
    ]

# Debug and release use separate object dirs so switching modes never
# links stale objects compiled with the other mode's flags.
OBJ_DIR = os.path.join(BUILD_DIR, "obj-" + MODE)
# GCC loads the precompiled header for "-include pch.h" from pch.h.gch
# (next to pch.h), and accepts it across -Og/-O2, so a single shared PCH is used.
PCH_OUTPUT = os.path.join(SRC_DIR, "pch.h.gch")

if os.environ.get("MIMITA_LINKER", "").strip().lower() == "lld":
    LINK_FLAGS.append("-fuse-ld=lld")

# ============================================================
# FLAGS
# ============================================================

LIBJUICE_DIR = os.path.join(ROOT, "external", "libjuice")

# Vendored Manifold (true boolean CSG). The static library and headers are
# produced by tools/build_manifold.py into external/manifold-prebuilt/ (ignored
# by Git). MANIFOLD_PAR=-1 must match the library build (parallel backend off).
MANIFOLD_PREBUILT = os.path.join(ROOT, "external", "manifold-prebuilt")
MANIFOLD_INCLUDE = os.path.join(MANIFOLD_PREBUILT, "include")
MANIFOLD_LIB = os.path.join(MANIFOLD_PREBUILT, "lib", "libmanifold.a")

if not os.path.isfile(MANIFOLD_LIB):
    print("[TOOLCHAIN] Vendored Manifold was not found at " + MANIFOLD_LIB)
    print("[TOOLCHAIN] Run: python tools/build_manifold.py")
    sys.exit(2)

INCLUDE_FLAGS = [
    "-Iinclude",
    "-Isrc",
    f"-I{GLFW_INCLUDE}",
    "-Iexternal/libjuice/include",
    "-Iexternal/manifold-prebuilt/include",
    "-Iexternal/recastnavigation/Recast/Include",
    "-Iexternal/recastnavigation/Detour/Include",
    "-Iexternal/recastnavigation/DetourTileCache/Include",
]

DEFINE_FLAGS = [
    "-DGLM_ENABLE_EXPERIMENTAL",
    "-DJUICE_STATIC",
    "-DMANIFOLD_PAR=-1",
]

LIB_FLAGS = [
    f"-L{GLFW_LIB}",
    MANIFOLD_LIB,
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

# ============================================================
# HELPERS
# ============================================================

def ensure_dirs():
    os.makedirs(BUILD_DIR, exist_ok=True)
    os.makedirs(OBJ_DIR, exist_ok=True)

    # Remove zero-byte object files left by aborted parallel builds.
    # When one compilation fails, sys.exit(1) kills other compiler
    # subprocesses mid-write, leaving empty .o files whose timestamps
    # are newer than their source files, causing incremental build
    # to skip them forever. See build-system-investigation task.
    if os.path.isdir(OBJ_DIR):
        for f in os.listdir(OBJ_DIR):
            if f.endswith(".o"):
                path = os.path.join(OBJ_DIR, f)
                if os.path.getsize(path) == 0:
                    try:
                        os.remove(path)
                    except PermissionError:
                        pass  # file locked by another process, skip




LINK_STATE = os.path.join(BUILD_DIR, "link-state-%s.json" % MODE)
LINK_PENDING = os.path.join(BUILD_DIR, "link.pending.json")


def file_signature(path):
    try:
        stat = os.stat(path)
    except OSError:
        return None
    return [stat.st_size, stat.st_mtime_ns]


def object_signature(object_files):
    return {
        os.path.relpath(path, BUILD_DIR): file_signature(path)
        for path in object_files
    }


def flags_fingerprint():
    payload = json.dumps({
        "compiler": COMPILER,
        "cxx": CXX_FLAGS,
        "include": INCLUDE_FLAGS,
        "define": DEFINE_FLAGS,
        "lib": LIB_FLAGS,
        "link_libs": LINK_LIBS,
        "link_flags": LINK_FLAGS,
    }, sort_keys=True)
    return hashlib.sha256(payload.encode("utf-8")).hexdigest()


def read_json(path):
    try:
        with open(path, "r", encoding="utf-8") as handle:
            data = json.load(handle)
        return data if isinstance(data, dict) else {}
    except (OSError, ValueError):
        return {}


def write_json_atomic(path, payload):
    temporary = path + ".tmp"
    with open(temporary, "w", encoding="utf-8", newline="\n") as handle:
        json.dump(payload, handle, indent=2, sort_keys=True)
        handle.write("\n")
    os.replace(temporary, path)


def write_progress(phase, **fields):
    payload = {
        "schema": 1,
        "phase": phase,
        "pid": os.getpid(),
        "updated_utc": datetime.datetime.now(
            datetime.timezone.utc).isoformat(
                timespec="milliseconds").replace("+00:00", "Z"),
        "elapsed_s": round(time.monotonic() - BUILD_MONOTONIC_START, 3),
        "phase_seconds": {
            key: round(value, 3) for key, value in PHASE_TIMINGS.items()
        },
    }
    payload.update(fields)
    with _PROGRESS_LOCK:
        try:
            write_json_atomic(PROGRESS_PATH, payload)
        except OSError:
            pass


def progress_current(source_path):
    global _LAST_CURRENT_PROGRESS
    now = time.monotonic()
    with _PROGRESS_LOCK:
        if now - _LAST_CURRENT_PROGRESS < 0.5:
            return
        _LAST_CURRENT_PROGRESS = now
    write_progress("compiling", current=source_path)


def ccache_stats():
    if not os.path.isfile(CCACHE):
        return {}
    try:
        result = subprocess.run(
            [CCACHE, "--print-stats", "--format=json"],
            env=os.environ.copy(), capture_output=True, text=True, timeout=15)
        if result.returncode != 0 or not result.stdout.strip():
            return {}
        data = json.loads(result.stdout)
        keys = (
            "direct_cache_hit", "direct_cache_miss", "cache_miss",
            "local_storage_hit", "local_storage_miss", "cache_size_kibibyte",
            "compile_failed", "modified_input_file", "no_input_file",
            "called_for_link", "stats_updated_timestamp",
        )
        return {key: data.get(key, 0) for key in keys}
    except (OSError, ValueError, subprocess.SubprocessError, json.JSONDecodeError):
        return {}


def sha256_file(path):
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def decide_link(object_files, exe_path):
    if os.environ.get("MIMITA_FORCE_LINK") == "1":
        return True, "forced"
    if os.path.isfile(LINK_PENDING):
        return True, "pending_or_failed_previous_link"
    if not os.path.isfile(exe_path) or os.path.getsize(exe_path) == 0:
        return True, "exe_missing_or_zero"
    missing = [path for path in object_files
               if not os.path.isfile(path) or os.path.getsize(path) == 0]
    if missing:
        return True, "missing_or_zero_objects"
    state = read_json(LINK_STATE)
    if state.get("status") != "linked":
        return True, "no_successful_link_state"
    if state.get("objects") != object_signature(object_files):
        return True, "object_set_changed"
    if state.get("flags") != flags_fingerprint():
        return True, "flags_changed"
    if state.get("exe") != file_signature(exe_path):
        return True, "exe_changed_since_state"
    newest_object = max(os.stat(path).st_mtime_ns for path in object_files)
    if newest_object > os.stat(exe_path).st_mtime_ns:
        return True, "objects_newer_than_exe"
    return False, "current"


def dep_path(src):
    rel = os.path.relpath(src, SRC_DIR)
    rel = os.path.splitext(rel)[0]

    rel = rel.replace("\\", "_").replace("/", "_")

    return os.path.join(OBJ_DIR, rel + ".d")

def obj_path(src):
    rel = os.path.relpath(src, SRC_DIR)
    rel = os.path.splitext(rel)[0]

    rel = rel.replace("\\", "_").replace("/", "_")

    return os.path.join(OBJ_DIR, rel + ".o")

def source_changed(src):
    obj = obj_path(src)
    dep = dep_path(src)

    # no object/dependency yet
    if not os.path.exists(obj) or not os.path.exists(dep):
        return True

    # Zero-byte object/dependency files are stale artifacts from aborted or
    # interrupted builds. A zero-byte .d file lists no headers, so trusting it
    # silently keeps an object compiled against an old struct layout forever.
    # Treat both as changed and retry this translation unit.
    if os.path.getsize(dep) == 0:
        try:
            os.remove(dep)
        except PermissionError:
            pass
        return True

    # zero-byte object file — stale artifact from aborted parallel build
    if os.path.getsize(obj) == 0:
        try:
            os.remove(obj)
        except PermissionError:
            # Another compiler thread/process can briefly hold stale zero-byte
            # objects after an aborted parallel build. Treat it as changed so
            # this translation unit is retried instead of crashing the build.
            pass
        return True

    obj_time = os.path.getmtime(obj)

    # Every C++ translation unit is compiled with ``-include pch.h``. GCC's
    # generated .d files do not reliably list the precompiled-header artifact,
    # so a changed PCH could otherwise leave old objects looking current.
    # Treat the PCH as a shared dependency explicitly.
    if os.path.exists(PCH_OUTPUT) and os.path.getmtime(PCH_OUTPUT) >= obj_time:
        return True

    # read dependency file
    with open(dep, "r") as f:
        content = f.read()

    # parse paths from .d file
    parts = content.replace("\\\n", " ").split()

    dependencies = []

    for p in parts[1:]:
        if p.endswith(":"):
            continue

        dependencies.append(p)

    # if any dependency newer than object -> rebuild
    for depfile in dependencies:
        if os.path.exists(depfile):
            if os.path.getmtime(depfile) >= obj_time:
                return True

    return False

def find_cpp_files():
    # The module catalog is the single source of truth: sources owned by a
    # reloadable module are excluded from the EXE, so build.py and the dev-loop
    # watcher cannot drift from the actual module inventory.
    from devscripts.module_ownership import exe_source_relpaths

    return [os.path.join(ROOT, relative)
            for relative in exe_source_relpaths(ROOT)]


RECAST_SOURCE_DIRS = [
    os.path.join(ROOT, "external", "recastnavigation", "Recast", "Source"),
    os.path.join(ROOT, "external", "recastnavigation", "Detour", "Source"),
    os.path.join(ROOT, "external", "recastnavigation", "DetourTileCache", "Source"),
]


def find_recast_cpp_files():
    out = []
    for source_dir in RECAST_SOURCE_DIRS:
        for name in sorted(os.listdir(source_dir)):
            if name.endswith(".cpp"):
                out.append(os.path.join(source_dir, name))
    return out


def recast_obj_path(src):
    rel = os.path.relpath(src, ROOT).replace("\\", "_").replace("/", "_")
    return os.path.join(OBJ_DIR, rel + ".o")


def recast_source_changed(src):
    obj = recast_obj_path(src)
    return not os.path.exists(obj) or os.path.getmtime(src) >= os.path.getmtime(obj)

# ============================================================
# CLEAN
# ============================================================

if MODE == "clean":
    require_authority(Path(ROOT), "exe", "build.py clean")
    if os.path.exists(BUILD_DIR):
        print("Deleting build folder...")
        shutil.rmtree(BUILD_DIR)

    exe = os.path.join(ROOT, EXE_NAME)

    if os.path.exists(exe):
        print("Deleting exe...")
        os.remove(exe)

    print("Clean complete.")
    sys.exit(0)


# ============================================================
# MAIN
# ============================================================

ensure_dirs()

# Builds are owned by the dev-loop. Agents must never reach this path directly.
require_authority(Path(ROOT), "exe", "build.py")
_EXCLUSIVE_BUILD = acquire_exclusive_build(Path(ROOT), "build.py")
build_slot_wait_started = time.monotonic()
build_slot = acquire_build_slot(
    "exe", f"build.py:{os.getpid()}",
    metadata={"lane": os.environ.get("MIMITA_LANE_ID", ""), "mode": MODE},
    timeout=1800)
if build_slot is None:
    print("[BUILD SCHEDULER] no EXE slot became available", file=sys.stderr)
    sys.exit(3)
atexit.register(build_slot.release)
PHASE_TIMINGS["scheduler_wait_s"] = time.monotonic() - build_slot_wait_started
write_progress("setup", scheduler_wait_s=round(
    PHASE_TIMINGS["scheduler_wait_s"], 3))

coordinator_root = resolve_coordinator_root(Path(ROOT))
ccache_root = resolve_within(
    coordinator_root, coordinator_root / ".dev" / "ccache", label="CCACHE_DIR")
os.environ["CCACHE_DIR"] = str(ccache_root)
os.environ["MIMITA_COORDINATOR_ROOT"] = str(coordinator_root)
apply_temp_environment(
    coordinator_root, os.environ.get("MIMITA_LANE_ID", "build"), os.environ)
os.environ.setdefault("CCACHE_BASEDIR", str(Path(ROOT).resolve()))
os.environ.setdefault("CCACHE_NOHASHDIR", "1")
os.environ.setdefault("CCACHE_SLOPPINESS", "time_macros,file_macro")
os.environ.setdefault("CCACHE_MAXSIZE", "8G")
CCACHE_START_STATS = ccache_stats()

start_time = time.time()

# Presentation DLL is an optional fast path. It must never block or fail the
# authoritative EXE build, and it only runs when one of its own inputs changed.
game_dll = os.path.join(ROOT, "build", "mimita-game.dll")
game_dll_inputs = [
    os.path.join(ROOT, "src", "effects", "effect-part.cpp"),
    os.path.join(ROOT, "src", "hot-reload", "quick-hit-debug-visual.cpp"),
    os.path.join(ROOT, "src", "hot-reload", "game-api.h"),
]
game_dll_needed = not os.path.isfile(game_dll)
if not game_dll_needed:
    dll_mtime = os.path.getmtime(game_dll)
    game_dll_needed = any(os.path.isfile(path) and os.path.getmtime(path) > dll_mtime
                          for path in game_dll_inputs)
if game_dll_needed:
    dll_environment = os.environ.copy()
    dll_environment["MIMITA_BUILD_LOCK_PARENT_PID"] = str(os.getpid())
    dll_creationflags = (getattr(subprocess, "CREATE_NO_WINDOW", 0)
                         if os.name == "nt" else 0)
    game_dll_build = subprocess.run(
        [sys.executable, os.path.join(ROOT, "build_game_dll.py")],
        env=dll_environment,
        creationflags=dll_creationflags,
    )
    if game_dll_build.returncode != 0:
        print("[HOT RELOAD] presentation DLL failed; continuing with the EXE build")

# Module DLLs are built on demand by the running game's ModuleHost. The EXE
# build must not compile every module: that serializes the whole machine and
# makes a one-file host edit wait on unrelated module work.

print("==================================================")
print(" MiMITA Build System")
print("==================================================")
print("Mode:", MODE)
print()

cpp_files = find_cpp_files()

# ============================================================
# BUILD PCH
# ============================================================

pch_needs_build = (
    not os.path.exists(PCH_OUTPUT)
    or os.path.getmtime(PCH_HEADER) >= os.path.getmtime(PCH_OUTPUT)
)
pch_started = time.monotonic()

if pch_needs_build:

    write_progress("pch", current=os.path.relpath(PCH_HEADER, ROOT))
    print("[PCH ]", os.path.relpath(PCH_HEADER, ROOT), flush=True)

    cmd = [
        COMPILER,

        "-x",
        "c++-header",

        os.path.relpath(PCH_HEADER, ROOT),

        "-o",
        os.path.relpath(PCH_OUTPUT, ROOT),

        "-fno-working-directory",
        "-ffile-prefix-map=" + ROOT.replace("\\", "/") + "=.",
    ]

    cmd += CXX_FLAGS
    cmd += INCLUDE_FLAGS
    cmd += DEFINE_FLAGS

    result = run_tool(cmd)

    if result.returncode != 0:
        print()
        print("==================================================")
        print(" PCH BUILD FAILED")
        print("==================================================")
        write_progress("failed", phase_name="pch", result="failed")
        sys.exit(1)

PHASE_TIMINGS["pch_s"] = (time.monotonic() - pch_started
                          if pch_needs_build else 0.0)
write_progress("pch", pch_s=round(PHASE_TIMINGS["pch_s"], 3))

# glad.c manually
glad_path = os.path.join(ROOT, "src", "glad.c")

object_files = []

compiled_count = 0
skipped_count = 0

# ============================================================
# COMPILE SINGLE FILE
# ============================================================

def compile_cpp_file(src):

    obj = obj_path(src)

    if not source_changed(src):
        return ("skip", src)

    src_arg = os.path.relpath(src, ROOT)
    obj_arg = os.path.relpath(obj, ROOT)
    print("[CXX ]", src_arg, flush=True)
    progress_current(src_arg)

    cmd = ccache_cmd() + [

        "-c",
        src_arg,

        "-o",
        obj_arg,
    ]

    cmd += CXX_FLAGS
    cmd += INCLUDE_FLAGS
    cmd += DEFINE_FLAGS

    cmd += [
        "-include",
        "pch.h",
    ]

    result = run_tool(cmd, stdout=subprocess.PIPE,
                      stderr=subprocess.STDOUT, text=True)

    if result.returncode != 0:
        if result.stdout:
            print(result.stdout, flush=True)
        # A failed compiler can leave a partial object and dependency file.
        # Remove both so the next incremental build cannot mistake the partial
        # artifact for a valid translation unit and skip the real source.
        for stale_path in (obj, dep_path(src)):
            try:
                if os.path.exists(stale_path):
                    os.remove(stale_path)
            except PermissionError:
                pass
        return ("fail", src)

    return ("compiled", src)


def compile_recast_file(src):
    obj = recast_obj_path(src)
    os.makedirs(os.path.dirname(obj), exist_ok=True)
    if not recast_source_changed(src):
        return ("skip", src)

    src_arg = os.path.relpath(src, ROOT)
    obj_arg = os.path.relpath(obj, ROOT)
    print("[CXX ]", src_arg, flush=True)
    progress_current(src_arg)
    cmd = ccache_cmd() + ["-c", src_arg, "-o", obj_arg]
    cmd += CXX_FLAGS
    cmd += [
        "-DRC_DISABLE_ASSERTS",
        "-Iexternal/recastnavigation/Recast/Include",
        "-Iexternal/recastnavigation/Detour/Include",
        "-Iexternal/recastnavigation/DetourTileCache/Include",
    ]
    result = run_tool(cmd)
    if result.returncode != 0:
        try:
            if os.path.exists(obj):
                os.remove(obj)
        except PermissionError:
            pass
        return ("fail", src)
    return ("compiled", src)

# ============================================================
# PARALLEL COMPILE CPP FILES
# ============================================================

compile_started = time.monotonic()
write_progress("compiling", current="", compiled=0, skipped=0,
               total=len(cpp_files))

with ThreadPoolExecutor(max_workers=BUILD_JOBS) as executor:

    futures = []

    for src in cpp_files:

        obj = obj_path(src)
        object_files.append(obj)

        futures.append(
            executor.submit(compile_cpp_file, src)
        )

    for future in as_completed(futures):

        status, src = future.result()

        if status == "skip":
            print("[SKIP]", os.path.relpath(src, ROOT), flush=True)
            skipped_count += 1

        elif status == "compiled":
            compiled_count += 1

        elif status == "fail":

            print()
            print("==================================================")
            print(" BUILD FAILED")
            print("==================================================")
            write_progress("failed", phase_name="compiling", result="failed",
                           current=os.path.relpath(src, ROOT))
            sys.exit(1)

        write_progress("compiling", current=os.path.relpath(src, ROOT),
                       compiled=compiled_count, skipped=skipped_count,
                       total=len(cpp_files))

# ============================================================
# COMPILE VENDORED RECAST / DETOUR SOURCES
# ============================================================

recast_cpp_files = find_recast_cpp_files()
with ThreadPoolExecutor(max_workers=BUILD_JOBS) as executor:
    futures = []
    for src in recast_cpp_files:
        object_files.append(recast_obj_path(src))
        futures.append(executor.submit(compile_recast_file, src))

    for future in as_completed(futures):
        status, src = future.result()
        if status == "skip":
            print("[SKIP]", os.path.relpath(src, ROOT))
            skipped_count += 1
        elif status == "compiled":
            compiled_count += 1
        else:
            print()
            print("==================================================")
            print(" RECAST/DETOUR BUILD FAILED")
            print("==================================================")
            sys.exit(1)

# ============================================================
# COMPILE GLAD.C
# ============================================================

glad_obj = os.path.join(OBJ_DIR, "glad.o")
object_files.append(glad_obj)

if source_changed(glad_path):

    print("[CC  ] src/glad.c", flush=True)
    progress_current("src/glad.c")

    cmd = ccache_cmd() + [
        "-c",
        os.path.relpath(glad_path, ROOT),
        "-o",
        os.path.relpath(glad_obj, ROOT),
    ]

    cmd += CXX_FLAGS
    cmd += INCLUDE_FLAGS
    cmd += DEFINE_FLAGS

    result = run_tool(cmd)

    if result.returncode != 0:
        print()
        print("==================================================")
        print(" BUILD FAILED")
        print("==================================================")
        sys.exit(1)

    compiled_count += 1

else:
    print("[SKIP] src/glad.c")
    skipped_count += 1

# ============================================================
# COMPILE LIBJUICE C SOURCES
# ============================================================

juice_src_dir = os.path.join(LIBJUICE_DIR, "src")
juice_sources = [
    "addr.c", "agent.c", "base64.c", "conn.c", "conn_mux.c",
    "conn_poll.c", "conn_thread.c", "const_time.c", "crc32.c",
    "hash.c", "hmac.c", "ice.c", "juice.c", "log.c", "random.c",
    "server.c", "stun.c", "tcp.c", "timestamp.c", "turn.c", "udp.c",
]

# Private includes: libjuice sources reference headers in src/, include/, and include/juice/
juice_include_flags = [
    "-Iexternal/libjuice/src",
    "-Iexternal/libjuice/include",
    "-Iexternal/libjuice/include/juice",
    "-DJUICE_STATIC",
]

def compile_juice_file(src_name):
    src_path = os.path.join(juice_src_dir, src_name)
    obj_path_full = os.path.join(OBJ_DIR, "juice_" + src_name.replace(".c", ".o"))

    # Simple mtime check since .d files are not generated for libjuice C sources
    if os.path.exists(obj_path_full):
        obj_mtime = os.path.getmtime(obj_path_full)
        src_mtime = os.path.getmtime(src_path)
        if src_mtime < obj_mtime:
            return ("skip", src_name)

    src_arg = os.path.relpath(src_path, ROOT)
    obj_arg = os.path.relpath(obj_path_full, ROOT)
    print("[CC  ] " + src_arg, flush=True)
    progress_current(src_arg)

    cmd = ccache_cmd() + [
        "-x", "c",
        "-c",
        src_arg,
        "-o",
        obj_arg,
    ]

    # Use C flags (no -std=c++17) for libjuice C sources
    if MODE == "release":
        cmd += [
            "-std=c11",
            "-O2",
            "-DNDEBUG",
            "-fmacro-prefix-map=" + ROOT.replace("\\", "/") + "=.",
            "-pipe",
        ]
    else:
        cmd += [
            "-std=c11",
            "-O0",
            "-g",
            "-pipe",
        ]
    cmd += DEFINE_FLAGS
    for f in juice_include_flags:
        cmd.append(f)

    result = run_tool(cmd)

    if result.returncode != 0:
        return ("fail", src_name)

    return ("compiled", src_name)

for src_name in juice_sources:
    object_files.append(os.path.join(OBJ_DIR, "juice_" + src_name.replace(".c", ".o")))

with ThreadPoolExecutor(max_workers=BUILD_JOBS) as juice_executor:

    juice_futures = [
        juice_executor.submit(compile_juice_file, src_name)
        for src_name in juice_sources
    ]

    for future in as_completed(juice_futures):

        status, src_name = future.result()

        if status == "skip":
            skipped_count += 1

        elif status == "compiled":
            compiled_count += 1

        elif status == "fail":

            print()
            print("==================================================")
            print(" BUILD FAILED (libjuice)")
            print("==================================================")
            sys.exit(1)

PHASE_TIMINGS["compile_s"] = time.monotonic() - compile_started
write_progress("compiling", current="", compiled=compiled_count,
               skipped=skipped_count, total=len(cpp_files),
               compile_s=round(PHASE_TIMINGS["compile_s"], 3))

# ============================================================
# RESOURCE (icon) — mimita.rc → build/mimita.res.o
# ============================================================

WINDRES = os.environ.get("MIMITA_WINDRES") or shutil.which("windres")
if not WINDRES:
    candidate = os.path.join(os.path.dirname(COMPILER), "windres.exe")
    if os.path.isfile(candidate):
        WINDRES = candidate
RC_FILE = os.path.join(ROOT, "mimita.rc")
RES_OBJ = os.path.join(BUILD_DIR, "mimita.res.o")
res_compiled = False

if os.path.isfile(RC_FILE):
    os.makedirs(BUILD_DIR, exist_ok=True)
    rc_mtime = os.path.getmtime(RC_FILE)
    icon_path = None
    try:
        with open(RC_FILE, "r", encoding="utf-8", errors="replace") as f:
            for line in f:
                if "ICON" in line and '"' in line:
                    icon_path = line.split('"')[1]
                    break
    except OSError:
        pass
    need_res = not os.path.exists(RES_OBJ)
    if not need_res and os.path.getmtime(RES_OBJ) < rc_mtime:
        need_res = True
    if not need_res and icon_path:
        full_icon = os.path.join(ROOT, icon_path)
        if os.path.isfile(full_icon) and os.path.getmtime(RES_OBJ) < os.path.getmtime(full_icon):
            need_res = True
    if need_res:
        if not WINDRES:
            print("[TOOLCHAIN] windres.exe was not found. Add MinGW to PATH or set MIMITA_WINDRES.")
            sys.exit(2)
        print("[RC  ]", os.path.relpath(RC_FILE, ROOT), flush=True)
        r = run_tool([WINDRES, RC_FILE, "-O", "coff", "-o", RES_OBJ])
        if r.returncode != 0:
            print()
            print("==================================================")
            print(" RESOURCE BUILD FAILED")
            print("==================================================")
            write_progress("failed", phase_name="resource", result="failed")
            sys.exit(1)
        res_compiled = True
    object_files.append(RES_OBJ)

# ============================================================
# DETERMINE IF LINK NEEDED
# ============================================================

exe_path = os.path.join(ROOT, EXE_NAME)

needs_link, link_reason = decide_link(object_files, exe_path)
if compiled_count > 0 and link_reason == "current":
    needs_link, link_reason = True, "compiled_objects"

if not needs_link:
    staging_started = time.monotonic()
    stage_runtime_dlls()
    PHASE_TIMINGS["staging_s"] = time.monotonic() - staging_started
    print()
    print("Nothing changed. (%s)" % link_reason, flush=True)
    print()

    elapsed = time.time() - start_time

    print("Time    : %.2f sec" % elapsed, flush=True)
    write_build_metrics("noop", link_reason)
    write_progress("build_complete", result="noop", link_reason=link_reason,
                   compiled=compiled_count, skipped=skipped_count,
                   elapsed_s=round(elapsed, 3))

    if RUN_AFTER_BUILD:
        print()
        print("Running MiMITA...")
        print()

        exe_path = os.path.join(ROOT, EXE_NAME)
        if os.path.exists(exe_path):
            subprocess.run([exe_path])
        else:
            print("[ERROR] mimita.exe missing")

    sys.exit(0)

# ============================================================
# LINK
# ============================================================

link_started = time.monotonic()
print()
print("[LINK]", EXE_NAME, "reason=%s" % link_reason, flush=True)
write_progress("linking", link_reason=link_reason, current=EXE_NAME)

# Windows limits CreateProcess command lines to roughly 32K characters.
# The full object list exceeds that limit, so pass linker arguments through a
# response file kept under build/ (which is already ignored by Git).
link_response = os.path.join(BUILD_DIR, "link.rsp")
link_out_dir = os.path.join(BUILD_DIR, "out")
os.makedirs(link_out_dir, exist_ok=True)
link_candidate = os.path.join(
    link_out_dir, "mimita-candidate-%d.exe" % os.getpid())
link_args = object_files + LIB_FLAGS + LINK_LIBS + LINK_FLAGS + ["-o", link_candidate]
with open(link_response, "w", encoding="utf-8", newline="\n") as response:
    for argument in link_args:
        response.write('"' + argument.replace('"', '\\"') + '"\n')

write_json_atomic(LINK_PENDING, {
    "status": "pending",
    "reason": link_reason,
    "pid": os.getpid(),
    "started_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
    "objects": object_signature(object_files),
    "flags": flags_fingerprint(),
})

print("[LINK] using build/link.rsp", flush=True)
cmd = ccache_cmd() + ["@" + link_response]

result = run_tool(cmd)

if result.returncode != 0:
    write_json_atomic(LINK_PENDING, {
        "status": "failed",
        "reason": link_reason,
        "pid": os.getpid(),
        "exit_code": result.returncode,
        "failed_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
    })
    print()
    print("==================================================")
    print(" LINK FAILED")
    print("==================================================")
    write_progress("failed", phase_name="linking", result="failed",
                   link_reason=link_reason)
    sys.exit(1)

if not os.path.isfile(link_candidate) or os.path.getsize(link_candidate) == 0:
    write_json_atomic(LINK_PENDING, {
        "status": "failed",
        "reason": "link_output_missing_or_zero",
        "pid": os.getpid(),
    })
    print("[LINK] output missing or zero: %s" % link_candidate, file=sys.stderr)
    write_progress("failed", phase_name="linking", result="failed",
                   link_reason=link_reason)
    sys.exit(1)

try:
    os.replace(link_candidate, exe_path)
except OSError as error:
    print("[LINK] could not publish %s: %s" % (exe_path, error), file=sys.stderr)
    write_json_atomic(LINK_PENDING, {
        "status": "failed",
        "reason": "exe_replace_locked_or_failed",
        "pid": os.getpid(),
        "error": str(error),
    })
    write_progress("failed", phase_name="linking", result="failed")
    sys.exit(1)

PHASE_TIMINGS["link_s"] = time.monotonic() - link_started
write_progress("staging", link_s=round(PHASE_TIMINGS["link_s"], 3))

write_json_atomic(LINK_STATE, {
    "status": "linked",
    "reason": link_reason,
    "pid": os.getpid(),
    "linked_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
    "objects": object_signature(object_files),
    "flags": flags_fingerprint(),
    "exe": file_signature(exe_path),
    "sha256": sha256_file(exe_path),
})
try:
    os.remove(LINK_PENDING)
except OSError:
    pass

staging_started = time.monotonic()
stage_runtime_dlls()
PHASE_TIMINGS["staging_s"] = time.monotonic() - staging_started
write_progress("staging", staging_s=round(PHASE_TIMINGS["staging_s"], 3))

# ============================================================
# DONE
# ============================================================

elapsed = time.time() - start_time

print()
print("==================================================")
print(" BUILD SUCCESS")
print("==================================================")
print()

print("Compiled:", compiled_count)
print("Skipped :", skipped_count)
print("Time    : %.2f sec" % elapsed)
write_build_metrics("success", link_reason)
write_progress("build_complete", result="success",
               compiled=compiled_count, skipped=skipped_count,
               elapsed_s=round(elapsed, 3))

if not RUN_AFTER_BUILD:
    sys.exit(0)

print()
print("Running MiMITA...")
print()

exe_path = os.path.join(ROOT, EXE_NAME)

if os.path.exists(exe_path):
    if AUTO_CLOSE_SECONDS > 0:
        print("(auto-close in %d seconds)" % AUTO_CLOSE_SECONDS)
        try:
            subprocess.run([exe_path], timeout=AUTO_CLOSE_SECONDS)
        except subprocess.TimeoutExpired:
            print()
            print("(auto-closed after %d seconds)" % AUTO_CLOSE_SECONDS)
    else:
        subprocess.run([exe_path])
else:
    print("[ERROR] mimita.exe missing 2")
