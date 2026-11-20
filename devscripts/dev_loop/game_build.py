"""Whole-game DLL build worker invoked only by the root dev-loop."""

from __future__ import annotations

import hashlib
import json
import os
import shutil
import subprocess
import sys
import threading
import time
import uuid
from datetime import datetime, timezone
from pathlib import Path

from devscripts.build_scheduler import acquire as acquire_build_slot
from devscripts.source_manifest import build_manifest, diff_manifests, validate


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
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n",
                          encoding="utf-8")
    temporary.replace(path)


def _append_timing(path: Path, event: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("a", encoding="utf-8", newline="\n") as output:
        output.write(json.dumps(event, separators=(",", ":")) + "\n")
        output.flush()


def _sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for block in iter(lambda: handle.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


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


def _create_source_snapshot(root: Path, mirror: Path, attempt_id: str) -> Path:
    """Create an immutable per-attempt tree by hardlinking the source mirror.

    The mirror itself is updated with temp-file replacement, so an existing
    snapshot keeps the old inode when the next build refreshes a file.
    """
    destination_root = _snapshot_root(root, attempt_id)
    shutil.rmtree(destination_root, ignore_errors=True)
    linked = copied = 0
    for source_root_name in ("src", "include", "external"):
        source_root = mirror / source_root_name
        if not source_root.is_dir():
            continue
        for source in source_root.rglob("*"):
            if not source.is_file():
                continue
            relative = source.relative_to(mirror)
            destination = destination_root / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            try:
                os.link(source, destination)
                linked += 1
            except OSError:
                shutil.copy2(source, destination)
                copied += 1
    (destination_root / ".snapshot.json").write_text(json.dumps({
        "attempt_id": attempt_id,
        "source_mirror": str(mirror.relative_to(root)),
        "linked": linked,
        "copied": copied,
        "created_utc": _utc_now(),
    }, indent=2) + "\n", encoding="utf-8")
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


def _create_overlay_snapshot(root: Path, base: Path, overlay_paths: list[str],
                             attempt_id: str) -> Path:
    destination_root = _snapshot_root(root, attempt_id)
    if not base.is_dir():
        raise RuntimeError(f"base snapshot missing: {base}")
    shutil.rmtree(destination_root, ignore_errors=True)
    linked = copied = 0
    for source in base.rglob("*"):
        if not source.is_file():
            continue
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


def _prune_snapshots(root: Path, keep: int = 5) -> int:
    snapshots = root / ".dev" / "build-snapshots"
    if not snapshots.is_dir():
        return 0
    protected = {path.name for path in sorted(
        (item for item in snapshots.iterdir() if item.is_dir()),
        key=lambda item: item.stat().st_mtime, reverse=True)[:max(1, keep)]}
    removed = 0
    for path in snapshots.iterdir():
        if not path.is_dir() or path.name in protected:
            continue
        try:
            shutil.rmtree(path)
            removed += 1
        except OSError:
            pass
    return removed


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


def _run_checked(root: Path, command: list[str], on_output,
                 timeout: float | None = None) -> int:
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

    def kill_on_timeout() -> None:
        timed_out.set()
        try:
            subprocess.run(["taskkill", "/F", "/T", "/PID", str(process.pid)],
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                           creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
        except OSError:
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
        if timer is not None:
            timer.cancel()
    if timed_out.is_set():
        return 124
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
        "source_generation": source_generation, "request_ids": request_ids,
        "notes": notes,
    })

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

    def run_stage(name: str, command: list[str],
                  timeout: float | None = None) -> int:
        started = time.monotonic()
        set_stage(name, "started")
        result = _run_checked(root, command, report_output, timeout=timeout)
        set_stage(name, "completed" if result == 0 else "failed",
                  duration_ms=round((time.monotonic() - started) * 1000, 3),
                  exit_code=result)
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

    worker_output_tail: list[str] = []

    def report_output(line: str) -> None:
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
    min_free = _min_free_bytes()
    free = _disk_free_bytes(root)
    if free is not None and free < min_free:
        _prune_generations(root, 5)
        _prune_logs(root, 3)
        free = _disk_free_bytes(root)
    if free is not None and free < min_free:
        return fail_attempt(
            f"disk_low: {free // (1024 * 1024)} MB free, "
            f"need {min_free // (1024 * 1024)} MB")

    for script in ("resident_state_codegen.py", "packet_registry_codegen.py"):
        result = run_stage(f"codegen:{script}",
                           [sys.executable, str(root / "devscripts" / script), "--check"],
                           timeout=_env_float("MIMITA_CODEGEN_TIMEOUT", 300.0))
        if result == 124:
            return fail_attempt(f"codegen timeout: {script}",
                                failed_phase="codegen_timeout")
        if result != 0:
            return fail_attempt(f"codegen check failed: {script}", exit_code=result)

    set_stage("manifest_snapshot", "started")
    manifest_started = time.monotonic()
    old_manifest = _read_json(manifest_path)
    hash_cache_path = root / ".dev" / "source-hash-cache.json"
    hash_cache = _read_json(hash_cache_path)
    source_mirror = _mirror_dir(root)
    manifest: dict = {}
    snapshot_errors: list[str] = []
    snapshot_retries = 0
    try:
        snapshot_attempts = max(1, int(os.environ.get(
            "MIMITA_SNAPSHOT_ATTEMPTS", "3") or 3))
    except ValueError:
        snapshot_attempts = 3
    for snapshot_try in range(snapshot_attempts):
        snapshot_retries = snapshot_try
        manifest = build_manifest(root, hash_cache=hash_cache)
        try:
            _write_json(hash_cache_path, hash_cache)
        except OSError:
            pass
        snapshot_errors = _update_source_mirror(root, source_mirror, manifest)
        if not snapshot_errors:
            break
    snapshot_duration_ms = round((time.monotonic() - manifest_started) * 1000, 3)
    set_stage("manifest_snapshot", "completed", duration_ms=snapshot_duration_ms,
              source_count=len(manifest.get("sources", [])),
              snapshot_retries=snapshot_retries)
    if snapshot_errors:
        return fail_attempt("source snapshot unstable: " + snapshot_errors[0],
                            errors=snapshot_errors[:20])
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
    set_stage("manifest_validation", "completed",
              duration_ms=round((time.monotonic() - validation_started) * 1000, 3),
              source_count=len(manifest.get("sources", [])))
    set_stage("source_snapshot", "started")
    snapshot_started = time.monotonic()
    metadata = build_metadata or {}
    try:
        if (str(metadata.get("kind") or "") == "preview" and
                metadata.get("overlay_paths")):
            base = _base_snapshot_for_generation(
                root, int(metadata.get("base_generation") or 0))
            source_snapshot = _create_overlay_snapshot(
                root, base, [str(value) for value in metadata["overlay_paths"]],
                attempt_id)
        else:
            source_snapshot = _create_source_snapshot(
                root, source_mirror, attempt_id)
    except (OSError, RuntimeError, ValueError) as error:
        return fail_attempt(f"source snapshot failed: {error}",
                            failed_phase="source_snapshot")
    set_stage("source_snapshot", "completed",
              duration_ms=round((time.monotonic() - snapshot_started) * 1000, 3),
              path=str(source_snapshot.relative_to(root)))
    progress["source_snapshot"] = str(source_snapshot.relative_to(root))
    _prune_snapshots(root, 5)
    revision = str(manifest.get("revision", ""))
    change_report = diff_manifests(old_manifest, manifest) if old_manifest else {}
    if change_report:
        change_report["generated_utc"] = _utc_now()
        _write_json(change_report_path, change_report)
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

    command = [
        sys.executable,
        "-u",
        str(root / "devscripts" / "game_build_worker.py"),
        "--manifest", str(manifest_path),
        "--source-root", str(source_snapshot),
        "--out", str(output_root),
        "--revision", revision,
        "--jobs", "0",
        "--priority", "high",
        "--attempt-context", str(context_path),
    ]
    if change_report:
        command.extend(["--change-report", str(change_report_path)])
    if str(mode).lower() == "release":
        command.extend(["--build-mode", "release"])

    if on_output:
        on_output(f"building whole-game revision {revision[:12]}")
    progress["phase"] = "worker_start"
    progress["updated_utc"] = _utc_now()
    _write_json(progress_path, progress)
    build_timeout = _env_float("MIMITA_GAME_BUILD_TIMEOUT", 3600.0)
    slot_timeout = _env_float("MIMITA_GAME_BUILD_SLOT_TIMEOUT", 600.0)
    slot = acquire_build_slot("module", "game-dll", timeout=slot_timeout,
                              metadata={"lane": os.environ.get("MIMITA_LANE_ID", ""),
                                        "root": str(root),
                                        "attempt": attempt_id})
    if slot is None:
        return fail_attempt("build_slot_timeout: no free module build slot",
                            failed_phase="build_slot_timeout")
    try:
        return_code = _run_checked(root, command, report_output,
                                   timeout=build_timeout)
    finally:
        slot.release()
    if return_code == 124:
        set_stage("worker_supervised", "failed", reason="timeout",
                  timeout_seconds=build_timeout)
    current = _read_json(output_root / "current.json")
    last_attempt = _read_json(last_attempt_path)
    attempt_records = (str(last_attempt.get("build_attempt_id") or "") == attempt_id)
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
    if return_code == 124:
        failed_phase = "worker_timeout"
        worker_error = f"worker timed out after {build_timeout:.0f}s"
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
    summary = {
        "schema_version": 1, "build_attempt_id": attempt_id,
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
        "worker_total_seconds": (last_attempt.get("worker_total_seconds")
                                 or current.get("worker_total_seconds")),
        "object_scan_seconds": (last_attempt.get("object_scan_seconds")
                                or current.get("object_scan_seconds")),
        "generation": current.get("generation"), "dll": current.get("dll"),
        "code_hash": current.get("code_hash"),
        "timeline_jsonl": str(timing_path.relative_to(root)),
        "worker_metrics": {key: current.get(key) for key in
                           ("source_count", "compiled", "skipped", "jobs",
                            "object_scan_seconds", "dependency_check_ms",
                            "scan_parse_ms", "scan_stat_ms",
                            "scan_unique_dependencies",
                            "changed_source_preflight_seconds",
                            "compile_seconds", "link_seconds", "publish_seconds",
                            "total_seconds", "worker_total_seconds",
                            "slowest_compiles")},
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
    try:
        return _run_game_build(root, request_ids, mode, source_generation,
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
        return {
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

