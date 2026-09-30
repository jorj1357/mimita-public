# 2026-09-30
# purpose
# Build the vendored Manifold static library and install it into
# external/manifold-prebuilt/ so build.py can link it.
# Reproduces the exact reviewed configuration (static, Release, C++ API only,
# no Python/JS/C bindings, no tests, parallel backend off) with the same
# compiler build.py uses, keeping the static lib ABI-compatible.
# DOES NOT modify gameplay code, DOES NOT run the game, and DOES NOT commit.
#
# Usage:
#   python tools/build_manifold.py
#
# Environment overrides:
#   MIMITA_CMAKE     full path to cmake.exe
#   MIMITA_NINJA     full path to ninja.exe
#   MIMITA_COMPILER  full path to g++.exe (see build_toolchain.py)

import os
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SOURCE = os.path.join(ROOT, "external", "manifold")
BUILD = os.path.join(ROOT, "build", "manifold-release")
PREFIX = os.path.join(ROOT, "external", "manifold-prebuilt")

KNOWN_CMAKE = [
    r"C:\important\winlibs-x86_64-posix-seh-gcc-15.2.0-mingw-w64ucrt-13.0.0-r4\mingw64\bin\cmake.exe",
    r"C:\Program Files\CMake\bin\cmake.exe",
]
KNOWN_NINJA = [
    r"C:\important\winlibs-x86_64-posix-seh-gcc-15.2.0-mingw-w64ucrt-13.0.0-r4\mingw64\bin\ninja.exe",
]


def resolve(env_name, executable, known):
    configured = os.environ.get(env_name)
    if configured:
        if os.path.isfile(configured):
            return configured
        sys.exit(f"[MANIFOLD] {env_name} points to a missing file: {configured}")
    found = shutil.which(executable)
    if found:
        return found
    for candidate in known:
        if os.path.isfile(candidate):
            return candidate
    sys.exit(f"[MANIFOLD] Could not find {executable}. Set {env_name}.")


def main():
    if not os.path.isdir(os.path.join(SOURCE, ".git")) and not os.path.isdir(
        os.path.join(SOURCE, "src")
    ):
        sys.exit("[MANIFOLD] external/manifold is missing. Run: "
                 "git submodule update --init external/manifold")

    sys.path.insert(0, ROOT)
    import build_toolchain

    compiler = build_toolchain.compiler()
    cmake = resolve("MIMITA_CMAKE", "cmake.exe", KNOWN_CMAKE)
    ninja = resolve("MIMITA_NINJA", "ninja.exe", KNOWN_NINJA)

    env = dict(os.environ)
    env["PATH"] = os.path.dirname(compiler) + os.pathsep + env.get("PATH", "")

    commit = subprocess.run(
        ["git", "-C", SOURCE, "rev-parse", "HEAD"],
        capture_output=True, text=True, check=True).stdout.strip()
    print(f"[MANIFOLD] source commit {commit}")

    configure = [
        cmake, "-S", SOURCE, "-B", BUILD, "-G", "Ninja",
        f"-DCMAKE_MAKE_PROGRAM={ninja}",
        f"-DCMAKE_CXX_COMPILER={compiler}",
        "-DCMAKE_BUILD_TYPE=Release",
        "-DBUILD_SHARED_LIBS=OFF",
        "-DMANIFOLD_PAR=OFF",
        "-DMANIFOLD_TEST=OFF",
        "-DMANIFOLD_CBIND=OFF",
        "-DMANIFOLD_PYBIND=OFF",
        "-DMANIFOLD_DEBUG=OFF",
        "-DMANIFOLD_CROSS_SECTION=OFF",
    ]
    print("[MANIFOLD] configure")
    if subprocess.run(configure, env=env).returncode != 0:
        sys.exit("[MANIFOLD] configure failed")

    print("[MANIFOLD] build")
    if subprocess.run([cmake, "--build", BUILD, "--config", "Release"],
                      env=env).returncode != 0:
        sys.exit("[MANIFOLD] build failed")

    print("[MANIFOLD] install")
    if subprocess.run([cmake, "--install", BUILD, "--prefix", PREFIX],
                      env=env).returncode != 0:
        sys.exit("[MANIFOLD] install failed")

    lib = os.path.join(PREFIX, "lib", "libmanifold.a")
    if not os.path.isfile(lib):
        sys.exit(f"[MANIFOLD] expected library not produced: {lib}")
    print(f"[MANIFOLD] installed {lib}")


if __name__ == "__main__":
    main()
