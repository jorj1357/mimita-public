import os
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.abspath(__file__))
COMPILER = r"C:\important\winlibs-x86_64-posix-seh-gcc-15.2.0-mingw-w64ucrt-13.0.0-r4\mingw64\bin\g++.exe"
GLFW_INCLUDE = r"C:\important\glfw-3.4.bin.WIN64\include"
BUILD_DIR = os.path.join(ROOT, "build")
SOURCE = os.path.join(ROOT, "src", "effects", "effect-part.cpp")
STAGING_DLL = os.path.join(BUILD_DIR, "mimita-game.build.dll")
OUTPUT_DLL = os.path.join(BUILD_DIR, "mimita-game.dll")

os.makedirs(BUILD_DIR, exist_ok=True)

command = [
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
