# build.py
# MiMITA incremental build system
# 5 23 2026
#
# goals:
# - MUCH faster rebuilds
# - only rebuild changed cpp files
# - debug/release modes
# - automatic obj folder
# - auto run after build
# - readable logs
# - future-proof for bigger engine
#
# usage:
#
# python build.py
# python build.py release
# python build.py build-only
# python build.py clean
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

COMPILER = r"C:\important\winlibs-x86_64-posix-seh-gcc-15.2.0-mingw-w64ucrt-13.0.0-r4\mingw64\bin\g++.exe"

GLFW_INCLUDE = r"C:\important\glfw-3.4.bin.WIN64\include"
GLFW_LIB = r"C:\important\glfw-3.4.bin.WIN64\lib-mingw-w64"

SRC_DIR = os.path.join(ROOT, "src")
BUILD_DIR = os.path.join(ROOT, "build")
OBJ_DIR = os.path.join(BUILD_DIR, "obj")

EXE_NAME = "mimita.exe"

PCH_HEADER = os.path.join(SRC_DIR, "pch.h")
PCH_OUTPUT = os.path.join(SRC_DIR, "pch.h.gch")

# ============================================================
# BUILD MODE
# ============================================================

MODE = "debug"
RUN_AFTER_BUILD = True

if len(sys.argv) > 1:
    MODE = sys.argv[1].lower()
    if MODE in ("build-only", "compile-only", "norun", "no-run"):
        MODE = "debug"
        RUN_AFTER_BUILD = False

if MODE == "release":
    CXX_FLAGS = [
        "-std=c++17",
        "-O2",
        "-march=native",
        "-pipe",
        "-MMD",
        "-MP",
    ]
else:
    MODE = "debug"

    # MUCH faster compile times
    CXX_FLAGS = [
        "-std=c++17",
        "-Og",
        "-g",
        "-pipe",
        "-MMD",
        "-MP",
    ]

# ============================================================
# FLAGS
# ============================================================

INCLUDE_FLAGS = [
    "-Iinclude",
    "-Isrc",
    f"-I{GLFW_INCLUDE}",
]

DEFINE_FLAGS = [
    "-DGLM_ENABLE_EXPERIMENTAL",
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
    "-lws2_32",
]

# ============================================================
# HELPERS
# ============================================================

def ensure_dirs():
    os.makedirs(BUILD_DIR, exist_ok=True)
    os.makedirs(OBJ_DIR, exist_ok=True)




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

    cmd = [
        COMPILER,

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

with ThreadPoolExecutor(max_workers=os.cpu_count()) as executor:

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

    cmd = [
        COMPILER,
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

if compiled_count == 0:
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
        subprocess.run([exe_path])

    sys.exit(0)

# ============================================================
# LINK
# ============================================================

print()
print("[LINK]", EXE_NAME)

cmd = [
    COMPILER,
]

cmd += object_files
cmd += LIB_FLAGS
cmd += LINK_LIBS

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

subprocess.run([exe_path])
