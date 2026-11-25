"""Whole-game DLL build worker invoked only by the root dev-loop."""

from __future__ import annotations

import hashlib
import json
import os
import queue
import shutil
import subprocess
import sys
import threading
import time
import uuid
from datetime import datetime, timezone
from pathlib import Path

from devscripts.build_scheduler import acquire as acquire_build_slot
from devscripts.dev_loop.build_resource_sampler import BuildResourceSampler
from devscripts.source_manifest import build_manifest, diff_manifests, validate


_TIMING_APPEND_LOCK = threading.Lock()
_SNAPSHOT_CLEANUP_QUEUE: queue.Queue[tuple[Path, str, str, Path]] = queue.Queue(
    maxsize=64)
_SNAPSHOT_CLEANUP_LOCK = threading.Lock()
_SNAPSHOT_CLEANUP_PENDING: set[str] = set()
_SNAPSHOT_CLEANUP_THREAD: threading.Thread | None = None


class _BuildAttemptDeadlineExceeded(RuntimeError):
    """Raised when a pre-worker phase reaches the whole-attempt deadline."""


def _utc_now() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="milliseconds").replace("+00:00", "Z")


def _read_json(path: Path) -> dict:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return {}
    return value if isinstance(value, dict) else {}


def _write_json(path: Path, payload: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(
        f"{path.name}.{os.getpid()}.{threading.get_ident()}.tmp")
    temporary.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n",
                          encoding="utf-8")
    for attempt in range(20):
        try:
            os.replace(temporary, path)
            return
        except PermissionError:
            if attempt == 19:
                raise
            time.sleep(0.05)


def _append_timing(path: Path, event: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with _TIMING_APPEND_LOCK:
        with path.open("a", encoding="utf-8", newline="\n") as output:
            output.write(json.dumps(event, separators=(",", ":")) + "\n")
            output.flush()


def _sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for block in iter(lambda: handle.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


_CODEGEN_CACHE_SCHEMA = 1
_CODEGEN_VALIDATION_FILES = {
    "resident_state_codegen.py": (
        "devscripts/resident_state_codegen.py",
        "config/hot-reload/state-schema.json",
        "src/host/generated/resident-state-v1.h",
        "config/hot-reload/state-ledger.json",
    ),
    "packet_registry_codegen.py": (
        "devscripts/packet_registry_codegen.py",
        "build/source-manifest.json",
        "src/network/packets.h",
        "src/network/schema/packet-header.h",
        "src/host/generated/packet-registry-v1.h",
        "config/hot-reload/packet-ledger.json",
    ),
}


def _codegen_validation_fingerprint(root: Path, script: str,
                                    manifest_revision: str) -> str:
    """Key a successful --check by every direct input and checked output.

    The packet registry also consumes the complete source manifest because it
    scans source references; its revision invalidates that derived result when
    any C++ source or header changes.
    """
    paths = _CODEGEN_VALIDATION_FILES[script]
    try:
        files = [(relative, _sha256_file(root / relative))
                 for relative in paths]
    except OSError:
        return ""
    payload = {
        "schema": _CODEGEN_CACHE_SCHEMA,
        "script": script,
        "manifest_revision": (manifest_revision
                              if script == "packet_registry_codegen.py" else ""),
        "files": files,
    }
    return hashlib.sha256(json.dumps(
        payload, sort_keys=True, separators=(",", ":")
    ).encode("utf-8")).hexdigest()


def _mirror_dir(root: Path) -> Path:
    return root / ".dev" / "build-source"


def _mirror_manifest_path(mirror: Path) -> Path:
    return mirror / ".mirror-manifest.json"


def _mirror_entries(manifest: dict) -> list[dict]:
    entries = [item for item in manifest.get("sources", [])
               if item.get("owner") == "game"]
    entries.extend(manifest.get("headers", []))
    return entries


def _verify_mirror(mirror: Path, entries: list[dict],
                   limit: int = 20) -> list[str]:
    mismatches: list[str] = []
    for item in entries:
        rel = str(item.get("path") or "")
        expected = str(item.get("sha256") or "")
        if not rel or not expected:
            continue
        if not rel.startswith(("src/", "include/", "external/")):
            continue
        try:
            actual = _sha256_file(mirror / rel)
        except OSError:
            mismatches.append(f"missing {rel}")
        else:
            if actual != expected:
                mismatches.append(f"changed {rel}")
        if len(mismatches) >= limit:
            break
    return mismatches


def _copy_into_mirror(root: Path, mirror: Path, rels: list[str],
                      mismatches: list[str]) -> None:
    for rel in rels:
        if not rel.startswith(("src/", "include/", "external/")):
            continue
        source = root / rel
        destination = mirror / rel
        temporary = destination.with_name(
            f"{destination.name}.{os.getpid()}.snapshot.tmp")
        try:
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, temporary)
            for attempt in range(40):
                try:
                    os.replace(temporary, destination)
                    break
                except PermissionError:
                    if attempt == 39:
                        raise
                    time.sleep(0.05)
        except OSError:
            try:
                temporary.unlink()
            except OSError:
                pass
            mismatches.append(f"copy failed {rel}")


def _populate_source_mirror(root: Path, mirror: Path,
                            manifest: dict) -> list[str]:
    shutil.rmtree(mirror, ignore_errors=True)
    mirror.mkdir(parents=True, exist_ok=True)
    for name in ("src", "include", "external"):
        source = root / name
        if source.is_dir():
            shutil.copytree(source, mirror / name,
                            ignore=shutil.ignore_patterns(".git", "*.gch"))
    return _verify_mirror(mirror, _mirror_entries(manifest))


def _update_source_mirror(root: Path, mirror: Path,
                          manifest: dict) -> list[str]:
    """Update the persistent build-source mirror with only changed files.

    Returns a list of mismatches. An empty list proves the mirror matches the
    manifest, so a save during the compile cannot mix source versions.
    """
    force = os.environ.get("MIMITA_REBUILD_SOURCE_MIRROR") == "1"
    reference = _read_json(_mirror_manifest_path(mirror))
    if (force or not reference or not (mirror / "src").is_dir()
            or not (mirror / "include").is_dir()):
        mismatches = _populate_source_mirror(root, mirror, manifest)
    else:
        change = diff_manifests(reference, manifest)
        copy: set[str] = set(change.get("changed", []))
        copy.update(change.get("added", []))
        copy.update(change.get("changed_headers", []))
        copy.update(change.get("added_headers", []))
        remove: set[str] = set(change.get("removed", []))
        remove.update(change.get("removed_headers", []))
        for item in change.get("renamed", []):
            copy.add(str(item.get("to", "")))
            remove.add(str(item.get("from", "")))
        copy.discard("")
        remove.discard("")
        mismatches: list[str] = []
        _copy_into_mirror(root, mirror, sorted(copy), mismatches)
        for rel in sorted(remove):
            if not rel.startswith(("src/", "include/", "external/")):
                continue
            try:
                (mirror / rel).unlink()
            except OSError:
                pass
        by_path = {str(item.get("path")): item
                   for item in _mirror_entries(manifest)}
        verify = [by_path[rel] for rel in copy if rel in by_path]
        mismatches.extend(_verify_mirror(mirror, verify))
    if not mismatches:
        _write_json(_mirror_manifest_path(mirror), manifest)
    return mismatches


def _looks_like_date(name: str) -> bool:
    parts = name.split("-")
    return (len(parts) == 3 and len(parts[0]) == 4 and
            all(part.isdigit() for part in parts))


def _disk_free_bytes(root: Path) -> int | None:
    try:
        return shutil.disk_usage(root).free
    except OSError:
        return None


def _prune_generations(root: Path, keep: int = 5) -> int:
    game = root / "build" / "game"
    dirs: list[tuple[int, Path]] = []
    for path in game.glob("generation-*"):
        try:
            number = int(path.name.split("-")[-1])
        except ValueError:
            continue
        if path.is_dir():
            dirs.append((number, path))
    if not dirs:
        return 0
    dirs.sort(reverse=True)
    protected = {number for number, _ in dirs[:max(1, keep)]}
    for name in ("ready.json", "current.json"):
        try:
            protected.add(int(_read_json(game / name).get("generation")))
        except (TypeError, ValueError):
            pass
    removed = 0
    for number, path in dirs:
        if number in protected:
            continue
        try:
            shutil.rmtree(path)
            removed += 1
        except OSError:
            pass
    return removed


def _snapshot_root(root: Path, attempt_id: str) -> Path:
    return root / ".dev" / "build-snapshots" / attempt_id / "source"


def _create_source_snapshot(root: Path, mirror: Path, attempt_id: str,
                            check_deadline=None) -> Path:
    """Create an immutable per-attempt tree by hardlinking the source mirror.

    The mirror itself is updated with temp-file replacement, so an existing
    snapshot keeps the old inode when the next build refreshes a file.
    """
    destination_root = _snapshot_root(root, attempt_id)
    remove_started = time.perf_counter()
    shutil.rmtree(destination_root, ignore_errors=True)
    remove_seconds = time.perf_counter() - remove_started
    linked = copied = 0
    file_count = directory_count = 0
    directory_seconds = link_seconds = copy_seconds = 0.0
    walk_started = time.perf_counter()

    def check_snapshot_deadline() -> None:
        if not check_deadline:
            return
        try:
            check_deadline("source_snapshot")
        except _BuildAttemptDeadlineExceeded:
            shutil.rmtree(destination_root, ignore_errors=True)
            raise

    for source_root_name in ("src", "include", "external"):
        source_root = mirror / source_root_name
        if not source_root.is_dir():
            continue
        for current, _dirs, filenames in os.walk(source_root):
            source_directory = Path(current)
            relative_directory = source_directory.relative_to(mirror)
            destination_directory = destination_root / relative_directory
            directory_started = time.perf_counter()
            destination_directory.mkdir(parents=True, exist_ok=True)
            directory_seconds += time.perf_counter() - directory_started
            directory_count += 1
            for filename in filenames:
                source = source_directory / filename
                destination = destination_directory / filename
                file_count += 1
                if check_deadline and file_count % 64 == 0:
                    check_snapshot_deadline()
                link_started = time.perf_counter()
                try:
                    os.link(source, destination)
                    link_seconds += time.perf_counter() - link_started
                    linked += 1
                except OSError:
                    link_seconds += time.perf_counter() - link_started
                    copy_started = time.perf_counter()
                    shutil.copy2(source, destination)
                    copy_seconds += time.perf_counter() - copy_started
                    copied += 1
    walk_seconds = max(
        0.0,
        time.perf_counter() - walk_started
        - directory_seconds - link_seconds - copy_seconds,
    )
    (destination_root / ".snapshot.json").write_text(json.dumps({
        "attempt_id": attempt_id,
        "source_mirror": str(mirror.relative_to(root)),
        "linked": linked,
        "copied": copied,
        "files": file_count,
        "directories": directory_count,
        "remove_seconds": round(remove_seconds, 6),
        "directory_prepare_seconds": round(directory_seconds, 6),
        "hardlink_seconds": round(link_seconds, 6),
        "copy_seconds": round(copy_seconds, 6),
        "walk_and_path_seconds": round(walk_seconds, 6),
        "created_utc": _utc_now(),
    }, indent=2) + "\n", encoding="utf-8")
    return destination_root


def _is_directory_junction(path: Path) -> bool:
    checker = getattr(os.path, "isjunction", None)
    if checker is not None:
        try:
            return bool(checker(path))
        except OSError:
            return False
    return path.is_symlink()


def _create_directory_junction(link: Path, target: Path) -> None:
    """Create an NTFS directory junction without spawning a shell process."""
    if os.name != "nt":
        raise OSError("directory junction snapshots require Windows")
    import ctypes
    import struct

    link.mkdir()
    target_path = str(target.resolve(strict=True))
    substitute_name = "\\??\\" + target_path
    substitute = substitute_name.encode("utf-16-le")
    printed = target_path.encode("utf-16-le")
    path_buffer = substitute + b"\0\0" + printed + b"\0\0"
    payload = struct.pack("<HHHH", 0, len(substitute),
                          len(substitute) + 2, len(printed)) + path_buffer
    record = struct.pack("<IHH", 0xA0000003, len(payload), 0) + payload

    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    create_file = kernel.CreateFileW
    create_file.argtypes = [ctypes.c_wchar_p, ctypes.c_uint32, ctypes.c_uint32,
                            ctypes.c_void_p, ctypes.c_uint32, ctypes.c_uint32,
                            ctypes.c_void_p]
    create_file.restype = ctypes.c_void_p
    device_io = kernel.DeviceIoControl
    device_io.argtypes = [ctypes.c_void_p, ctypes.c_uint32, ctypes.c_void_p,
                          ctypes.c_uint32, ctypes.c_void_p, ctypes.c_uint32,
                          ctypes.POINTER(ctypes.c_uint32), ctypes.c_void_p]
    device_io.restype = ctypes.c_int
    close_handle = kernel.CloseHandle
    close_handle.argtypes = [ctypes.c_void_p]
    invalid_handle = ctypes.c_void_p(-1).value
    handle = create_file(
        str(link), 0x40000000, 0x7, None, 3,
        0x02000000 | 0x00200000, None)
    if handle == invalid_handle:
        raise ctypes.WinError(ctypes.get_last_error())
    returned = ctypes.c_uint32()
    buffer = ctypes.create_string_buffer(record)
    try:
        if not device_io(handle, 0x000900A4, buffer, len(record), None, 0,
                         ctypes.byref(returned), None):
            raise ctypes.WinError(ctypes.get_last_error())
    except Exception:
        # Remove the empty reparse directory if the mount point could not be set.
        try:
            os.rmdir(link)
        except OSError:
            pass
        raise
    finally:
        close_handle(handle)


def _remove_snapshot_tree(path: Path) -> None:
    """Remove junction entries first so recursive cleanup never follows them."""
    if not path.exists():
        return
    for current, directories, _files in os.walk(path, topdown=True,
                                                 followlinks=False):
        current_path = Path(current)
        for name in list(directories):
            candidate = current_path / name
            if _is_directory_junction(candidate):
                os.rmdir(candidate)
                directories.remove(name)
    shutil.rmtree(path, ignore_errors=False)


def _create_junction_overlay_snapshot(
        root: Path, base: Path, mirror: Path, replacement_paths: list[str],
        removed_paths: list[str], attempt_id: str,
        expected_hashes: dict[str, str] | None = None,
        check_deadline=None) -> Path:
    """Share unchanged directory subtrees and materialize only edited branches."""
    if os.name != "nt":
        raise OSError("directory junction snapshots require Windows")
    snapshots_root = (root / ".dev" / "build-snapshots").resolve()
    base_resolved = base.resolve(strict=True)
    base_resolved.relative_to(snapshots_root)
    destination_root = _snapshot_root(root, attempt_id)
    if destination_root.exists():
        _remove_snapshot_tree(destination_root)

    def normalized(value: str) -> str:
        path = Path(str(value).replace("\\", "/"))
        if path.is_absolute() or ".." in path.parts:
            raise RuntimeError(f"snapshot overlay path escapes repository: {value}")
        path = Path(*[part for part in path.parts if part not in ("", ".")])
        if not path.parts or path.parts[0] not in ("src", "include", "external"):
            raise RuntimeError(f"unsupported snapshot overlay path: {value}")
        return path.as_posix()

    replacements = {normalized(path) for path in replacement_paths}
    removals = {normalized(path) for path in removed_paths}
    overlays = replacements | removals
    if not overlays:
        raise RuntimeError("junction snapshot requires changed paths")
    if any(not (base_resolved / path.split("/")[0]).is_dir()
           for path in overlays):
        raise RuntimeError("base snapshot is missing a source root")

    linked = copied = removed = 0
    junction_seconds = hardlink_seconds = copy_seconds = walk_seconds = 0.0
    junction_targets: set[str] = set()
    started = time.perf_counter()

    def changed_below(relative: str) -> bool:
        prefix = relative.rstrip("/") + "/"
        return any(path == relative or path.startswith(prefix)
                   for path in overlays)

    def materialize(relative: str) -> None:
        nonlocal linked, copied, removed
        nonlocal junction_seconds, hardlink_seconds, walk_seconds
        if check_deadline:
            check_deadline("source_snapshot")
        source_directory = base_resolved / relative
        destination_directory = destination_root / relative
        if not changed_below(relative):
            junction_started = time.perf_counter()
            target = source_directory.resolve(strict=True)
            _create_directory_junction(destination_directory, target)
            junction_seconds += time.perf_counter() - junction_started
            try:
                target_relative = target.relative_to(root.resolve()).as_posix()
                if target_relative.startswith(".dev/build-snapshots/"):
                    junction_targets.add(target_relative)
            except ValueError:
                pass
            return

        destination_directory.mkdir(parents=True, exist_ok=True)
        scan_started = time.perf_counter()
        if source_directory.is_dir():
            for item in source_directory.iterdir():
                child = f"{relative}/{item.name}"
                if item.is_dir():
                    materialize(child)
                    continue
                if not item.is_file():
                    continue
                if child in removals or child in replacements:
                    if child in removals:
                        removed += 1
                    continue
                target_file = destination_directory / item.name
                link_started = time.perf_counter()
                try:
                    os.link(item, target_file)
                    linked += 1
                except OSError:
                    link_failed = time.perf_counter() - link_started
                    hardlink_seconds += link_failed
                    copy_started = time.perf_counter()
                    shutil.copy2(item, target_file)
                    copy_seconds += time.perf_counter() - copy_started
                    continue
                hardlink_seconds += time.perf_counter() - link_started
        walk_seconds += time.perf_counter() - scan_started

    try:
        for source_root in ("src", "include", "external"):
            if (base_resolved / source_root).is_dir():
                materialize(source_root)
        expected_hashes = expected_hashes or {}
        for relative in sorted(replacements):
            if check_deadline:
                check_deadline("source_snapshot")
            source = mirror / Path(relative)
            if not source.is_file():
                raise RuntimeError(f"overlay source missing: {relative}")
            destination = destination_root / Path(relative)
            destination.parent.mkdir(parents=True, exist_ok=True)
            temporary = destination.with_name(destination.name + ".overlay.tmp")
            shutil.copy2(source, temporary)
            expected = expected_hashes.get(relative)
            if expected and _sha256_file(temporary) != expected:
                raise RuntimeError(f"overlay source changed during snapshot: {relative}")
            os.replace(temporary, destination)
            copied += 1

        marker = {
            "attempt_id": attempt_id,
            "strategy": "junction_overlay",
            "base_snapshot": str(base.relative_to(root)),
            "replacement_paths": sorted(replacements),
            "removed_paths": sorted(removals),
            "junction_targets": sorted(junction_targets),
            "linked": linked,
            "copied": copied,
            "removed": removed,
            "junction_seconds": round(junction_seconds, 6),
            "hardlink_seconds": round(hardlink_seconds, 6),
            "copy_seconds": round(copy_seconds, 6),
            "walk_seconds": round(walk_seconds, 6),
            "total_seconds": round(time.perf_counter() - started, 6),
            "created_utc": _utc_now(),
        }
        (destination_root / ".snapshot.json").write_text(
            json.dumps(marker, indent=2) + "\n", encoding="utf-8")
    except Exception:
        if destination_root.exists():
            try:
                _remove_snapshot_tree(destination_root)
            except OSError:
                pass
        try:
            destination_root.parent.rmdir()
        except OSError:
            pass
        raise
    return destination_root


def _base_snapshot_for_generation(root: Path, generation: int) -> Path:
    path = (root / "build" / "game" / f"generation-{int(generation)}"
            / "generation.json")
    data = _read_json(path)
    value = str(data.get("source_root") or "")
    if value:
        candidate = Path(value)
        if not candidate.is_absolute():
            candidate = root / candidate
        if candidate.is_dir():
            return candidate
    raise RuntimeError(f"base snapshot for generation {generation} is missing")


def _reusable_published_snapshot(root: Path, revision: str) -> tuple[int, Path] | None:
    """Return the immutable snapshot for an already-published identical revision."""
    current = _read_json(root / "build" / "game" / "current.json")
    if (str(current.get("status") or "") != "ok"
            or str(current.get("revision") or "") != revision):
        return None
    try:
        generation = int(current.get("generation") or 0)
        metadata_path = (root / "build" / "game" / f"generation-{generation}"
                         / "generation.json")
        metadata = _read_json(metadata_path)
        if (generation <= 0 or str(metadata.get("revision") or "") != revision):
            return None
        snapshot = _base_snapshot_for_generation(root, generation).resolve()
        snapshot.relative_to((root / ".dev" / "build-snapshots").resolve())
        marker = _read_json(snapshot / ".snapshot.json")
        if (not marker or str(marker.get("attempt_id") or "")
                != snapshot.parent.name):
            return None
        return generation, snapshot
    except (OSError, RuntimeError, TypeError, ValueError):
        return None


def _snapshot_manifest(snapshot: Path, revision: str = "") -> dict | None:
    manifest = _read_json(snapshot.parent / "source-manifest.json")
    if (not isinstance(manifest, dict)
            or not isinstance(manifest.get("sources"), list)
            or not isinstance(manifest.get("headers"), list)
            or not manifest.get("revision")):
        return None
    if revision and str(manifest.get("revision")) != revision:
        return None
    marker = _read_json(snapshot / ".snapshot.json")
    if (not isinstance(marker, dict)
            or str(marker.get("attempt_id") or "")
            != snapshot.parent.name):
        return None
    marker_revision = str(marker.get("source_revision") or "")
    if marker_revision and marker_revision != str(manifest.get("revision")):
        return None
    return manifest


def _snapshot_for_revision(root: Path, revision: str) -> Path | None:
    """Find an immutable source tree matching a prior manifest revision.

    Prefer the currently published snapshot. If a newer candidate failed after
    its source snapshot was captured, its manifest sidecar still makes that
    immutable snapshot a safe base for the next incremental candidate.
    """
    if not revision:
        return None
    published = _reusable_published_snapshot(root, revision)
    if published is not None:
        return published[1]
    snapshots = root / ".dev" / "build-snapshots"
    try:
        entries = sorted((item for item in snapshots.iterdir()
                          if item.is_dir()),
                         key=lambda item: item.stat().st_mtime,
                         reverse=True)
    except OSError:
        return None
    for attempt in entries:
        source = attempt / "source"
        if source.is_dir() and _snapshot_manifest(source, revision) is not None:
            return source
    return None


def _write_snapshot_manifest(snapshot: Path, manifest: dict) -> None:
    """Persist the exact source inventory beside its immutable source tree."""
    revision = str(manifest.get("revision") or "")
    if not revision:
        raise RuntimeError("source manifest has no revision")
    payload = {
        "schema": 1,
        "revision": revision,
        "sources": manifest.get("sources", []),
        "headers": manifest.get("headers", []),
    }
    destination = snapshot.parent / "source-manifest.json"
    temporary = destination.with_name(destination.name + ".tmp")
    temporary.write_text(json.dumps(payload, separators=(",", ":")) + "\n",
                         encoding="utf-8")
    os.replace(temporary, destination)
    marker_path = snapshot / ".snapshot.json"
    marker = _read_json(marker_path)
    if isinstance(marker, dict) and marker:
        marker["source_revision"] = revision
        marker_path.write_text(json.dumps(marker, indent=2) + "\n",
                               encoding="utf-8")


def _create_overlay_snapshot(root: Path, base: Path, overlay_paths: list[str],
                             attempt_id: str, check_deadline=None) -> Path:
    destination_root = _snapshot_root(root, attempt_id)
    if not base.is_dir():
        raise RuntimeError(f"base snapshot missing: {base}")
    shutil.rmtree(destination_root, ignore_errors=True)
    linked = copied = 0

    def check_snapshot_deadline() -> None:
        if not check_deadline:
            return
        try:
            check_deadline("source_snapshot")
        except _BuildAttemptDeadlineExceeded:
            shutil.rmtree(destination_root, ignore_errors=True)
            raise

    for source in base.rglob("*"):
        if not source.is_file():
            continue
        if check_deadline and linked % 64 == 0:
            check_snapshot_deadline()
        relative = source.relative_to(base)
        destination = destination_root / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        try:
            os.link(source, destination)
            linked += 1
        except OSError:
            shutil.copy2(source, destination)
            copied += 1
    root_resolved = root.resolve()
    for value in overlay_paths:
        check_snapshot_deadline()
        relative = Path(str(value))
        if relative.is_absolute():
            try:
                relative = relative.resolve().relative_to(root_resolved)
            except ValueError as error:
                raise RuntimeError(f"overlay path escapes repository: {value}") from error
        else:
            relative = Path(str(relative).replace("\\", "/"))
        source = root / relative
        if not source.is_file():
            raise RuntimeError(f"overlay file missing: {relative}")
        destination = destination_root / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        temporary = destination.with_name(destination.name + ".overlay.tmp")
        shutil.copy2(source, temporary)
        os.replace(temporary, destination)
        copied += 1
    (destination_root / ".snapshot.json").write_text(json.dumps({
        "attempt_id": attempt_id,
        "base_snapshot": str(base.relative_to(root)),
        "overlay_paths": overlay_paths,
        "linked": linked,
        "copied": copied,
        "created_utc": _utc_now(),
    }, indent=2) + "\n", encoding="utf-8")
    return destination_root


def _snapshot_cleanup_worker() -> None:
    while True:
        path, original_name, attempt_id, timing_path = _SNAPSHOT_CLEANUP_QUEUE.get()
        started = time.monotonic()
        error = ""
        try:
            try:
                shutil.rmtree(path)
            except Exception as exc:  # cleanup must not kill the queue worker
                error = str(exc)[:500]
            try:
                _append_timing(timing_path, {
                    "event": "snapshot.cleanup.completed" if not error
                    else "snapshot.cleanup.failed",
                    "time_utc": _utc_now(), "build_attempt_id": attempt_id,
                    "snapshot": original_name,
                    "background": True,
                    "duration_ms": round((time.monotonic() - started) * 1000, 3),
                    "error": error,
                })
            except Exception:
                # Snapshot cleanup and telemetry are housekeeping; neither can
                # invalidate a successfully built generation.
                pass
        finally:
            with _SNAPSHOT_CLEANUP_LOCK:
                _SNAPSHOT_CLEANUP_PENDING.discard(str(path))
            _SNAPSHOT_CLEANUP_QUEUE.task_done()


def _queue_snapshot_cleanup(path: Path, original_name: str,
                            attempt_id: str, timing_path: Path) -> bool:
    global _SNAPSHOT_CLEANUP_THREAD
    key = str(path)
    with _SNAPSHOT_CLEANUP_LOCK:
        if key in _SNAPSHOT_CLEANUP_PENDING:
            return True
        _SNAPSHOT_CLEANUP_PENDING.add(key)
        if (_SNAPSHOT_CLEANUP_THREAD is None
                or not _SNAPSHOT_CLEANUP_THREAD.is_alive()):
            _SNAPSHOT_CLEANUP_THREAD = threading.Thread(
                target=_snapshot_cleanup_worker,
                name="game-build-snapshot-cleanup", daemon=True)
            try:
                _SNAPSHOT_CLEANUP_THREAD.start()
            except RuntimeError:
                _SNAPSHOT_CLEANUP_THREAD = None
                _SNAPSHOT_CLEANUP_PENDING.discard(key)
                return False
        try:
            _SNAPSHOT_CLEANUP_QUEUE.put_nowait(
                (path, original_name, attempt_id, timing_path))
        except queue.Full:
            _SNAPSHOT_CLEANUP_PENDING.discard(key)
            return False
    return True


def _queue_snapshot_trash(trash: Path, attempt_id: str,
                          timing_path: Path) -> int:
    queued = 0
    try:
        entries = list(trash.iterdir())
    except OSError:
        return 0
    for path in entries:
        if not path.is_dir():
            continue
        if _queue_snapshot_cleanup(path, path.name, attempt_id, timing_path):
            queued += 1
    return queued


def _prune_snapshots(root: Path, keep: int = 5,
                     protect_attempts: list[str] | None = None,
                     attempt_id: str = "snapshot-housekeeping",
                     timing_path: Path | None = None,
                     synchronous: bool = False) -> int:
    snapshots = root / ".dev" / "build-snapshots"
    trash = root / ".dev" / "build-snapshot-trash"
    timing_path = timing_path or (root / ".dev" / "build-timings"
                                  / f"{attempt_id}.jsonl")
    if synchronous:
        # Only low-disk recovery waits for recursive deletion. Normal builds
        # rename snapshots away and let the idle cleanup thread remove them.
        _SNAPSHOT_CLEANUP_QUEUE.join()
        try:
            trash_entries = list(trash.iterdir()) if trash.is_dir() else []
        except OSError:
            trash_entries = []
        for stale in trash_entries:
            try:
                shutil.rmtree(stale)
            except Exception:
                pass
    else:
        _queue_snapshot_trash(trash, attempt_id, timing_path)
    try:
        if not snapshots.is_dir():
            return 0
        entries = [item for item in snapshots.iterdir()
                   if item.is_dir()]
        protected = {path.name for path in sorted(
            entries, key=lambda item: item.stat().st_mtime,
            reverse=True)[:max(1, keep)]}
        protected.update(str(value) for value in (protect_attempts or []) if value)
        by_name = {path.name: path for path in entries}
        # A published or in-flight overlay can still depend on directories in
        # an older snapshot. Protect every referenced target before renaming
        # any snapshot; old unreferenced overlays age out on a later prune.
        for snapshot in entries:
            marker = _read_json(snapshot / "source" / ".snapshot.json")
            targets = (marker.get("junction_targets", [])
                       if isinstance(marker, dict) else [])
            for value in targets:
                parts = Path(str(value).replace("\\", "/")).parts
                if (len(parts) >= 3 and parts[0:2] == (".dev", "build-snapshots")
                        and parts[2] in by_name):
                    protected.add(parts[2])
        game_root = root / "build" / "game"
        for manifest_name in ("ready.json", "current.json"):
            try:
                generation = int(_read_json(game_root / manifest_name).get(
                    "generation") or 0)
                metadata = _read_json(
                    game_root / f"generation-{generation}" / "generation.json")
                source_root = Path(str(metadata.get("source_root") or ""))
                if not source_root.is_absolute():
                    source_root = root / source_root
                relative_source = source_root.resolve().relative_to(
                    (root / ".dev" / "build-snapshots").resolve())
                if relative_source.parts and relative_source.parts[0] in by_name:
                    protected.add(relative_source.parts[0])
            except (OSError, RuntimeError, TypeError, ValueError):
                pass
    except OSError:
        # Retention is best-effort. An inaccessible old snapshot must not stop
        # a candidate build from using the last known-good published DLL.
        return 0

    deferred = 0
    try:
        trash.mkdir(parents=True, exist_ok=True)
    except OSError:
        return 0
    for path in entries:
        if path.name in protected:
            continue
        tombstone = trash / f"{path.name}-{uuid.uuid4().hex[:10]}"
        try:
            # Same-volume rename drops the old snapshot from the live index in
            # milliseconds; the recursive directory walk happens off-path.
            os.replace(path, tombstone)
        except OSError:
            continue
        if synchronous:
            try:
                shutil.rmtree(tombstone)
                deferred += 1
            except OSError:
                _queue_snapshot_cleanup(tombstone, path.name, attempt_id,
                                        timing_path)
        else:
            _queue_snapshot_cleanup(tombstone, path.name, attempt_id,
                                    timing_path)
            deferred += 1
    return deferred


def _prune_logs(root: Path, keep_days: int = 3) -> int:
    logs = root / "logs"
    if not logs.is_dir() or keep_days <= 0:
        return 0
    dates = sorted((path for path in logs.glob("*")
                    if path.is_dir() and _looks_like_date(path.name)),
                   key=lambda path: path.name, reverse=True)
    removed = 0
    for path in dates[max(1, keep_days):]:
        shutil.rmtree(path, ignore_errors=True)
        removed += 1
    return removed


def _min_free_bytes() -> int:
    try:
        megabytes = int(os.environ.get("MIMITA_GAME_BUILD_MIN_FREE_MB", "2048") or 2048)
    except ValueError:
        megabytes = 2048
    return max(0, megabytes) * 1024 * 1024


def _env_float(name: str, default: float) -> float:
    try:
        return float(os.environ.get(name, str(default)) or default)
    except ValueError:
        return float(default)


def _worker_metrics_summary(current: dict, worker_progress: dict,
                            last_attempt: dict, attempt_id: str,
                            no_op: bool, success: bool) -> dict:
    """Summarize only metrics belonging to this attempt."""
    metric_keys = (
        "source_count", "compiled", "skipped", "jobs", "object_scan_seconds",
        "dependency_check_ms", "scan_parse_ms", "scan_stat_ms",
        "scan_unique_dependencies", "changed_source_preflight_seconds",
        "compile_seconds", "link_seconds", "publish_seconds", "total_seconds",
        "worker_total_seconds", "slowest_compiles",
    )
    if not success:
        if worker_progress.get("build_attempt_id") != attempt_id:
            return {key: ([] if key == "slowest_compiles" else None)
                    for key in metric_keys}
        progress_keys = {
            "source_count": ("scan_total", "source_count"),
            "compiled": ("compiled",),
            "skipped": ("skipped", "skipped_count"),
            "jobs": ("jobs",),
            "object_scan_seconds": ("object_scan_s",),
            "dependency_check_ms": ("dependency_check_ms",),
            "scan_parse_ms": ("scan_parse_ms", "parse_ms"),
            "scan_stat_ms": ("scan_stat_ms", "stat_ms"),
            "scan_unique_dependencies": ("scan_unique_dependencies",),
            "changed_source_preflight_seconds": (
                "changed_source_preflight_s",),
            "compile_seconds": ("compile_s",),
            "link_seconds": ("link_s",),
            "publish_seconds": ("publish_s",),
            "total_seconds": ("total_s",),
            "worker_total_seconds": ("worker_total_s", "worker_elapsed_s"),
            "slowest_compiles": ("slowest",),
        }
        return {
            key: next((worker_progress[name] for name in names
                       if name in worker_progress),
                      [] if key == "slowest_compiles" else None)
            for key, names in progress_keys.items()
        }

    metrics = {key: current.get(key) for key in metric_keys}
    if not no_op or worker_progress.get("build_attempt_id") != attempt_id:
        return metrics

    # A no-op has no current compile/link/publish measurements. `current.json`
    # intentionally continues to describe the last published generation, so
    # reading worker totals from it falsely attributes old work to this attempt.
    worker_seconds = last_attempt.get("worker_total_seconds", 0.0)
    metrics.update({
        "source_count": worker_progress.get("scan_scanned", 0),
        "compiled": 0,
        "skipped": worker_progress.get("skipped", 0),
        "jobs": worker_progress.get("jobs", 0),
        "object_scan_seconds": worker_progress.get("object_scan_s", 0.0),
        "dependency_check_ms": worker_progress.get("dependency_check_ms", 0.0),
        "scan_parse_ms": worker_progress.get("scan_parse_ms", 0.0),
        "scan_stat_ms": worker_progress.get("scan_stat_ms", 0.0),
        "scan_unique_dependencies": worker_progress.get(
            "scan_unique_dependencies", 0),
        "changed_source_preflight_seconds": 0.0,
        "compile_seconds": 0.0,
        "link_seconds": 0.0,
        "publish_seconds": 0.0,
        "total_seconds": worker_seconds,
        "worker_total_seconds": worker_seconds,
        "slowest_compiles": [],
    })
    return metrics


def _consume_build_notes(root: Path, attempt_id: str) -> list[dict]:
    pending = (root / ".dev" / "build-notes" / "pending").resolve()
    archive = (root / ".dev" / "build-notes" / "consumed" / attempt_id).resolve()
    for path in (pending, archive):
        try:
            path.relative_to(root.resolve())
        except ValueError as error:
            raise RuntimeError("build note path resolves outside the repository") from error
    if not pending.is_dir():
        return []
    notes: list[dict] = []
    for source in sorted(pending.glob("*.json")):
        try:
            payload = _read_json(source)
        except OSError:
            continue
        if not payload or not isinstance(payload.get("changes"), str):
            continue
        archive.mkdir(parents=True, exist_ok=True)
        destination = archive / source.name
        try:
            source.replace(destination)
        except OSError:
            continue
        notes.append(payload)
    return notes


def _wait_for_process_exit(process: subprocess.Popen,
                           grace_seconds: float) -> bool:
    """Let a worker that reached its terminal record exit before killing it."""
    if process.poll() is not None:
        return True
    try:
        process.wait(timeout=max(0.0, float(grace_seconds)))
    except subprocess.TimeoutExpired:
        return process.poll() is not None
    return True


def _terminal_attempt_failure_after_timeout(attempt_id: str,
                                            last_attempt: dict) -> bool:
    """Trust only a complete failure record written by this exact attempt."""
    if str(last_attempt.get("build_attempt_id") or "") != attempt_id:
        return False
    if str(last_attempt.get("status") or "") != "failed":
        return False
    phase = str(last_attempt.get("failed_phase") or last_attempt.get("phase") or "")
    error = str(last_attempt.get("error") or "").strip()
    return bool(phase and error)


def _run_checked(root: Path, command: list[str], on_output,
                 timeout: float | None = None,
                 terminal_failure_attempt: tuple[Path, str] | None = None) -> int:
    startupinfo = None
    creationflags = 0
    if os.name == "nt":
        startupinfo = subprocess.STARTUPINFO()
        startupinfo.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        startupinfo.wShowWindow = 0
        creationflags = (getattr(subprocess, "CREATE_NO_WINDOW", 0)
                         | getattr(subprocess, "CREATE_NEW_PROCESS_GROUP", 0))
    process = subprocess.Popen(
        command,
        cwd=root,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        bufsize=1,
        env={**os.environ, "PYTHONUNBUFFERED": "1"},
        creationflags=creationflags,
        startupinfo=startupinfo,
        close_fds=os.name == "nt",
    )
    timed_out = threading.Event()
    terminal_failure_stopped = threading.Event()
    monitor_stop = threading.Event()

    def stop_after_terminal_failure() -> None:
        if terminal_failure_attempt is None:
            return
        record_path, expected_attempt = terminal_failure_attempt
        while not monitor_stop.wait(0.25):
            if process.poll() is not None:
                return
            record = _read_json(record_path)
            if not _terminal_attempt_failure_after_timeout(expected_attempt,
                                                            record):
                continue
            # At this point compile/link children have already returned and the
            # worker has durably recorded failure. Give its final result write
            # a brief grace, then stop the worker rather than waiting for the
            # global watchdog.
            if _wait_for_process_exit(process, 0.5):
                return
            terminal_failure_stopped.set()
            try:
                process.kill()
            except OSError:
                pass
            return

    monitor = None
    if terminal_failure_attempt is not None:
        monitor = threading.Thread(
            target=stop_after_terminal_failure,
            name="game-build-terminal-failure-watchdog", daemon=True)
        monitor.start()

    def kill_on_timeout() -> None:
        # A terminal compiler/linker result can be written just before the
        # worker process exits. Give that short cleanup window a chance before
        # killing the tree and replacing its real failure with exit 124.
        if _wait_for_process_exit(process, 2.0):
            return
        if process.poll() is not None:
            return
        timed_out.set()
        try:
            subprocess.run(["taskkill", "/F", "/T", "/PID", str(process.pid)],
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                           timeout=5.0,
                           creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
        except (OSError, subprocess.TimeoutExpired):
            pass
        try:
            process.kill()
        except OSError:
            pass

    timer = None
    if timeout and timeout > 0:
        timer = threading.Timer(timeout, kill_on_timeout)
        timer.daemon = True
        timer.start()
    try:
        assert process.stdout is not None
        for line in process.stdout:
            if on_output:
                on_output(line.rstrip())
        code = process.wait()
    finally:
        monitor_stop.set()
        if monitor is not None:
            monitor.join(timeout=1.25)
        if timer is not None:
            timer.cancel()
    if timed_out.is_set():
        return 124
    if terminal_failure_stopped.is_set():
        return 125
    return code


def _run_game_build(root: Path, request_ids: list[str], mode: str = "debug",
                    source_generation: int = 0, on_output=None,
                    attempt_id: str | None = None,
                    build_metadata: dict | None = None) -> dict:
    """Snapshot, validate, build, and publish one whole-game generation."""
    root = Path(root)
    attempt_id = attempt_id or f"game-dll-{uuid.uuid4().hex[:16]}"
    attempt_started = time.monotonic()
    attempt_started_utc = _utc_now()
    # Bound all driver phases together, not only the worker subprocess. This
    # guarantees a terminal result before the user's 19-minute failure limit.
    attempt_timeout = min(
        18 * 60.0,
        max(1.0, _env_float("MIMITA_GAME_ATTEMPT_TIMEOUT", 18 * 60.0)))
    timing_path = root / ".dev" / "build-timings" / f"{attempt_id}.jsonl"
    summary_path = root / ".dev" / "build-timings" / f"{attempt_id}.json"
    last_attempt_path = root / "build" / "game" / "last-attempt.json"
    for path in (timing_path, summary_path):
        try:
            path.resolve().relative_to(root.resolve())
        except ValueError as error:
            raise RuntimeError("build timing path resolves outside the repository") from error
    notes = _consume_build_notes(root, attempt_id)
    progress_path = root / "build" / "game" / "progress.json"
    progress = {
        "status": "preparing", "phase": "codegen_check",
        "build_attempt_id": attempt_id,
        "attempt_timeout_seconds": attempt_timeout,
        "source_generation": source_generation,
        "build_mode": str(mode).lower(),
        "started_utc": attempt_started_utc, "updated_utc": attempt_started_utc,
        "elapsed_s": 0.0, "notes": notes, "total": 0, "completed": 0,
        "compiled": 0, "skipped": 0, "failed": 0, "file_events": [],
        "timeline": [],
    }
    _write_json(progress_path, progress)
    _append_timing(timing_path, {
        "event": "build.started", "time_utc": attempt_started_utc,
        "elapsed_ms": 0, "build_attempt_id": attempt_id,
        "attempt_timeout_seconds": attempt_timeout,
        "source_generation": source_generation, "request_ids": request_ids,
        "notes": notes,
    })

    def remaining_attempt_seconds() -> float:
        return max(0.0, attempt_timeout - (time.monotonic() - attempt_started))

    def attempt_deadline_expired() -> bool:
        return remaining_attempt_seconds() <= 0.0

    def check_attempt_budget(phase: str) -> None:
        if attempt_deadline_expired():
            raise _BuildAttemptDeadlineExceeded(
                f"whole DLL build attempt exceeded {attempt_timeout:.0f}s "
                f"during {phase}")

    def bounded_timeout(local_timeout: float | None) -> float:
        remaining = remaining_attempt_seconds()
        if local_timeout is None or float(local_timeout) <= 0.0:
            return remaining
        return min(float(local_timeout), remaining)

    def set_stage(name: str, state: str, **fields) -> None:
        now = time.monotonic()
        utc = _utc_now()
        entry = {"stage": name, "state": state, "time_utc": utc,
                 "elapsed_ms": round((now - attempt_started) * 1000, 3)}
        entry.update(fields)
        progress["phase"] = name
        progress["status"] = "preparing" if state == "started" else progress["status"]
        progress["updated_utc"] = utc
        progress["elapsed_s"] = round(now - attempt_started, 3)
        progress["timeline"].append(entry)
        progress["timeline"] = progress["timeline"][-64:]
        _write_json(progress_path, progress)
        _append_timing(timing_path, {"event": f"stage.{state}", **entry,
                                     "build_attempt_id": attempt_id})
        if on_output:
            on_output("[GAME_STAGE] " + json.dumps(
                {**entry, "build_attempt_id": attempt_id}, separators=(",", ":")))

    def set_driver_subphase(name: str, state: str, started: float,
                            **fields) -> None:
        now = time.monotonic()
        entry = {
            "phase": "driver_pre_worker",
            "subphase": name,
            "state": state,
            "time_utc": _utc_now(),
            "elapsed_ms": round((now - attempt_started) * 1000.0, 3),
            "build_attempt_id": attempt_id,
        }
        if state == "completed":
            entry["duration_ms"] = round((now - started) * 1000.0, 3)
        entry.update(fields)
        progress["phase"] = "driver_pre_worker"
        progress["driver_subphase"] = entry
        progress["updated_utc"] = entry["time_utc"]
        progress["elapsed_s"] = round(now - attempt_started, 3)
        try:
            _write_json(progress_path, progress)
            _append_timing(timing_path, {
                "event": f"driver.subphase.{state}", **entry,
            })
        except OSError:
            # Telemetry must not turn a valid build into a failed build.
            pass

    def set_manifest_subphase(event: dict) -> None:
        """Persist bounded manifest subphase progress for the dashboard."""
        check_attempt_budget(
            f"manifest snapshot ({event.get('subphase') or 'unknown'})")
        now = time.monotonic()
        utc = _utc_now()
        name = str(event.get("subphase") or "unknown")
        state = str(event.get("state") or "progress")
        previous = progress.get("subphase")
        if state == "started":
            started_monotonic = now
        elif (isinstance(previous, dict) and previous.get("subphase") == name
              and isinstance(previous.get("started_monotonic"), (int, float))):
            started_monotonic = float(previous["started_monotonic"])
        else:
            started_monotonic = now
        entry = {**event, "phase": "manifest_snapshot", "time_utc": utc,
                 "started_monotonic": started_monotonic,
                 "elapsed_ms": (round((now - started_monotonic) * 1000.0, 3)
                                if state in ("started", "progress")
                                else float(event.get("elapsed_ms") or 0))}
        progress["subphase"] = entry
        progress["updated_utc"] = utc
        progress["last_progress_utc"] = utc
        progress["elapsed_s"] = round(now - attempt_started, 3)
        file_total = int(event.get("files_total") or 0)
        file_completed = int(event.get("files_completed") or 0)
        progress["manifest_progress"] = {
            "subphase": name,
            "completed": file_completed,
            "total": file_total,
            "percent": (min(100, round(file_completed * 100 / file_total))
                        if file_total > 0 else None),
        }
        if state == "completed":
            durations = progress.setdefault("subphase_durations_ms", {})
            if isinstance(durations, dict):
                durations[f"manifest_snapshot.{name}"] = float(event.get("elapsed_ms") or 0)
        try:
            _write_json(progress_path, progress)
            _append_timing(timing_path, {
                "event": f"manifest.subphase.{event.get('state', 'progress')}",
                **entry, "build_attempt_id": attempt_id,
            })
        except OSError:
            # Timing I/O must not turn a valid source snapshot into a failed build.
            pass

    def run_stage(name: str, command: list[str],
                  timeout: float | None = None) -> int:
        started = time.monotonic()
        set_stage(name, "started")
        effective_timeout = bounded_timeout(timeout)
        if effective_timeout <= 0.0:
            result = 124
        else:
            result = _run_checked(root, command, report_output,
                                  timeout=effective_timeout)
        set_stage(name, "completed" if result == 0 else "failed",
                  duration_ms=round((time.monotonic() - started) * 1000, 3),
                  exit_code=result,
                  attempt_timeout_seconds=attempt_timeout)
        return result

    def fail_attempt(error: str, **fields) -> dict:
        completed_utc = _utc_now()
        elapsed = round(time.monotonic() - attempt_started, 3)
        progress.update({"status": "failed", "phase": "failed",
                         "error": error, "updated_utc": completed_utc,
                         "elapsed_s": elapsed})
        _write_json(progress_path, progress)
        _append_timing(timing_path, {
            "event": "build.failed", "time_utc": completed_utc,
            "elapsed_ms": round(elapsed * 1000, 3),
            "build_attempt_id": attempt_id,
            "source_generation": source_generation, "error": error, **fields,
        })
        _write_json(summary_path, {
            "schema_version": 1, "build_attempt_id": attempt_id,
            "source_generation": source_generation,
            "started_utc": attempt_started_utc, "completed_utc": completed_utc,
            "total_seconds": elapsed, "success": False, "notes": notes,
            "timeline_jsonl": str(timing_path.relative_to(root)),
            "error": error, **fields,
        })
        return {"success": False, "error": error, "request_ids": request_ids,
                "build_attempt_id": attempt_id, "notes": notes,
                "no_op": False, "failed_phase": "pre_worker",
                "source_generation": source_generation,
                "revision": progress.get("revision", ""),
                "changed_paths": progress.get("changed_paths", []),
                "timing_jsonl": str(timing_path.relative_to(root)),
                "summary_json": str(summary_path.relative_to(root)),
                "last_attempt_json": str(last_attempt_path.relative_to(root)),
                **fields}

    def fail_if_attempt_expired(phase: str) -> dict | None:
        if not attempt_deadline_expired():
            return None
        return fail_attempt(
            f"whole DLL build attempt exceeded {attempt_timeout:.0f}s",
            failed_phase="attempt_timeout", timeout_seconds=attempt_timeout,
            timed_out_phase=phase)

    worker_output_tail: list[str] = []
    first_worker_output_recorded = False
    worker_startup_started: float | None = None

    def report_output(line: str) -> None:
        nonlocal first_worker_output_recorded
        if (worker_startup_started is not None
                and not first_worker_output_recorded):
            first_worker_output_recorded = True
            set_driver_subphase(
                "worker_launch_to_first_output", "completed",
                worker_startup_started, first_output_prefix=line[:96])
        if not on_output:
            return
        for prefix in ("[GAME_NOTE] ", "[GAME_FILE] ", "[GAME_STAGE] ",
                       "[GAME_SCAN] ", "[GAME_ERROR] ", "[GAME_RESULT] "):
            if line.startswith(prefix):
                try:
                    payload = json.loads(line[len(prefix):])
                except json.JSONDecodeError:
                    break
                if isinstance(payload, dict):
                    payload["build_attempt_id"] = attempt_id
                    normalized = prefix + json.dumps(payload, separators=(",", ":"))
                    handler_started = time.perf_counter()
                    on_output(normalized)
                    handler_ms = round((time.perf_counter() - handler_started) * 1000, 3)
                    event = {
                        "[GAME_FILE] ": "dev_loop.file_output_handled",
                        "[GAME_SCAN] ": "dev_loop.scan_output_handled",
                        "[GAME_ERROR] ": "dev_loop.error_output_handled",
                        "[GAME_RESULT] ": "dev_loop.result_output_handled",
                    }.get(prefix)
                    if event:
                        _append_timing(timing_path, {
                            "event": event,
                            "time_utc": _utc_now(), "path": payload.get("path", ""),
                            "status": payload.get("status", ""),
                            "duration_ms": handler_ms,
                            "build_attempt_id": attempt_id,
                            "revision": payload.get("revision", ""),
                        })
                    return
                break
        worker_output_tail.append(line)
        del worker_output_tail[:-200]
        on_output(line)
    manifest_path = root / "build" / "source-manifest.json"
    output_root = root / "build" / "game"
    change_report_path = root / "build" / "hot-reload" / "change-report.json"
    ready_path = output_root / "ready.json"

    _prune_logs(root, 3)
    _prune_snapshots(root, 5, attempt_id=attempt_id, timing_path=timing_path)
    min_free = _min_free_bytes()
    free = _disk_free_bytes(root)
    if free is not None and free < min_free:
        _prune_generations(root, 3)
        _prune_snapshots(root, 2, attempt_id=attempt_id,
                         timing_path=timing_path, synchronous=True)
        _prune_logs(root, 2)
        free = _disk_free_bytes(root)
    if free is not None and free < min_free:
        return fail_attempt(
            f"disk_low: {free // (1024 * 1024)} MB free, "
            f"need {min_free // (1024 * 1024)} MB after auto-prune")
    timeout_failure = fail_if_attempt_expired("pre_manifest")
    if timeout_failure:
        return timeout_failure

    set_stage("manifest_snapshot", "started")
    manifest_started = time.monotonic()
    inputs_started = time.monotonic()
    old_manifest = _read_json(manifest_path)
    hash_cache_path = root / ".dev" / "source-hash-cache.json"
    hash_cache = _read_json(hash_cache_path)
    source_mirror = _mirror_dir(root)
    timeout_failure = fail_if_attempt_expired("manifest_inputs_load")
    if timeout_failure:
        return timeout_failure
    try:
        set_manifest_subphase({
            "subphase": "manifest_inputs_load", "state": "completed",
            "elapsed_ms": round((time.monotonic() - inputs_started) * 1000.0, 3),
            "hash_cache_entries": len(hash_cache),
        })
    except _BuildAttemptDeadlineExceeded as error:
        return fail_attempt(str(error), failed_phase="attempt_timeout",
                            timeout_seconds=attempt_timeout,
                            timed_out_phase="manifest_inputs_load")
    manifest: dict = {}
    snapshot_errors: list[str] = []
    snapshot_retries = 0
    try:
        snapshot_attempts = max(1, int(os.environ.get(
            "MIMITA_SNAPSHOT_ATTEMPTS", "3") or 3))
    except ValueError:
        snapshot_attempts = 3
    try:
        for snapshot_try in range(snapshot_attempts):
            snapshot_retries = snapshot_try
            manifest_started_one = time.monotonic()
            set_manifest_subphase({
                "subphase": "build_manifest", "state": "started",
                "elapsed_ms": 0.0, "snapshot_try": snapshot_try + 1,
                "snapshot_attempts": snapshot_attempts,
            })
            manifest = build_manifest(root, hash_cache=hash_cache,
                                      progress_callback=set_manifest_subphase)
            set_manifest_subphase({
                "subphase": "build_manifest", "state": "completed",
                "elapsed_ms": round((time.monotonic() - manifest_started_one) * 1000.0, 3),
                "snapshot_try": snapshot_try + 1,
                "source_count": len(manifest.get("sources", [])),
                "header_count": len(manifest.get("headers", [])),
            })
            cache_started = time.monotonic()
            set_manifest_subphase({"subphase": "hash_cache_persist", "state": "started",
                                   "elapsed_ms": 0.0, "entries": len(hash_cache)})
            try:
                _write_json(hash_cache_path, hash_cache)
            except OSError as error:
                set_manifest_subphase({"subphase": "hash_cache_persist", "state": "failed",
                                       "elapsed_ms": round((time.monotonic() - cache_started) * 1000.0, 3),
                                       "error": str(error)[:240]})
            else:
                set_manifest_subphase({"subphase": "hash_cache_persist", "state": "completed",
                                       "elapsed_ms": round((time.monotonic() - cache_started) * 1000.0, 3),
                                       "entries": len(hash_cache)})
            mirror_started = time.monotonic()
            set_manifest_subphase({"subphase": "source_mirror_update", "state": "started",
                                   "elapsed_ms": 0.0, "snapshot_try": snapshot_try + 1})
            snapshot_errors = _update_source_mirror(root, source_mirror, manifest)
            set_manifest_subphase({"subphase": "source_mirror_update",
                                   "state": "failed" if snapshot_errors else "completed",
                                   "elapsed_ms": round((time.monotonic() - mirror_started) * 1000.0, 3),
                                   "snapshot_try": snapshot_try + 1,
                                   "mismatch_count": len(snapshot_errors),
                                   "mismatches": snapshot_errors[:4]})
            if not snapshot_errors:
                break
    except _BuildAttemptDeadlineExceeded as error:
        return fail_attempt(str(error), failed_phase="attempt_timeout",
                            timeout_seconds=attempt_timeout,
                            timed_out_phase="manifest_snapshot")
    snapshot_duration_ms = round((time.monotonic() - manifest_started) * 1000, 3)
    set_stage("manifest_snapshot", "completed", duration_ms=snapshot_duration_ms,
              source_count=len(manifest.get("sources", [])),
              snapshot_retries=snapshot_retries)
    if snapshot_errors:
        return fail_attempt("source snapshot unstable: " + snapshot_errors[0],
                            errors=snapshot_errors[:20])
    timeout_failure = fail_if_attempt_expired("manifest_snapshot")
    if timeout_failure:
        return timeout_failure
    set_stage("manifest_validation", "started")
    validation_started = time.monotonic()
    errors, warnings = validate(manifest)
    if errors:
        set_stage("manifest_validation", "failed",
                  duration_ms=round((time.monotonic() - validation_started) * 1000, 3),
                  errors=errors[:20])
        return fail_attempt("source manifest validation failed",
                            errors=errors[:20], warnings=warnings[:20])

    _write_json(manifest_path, manifest)
    revision = str(manifest.get("revision", ""))
    set_stage("manifest_validation", "completed",
              duration_ms=round((time.monotonic() - validation_started) * 1000, 3),
              source_count=len(manifest.get("sources", [])))
    timeout_failure = fail_if_attempt_expired("manifest_validation")
    if timeout_failure:
        return timeout_failure
    diff_started = time.monotonic()
    set_driver_subphase("manifest_diff", "started", diff_started)
    previous_revision = str(old_manifest.get("revision") or "")
    source_base_snapshot = _snapshot_for_revision(root, previous_revision)
    previous_manifest = (_snapshot_manifest(source_base_snapshot,
                                             previous_revision)
                         if source_base_snapshot is not None else None)
    change_basis = previous_manifest or old_manifest
    change_report = diff_manifests(change_basis, manifest) if change_basis else {}
    set_driver_subphase("manifest_diff", "completed", diff_started,
                        old_manifest_available=bool(old_manifest),
                        changed_count=len(change_report.get("changed", [])),
                        added_count=len(change_report.get("added", [])),
                        changed_header_count=len(change_report.get(
                            "changed_headers", [])))
    timeout_failure = fail_if_attempt_expired("manifest_diff")
    if timeout_failure:
        return timeout_failure
    set_stage("source_snapshot", "started")
    snapshot_started = time.monotonic()
    metadata = build_metadata or {}
    reused_generation = 0
    snapshot_strategy = "full_hardlink"
    snapshot_fallback_reason = ""
    try:
        if (str(metadata.get("kind") or "") == "preview" and
                metadata.get("overlay_paths")):
            base = _base_snapshot_for_generation(
                root, int(metadata.get("base_generation") or 0))
            source_snapshot = _create_overlay_snapshot(
                root, base, [str(value) for value in metadata["overlay_paths"]],
                attempt_id, check_deadline=check_attempt_budget)
            snapshot_strategy = "preview_overlay"
        else:
            reusable = _reusable_published_snapshot(root, revision)
            if reusable is not None:
                reused_generation, source_snapshot = reusable
                snapshot_strategy = "reused_published"
            elif (source_base_snapshot is not None
                  and previous_revision == revision):
                source_snapshot = source_base_snapshot
                snapshot_strategy = "reused_snapshot_revision"
            else:
                replacement_paths = (
                    list(change_report.get("changed", []))
                    + list(change_report.get("added", []))
                    + [str(item.get("to") or "")
                       for item in change_report.get("renamed", [])]
                    + list(change_report.get("changed_headers", []))
                    + list(change_report.get("added_headers", [])))
                removed_paths = (
                    list(change_report.get("removed", []))
                    + [str(item.get("from") or "")
                       for item in change_report.get("renamed", [])]
                    + list(change_report.get("removed_headers", [])))
                previous = source_base_snapshot
                expected_hashes = {
                    str(item.get("path")): str(item.get("sha256"))
                    for collection in (manifest.get("sources", []),
                                       manifest.get("headers", []))
                    for item in collection
                    if item.get("path") and item.get("sha256")}
                if (previous is not None and replacement_paths + removed_paths
                        and len(replacement_paths) + len(removed_paths) <= 256):
                    try:
                        source_snapshot = _create_junction_overlay_snapshot(
                            root, previous, source_mirror,
                            replacement_paths, removed_paths, attempt_id,
                            expected_hashes,
                            check_deadline=check_attempt_budget)
                        snapshot_strategy = "junction_overlay"
                    except _BuildAttemptDeadlineExceeded:
                        raise
                    except (OSError, RuntimeError, ValueError) as error:
                        snapshot_fallback_reason = str(error)[:300]
                        source_snapshot = _create_source_snapshot(
                            root, source_mirror, attempt_id,
                            check_deadline=check_attempt_budget)
                else:
                    if previous is None:
                        snapshot_fallback_reason = "no matching published base snapshot"
                    elif not replacement_paths and not removed_paths:
                        snapshot_fallback_reason = "no source path delta"
                    else:
                        snapshot_fallback_reason = "delta exceeds 256 paths"
                    source_snapshot = _create_source_snapshot(
                        root, source_mirror, attempt_id,
                        check_deadline=check_attempt_budget)
        if str(metadata.get("kind") or "") != "preview":
            manifest_archive_started = time.monotonic()
            try:
                _write_snapshot_manifest(source_snapshot, manifest)
            except (OSError, RuntimeError, TypeError, ValueError) as error:
                set_driver_subphase(
                    "source_manifest_archive", "failed", manifest_archive_started,
                    fail_open=True, error=str(error)[:300])
            else:
                set_driver_subphase(
                    "source_manifest_archive", "completed", manifest_archive_started,
                    revision=revision,
                    bytes=(source_snapshot.parent / "source-manifest.json").stat().st_size)
    except _BuildAttemptDeadlineExceeded as error:
        return fail_attempt(str(error), failed_phase="attempt_timeout",
                            timeout_seconds=attempt_timeout,
                            timed_out_phase="source_snapshot")
    except (OSError, RuntimeError, ValueError) as error:
        return fail_attempt(f"source snapshot failed: {error}",
                            failed_phase="source_snapshot")
    timeout_failure = fail_if_attempt_expired("source_snapshot")
    if timeout_failure:
        return timeout_failure
    set_stage("source_snapshot", "completed",
              duration_ms=round((time.monotonic() - snapshot_started) * 1000, 3),
              path=str(source_snapshot.relative_to(root)),
              reused_generation=reused_generation or None,
              strategy=snapshot_strategy,
              fallback_reason=snapshot_fallback_reason or None)
    progress["source_snapshot"] = str(source_snapshot.relative_to(root))
    driver_phase_started = time.monotonic()
    set_driver_subphase("snapshot_prune", "started", driver_phase_started)
    pruned_snapshots = _prune_snapshots(
        root, 5, protect_attempts=[source_snapshot.parent.name],
        attempt_id=attempt_id, timing_path=timing_path)
    set_driver_subphase("snapshot_prune", "completed", driver_phase_started,
                        deferred_count=pruned_snapshots,
                        background_cleanup=True)
    timeout_failure = fail_if_attempt_expired("snapshot_prune")
    if timeout_failure:
        return timeout_failure

    # Codegen checks are correctness gates, but running both Python generators
    # before every manifest scan makes a true no-op pay their full cost. The
    # manifest is now validated and written, so key each successful check by
    # its complete direct inputs/outputs; packet registry also keys on the
    # complete source manifest because its reference scan covers all sources.
    codegen_cache_path = root / ".dev" / "codegen-validation-cache.json"
    prior_codegen_cache = _read_json(codegen_cache_path)
    cached_checks = (prior_codegen_cache.get("checks", {})
                     if prior_codegen_cache.get("schema") == _CODEGEN_CACHE_SCHEMA
                     and isinstance(prior_codegen_cache.get("checks"), dict)
                     else {})
    new_checks = dict(cached_checks)
    validated_this_attempt: dict[str, str] = {}
    for script in ("resident_state_codegen.py", "packet_registry_codegen.py"):
        stage_name = f"codegen:{script}"
        fingerprint = _codegen_validation_fingerprint(root, script, revision)
        if fingerprint and cached_checks.get(script) == fingerprint:
            set_stage(stage_name, "started", cache_hit=True,
                      validation_fingerprint=fingerprint[:16])
            set_stage(stage_name, "completed", duration_ms=0.0, exit_code=0,
                      cache_hit=True, validation_fingerprint=fingerprint[:16])
            validated_this_attempt[script] = fingerprint
            continue

        result = run_stage(
            stage_name,
            [sys.executable, str(root / "devscripts" / script), "--check"],
            timeout=_env_float("MIMITA_CODEGEN_TIMEOUT", 300.0))
        if result == 124:
            if attempt_deadline_expired():
                return fail_attempt(
                    f"whole DLL build attempt exceeded {attempt_timeout:.0f}s",
                    failed_phase="attempt_timeout", timeout_seconds=attempt_timeout,
                    timed_out_phase=stage_name)
            return fail_attempt(f"codegen timeout: {script}",
                                failed_phase="codegen_timeout")
        if result != 0:
            return fail_attempt(f"codegen check failed: {script}", exit_code=result)
        if fingerprint:
            verified_fingerprint = _codegen_validation_fingerprint(
                root, script, revision)
            if verified_fingerprint != fingerprint:
                return fail_attempt(
                    f"codegen inputs changed while checking: {script}",
                    failed_phase="codegen_input_changed")
            new_checks[script] = fingerprint
            validated_this_attempt[script] = fingerprint
        timeout_failure = fail_if_attempt_expired(stage_name)
        if timeout_failure:
            return timeout_failure

    if new_checks != cached_checks:
        cache_started = time.monotonic()
        set_driver_subphase("codegen_cache_persist", "started", cache_started)
        try:
            _write_json(codegen_cache_path, {
                "schema": _CODEGEN_CACHE_SCHEMA,
                "checks": new_checks,
            })
        except OSError as error:
            # Cache persistence is optional. A failed write only means the
            # validated checks will run again on the next attempt.
            set_driver_subphase("codegen_cache_persist", "failed", cache_started,
                                error=str(error)[:240])
        else:
            set_driver_subphase("codegen_cache_persist", "completed", cache_started,
                                check_count=len(new_checks))

    if change_report:
        change_report["generated_utc"] = _utc_now()
        driver_phase_started = time.monotonic()
        set_driver_subphase("manifest_diff_write", "started", driver_phase_started)
        _write_json(change_report_path, change_report)
        set_driver_subphase("manifest_diff_write", "completed", driver_phase_started)
    driver_phase_started = time.monotonic()
    set_driver_subphase("attempt_progress_and_context_write", "started",
                        driver_phase_started)
    progress["revision"] = revision
    progress["changed_paths"] = list(dict.fromkeys(
        str(path) for key in ("changed", "added", "changed_headers")
        for path in change_report.get(key, []) if isinstance(path, str)))[:40]
    progress["total"] = sum(1 for item in manifest.get("sources", [])
                            if item.get("owner") == "game")
    _write_json(progress_path, progress)
    context_path = root / ".dev" / "build-timings" / f"{attempt_id}.context.json"
    try:
        context_path.resolve().relative_to(root.resolve())
    except ValueError as error:
        raise RuntimeError("build context path resolves outside the repository") from error
    _write_json(context_path, {
        "build_attempt_id": attempt_id,
        "source_generation": source_generation,
        "build_mode": str(mode).lower(),
        "started_utc": attempt_started_utc,
        "attempt_started_monotonic": attempt_started,
        "timing_path": str(timing_path),
        "summary_path": str(summary_path),
        "notes": notes,
        "changed_paths": progress["changed_paths"],
        "revision": revision,
    })
    if on_output:
        changed_paths = list(dict.fromkeys(
            str(path) for key in ("changed", "added", "changed_headers")
            for path in change_report.get(key, []) if isinstance(path, str)))
        on_output("[GAME_NOTE] " + json.dumps({
            "build_attempt_id": attempt_id,
            "source_generation": source_generation,
            "revision": revision,
            "notes": notes,
            "changed_paths": changed_paths[:40],
        }, separators=(",", ":")))
    set_driver_subphase("attempt_progress_and_context_write", "completed",
                        driver_phase_started,
                        changed_path_count=len(progress["changed_paths"]))

    driver_phase_started = time.monotonic()
    set_driver_subphase("worker_command_prepare", "started", driver_phase_started)
    command = [
        sys.executable,
        "-u",
        str(root / "devscripts" / "game_build_worker.py"),
        "--manifest", str(manifest_path),
        "--source-root", str(source_snapshot),
        "--out", str(output_root),
        "--revision", revision,
        "--jobs", "10",
        "--priority", "high",
        "--attempt-context", str(context_path),
    ]
    linker = str((build_metadata or {}).get("linker") or "auto").strip().lower()
    if linker not in {"auto", "gnu", "lld"}:
        return fail_attempt(f"unsupported game DLL linker: {linker}",
                            failed_phase="linker_selection")
    command.extend(["--linker", linker])
    if change_report:
        command.extend(["--change-report", str(change_report_path)])
    if str(mode).lower() == "release":
        command.extend(["--build-mode", "release"])
    set_driver_subphase("worker_command_prepare", "completed", driver_phase_started,
                        argument_count=len(command))

    for script, expected_fingerprint in validated_this_attempt.items():
        if _codegen_validation_fingerprint(root, script, revision) != expected_fingerprint:
            return fail_attempt(
                f"codegen inputs changed before worker launch: {script}",
                failed_phase="codegen_input_changed")

    timeout_failure = fail_if_attempt_expired("worker_command_prepare")
    if timeout_failure:
        return timeout_failure

    if on_output:
        on_output(f"building whole-game revision {revision[:12]}")
    progress["phase"] = "worker_start"
    progress["updated_utc"] = _utc_now()
    _write_json(progress_path, progress)
    # Fail a wedged or unproductive DLL worker well before the old 60-minute
    # ceiling. Keep the environment override for exceptional, intentionally
    # long experiments; the root dev-loop supervisor kills the worker tree.
    build_timeout = _env_float("MIMITA_GAME_BUILD_TIMEOUT", 300.0)
    slot_timeout = _env_float("MIMITA_GAME_BUILD_SLOT_TIMEOUT", 600.0)
    slot_budget = remaining_attempt_seconds()
    if slot_budget <= 0.0:
        return fail_attempt(
            f"whole DLL build attempt exceeded {attempt_timeout:.0f}s",
            failed_phase="attempt_timeout", timeout_seconds=attempt_timeout,
            timed_out_phase="module_slot_wait")
    effective_slot_timeout = bounded_timeout(slot_timeout)
    driver_phase_started = time.monotonic()
    set_driver_subphase("module_slot_wait", "started", driver_phase_started,
                        timeout_seconds=effective_slot_timeout,
                        attempt_timeout_seconds=attempt_timeout)
    if effective_slot_timeout <= 0.0:
        return fail_attempt(
            f"whole DLL build attempt exceeded {attempt_timeout:.0f}s",
            failed_phase="attempt_timeout", timeout_seconds=attempt_timeout,
            timed_out_phase="module_slot_wait")
    slot = acquire_build_slot("module", "game-dll", timeout=effective_slot_timeout,
                              metadata={"lane": os.environ.get("MIMITA_LANE_ID", ""),
                                        "root": str(root),
                                        "attempt": attempt_id})
    set_driver_subphase("module_slot_wait", "completed", driver_phase_started,
                        acquired=slot is not None,
                        timeout_seconds=effective_slot_timeout,
                        attempt_timeout_seconds=attempt_timeout)
    if slot is None:
        if attempt_deadline_expired():
            return fail_attempt(
                f"whole DLL build attempt exceeded {attempt_timeout:.0f}s",
                failed_phase="attempt_timeout", timeout_seconds=attempt_timeout,
                timed_out_phase="module_slot_wait")
        return fail_attempt("build_slot_timeout: no free module build slot",
                            failed_phase="build_slot_timeout")
    worker_startup_started = time.monotonic()
    set_driver_subphase("worker_launch_to_first_output", "started",
                        worker_startup_started)
    remaining = remaining_attempt_seconds()
    if remaining <= 0.0:
        slot.release()
        return fail_attempt(
            f"whole DLL build attempt exceeded {attempt_timeout:.0f}s",
            failed_phase="attempt_timeout", timeout_seconds=attempt_timeout,
            timed_out_phase="worker_start")
    effective_build_timeout = bounded_timeout(build_timeout)
    if effective_build_timeout <= 0.0:
        slot.release()
        return fail_attempt(
            f"whole DLL build attempt exceeded {attempt_timeout:.0f}s",
            failed_phase="attempt_timeout", timeout_seconds=attempt_timeout,
            timed_out_phase="worker_start")
    try:
        return_code = _run_checked(root, command, report_output,
                                   timeout=effective_build_timeout,
                                   terminal_failure_attempt=(
                                       last_attempt_path, attempt_id))
    finally:
        if (worker_startup_started is not None
                and not first_worker_output_recorded):
            set_driver_subphase(
                "worker_launch_to_first_output", "completed",
                worker_startup_started, output_seen=False)
        slot.release()
    current = _read_json(output_root / "current.json")
    last_attempt = _read_json(last_attempt_path)
    attempt_records = (str(last_attempt.get("build_attempt_id") or "") == attempt_id)
    worker_timed_out = return_code == 124
    terminal_worker_failure = return_code == 125
    if (terminal_worker_failure and
            _terminal_attempt_failure_after_timeout(attempt_id, last_attempt)):
        return_code = 1
        set_stage("worker_supervised", "failed",
                  reason="terminal_worker_failure_detected",
                  failed_phase=(last_attempt.get("failed_phase")
                                or last_attempt.get("phase") or ""))
    elif (worker_timed_out and
            _terminal_attempt_failure_after_timeout(attempt_id, last_attempt)):
        # The worker completed and persisted this exact attempt's terminal
        # failure immediately before the watchdog's process-tree kill.
        return_code = 1
        worker_timed_out = False
        set_stage("worker_supervised", "failed",
                  reason="terminal_worker_failure_at_timeout_boundary",
                  failed_phase=(last_attempt.get("failed_phase")
                                or last_attempt.get("phase") or ""))
    elif worker_timed_out:
        set_stage("worker_supervised", "failed", reason="timeout",
                  timeout_seconds=effective_build_timeout,
                  attempt_timeout_seconds=attempt_timeout)
    success = (return_code == 0 and str(current.get("status", "")) == "ok"
               and str(current.get("revision", "")) == revision)
    no_op = bool(success and attempt_records and last_attempt.get("no_op"))
    failed_phase = str(last_attempt.get("failed_phase")
                       or last_attempt.get("phase") or "") if attempt_records else ""
    worker_error = str(last_attempt.get("error") or "") if attempt_records else ""
    worker_errors = (last_attempt.get("errors")
                     if attempt_records and isinstance(last_attempt.get("errors"), list)
                     else [])
    worker_tail = "\n".join(worker_output_tail)[-4000:]
    if worker_timed_out:
        if attempt_deadline_expired():
            failed_phase = "attempt_timeout"
            worker_error = (
                f"whole DLL build attempt exceeded {attempt_timeout:.0f}s "
                "during worker")
        else:
            failed_phase = "worker_timeout"
            worker_error = f"worker timed out after {effective_build_timeout:.0f}s"
        worker_errors = [worker_error]
    elif not success and not worker_error and worker_tail:
        worker_error = worker_tail
        worker_errors = [worker_tail]
    elapsed = round(time.monotonic() - attempt_started, 3)
    driver_file_events = []
    try:
        for line in timing_path.read_text(encoding="utf-8").splitlines():
            try:
                event = json.loads(line)
            except json.JSONDecodeError:
                continue
            if event.get("event") == "dev_loop.file_output_handled":
                driver_file_events.append(event)
    except OSError:
        pass
    driver_handler_ms = round(sum(
        float(event.get("duration_ms") or 0) for event in driver_file_events), 3)
    worker_progress = _read_json(progress_path)
    if (not no_op and worker_progress.get("build_attempt_id") == attempt_id
            and worker_progress.get("no_op")):
        no_op = True
    _append_timing(timing_path, {
        "event": "build.completed" if success else "build.failed",
        "time_utc": _utc_now(), "elapsed_ms": round(elapsed * 1000, 3),
        "build_attempt_id": attempt_id, "source_generation": source_generation,
        "attempt_timeout_seconds": attempt_timeout,
        "revision": revision, "exit_code": return_code,
        "generation": current.get("generation"), "dll": current.get("dll"),
        "no_op": no_op, "failed_phase": "" if success else failed_phase,
        "error": "" if success else (
            worker_error or current.get("error")
            or f"game build failed with exit code {return_code}"),
        "errors": [] if success else worker_errors[:20],
        "worker_output_tail": "" if success else worker_tail,
    })
    if worker_progress.get("build_attempt_id") == attempt_id:
        progress.update(worker_progress)
    attempt_metrics = _worker_metrics_summary(
        current, worker_progress, last_attempt, attempt_id, no_op, success)
    summary = {
        "schema_version": 1, "build_attempt_id": attempt_id,
        "attempt_timeout_seconds": attempt_timeout,
        "source_generation": source_generation, "revision": revision,
        "build_mode": str(mode).lower(),
        "request_ids": request_ids, "notes": notes,
        "changed_paths": progress["changed_paths"],
        "started_utc": attempt_started_utc, "completed_utc": _utc_now(),
        "total_seconds": elapsed, "success": success, "no_op": no_op,
        "failed_phase": "" if success else failed_phase,
        "worker_error": "" if success else worker_error,
        "worker_errors": [] if success else worker_errors[:20],
        "worker_output_tail": "" if success else worker_tail,
        "last_attempt_json": str(last_attempt_path.relative_to(root)),
        "worker_total_seconds": attempt_metrics["worker_total_seconds"],
        "object_scan_seconds": attempt_metrics["object_scan_seconds"],
        "generation": current.get("generation"), "dll": current.get("dll"),
        "code_hash": current.get("code_hash"),
        "timeline_jsonl": str(timing_path.relative_to(root)),
        "phase_durations_ms": worker_progress.get("phase_durations_ms", {}),
        "subphase_durations_ms": worker_progress.get("subphase_durations_ms", {}),
        "compile_sum_ms": worker_progress.get("compile_sum_ms", 0.0),
        "compile_min_ms": worker_progress.get("compile_min_ms"),
        "compile_max_ms": worker_progress.get("compile_max_ms", 0.0),
        "cache_invalidations": worker_progress.get("cache_invalidations", []),
        "dependency_check_stats": {
            "count": worker_progress.get("dependency_check_count", 0),
            "min_ms": worker_progress.get("dependency_check_min_ms", 0.0),
            "max_ms": worker_progress.get("dependency_check_max_ms", 0.0),
            "average_ms": worker_progress.get("dependency_check_avg_ms", 0.0),
            "slowest": worker_progress.get("dependency_check_slowest", []),
        },
        "worker_metrics": {
            **attempt_metrics,
            "compile_sum_ms": worker_progress.get("compile_sum_ms", 0.0),
            "compile_min_ms": worker_progress.get("compile_min_ms"),
            "compile_max_ms": worker_progress.get("compile_max_ms", 0.0),
            "dependency_prepass_ms": worker_progress.get("dependency_prepass_ms", 0.0),
            "dependency_index_load_ms": worker_progress.get("dependency_index_load_ms", 0.0),
            "dependency_index_save_ms": worker_progress.get("dependency_index_save_ms", 0.0),
            "dependency_path_and_fanout_ms": worker_progress.get(
                "dependency_path_and_fanout_ms", 0.0),
            "dependency_path_normalization_ms": worker_progress.get(
                "dependency_path_normalization_ms", 0.0),
            "external_dependency_classification_ms": worker_progress.get(
                "external_dependency_classification_ms", 0.0),
            "dependency_prepass_other_ms": worker_progress.get(
                "dependency_prepass_other_ms", 0.0),
            "stat_prewarm_ms": worker_progress.get("stat_prewarm_ms", 0.0),
            "object_check_wall_ms": worker_progress.get("object_check_wall_ms", 0.0),
            "object_check_overhead_ms": worker_progress.get(
                "object_check_overhead_ms", 0.0),
            "unique_stat_targets": worker_progress.get("unique_stat_targets", 0),
            "dependency_check_stats": {
                "count": worker_progress.get("dependency_check_count", 0),
                "min_ms": worker_progress.get("dependency_check_min_ms", 0.0),
                "max_ms": worker_progress.get("dependency_check_max_ms", 0.0),
                "average_ms": worker_progress.get("dependency_check_avg_ms", 0.0),
                "slowest": worker_progress.get("dependency_check_slowest", []),
            },
            "link_input_count": worker_progress.get("object_input_count", 0),
            "link_argument_count": worker_progress.get("linker_argument_count", 0),
            "link_object_input_bytes": worker_progress.get("object_input_bytes", 0),
        },
        "toolchain": worker_progress.get("compiler", ""),
        "flags_identity": worker_progress.get("flags_identity", ""),
        "object_cache": current.get("object_cache", ""),
        "dev_loop_output_handler_ms": driver_handler_ms,
        "slowest_dev_loop_output_handlers": sorted(
            driver_file_events, key=lambda item: float(item.get("duration_ms") or 0),
            reverse=True)[:10],
    }
    _write_json(summary_path, summary)
    progress["status"] = "no_op" if no_op else ("published" if success else "failed")
    progress["phase"] = "done" if success else "failed"
    progress["updated_utc"] = _utc_now()
    progress["elapsed_s"] = elapsed
    progress["build_attempt_id"] = attempt_id
    progress["source_generation"] = source_generation
    progress["generation"] = current.get("generation")
    progress["no_op"] = no_op
    progress["dev_loop_output_handler_ms"] = driver_handler_ms
    _write_json(progress_path, progress)
    if success and not no_op:
        freed = _prune_generations(root, 5)
        if freed:
            _append_timing(timing_path, {
                "event": "build.pruned", "time_utc": _utc_now(),
                "build_attempt_id": attempt_id, "freed_bytes": freed,
            })
    try:
        context_path.unlink()
    except OSError:
        pass
    if not success:
        return {
            "success": False,
            "error": worker_error or current.get("error")
            or f"game build failed with exit code {return_code}",
            "failed_phase": failed_phase,
            "errors": worker_errors[:20],
            "revision": revision,
            "request_ids": request_ids,
            "build_attempt_id": attempt_id,
            "source_generation": source_generation,
            "notes": notes,
            "changed_paths": progress["changed_paths"],
            "timing_jsonl": str(timing_path.relative_to(root)),
            "summary_json": str(summary_path.relative_to(root)),
            "last_attempt_json": str(last_attempt_path.relative_to(root)),
        }

    ready = None
    if not no_op:
        ready = {
            "status": "ready",
            "revision": current.get("revision", revision),
            "generation": current.get("generation"),
            "dll": current.get("dll"),
            "code_hash": current.get("code_hash"),
            "published_utc": _utc_now(),
        }
        if change_report:
            ready["change_report"] = change_report
        _write_json(ready_path, ready)
    return {
        "success": True,
        "no_op": no_op,
        "build_attempt_id": attempt_id,
        "source_generation": source_generation,
        "revision": revision,
        "generation": current.get("generation"),
        "dll": current.get("dll"),
        "code_hash": current.get("code_hash"),
        "notes": notes,
        "changed_paths": progress["changed_paths"],
        "timing_jsonl": str(timing_path.relative_to(root)),
        "summary_json": str(summary_path.relative_to(root)),
        "last_attempt_json": str(last_attempt_path.relative_to(root)),
        "total_seconds": elapsed,
        "worker_total_seconds": (last_attempt.get("worker_total_seconds")
                                 or current.get("worker_total_seconds")),
        "request_ids": request_ids,
        "ready": ready,
    }


def run_game_build(root: Path, request_ids: list[str], mode: str = "debug",
                   source_generation: int = 0, on_output=None,
                   build_metadata: dict | None = None) -> dict:
    """Snapshot, validate, build, and publish one whole-game generation."""
    root = Path(root)
    stamp = datetime.now(timezone.utc)
    attempt_id = (f"game-dll-{stamp:%Y%m%dT%H%M%S%fZ}-"
                  f"{uuid.uuid4().hex[:6]}")

    started_utc = _utc_now()
    started = time.monotonic()
    resource_sampler = BuildResourceSampler(root, attempt_id)
    try:
        resource_sampler.start()
    except Exception as error:  # resource telemetry is best-effort
        resource_sampler._errors.append(
            f"sampler start failed: {type(error).__name__}: {error}"[:500])
        resource_sampler._status = "unavailable"
    result = None
    try:
        result = _run_game_build(root, request_ids, mode, source_generation,
                                 on_output, attempt_id, build_metadata)
    except Exception as error:  # noqa: BLE001 - convert to a durable failed attempt
        completed_utc = _utc_now()
        elapsed = round(time.monotonic() - started, 3)
        timing_path = root / ".dev" / "build-timings" / f"{attempt_id}.jsonl"
        summary_path = root / ".dev" / "build-timings" / f"{attempt_id}.json"
        progress_path = root / "build" / "game" / "progress.json"
        progress = _read_json(progress_path)
        owns_progress = progress.get("build_attempt_id") == attempt_id
        notes = progress.get("notes", []) if owns_progress else []
        if not owns_progress:
            archived_notes = (root / ".dev" / "build-notes" / "consumed" /
                              attempt_id)
            notes = [_read_json(path) for path in sorted(archived_notes.glob("*.json"))]
            notes = [note for note in notes if note]
        revision = str(progress.get("revision", "")) if owns_progress else ""
        changed_paths = progress.get("changed_paths", []) if owns_progress else []
        message = f"{type(error).__name__}: {error}"
        try:
            _append_timing(timing_path, {
                "event": "build.failed", "time_utc": completed_utc,
                "elapsed_ms": round(elapsed * 1000),
                "build_attempt_id": attempt_id,
                "source_generation": source_generation, "error": message,
            })
            _write_json(summary_path, {
                "schema_version": 1, "build_attempt_id": attempt_id,
                "source_generation": source_generation,
                "revision": revision, "started_utc": started_utc,
                "completed_utc": completed_utc, "total_seconds": elapsed,
                "success": False, "notes": notes,
                "changed_paths": changed_paths,
                "timeline_jsonl": str(timing_path.relative_to(root)),
                "error": message,
            })
            if owns_progress:
                progress.update({"status": "failed", "phase": "failed",
                                 "error": message, "updated_utc": completed_utc,
                                 "elapsed_s": elapsed})
                _write_json(progress_path, progress)
        except OSError:
            pass
        result = {
            "success": False, "error": message,
            "failed_phase": "harness_exception", "no_op": False,
            "errors": [message],
            "request_ids": request_ids,
            "build_attempt_id": attempt_id,
            "source_generation": source_generation,
            "revision": revision, "notes": notes,
            "changed_paths": changed_paths,
            "timing_jsonl": str(timing_path.relative_to(root)),
            "summary_json": str(summary_path.relative_to(root)),
        }
    finally:
        try:
            resource_summary = resource_sampler.stop()
            timing_path = root / ".dev" / "build-timings" / f"{attempt_id}.jsonl"
            summary_path = root / ".dev" / "build-timings" / f"{attempt_id}.json"
            progress_path = root / "build" / "game" / "progress.json"
            try:
                _append_timing(timing_path, {
                    "event": "build.resource_sampling.completed",
                    "time_utc": _utc_now(),
                    "build_attempt_id": attempt_id,
                    **resource_summary,
                })
            except OSError:
                pass
            attempt_summary = _read_json(summary_path)
            if attempt_summary.get("build_attempt_id") == attempt_id:
                attempt_summary["started_utc"] = started_utc
                attempt_summary["completed_utc"] = _utc_now()
                attempt_summary["total_seconds"] = round(
                    time.monotonic() - started, 3)
                attempt_summary["resource_sampling"] = resource_summary
                try:
                    _write_json(summary_path, attempt_summary)
                except OSError:
                    pass
            progress = _read_json(progress_path)
            if progress.get("build_attempt_id") == attempt_id:
                progress["resource_sampling"] = resource_summary
                try:
                    _write_json(progress_path, progress)
                except OSError:
                    pass
            if isinstance(result, dict):
                result["resource_sampling"] = resource_summary
                result["total_seconds"] = float(
                    attempt_summary.get("total_seconds")
                    or round(time.monotonic() - started, 3))
        except Exception:
            # Telemetry errors are explicit in the sampler sidecar when possible;
            # they must never replace the actual build outcome.
            pass
    return result if isinstance(result, dict) else {
        "success": False, "error": "build attempt returned no result",
        "failed_phase": "harness_exception", "build_attempt_id": attempt_id,
        "source_generation": source_generation,
    }
