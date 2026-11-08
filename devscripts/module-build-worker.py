#!/usr/bin/env python3
# 2026-11-07
# purpose
# Standalone background module compiler. Reads config/modules/modules.json,
# snapshots the module's declared sources/headers, and produces an immutable
# generation DLL plus current.json for the running host to validate and load.
# Does NOT link mimita.exe and does NOT require the game process to stop.

import argparse
import ctypes
import glob
import hashlib
import json
import os
import shutil
import subprocess
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT)

from build_toolchain import compiler, ccache, glfw_include, runtime_path  # noqa: E402

# GCC invokes cc1plus from lib/gcc/...; on Windows that helper needs the
# toolchain bin directory on PATH for its own runtime DLLs.
os.environ["PATH"] = runtime_path(compiler()) + os.pathsep + os.environ.get("PATH", "")

CATALOG = os.path.join(ROOT, "config", "modules", "modules.json")
BUILD_ROOT = os.path.join(ROOT, "build", "modules")


def read_json(path):
    with open(path, "r", encoding="utf-8") as handle:
        return json.load(handle)


def write_json_atomic(path, payload):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    temporary = path + ".tmp"
    with open(temporary, "w", encoding="utf-8") as handle:
        json.dump(payload, handle, indent=2)
        handle.write("\n")
    os.replace(temporary, path)


def expand_patterns(patterns):
    files = []
    for pattern in patterns:
        absolute = os.path.join(ROOT, pattern)
        matches = sorted(glob.glob(absolute))
        if not matches and os.path.isfile(absolute):
            matches = [absolute]
        files.extend(matches)
    return files


def expand_include_dirs(directories):
    files = []
    for directory in directories:
        absolute = os.path.abspath(os.path.join(ROOT, directory))
        if os.path.commonpath([ROOT, absolute]) != ROOT or not os.path.isdir(absolute):
            continue
        for current, _, names in os.walk(absolute):
            for name in names:
                path = os.path.join(current, name)
                if os.path.isfile(path):
                    files.append(path)
    return sorted(files)


def previous_dependencies(output_dir):
    path = os.path.join(output_dir, "deps.json")
    if not os.path.isfile(path):
        return []
    try:
        with open(path, "r", encoding="utf-8") as handle:
            values = json.load(handle)
    except (OSError, ValueError):
        return []
    if not isinstance(values, list):
        return []
    result = []
    for value in values:
        if not isinstance(value, str):
            continue
        absolute = os.path.join(ROOT, value)
        if os.path.isfile(absolute):
            result.append(absolute)
    return result


def source_hash(module, sources, headers, dependencies):
    digest = hashlib.sha256()
    digest.update(module["name"].encode("utf-8"))
    digest.update(str(module.get("abi", 1)).encode("utf-8"))
    digest.update(str(module.get("state_schema", 1)).encode("utf-8"))
    unique = sorted(set(sources + headers + dependencies))
    for path in unique:
        relative = os.path.relpath(path, ROOT).replace("\\", "/")
        digest.update(relative.encode("utf-8"))
        with open(path, "rb") as handle:
            for chunk in iter(lambda: handle.read(65536), b""):
                digest.update(chunk)
    return digest.hexdigest()


def parse_depfile(path, snapshot_dir):
    if not os.path.isfile(path):
        return []
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as handle:
            text = handle.read()
    except OSError:
        return []
    text = text.replace("\\\n", " ")
    if ":" not in text:
        return []
    # A Windows drive letter also contains ':'. GCC separates the target from
    # prerequisites with ': ', including when the target has a drive prefix.
    separator = text.find(": ")
    if separator < 0:
        return []
    dependencies = text[separator + 2:].split()
    result = []
    seen = set()
    for dependency in dependencies:
        normalized = os.path.normpath(dependency.replace("\\\\", "\\"))
        original = None
        if normalized.startswith(snapshot_dir):
            relative = os.path.relpath(normalized, snapshot_dir)
            original = os.path.join(ROOT, relative)
        elif os.path.isabs(normalized) and normalized.startswith(ROOT):
            original = normalized
        if not original or not os.path.isfile(original):
            continue
        relative = os.path.relpath(original, ROOT).replace("\\", "/")
        if relative not in seen:
            seen.add(relative)
            result.append(relative)
    return sorted(result)


def object_key(source, dependencies, command, generation):
    digest = hashlib.sha256()
    digest.update("\0".join(command).encode("utf-8"))
    for path in sorted(set([source] + dependencies)):
        digest.update(os.path.relpath(path, ROOT).encode("utf-8"))
        with open(path, "rb") as handle:
            for chunk in iter(lambda: handle.read(65536), b""):
                digest.update(chunk)
    # Only the entry translation unit includes the generated identity header.
    with open(source, "rb") as handle:
        if b"module-identity.h" in handle.read():
            digest.update(str(generation).encode("ascii"))
    return digest.hexdigest()


def next_generation(output_dir, code_hash):
    current_path = os.path.join(output_dir, "current.json")
    if not os.path.isfile(current_path):
        return 1
    try:
        current = read_json(current_path)
    except (OSError, ValueError):
        return 1
    if current.get("code_hash") == code_hash and os.path.isfile(current.get("dll", "")):
        return 0
    return int(current.get("generation", 0)) + 1


def acquire_lock(output_dir):
    os.makedirs(output_dir, exist_ok=True)
    lock_path = os.path.join(output_dir, ".build.lock")
    try:
        if os.path.isfile(lock_path):
            try:
                with open(lock_path, "r", encoding="ascii") as handle:
                    owner_pid = int(handle.read().strip())
                process = ctypes.windll.kernel32.OpenProcess(0x00100000, False, owner_pid)
                if process:
                    running = ctypes.windll.kernel32.WaitForSingleObject(process, 0) == 258
                    ctypes.windll.kernel32.CloseHandle(process)
                else:
                    # Access denied is not proof the owner exited.
                    running = ctypes.windll.kernel32.GetLastError() != 87
                if not running:
                    os.remove(lock_path)
            except (OSError, ValueError):
                # An unreadable lock is retained; guessing that a live build
                # is stale can allow two writers to publish one generation.
                pass
        descriptor = os.open(lock_path, os.O_CREAT | os.O_EXCL | os.O_WRONLY)
        os.write(descriptor, str(os.getpid()).encode("ascii"))
        return descriptor, lock_path
    except FileExistsError:
        return None, lock_path


def release_lock(descriptor, lock_path):
    if descriptor is not None:
        try:
            os.close(descriptor)
        except OSError:
            pass
    try:
        os.remove(lock_path)
    except OSError:
        pass


def build_module(module):
    name = module["name"]
    output_dir = os.path.join(BUILD_ROOT, name)
    sources = expand_patterns(module.get("sources", []))
    headers = expand_patterns(module.get("headers", []))
    include_dirs = module.get("include_dirs", [])
    include_files = expand_include_dirs(include_dirs) if isinstance(include_dirs, list) else []
    dependencies = previous_dependencies(output_dir)
    inputs = list(dict.fromkeys(sources + headers + include_files + dependencies))
    missing = [path for path in inputs if not os.path.isfile(path)]
    if missing:
        return {"status": "failed", "module": name,
                "error": "missing module input: " + ", ".join(missing)}
    if not sources:
        return {"status": "failed", "module": name, "error": "no module sources"}

    code_hash = source_hash(module, sources, headers, dependencies)
    generation = next_generation(output_dir, code_hash)
    if generation == 0:
        return {"status": "skipped", "module": name, "generation": 0,
                "code_hash": code_hash, "reason": "source hash unchanged"}

    descriptor, lock_path = acquire_lock(output_dir)
    if descriptor is None:
        return {"status": "skipped", "module": name, "reason": "build already running"}
    try:
        snapshot_dir = os.path.join(output_dir, "snapshots", code_hash[:16])
        if os.path.isdir(snapshot_dir):
            shutil.rmtree(snapshot_dir)
        os.makedirs(snapshot_dir, exist_ok=True)

        snapshot_sources = []
        for path in inputs:
            relative = os.path.relpath(path, ROOT)
            target = os.path.join(snapshot_dir, relative)
            os.makedirs(os.path.dirname(target), exist_ok=True)
            shutil.copy2(path, target)
        for path in sources:
            relative = os.path.relpath(path, ROOT)
            snapshot_sources.append(os.path.join(snapshot_dir, relative))

        identity_path = os.path.join(snapshot_dir, "module-identity.h")
        with open(identity_path, "w", encoding="utf-8") as handle:
            handle.write("#pragma once\n")
            handle.write(f"#define MIMITA_MODULE_GENERATION {generation}u\n")
            handle.write(f'#define MIMITA_MODULE_SOURCE_HASH "{code_hash}"\n')
            handle.write(
                f"#define MIMITA_MODULE_STATE_SCHEMA "
                f"{int(module.get('state_schema', 1))}u\n")

        generation_dir = os.path.join(output_dir, f"generation-{generation}")
        os.makedirs(generation_dir, exist_ok=True)
        staging = os.path.join(generation_dir, f"{name}-g{generation}.build.dll")
        final = os.path.join(generation_dir, f"{name}-g{generation}.dll")

        command = []
        ccache_path = ccache(compiler())
        if os.path.isfile(ccache_path):
            command.append(ccache_path)
        command += [
            compiler(),
            "-std=c++17",
            "-O1",
            "-g",
            "-DMIMITA_MODULE_BUILD",
            f"-I{snapshot_dir}",
            f"-I{os.path.join(snapshot_dir, 'src')}",
        ]
        if isinstance(include_dirs, list):
            for directory in include_dirs:
                if isinstance(directory, str) and directory:
                    include_root = os.path.dirname(directory.rstrip("/\\"))
                    command.append(f"-I{os.path.join(snapshot_dir, include_root)}")
        compile_defines = module.get("compile_defines", [])
        if isinstance(compile_defines, list):
            for define in compile_defines:
                if isinstance(define, str) and define and define.replace("_", "").isalnum():
                    command.append(f"-D{define}")
        try:
            command.append(f"-I{glfw_include()}")
        except FileNotFoundError:
            pass
        build_env = os.environ.copy()
        build_env["PATH"] = runtime_path(compiler()) + os.pathsep + build_env.get("PATH", "")
        objects = []
        dependency_files = set()
        compiled = 0
        for source, snapshot_source in zip(sources, snapshot_sources):
            relative = os.path.relpath(source, ROOT).replace("\\", "/")
            object_dir = os.path.join(output_dir, "objects", hashlib.sha256(
                relative.encode("utf-8")).hexdigest()[:16])
            os.makedirs(object_dir, exist_ok=True)
            cached_object = os.path.join(object_dir, "module.o")
            cache_path = os.path.join(object_dir, "cache.json")
            cached = read_json(cache_path) if os.path.isfile(cache_path) else {}
            cached_deps = cached.get("dependencies", [])
            dep_paths = [os.path.join(ROOT, path) for path in cached_deps]
            if any(not os.path.isfile(path) for path in dep_paths):
                dep_paths = []
                cached = {}
            cache_command = [part.replace(snapshot_dir, "<snapshot>")
                             for part in command]
            key = object_key(source, dep_paths, cache_command, generation)
            obj = os.path.join(generation_dir, os.path.basename(source) + ".o")
            if cached.get("key") == key and os.path.isfile(cached_object):
                shutil.copy2(cached_object, obj)
                dependency_files.update(cached_deps)
            else:
                depfile = obj + ".d"
                compile_command = command + ["-c", "-MMD", "-MF", depfile,
                                             snapshot_source, "-o", obj]
                result = subprocess.run(compile_command, cwd=ROOT,
                                        capture_output=True, text=True, env=build_env)
                if result.returncode != 0:
                    return {"status": "failed", "module": name,
                            "generation": generation, "code_hash": code_hash,
                            "translation_unit": relative,
                            "exit_code": result.returncode,
                            "compiler_output": ((result.stdout or "") +
                                                (result.stderr or ""))[-12000:]}
                deps = parse_depfile(depfile, snapshot_dir)
                dependency_files.update(deps)
                shutil.copy2(obj, cached_object)
                write_json_atomic(cache_path, {
                    "key": object_key(source, [os.path.join(ROOT, path)
                                               for path in deps], cache_command, generation),
                    "dependencies": deps,
                })
                compiled += 1
            objects.append(obj)

        link_command = [compiler(), "-shared"] + objects + ["-o", staging]
        result = subprocess.run(link_command, cwd=ROOT, capture_output=True,
                                text=True, env=build_env)
        if result.returncode != 0:
            return {"status": "failed", "module": name,
                    "generation": generation, "code_hash": code_hash,
                    "phase": "link", "exit_code": result.returncode,
                    "compiler_output": ((result.stdout or "") +
                                        (result.stderr or ""))[-12000:]}

        os.replace(staging, final)
        dependency_files = sorted(dependency_files)
        if dependency_files:
            write_json_atomic(os.path.join(output_dir, "deps.json"),
                              dependency_files)
        current = {
            "status": "ok",
            "module": name,
            "generation": generation,
            "code_hash": code_hash,
            "dll": final,
            "dependencies": dependency_files,
            "built_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        }
        write_json_atomic(os.path.join(output_dir, "current.json"), current)
        return {
            "status": "ok",
            "module": name,
            "generation": generation,
            "code_hash": code_hash,
            "dll": final,
            "dependency_count": len(dependency_files),
            "compiled_translation_units": compiled,
            "reused_translation_units": len(objects) - compiled,
        }
    finally:
        release_lock(descriptor, lock_path)


def audit_modules(modules):
    owners = {}
    event_owners = {}
    capability_owners = {}
    ok = True
    for module in modules:
        name = module.get("name", "")
        sources = expand_patterns(module.get("sources", []))
        headers = expand_patterns(module.get("headers", []))
        print(f"[MODULE AUDIT] {name}: sources={len(sources)} headers={len(headers)} "
              f"events={module.get('events', [])} "
              f"capabilities={module.get('capabilities', [])}")
        for path in sources:
            relative = os.path.relpath(path, ROOT).replace("\\", "/")
            previous = owners.get(relative)
            if previous and previous != name:
                print(f"[MODULE AUDIT] SOURCE OVERLAP {relative}: {previous} and {name}")
                ok = False
            owners[relative] = name
        for path in headers:
            relative = os.path.relpath(path, ROOT).replace("\\", "/")
            owners.setdefault(relative, name)
        for event in module.get("events", []):
            previous = event_owners.get(event)
            if previous and previous != name:
                print(f"[MODULE AUDIT] EVENT OVERLAP {event}: {previous} and {name}")
                ok = False
            event_owners[event] = name
        for capability in module.get("capabilities", []):
            capability_owners.setdefault(capability, []).append(name)
    if not ok:
        print("[MODULE AUDIT] FAIL")
        return 1
    print(f"[MODULE AUDIT] PASS files={len(owners)} events={len(event_owners)}")
    return 0


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--module", action="append", default=[])
    parser.add_argument("--all", action="store_true")
    parser.add_argument("--audit", action="store_true")
    arguments = parser.parse_args()

    catalog = read_json(CATALOG)
    modules = catalog.get("modules", [])
    if arguments.audit:
        return audit_modules(modules)
    selected = [m for m in modules if arguments.all or m.get("name") in arguments.module]
    if not selected:
        print("[MODULE WORKER] no matching modules")
        return 2

    exit_code = 0
    for module in selected:
        result = build_module(module)
        result_path = os.path.join(BUILD_ROOT, module["name"], "result.json")
        write_json_atomic(result_path, result)
        print(f"[MODULE WORKER] {module['name']}: {result.get('status')}")
        if result.get("status") == "failed":
            if result.get("compiler_output"):
                print(result["compiler_output"])
            exit_code = 1
    return exit_code


if __name__ == "__main__":
    sys.exit(main())
