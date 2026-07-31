#!/usr/bin/env python3
# 07 31 2026, 00 00
# purpose
# Builds and runs the crash-handler test suite.
# Verifies crash directory creation, text reports, minidumps, and that forced
# access violations (main thread and worker thread) produce nonzero artifacts.
# Does NOT launch mimita.exe, open a window, or contact the network.
# Does NOT modify game source; only compiles tests into build/.

import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
COMPILER = r"C:\important\winlibs-x86_64-posix-seh-gcc-15.2.0-mingw-w64ucrt-13.0.0-r4\mingw64\bin\g++.exe"
BUILD = ROOT / "build"
SRC = ROOT / "src"

CXX_FLAGS = ["-std=c++17", "-Og", "-g", "-pipe", "-I.", "-Isrc", "-Iinclude"]
LINK_LIBS = ["-luser32", "-lshell32", "-ldbghelp", "-lbcrypt"]


def compile_test(name, sources):
    out = BUILD / f"{name}.exe"
    cmd = [COMPILER] + CXX_FLAGS + sources + ["-o", str(out)] + LINK_LIBS
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
    return result.returncode


def crash_dir():
    import ctypes
    buf = ctypes.create_unicode_buffer(300)
    ctypes.windll.shell32.SHGetFolderPathW(None, 0x001c, None, 0, buf)
    return Path(buf.value) / "MiMITA" / "crashes"


def count_artifacts(d, suffixes):
    if not d.exists():
        return 0
    return sum(1 for p in d.glob("crash-*") if p.suffix in suffixes and p.stat().st_size > 0)


def main():
    ok = True

    # 1. Unit tests (no crash)
    unit = compile_test("crash-handler-test", [str(ROOT / "tests" / "crash-handler-test.cpp")])
    if run_test(unit) != 0:
        print("[FAIL] crash-handler-test")
        ok = False

    # 2. Intentional main-thread access violation
    cd = crash_dir()
    cd.mkdir(parents=True, exist_ok=True)
    before_txt = count_artifacts(cd, {".txt"})
    before_dmp = count_artifacts(cd, {".dmp"})

    av_main = compile_test("crash-av-main-test", [str(ROOT / "tests" / "crash-av-main-test.cpp")])
    rc = run_test(av_main)
    # EXCEPTION_CONTINUE_SEARCH terminates the process; expect a nonzero exit.
    after_txt = count_artifacts(cd, {".txt"})
    after_dmp = count_artifacts(cd, {".dmp"})
    if rc == 0:
        print("[FAIL] main-thread AV process exited 0 (expected nonzero)")
        ok = False
    if after_txt <= before_txt:
        print("[FAIL] main-thread AV produced no new nonzero .txt report")
        ok = False
    if after_dmp <= before_dmp:
        print("[FAIL] main-thread AV produced no new nonzero .dmp minidump")
        ok = False

    # 3. Intentional worker-thread access violation
    before_txt = count_artifacts(cd, {".txt"})
    before_dmp = count_artifacts(cd, {".dmp"})
    av_worker = compile_test("crash-av-worker-test", [str(ROOT / "tests" / "crash-av-worker-test.cpp")])
    rc = run_test(av_worker)
    after_txt = count_artifacts(cd, {".txt"})
    after_dmp = count_artifacts(cd, {".dmp"})
    if rc == 0:
        print("[FAIL] worker-thread AV process exited 0 (expected nonzero)")
        ok = False
    if after_txt <= before_txt:
        print("[FAIL] worker-thread AV produced no new nonzero .txt report")
        ok = False
    if after_dmp <= before_dmp:
        print("[FAIL] worker-thread AV produced no new nonzero .dmp minidump")
        ok = False

    print()
    if ok:
        print("[TEST-SUITE] PASS")
        return 0
    print("[TEST-SUITE] FAIL")
    return 1


if __name__ == "__main__":
    sys.exit(main())
