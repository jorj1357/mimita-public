# 08 31 2026, 00 00
# purpose
# Resolve the local C++ toolchain without machine-specific paths.
# Prefer explicit environment configuration, then tools available on PATH.
# Keep build scripts consistent on developer machines and CI.
# DOES NOT install compilers or third-party dependencies.
# DOES NOT select a different compiler silently when MIMITA_COMPILER is set.

import os
import shutil

ROOT = os.path.dirname(os.path.abspath(__file__))
DEVELOPER = os.path.join(ROOT, "developer")
LOCAL_MINGW_BIN = os.path.join(DEVELOPER, "toolchain", "mingw64", "bin")
LOCAL_GLFW_ROOT = os.path.join(DEVELOPER, "glfw", "glfw-3.4.bin.WIN64")


def _required_file(value, variable):
    path = os.path.abspath(value)
    if not os.path.isfile(path):
        raise FileNotFoundError(
            f"{variable} points to a missing file: {path}. "
            f"Set {variable} to a valid installation path."
        )
    return path


def compiler():
    configured = os.environ.get("MIMITA_COMPILER")
    if configured:
        return _required_file(configured, "MIMITA_COMPILER")
    found = (
        os.path.join(LOCAL_MINGW_BIN, "g++.exe")
        if os.path.isfile(os.path.join(LOCAL_MINGW_BIN, "g++.exe"))
        else shutil.which("g++") or shutil.which("clang++")
    )
    if found:
        return os.path.abspath(found)
    raise FileNotFoundError(
        "No C++ compiler found. Install MinGW-w64 or LLVM and add its bin "
        "directory to PATH, or set MIMITA_COMPILER to g++.exe."
    )


def ccache(cxx):
    configured = os.environ.get("MIMITA_CCACHE")
    if configured:
        return _required_file(configured, "MIMITA_CCACHE")
    local = os.path.join(os.path.dirname(cxx), "ccache.exe")
    return shutil.which("ccache") or local


def runtime_path(cxx):
    """Return the compiler's DLL directory for locally launched builds."""
    return os.path.dirname(os.path.abspath(cxx))


def glfw_include():
    value = os.environ.get("MIMITA_GLFW_INCLUDE") or os.path.join(LOCAL_GLFW_ROOT, "include")
    if not value or not os.path.isdir(value):
        raise FileNotFoundError(
            "GLFW headers were not found. Set MIMITA_GLFW_INCLUDE to GLFW's "
            "include directory."
        )
    return os.path.abspath(value)


def glfw_lib():
    value = os.environ.get("MIMITA_GLFW_LIB") or os.path.join(LOCAL_GLFW_ROOT, "lib-mingw-w64")
    if not value or not os.path.isdir(value):
        raise FileNotFoundError(
            "GLFW libraries were not found. Set MIMITA_GLFW_LIB to GLFW's "
            "MinGW library directory."
        )
    return os.path.abspath(value)
