#!/usr/bin/env python3
# 08 02 2026, 16 40
# purpose
# Builds and runs the npc-combat regression tests.
# The default subset is header-only (range gate + muzzle/LOS origin consistency)
# and compiles in seconds. Passing --full-link additionally defines
# MIMITA_NPC_FULL_LINK to compile the guarded integration tests that need the
# full game object graph.
# Does NOT launch mimita.exe, open a window, or contact the network.

import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
COMPILER = r"C:\important\winlibs-x86_64-posix-seh-gcc-15.2.0-mingw-w64ucrt-13.0.0-r4\mingw64\bin\g++.exe"
BUILD = ROOT / "build"
CXX_FLAGS = ["-std=c++17", "-Og", "-g", "-pipe", "-I.", "-Isrc", "-Iinclude"]


def compile_test(name, sources, extra_flags=None):
    out = BUILD / f"{name}.exe"
    cmd = [COMPILER] + CXX_FLAGS + (extra_flags or []) + sources + ["-o", str(out)]
    print(f"[TEST-BUILD] {name}")
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        print(result.stdout)
        print(result.stderr)
        print(f"[TEST-BUILD] {name} FAILED")
        sys.exit(1)
    return out


def run_test(exe, timeout=60):
    print(f"[TEST-RUN] {exe.name}")
    result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=timeout)
    print(result.stdout.strip())
    if result.stderr.strip():
        print(result.stderr.strip())
    if result.returncode != 0:
        print(f"[TEST-RUN] {exe.name} FAILED (exit {result.returncode})")
        sys.exit(1)
    return True


def main():
    os.makedirs(BUILD, exist_ok=True)
    full_link = "--full-link" in sys.argv

    # Header-only subset: no game sources needed.
    test = ROOT / "tests" / "npc-combat-regression-test.cpp"
    flags = ["-DMIMITA_NPC_FULL_LINK"] if full_link else []
    exe = compile_test("npc-combat-regression-test", [str(test)], flags)
    run_test(exe)

    print("[run-npc-combat-tests] ALL PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
