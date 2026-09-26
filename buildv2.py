# 08 20 2026, 00 00
# purpose
# MiMITA clean-relink build entry point.
# Deletes the canonical executable before invoking the existing build system.
# Keeps the established compiler, linker, and launch behavior in build.py.
# Does NOT delete build objects, source files, configuration, or other binaries.
# Does NOT change the build system's compiler or build mode.

import os
import runpy
import subprocess
import time


ROOT = os.path.dirname(os.path.abspath(__file__))
EXE_PATH = os.path.join(ROOT, "mimita.exe")


def _first_existing_file(candidates):
    for path in candidates:
        if os.path.isfile(path):
            return path
    return ""


def configure_local_dependencies():
    """Make this one-file local build entry point work after a fresh clone.

    The compiler and GLFW package are intentionally not committed to Git.
    Prefer explicit environment variables, then use the developer installs
    already present on this Windows machine.  This keeps buildv2.py convenient
    without putting machine paths into game source or the shared toolchain
    resolver.
    """
    if not os.environ.get("MIMITA_COMPILER"):
        compiler = _first_existing_file([
            r"C:\important\msys64\mingw64\bin\g++.exe",
            r"C:\important\winlibs-x86_64-posix-seh-gcc-15.2.0-mingw-w64ucrt-13.0.0-r4\mingw64\bin\g++.exe",
            r"C:\mimita-priv-v8\developer\toolchain\mingw64\bin\g++.exe",
        ])
        if compiler:
            os.environ["MIMITA_COMPILER"] = compiler
            print(f"[TOOLCHAIN] compiler={compiler}")

    if not os.environ.get("MIMITA_GLFW_INCLUDE"):
        glfw_include = _first_existing_file([
            r"C:\important\glfw-3.4.bin.WIN64\include\GLFW\glfw3.h",
            r"C:\mimita-priv-v8\developer\glfw\glfw-3.4.bin.WIN64\include\GLFW\glfw3.h",
        ])
        if glfw_include:
            include_dir = os.path.dirname(os.path.dirname(glfw_include))
            os.environ["MIMITA_GLFW_INCLUDE"] = include_dir
            print(f"[TOOLCHAIN] glfw_include={include_dir}")

    if not os.environ.get("MIMITA_GLFW_LIB"):
        glfw_lib = _first_existing_file([
            r"C:\important\glfw-3.4.bin.WIN64\lib-mingw-w64\libglfw3.a",
            r"C:\mimita-priv-v8\developer\glfw\glfw-3.4.bin.WIN64\lib-mingw-w64\libglfw3.a",
        ])
        if glfw_lib:
            lib_dir = os.path.dirname(glfw_lib)
            os.environ["MIMITA_GLFW_LIB"] = lib_dir
            print(f"[TOOLCHAIN] glfw_lib={lib_dir}")


def initialize_submodules():
    """Fetch the source dependency that a normal GitHub clone leaves empty."""
    juice_header = os.path.join(ROOT, "external", "libjuice", "include", "juice", "juice.h")
    if os.path.isfile(juice_header):
        return

    print("[SUBMODULE] external/libjuice is missing; initializing it...")
    result = subprocess.run(
        ["git", "submodule", "update", "--init", "--recursive"],
        cwd=ROOT,
        check=False,
    )
    if result.returncode != 0 or not os.path.isfile(juice_header):
        raise SystemExit(
            "[SUBMODULE] Could not initialize external/libjuice. "
            "Run: git submodule update --init --recursive"
        )


initialize_submodules()
configure_local_dependencies()

print("Closing any running mimita.exe process...")
subprocess.run(
    ["taskkill", "/F", "/T", "/IM", "mimita.exe"],
    stdout=subprocess.DEVNULL,
    stderr=subprocess.DEVNULL,
    check=False,
)
if os.path.exists(EXE_PATH):
    time.sleep(0.5)
    print("Deleting existing mimita.exe...")
    os.remove(EXE_PATH)

# build.py sees that the executable is missing, relinks it, and then runs it.
runpy.run_path(os.path.join(ROOT, "build.py"), run_name="__main__")
