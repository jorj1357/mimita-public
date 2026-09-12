# 09 12 2026
# purpose
# Rebuilds the hot-reload game DLL from the hot-modules manifest.
# Default mode keeps the legacy behavior: build build/mimita-game.dll and skip
# when it is already newer than the sources.
# Generation mode is used by the running game's background worker: it compiles
# into a unique generation path, never overwrites the active DLL, hashes every
# source, and writes a machine-readable build-result.json for the runtime.
# Does NOT link mimita.exe, launch the game, or modify game source code.

import hashlib
import json
import os
import subprocess
import sys
import time
from datetime import datetime, timezone

from build_toolchain import compiler, ccache, glfw_include

ROOT = os.path.dirname(os.path.abspath(__file__))
BUILD_DIR = os.path.join(ROOT, "build")
DEFAULT_MANIFEST = os.path.join(ROOT, "src", "hot-reload", "hot-modules.json")
DEFAULT_OUTPUT = os.path.join(BUILD_DIR, "mimita-game.dll")


def parse_args(argv):
    args = {
        "generation": None,
        "output": DEFAULT_OUTPUT,
        "result": None,
        "hot_modules": DEFAULT_MANIFEST,
    }
    index = 0
    while index < len(argv):
        arg = argv[index]
        if arg in ("--generation", "--output", "--result", "--hot-modules"):
            if index + 1 >= len(argv):
                raise SystemExit(f"[HOT RELOAD] {arg} requires a value")
            key = arg[2:].replace("-", "_")
            args[key] = argv[index + 1]
            index += 2
            continue
        index += 1
    return args


def load_manifest(path):
    with open(path, "r", encoding="utf-8") as handle:
        manifest = json.load(handle)
    sources = []
    for module in manifest.get("modules", []):
        for source in module.get("sources", []):
            if source not in sources:
                sources.append(source)
    return sources, list(manifest.get("headers", []))


def sha256_file(path):
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for chunk in iter(lambda: handle.read(65536), b""):
            digest.update(chunk)
    return digest.hexdigest()


def compute_code_hash(files):
    combined = "".join(f"{rel}:{sha256_file(os.path.join(ROOT, rel))}\n" for rel in files)
    return hashlib.sha256(combined.encode("utf-8")).hexdigest()


def utc_now():
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%S.%f")[:-3] + "Z"


def write_result(path, payload):
    if not path:
        return
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    with open(path, "w", encoding="utf-8") as handle:
        json.dump(payload, handle, indent=2)


def main():
    args = parse_args(sys.argv[1:])
    generation = args["generation"]
    output = os.path.abspath(args["output"])
    result_path = args["result"]
    manifest_path = os.path.abspath(args["hot_modules"])

    try:
        COMPILER = compiler()
        CCACHE = ccache(COMPILER)
        GLFW_INCLUDE = glfw_include()
    except FileNotFoundError as error:
        write_result(result_path, {
            "status": "failed",
            "error": f"toolchain: {error}",
            "generation": generation,
            "utc": utc_now(),
        })
        print(f"[TOOLCHAIN] {error}")
        return 2

    try:
        sources, headers = load_manifest(manifest_path)
    except OSError as error:
        write_result(result_path, {
            "status": "failed",
            "error": f"manifest: {error}",
            "generation": generation,
            "utc": utc_now(),
        })
        print(f"[HOT RELOAD] manifest error: {error}")
        return 3

    watched = sorted(set(sources + headers))
    code_hash = compute_code_hash(watched)

    os.makedirs(os.path.dirname(output), exist_ok=True)
    os.environ.setdefault("CCACHE_DIR", os.path.join(BUILD_DIR, "ccache"))
    os.environ.setdefault("CCACHE_SLOPPINESS", "time_macros,file_macro")
    os.environ.setdefault("CCACHE_MAXSIZE", "8G")

    # Legacy default mode: skip when the single active DLL is already newer.
    if generation is None and os.path.exists(output):
        newest_source = max(os.path.getmtime(os.path.join(ROOT, rel)) for rel in watched)
        if newest_source < os.path.getmtime(output):
            print("[HOT RELOAD] DLL up to date, skipping")
            return 0

    command = [CCACHE] if os.path.isfile(CCACHE) else []
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
    ]
    command += [os.path.join(ROOT, source) for source in sources]
    staging = output + ".staging"
    command += ["-o", staging]

    print(f"[HOT RELOAD] rebuilding DLL generation={generation} sources={len(sources)}")
    started = time.time()
    completed = subprocess.run(command, cwd=ROOT, capture_output=True, text=True)
    duration_ms = int((time.time() - started) * 1000)

    if completed.returncode != 0:
        write_result(result_path, {
            "status": "failed",
            "error": (completed.stderr or completed.stdout or "compile failed")[-4000:],
            "generation": generation,
            "code_hash": code_hash,
            "sources": {rel: sha256_file(os.path.join(ROOT, rel)) for rel in watched},
            "duration_ms": duration_ms,
            "utc": utc_now(),
        })
        print(completed.stdout or "")
        print(completed.stderr or "")
        print("[HOT RELOAD] reload failed")
        return completed.returncode or 1

    os.replace(staging, output)
    write_result(result_path, {
        "status": "ok",
        "error": "",
        "generation": generation,
        "code_hash": code_hash,
        "output": output,
        "sources": {rel: sha256_file(os.path.join(ROOT, rel)) for rel in watched},
        "duration_ms": duration_ms,
        "utc": utc_now(),
    })
    print(f"[HOT RELOAD] DLL build success: {os.path.relpath(output, ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
