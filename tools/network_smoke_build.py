#!/usr/bin/env python3
# 08 05 2026, 00 00
# purpose
# Builds the live UDP network protocol smoke executable for process harnesses.
# Names the executable by packet protocol version to avoid stale binary reuse.
# Shares one compile command between UDP and mixed ICE networking tests.
# Does NOT build mimita.exe, launch the game, or contact network services.
# Does NOT overwrite old smoke executables that may be locked by Windows.
# Does NOT validate gameplay outcomes beyond producing the helper executable.

import re
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
COMPILER = (
    r"C:\important\winlibs-x86_64-posix-seh-gcc-15.2.0-mingw-w64ucrt-13.0.0-r4"
    r"\mingw64\bin\g++.exe"
)

SOURCES = [
    ROOT / "tests" / "network-protocol-smoke.cpp",
    ROOT / "src" / "network" / "net_common.cpp",
    ROOT / "src" / "network" / "snapshot-chunks.cpp",
    ROOT / "src" / "network" / "movement-validation.cpp",
]

DEPENDENCIES = SOURCES + [
    ROOT / "src" / "network" / "packets.h",
    ROOT / "src" / "network" / "net_common.h",
    ROOT / "src" / "network" / "snapshot-chunks.h",
    ROOT / "src" / "network" / "movement-validation.h",
    ROOT / "src" / "network" / "server.h",
]


def protocol_version():
    packets = ROOT / "src" / "network" / "packets.h"
    text = packets.read_text(encoding="utf-8", errors="replace")
    match = re.search(r"constexpr\s+uint16_t\s+PROTOCOL_VERSION\s*=\s*(\d+)", text)
    if not match:
        raise RuntimeError("could not read PROTOCOL_VERSION from src/network/packets.h")
    return int(match.group(1))


def newest_input_mtime():
    newest = 0.0
    for path in DEPENDENCIES:
        if path.exists():
            newest = max(newest, path.stat().st_mtime)
    return newest


def ensure_network_protocol_smoke(verbose=False):
    version = protocol_version()
    output = ROOT / "build" / f"network-protocol-smoke-v{version}.exe"
    output.parent.mkdir(parents=True, exist_ok=True)
    if output.exists() and output.stat().st_mtime >= newest_input_mtime():
        return output

    cmd = [
        COMPILER,
        "-std=c++17",
        "-Og",
        "-g",
        "-pipe",
        "-I.",
        "-Isrc",
        "-Iinclude",
        "-Iexternal/libjuice/include",
        "-DGLM_ENABLE_EXPERIMENTAL",
        "-DJUICE_STATIC",
    ]
    cmd += [str(path) for path in SOURCES]
    cmd += ["-o", str(output), "-lws2_32"]

    result = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True)
    if result.returncode != 0:
        if verbose:
            print(result.stdout)
            print(result.stderr)
        raise RuntimeError(
            f"failed to build network protocol smoke executable exit={result.returncode}"
        )
    return output
