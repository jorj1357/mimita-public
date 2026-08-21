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
