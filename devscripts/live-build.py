# 09 12 2026
# purpose
# LIVE BUILD entry. Compiles only replaceable hot-module sources into a new,
# immutable generation DLL. It NEVER links or writes mimita.exe, so it can run
# indefinitely while mimita.exe stays open.
# Delegates the actual compilation to build_game_dll.py (the runtime's worker
# uses the same script).
# Does NOT relink the executable, kill a running game, or touch cold-kernel code.
# Does NOT overwrite an existing generation file.

import argparse
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GENERATION_COUNTER = os.path.join(ROOT, "build", "hotreload", "live-generation.txt")
DEFAULT_MANIFEST = os.path.join(ROOT, "src", "hot-reload", "hot-modules.json")


def next_generation():
    os.makedirs(os.path.dirname(GENERATION_COUNTER), exist_ok=True)
    current = 0
    if os.path.isfile(GENERATION_COUNTER):
        try:
            with open(GENERATION_COUNTER, "r", encoding="utf-8") as handle:
                current = int(handle.read().strip() or "0")
        except (OSError, ValueError):
            current = 0
    current += 1
    with open(GENERATION_COUNTER, "w", encoding="utf-8") as handle:
        handle.write(str(current))
    return current


def parse_args(argv):
    parser = argparse.ArgumentParser(description="LIVE BUILD: emit a hot generation without touching mimita.exe")
    parser.add_argument("--generation", type=int, default=None)
    parser.add_argument("--hot-modules", default=DEFAULT_MANIFEST)
    return parser.parse_args(argv)


def main():
    args = parse_args(sys.argv[1:])
    generation = args.generation if args.generation is not None else next_generation()

    output = os.path.join(ROOT, "build", "hotreload", f"mimita-live-g{generation:06d}.dll")
    result = os.path.join(ROOT, "build", "hotreload", f"mimita-live-g{generation:06d}.json")

    print(f"[LIVE BUILD] generation={generation}")
    print(f"[LIVE BUILD] output={os.path.relpath(output, ROOT)}")
    print("[LIVE BUILD] mimita.exe is never written by this path")

    command = [
        sys.executable,
        os.path.join(ROOT, "build_game_dll.py"),
        "--generation", str(generation),
        "--output", output,
        "--result", result,
        "--hot-modules", args.hot_modules,
    ]
    completed = subprocess.run(command, cwd=ROOT, text=True)
    if completed.returncode != 0:
        print("[LIVE BUILD] FAILED: active generation is unchanged")
        return completed.returncode
    print("[LIVE BUILD] candidate ready; load it via the running process or hotreload")
    return 0


if __name__ == "__main__":
    sys.exit(main())
