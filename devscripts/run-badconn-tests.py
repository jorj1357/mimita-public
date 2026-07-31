#!/usr/bin/env python3
# 07 31 2026, 15 30
# purpose
# Builds and runs the badconn test suite (config loading + simulator core).
# Compiles the badconn module sources with each test into build/.
# Does NOT launch mimita.exe, open a window, or contact the network.
# Does NOT modify game source; only compiles tests into build/.

import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
COMPILER = r"C:\important\winlibs-x86_64-posix-seh-gcc-15.2.0-mingw-w64ucrt-13.0.0-r4\mingw64\bin\g++.exe"
BUILD = ROOT / "build"
CXX_FLAGS = ["-std=c++17", "-Og", "-g", "-pipe", "-I.", "-Isrc", "-Iinclude"]

BADCONN_SOURCES = [
    ROOT / "src" / "network" / "badconn" / "badconn.cpp",
    ROOT / "src" / "network" / "badconn" / "badconn-config.cpp",
    ROOT / "src" / "network" / "badconn" / "badconn-registry.cpp",
    ROOT / "src" / "network" / "badconn" / "badconn-latency.cpp",
    ROOT / "src" / "network" / "badconn" / "badconn-loss.cpp",
    ROOT / "src" / "network" / "badconn" / "badconn-reorder.cpp",
    ROOT / "src" / "network" / "badconn" / "badconn-blackout.cpp",
    ROOT / "src" / "debug" / "debug-log.cpp",
]


def compile_test(name, sources):
    out = BUILD / f"{name}.exe"
    cmd = [COMPILER] + CXX_FLAGS + [str(s) for s in sources] + ["-o", str(out)]
    print(f"[TEST-BUILD] {name}")
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        print(result.stdout)
        print(result.stderr)
        print(f"[TEST-BUILD] {name} FAILED")
        sys.exit(1)
    return out


def run_test(exe, timeout=120):
    print(f"[TEST-RUN] {exe.name}")
    result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=timeout, cwd=str(ROOT))
    print(result.stdout.strip())
    if result.stderr.strip():
        print(result.stderr.strip())
    return result.returncode


def main():
    ok = True

    for name in ("badconn-config-test", "badconn-core-test"):
        test_sources = BADCONN_SOURCES + [ROOT / "tests" / f"{name}.cpp"]
        exe = compile_test(name, test_sources)
        if run_test(exe) != 0:
            print(f"[FAIL] {name}")
            ok = False

    print()
    if ok:
        print("[TEST-SUITE] PASS")
        return 0
    print("[TEST-SUITE] FAIL")
    return 1


if __name__ == "__main__":
    sys.exit(main())
