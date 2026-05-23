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
# python build.py clean
#

import os
import sys
import subprocess
import hashlib
import shutil
import time

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
CACHE_DIR = os.path.join(BUILD_DIR, "cache")

EXE_NAME = "mimita.exe"

# ============================================================
# BUILD MODE
# ============================================================

MODE = "debug"

if len(sys.argv) > 1:
    MODE = sys.argv[1].lower()

if MODE == "release":
    CXX_FLAGS = [
        "-std=c++17",
        "-O2",
        "-march=native",
        "-pipe",
    ]
else:
    MODE = "debug"

    # MUCH faster compile times
    CXX_FLAGS = [
        "-std=c++17",
        "-Og",
        "-g",
        "-pipe",
    ]

# ============================================================
# FLAGS
# ============================================================

INCLUDE_FLAGS = [
    "-Iinclude",
    "-Isrc",
    f"-I{GLFW_INCLUDE}",
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
]

# ============================================================
# HELPERS
# ============================================================

def ensure_dirs():
    os.makedirs(BUILD_DIR, exist_ok=True)
    os.makedirs(OBJ_DIR, exist_ok=True)
    os.makedirs(CACHE_DIR, exist_ok=True)

def hash_file(path):
    h = hashlib.md5()

    with open(path, "rb") as f:
        while True:
            chunk = f.read(8192)
            if not chunk:
                break
            h.update(chunk)

    return h.hexdigest()

def cache_path(src):
    rel = os.path.relpath(src, ROOT)
    rel = rel.replace("\\", "_").replace("/", "_")
    return os.path.join(CACHE_DIR, rel + ".hash")

def obj_path(src):
    rel = os.path.relpath(src, SRC_DIR)
    rel = os.path.splitext(rel)[0]

    rel = rel.replace("\\", "_").replace("/", "_")

    return os.path.join(OBJ_DIR, rel + ".o")

def source_changed(src):
    h = hash_file(src)
    cp = cache_path(src)

    if not os.path.exists(cp):
        return True

    old = open(cp, "r").read().strip()

    return old != h

def update_cache(src):
    h = hash_file(src)

    with open(cache_path(src), "w") as f:
        f.write(h)

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

# glad.c manually
glad_path = os.path.join(ROOT, "src", "glad.c")

object_files = []

compiled_count = 0
skipped_count = 0

# ============================================================
# COMPILE CPP FILES
# ============================================================

for src in cpp_files:

    obj = obj_path(src)
    object_files.append(obj)

    if not source_changed(src) and os.path.exists(obj):
        print("[SKIP]", os.path.relpath(src, ROOT))
        skipped_count += 1
        continue

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

    result = subprocess.run(cmd)

    if result.returncode != 0:
        print()
        print("==================================================")
        print(" BUILD FAILED")
        print("==================================================")
        sys.exit(1)

    update_cache(src)
    compiled_count += 1

# ============================================================
# COMPILE GLAD.C
# ============================================================

glad_obj = os.path.join(OBJ_DIR, "glad.o")
object_files.append(glad_obj)

if source_changed(glad_path) or not os.path.exists(glad_obj):

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

    result = subprocess.run(cmd)

    if result.returncode != 0:
        print()
        print("==================================================")
        print(" BUILD FAILED")
        print("==================================================")
        sys.exit(1)

    update_cache(glad_path)
    compiled_count += 1

else:
    print("[SKIP] src/glad.c")
    skipped_count += 1

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

print()
print("Running MiMITA...")
print()

exe_path = os.path.join(ROOT, EXE_NAME)

subprocess.run([exe_path])