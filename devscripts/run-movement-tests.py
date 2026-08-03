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

CSGO_SOURCES = [
    ROOT / "tests" / "movement-csgo-test.cpp",
    ROOT / "src" / "physics" / "movement" / "movement-step.cpp",
]


def compile_and_run(name, sources, timeout=60):
    out = BUILD / (name + ".exe")
    cmd = [COMPILER] + CXX_FLAGS + [str(s) for s in sources] + ["-o", str(out)]
    print("[TEST-BUILD] " + name)
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        print(result.stdout)
        print(result.stderr)
        print("[TEST-BUILD] " + name + " FAILED")
        return False

    print("[TEST-RUN] " + out.name)
    run = subprocess.run([str(out)], capture_output=True, text=True, timeout=timeout)
    print(run.stdout.strip())
    if run.stderr.strip():
        print(run.stderr.strip())
    if run.returncode != 0:
        print("[TEST-RUN] " + name + " FAILED (exit %d)" % run.returncode)
        return False
    return True


def main():
    os.makedirs(BUILD, exist_ok=True)
    ok = True
    ok &= compile_and_run("movement-bhop-test", SOURCES)
    ok &= compile_and_run("movement-csgo-test", CSGO_SOURCES)
    if not ok:
        return 1
    print("[run-movement-tests] ALL PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
