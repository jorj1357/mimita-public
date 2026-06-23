"""
build_mini.py - Build and run mini-mimita-collision.

Usage:
    python build_mini.py
    (or double-click)

Stages:
    1. Compile (incremental via g++ dependency tracking)
    2. Link
    3. Launch executable
    4. Live stdout/stderr
    5. Exit code + pause on crash

No CMake required. Uses the same compiler and flags as build.bat.
"""

import subprocess
import sys
import os
import threading
from pathlib import Path

ROOT = Path(__file__).parent.resolve()
SRC = ROOT / "src"
BUILD = ROOT / "build"
PARENT = ROOT.parent.resolve()

COMPILER = r"C:\important\winlibs-x86_64-posix-seh-gcc-15.2.0-mingw-w64ucrt-13.0.0-r4\mingw64\bin\g++.exe"
GLFW_INC = r"C:\important\glfw-3.4.bin.WIN64\include"
GLFW_LIB = r"C:\important\glfw-3.4.bin.WIN64\lib-mingw-w64"

CXXFLAGS = ["-std=c++17", "-O0", "-g", "-pipe", "-MMD", "-MP"]
INCLUDES = [f"-I{PARENT / 'include'}", f"-I{SRC}", f"-I{GLFW_INC}"]
DEFINES = ["-DGLM_ENABLE_EXPERIMENTAL"]
LIBS = [f"-L{GLFW_LIB}", "-lglfw3", "-lopengl32", "-lgdi32", "-luser32", "-ldwmapi"]

SOURCES = [
    PARENT / "src" / "glad.c",
    SRC / "main.cpp",
    SRC / "physics.cpp",
    SRC / "render.cpp",
    SRC / "maps.cpp",
    SRC / "glb-loader.cpp",
]

EXE_NAME = "mini-mimita-collision.exe"
EXE_PATH = ROOT / EXE_NAME


def log(tag, msg):
    print(f"[{tag}] {msg}")


def obj_path(src: Path) -> Path:
    s = str(src.resolve())
    s = s.replace(":", "")
    s = s.replace("\\", "_").replace("/", "_")
    return BUILD / (s + ".o")


def dep_path(src: Path) -> Path:
    s = str(src.resolve())
    s = s.replace(":", "")
    s = s.replace("\\", "_").replace("/", "_")
    return BUILD / (s + ".d")


def needs_compile(src: Path) -> bool:
    obj = obj_path(src)
    if not obj.exists() or obj.stat().st_size == 0:
        return True

    obj_time = obj.stat().st_mtime

    dep = dep_path(src)
    if not dep.exists():
        return True

    try:
        text = dep.read_text()
        parts = text.replace("\\\n", " ").split()
        for p in parts[1:]:
            p = p.rstrip(":")
            path = Path(p)
            if path.exists() and path.stat().st_mtime >= obj_time:
                return True
    except Exception:
        return True

    return False


def stage_compile():
    BUILD.mkdir(parents=True, exist_ok=True)

    any_fail = False
    any_compile = False

    for src in SOURCES:
        if not needs_compile(src):
            log("SKIP", src.name)
            continue

        obj = obj_path(src)
        log("CXX", src.name)

        cmd = [COMPILER, "-c", str(src), "-o", str(obj)]
        cmd += CXXFLAGS + INCLUDES + DEFINES

        ret = subprocess.run(cmd, capture_output=False)
        if ret.returncode != 0:
            log("FAIL", f"{src.name} (code {ret.returncode})")
            any_fail = True
        else:
            any_compile = True

    if any_fail:
        return False

    return True


def stage_link() -> bool:
    objs = [obj_path(s) for s in SOURCES]
    missing = [o for o in objs if not o.exists()]
    if missing:
        for m in missing:
            log("MISS", m.name)
        return False

    log("LINK", EXE_NAME)
    cmd = [COMPILER] + [str(o) for o in objs] + LIBS + ["-o", str(EXE_PATH)]
    ret = subprocess.run(cmd, capture_output=False)
    if ret.returncode != 0:
        log("LINK", f"FAILED (code {ret.returncode})")
        return False

    log("LINK", "success")
    return True


def stage_run():
    if not EXE_PATH.exists():
        log("RUN", f"not found: {EXE_PATH}")
        return

    # Ensure glfw3.dll is findable (copy from parent if needed)
    dll_src = PARENT / "glfw3.dll"
    dll_dst = ROOT / "glfw3.dll"
    if dll_src.exists() and not dll_dst.exists():
        import shutil
        shutil.copy2(str(dll_src), str(dll_dst))
        log("DLL", f"copied {dll_src.name}")

    log("RUN", f"launching {EXE_NAME}")
    print(f"[RUN] {EXE_PATH}")
    print("[RUN] close the window to stop")
    print()

    proc = subprocess.Popen(
        [str(EXE_PATH)],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        bufsize=1,
    )

    def forward(stream):
        try:
            for line in iter(stream.readline, ""):
                print(line, end="", flush=True)
        except ValueError:
            pass
        stream.close()

    t1 = threading.Thread(target=forward, args=(proc.stdout,), daemon=True)
    t2 = threading.Thread(target=forward, args=(proc.stderr,), daemon=True)
    t1.start()
    t2.start()

    proc.wait()
    t1.join(timeout=2)
    t2.join(timeout=2)
    print()

    log("EXIT", f"code {proc.returncode}")

    if proc.returncode != 0:
        print()
        log("CRASH", "non-zero exit code — crash or error")
        print()
        try:
            input("Press Enter to exit...")
        except EOFError:
            pass


def remove_stale_objs():
    """Remove zero-byte .o files left by aborted builds."""
    if not BUILD.exists():
        return
    for f in BUILD.iterdir():
        if f.suffix == ".o" and f.stat().st_size == 0:
            try:
                f.unlink()
                log("CLEAN", f"removed stale {f.name}")
            except PermissionError:
                pass


def main():
    print("=" * 52)
    log("BUILD", "mini-mimita-collision")
    print("=" * 52)
    print()

    remove_stale_objs()

    if not stage_compile():
        log("BUILD", "COMPILE FAILED")
        sys.exit(1)

    if not stage_link():
        log("BUILD", "LINK FAILED")
        sys.exit(1)

    log("BUILD", "done")
    print()

    stage_run()


if __name__ == "__main__":
    main()
