# 08 03 2026, 13 15
# purpose
# Rebuilds the hot-reload game DLL (build/mimita-game.dll) from src/effects/effect-part.cpp.
# Skips the rebuild when the DLL is already newer than the source, and uses ccache.
# Is invoked automatically at the start of every build.py run.
# Does NOT compile any other source files or modify game source code.
# Does NOT link mimita.exe or launch the game.

import os
import subprocess
import sys

from build_toolchain import compiler, ccache, glfw_include

ROOT = os.path.dirname(os.path.abspath(__file__))

try:
    COMPILER = compiler()
    CCACHE = ccache(COMPILER)
    GLFW_INCLUDE = glfw_include()
except FileNotFoundError as error:
    print(f"[TOOLCHAIN] {error}")
    sys.exit(2)
BUILD_DIR = os.path.join(ROOT, "build")
SOURCE = os.path.join(ROOT, "src", "effects", "effect-part.cpp")
STAGING_DLL = os.path.join(BUILD_DIR, "mimita-game.build.dll")
OUTPUT_DLL = os.path.join(BUILD_DIR, "mimita-game.dll")

os.makedirs(BUILD_DIR, exist_ok=True)

os.environ.setdefault("CCACHE_DIR", os.path.join(BUILD_DIR, "ccache"))
os.environ.setdefault("CCACHE_SLOPPINESS", "time_macros,file_macro")
os.environ.setdefault("CCACHE_MAXSIZE", "8G")

if os.path.exists(OUTPUT_DLL) and os.path.getmtime(SOURCE) < os.path.getmtime(OUTPUT_DLL):
    print("[HOT RELOAD] DLL up to date, skipping")
    sys.exit(0)

command = []
# Fall back to the plain compiler when ccache.exe is not installed (e.g. CI
# runners), so the hot-reload DLL still builds without ccache.
if os.path.isfile(CCACHE):
    command.append(CCACHE)
command += [
    COMPILER,
    "-std=c++17",
    "-Og",
    "-g",
    "-shared",
    "-DMIMITA_GAME_DLL",
    "-DGLM_ENABLE_EXPERIMENTAL",
    "-Iinclude",
    "-Isrc",
    f"-I{GLFW_INCLUDE}",
    SOURCE,
    "-o",
    STAGING_DLL,
]

print("[HOT RELOAD] rebuilding DLL")
result = subprocess.run(command, cwd=ROOT)
if result.returncode != 0:
    print("[HOT RELOAD] reload failed")
    sys.exit(result.returncode)

os.replace(STAGING_DLL, OUTPUT_DLL)
print("[HOT RELOAD] DLL build success:", os.path.relpath(OUTPUT_DLL, ROOT))
