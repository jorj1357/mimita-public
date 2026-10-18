# 08 20 2026, 00 00
# purpose
# Compatibility entry point for older setup scripts.
# Routes build requests through the one dev-loop daemon and shared build lock.
# Preserves fresh-clone dependency and submodule setup behavior.
# Does NOT terminate running game/server processes or delete executables.

import os
import subprocess
import sys


ROOT = os.path.dirname(os.path.abspath(__file__))


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


def main():
    initialize_submodules()
    configure_local_dependencies()

    timeout = 36000
    build_mode = "release" if any(arg.lower() == "release" for arg in sys.argv[1:]) else "debug"
    if "timeout" in sys.argv:
        index = sys.argv.index("timeout")
        if index + 1 < len(sys.argv):
            try:
                timeout = max(1, int(sys.argv[index + 1]))
            except ValueError:
                print("[BUILD] invalid timeout value", file=sys.stderr)
                return 2

    print("[BUILD] routing compatibility request through the dev-loop daemon", flush=True)
    return subprocess.run(
        [
            sys.executable,
            os.path.join(ROOT, "devscripts", "dev-loop.py"),
            "--request", "build",
            "--build-mode", build_mode,
            "--wait-for-build",
            "--build-timeout", str(timeout),
        ],
        cwd=ROOT,
        env=os.environ.copy(),
        check=False,
    ).returncode


if __name__ == "__main__":
    raise SystemExit(main())
