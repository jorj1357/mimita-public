# 08 16 2026, 01 35
# purpose
# MiMITA incremental build system normally invoked through build_agent.py.
# Compiles only changed cpp files in parallel, uses a ccache wrapper and a
# precompiled header (pch.h), then links mimita.exe.
# Supports debug/release modes, clean, build-only, and auto-close flags.
# Does NOT auto-run the game when given build-only; agents must use build_agent.py.
# Does NOT rebuild translation units whose objects are up to date.
# Does NOT modify source files.

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

from concurrent.futures import ThreadPoolExecutor, as_completed

# ============================================================
# CONFIG
# ============================================================

ROOT = os.path.dirname(os.path.abspath(__file__))

# Toolchain paths are overridable via environment variables so the same scripts
# run locally (defaults below) and on CI runners (e.g. GitHub Actions).
DEFAULT_COMPILER = r"C:\important\winlibs-x86_64-posix-seh-gcc-15.2.0-mingw-w64ucrt-13.0.0-r4\mingw64\bin\g++.exe"
COMPILER = os.environ.get("MIMITA_COMPILER", DEFAULT_COMPILER)
CCACHE = os.environ.get("MIMITA_CCACHE", os.path.join(os.path.dirname(COMPILER), "ccache.exe"))

def ccache_cmd():
    # Fall back to the plain compiler when ccache.exe is not installed (e.g.
    # CI runners), so the build works without ccache.
    if os.path.isfile(CCACHE):
        return [CCACHE, COMPILER]
    return [COMPILER]

DEFAULT_GLFW_INCLUDE = r"C:\important\glfw-3.4.bin.WIN64\include"
DEFAULT_GLFW_LIB = r"C:\important\glfw-3.4.bin.WIN64\lib-mingw-w64"
GLFW_INCLUDE = os.environ.get("MIMITA_GLFW_INCLUDE", DEFAULT_GLFW_INCLUDE)
GLFW_LIB = os.environ.get("MIMITA_GLFW_LIB", DEFAULT_GLFW_LIB)

SRC_DIR = os.path.join(ROOT, "src")
BUILD_DIR = os.path.join(ROOT, "build")
# OBJ_DIR and PCH_OUTPUT are set per build mode after MODE is resolved below.

EXE_NAME = os.environ.get("MIMITA_EXE_NAME", "mimita.exe")
try:
    BUILD_JOBS = max(1, int(os.environ.get("MIMITA_BUILD_JOBS", os.cpu_count() or 1)))
except ValueError:
    BUILD_JOBS = os.cpu_count() or 1

PCH_HEADER = os.path.join(SRC_DIR, "pch.h")

# ============================================================
# BUILD MODE
# ============================================================

MODE = "debug"
RUN_AFTER_BUILD = True
AUTO_CLOSE_SECONDS = 0

if len(sys.argv) > 1:
    for arg in sys.argv[1:]:
        a = arg.lower()
        if a in ("build-only", "compile-only", "norun", "no-run"):
            if MODE != "release":
                MODE = "debug"
            RUN_AFTER_BUILD = False
        elif a in ("auto-close", "timeout"):
            AUTO_CLOSE_SECONDS = 5
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

# ============================================================
# FLAGS
# ============================================================

LIBJUICE_DIR = os.path.join(ROOT, "external", "libjuice")

INCLUDE_FLAGS = [
    "-Iinclude",
    "-Isrc",
    f"-I{GLFW_INCLUDE}",
    f"-I{LIBJUICE_DIR}/include",
]

DEFINE_FLAGS = [
    "-DGLM_ENABLE_EXPERIMENTAL",
    "-DJUICE_STATIC",
]

LIB_FLAGS = [
    f"-L{GLFW_LIB}",
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
    out = []

    for root, dirs, files in os.walk(SRC_DIR):
        for f in files:
            if f.endswith(".cpp"):
                out.append(os.path.join(root, f))

    return out

# ============================================================
# CLEAN
# ============================================================

if MODE == "clean":
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

os.environ.setdefault("CCACHE_DIR", os.path.join(BUILD_DIR, "ccache"))
os.environ.setdefault("CCACHE_SLOPPINESS", "time_macros,file_macro")
os.environ.setdefault("CCACHE_MAXSIZE", "8G")

game_dll_build = subprocess.run([sys.executable, os.path.join(ROOT, "build_game_dll.py")])
if game_dll_build.returncode != 0:
    print("[HOT RELOAD] reload failed")
    sys.exit(game_dll_build.returncode)

print("==================================================")
print(" MiMITA Build System")
print("==================================================")
print("Mode:", MODE)
print()

start_time = time.time()

cpp_files = find_cpp_files()

# ============================================================
# BUILD PCH
# ============================================================

pch_needs_build = (
    not os.path.exists(PCH_OUTPUT)
    or os.path.getmtime(PCH_HEADER) >= os.path.getmtime(PCH_OUTPUT)
)

if pch_needs_build:

    print("[PCH ]", os.path.relpath(PCH_HEADER, ROOT))

    cmd = [
        COMPILER,

        "-x",
        "c++-header",

        PCH_HEADER,

        "-o",
        PCH_OUTPUT,
    ]

    cmd += CXX_FLAGS
    cmd += INCLUDE_FLAGS
    cmd += DEFINE_FLAGS

    result = subprocess.run(cmd)

    if result.returncode != 0:
        print()
        print("==================================================")
        print(" PCH BUILD FAILED")
        print("==================================================")
        sys.exit(1)

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

    print("[CXX ]", os.path.relpath(src, ROOT))

    cmd = ccache_cmd() + [

        "-c",
        src,

        "-o",
        obj,
    ]

    cmd += CXX_FLAGS
    cmd += INCLUDE_FLAGS
    cmd += DEFINE_FLAGS

    cmd += [
        "-include",
        "pch.h",
    ]

    result = subprocess.run(cmd)

    if result.returncode != 0:
        return ("fail", src)

    return ("compiled", src)

# ============================================================
# PARALLEL COMPILE CPP FILES
# ============================================================

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
            print("[SKIP]", os.path.relpath(src, ROOT))
            skipped_count += 1

        elif status == "compiled":
            compiled_count += 1

        elif status == "fail":

            print()
            print("==================================================")
            print(" BUILD FAILED")
            print("==================================================")

            sys.exit(1)

# ============================================================
# COMPILE GLAD.C
# ============================================================

glad_obj = os.path.join(OBJ_DIR, "glad.o")
object_files.append(glad_obj)

if source_changed(glad_path):

    print("[CC  ] src/glad.c")

    cmd = ccache_cmd() + [
        "-c",
        glad_path,
        "-o",
        glad_obj,
    ]

    cmd += CXX_FLAGS
    cmd += INCLUDE_FLAGS
    cmd += DEFINE_FLAGS

    result = subprocess.run(cmd)

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
    f"-I{juice_src_dir}",
    f"-I{LIBJUICE_DIR}/include",
    f"-I{LIBJUICE_DIR}/include/juice",
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

    print("[CC  ] external/libjuice/src/" + src_name)

    cmd = ccache_cmd() + [
        "-x", "c",
        "-c",
        src_path,
        "-o",
        obj_path_full,
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

    result = subprocess.run(cmd)

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

# ============================================================
# RESOURCE (icon) — mimita.rc → build/mimita.res.o
# ============================================================

DEFAULT_WINDRES = r"C:\important\winlibs-x86_64-posix-seh-gcc-15.2.0-mingw-w64ucrt-13.0.0-r4\mingw64\bin\windres.exe"
WINDRES = os.environ.get("MIMITA_WINDRES", DEFAULT_WINDRES)
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
        print("[RC  ]", os.path.relpath(RC_FILE, ROOT))
        r = subprocess.run([WINDRES, RC_FILE, "-O", "coff", "-o", RES_OBJ])
        if r.returncode != 0:
            print()
            print("==================================================")
            print(" RESOURCE BUILD FAILED")
            print("==================================================")
            sys.exit(1)
        res_compiled = True
    object_files.append(RES_OBJ)

# ============================================================
# DETERMINE IF LINK NEEDED
# ============================================================

exe_path = os.path.join(ROOT, EXE_NAME)

needs_link = (
    compiled_count > 0 or
    res_compiled or
    os.environ.get("MIMITA_FORCE_LINK") == "1" or
    not os.path.exists(exe_path)
)  

if not needs_link:
    print()
    print("Nothing changed.")
    print()

    elapsed = time.time() - start_time

    print("Time    : %.2f sec" % elapsed)

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

print()
print("[LINK]", EXE_NAME)

cmd = ccache_cmd()

cmd += object_files
cmd += LIB_FLAGS
cmd += LINK_LIBS
cmd += LINK_FLAGS

cmd += [
    "-o",
    EXE_NAME,
]

result = subprocess.run(cmd)

if result.returncode != 0:
    print()
    print("==================================================")
    print(" LINK FAILED")
    print("==================================================")
    sys.exit(1)

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
