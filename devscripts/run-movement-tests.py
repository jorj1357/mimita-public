#!/usr/bin/env python3
# 08 02 2026, 18 00
# purpose
# Builds and runs the Counter-Strike-style bunny-hop kernel tests.
# Links only the shared movement kernel source, so it compiles in seconds.
# Does NOT launch mimita.exe, open a window, or contact the network.

import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
COMPILER = r"C:\important\winlibs-x86_64-posix-seh-gcc-15.2.0-mingw-w64ucrt-13.0.0-r4\mingw64\bin\g++.exe"
BUILD = ROOT / "build"
CXX_FLAGS = ["-std=c++17", "-Og", "-g", "-pipe", "-I.", "-Isrc", "-Iinclude"]

SOURCES = [
    ROOT / "tests" / "movement-bhop-test.cpp",
    ROOT / "src" / "physics" / "movement" / "movement-step.cpp",
]


def main():
    os.makedirs(BUILD, exist_ok=True)
    out = BUILD / "movement-bhop-test.exe"
    cmd = [COMPILER] + CXX_FLAGS + [str(s) for s in SOURCES] + ["-o", str(out)]
    print("[TEST-BUILD] movement-bhop-test")
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        print(result.stdout)
        print(result.stderr)
        print("[TEST-BUILD] movement-bhop-test FAILED")
        return 1

    print("[TEST-RUN] movement-bhop-test.exe")
    run = subprocess.run([str(out)], capture_output=True, text=True, timeout=60)
    print(run.stdout.strip())
    if run.stderr.strip():
        print(run.stderr.strip())
    if run.returncode != 0:
        print("[TEST-RUN] movement-bhop-test FAILED (exit %d)" % run.returncode)
        return 1

    print("[run-movement-tests] ALL PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
