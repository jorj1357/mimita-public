"""Whole-game DLL build worker invoked only by the root dev-loop."""

from __future__ import annotations

import json
import os
import subprocess
import sys
import time
import uuid
from datetime import datetime, timezone
from pathlib import Path

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


def _run_checked(root: Path, command: list[str], on_output) -> int:
    startupinfo = None
    creationflags = 0
    if os.name == "nt":
        startupinfo = subprocess.STARTUPINFO()
        startupinfo.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        startupinfo.wShowWindow = 0
        creationflags = getattr(subprocess, "CREATE_NO_WINDOW", 0)
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
    assert process.stdout is not None
    for line in process.stdout:
        if on_output:
            on_output(line.rstrip())
    return process.wait()


def run_game_build(root: Path, request_ids: list[str], mode: str = "debug",
                   source_generation: int = 0, on_output=None) -> dict:
    """Snapshot, validate, build, and publish one whole-game generation."""
    root = Path(root)
    attempt_id = f"game-dll-{uuid.uuid4().hex[:16]}"
    attempt_started = time.monotonic()
    attempt_started_utc = _utc_now()
    timing_path = root / ".dev" / "build-timings" / f"{attempt_id}.jsonl"
    summary_path = root / ".dev" / "build-timings" / f"{attempt_id}.json"
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

    def run_stage(name: str, command: list[str]) -> int:
        started = time.monotonic()
        set_stage(name, "started")
        result = _run_checked(root, command, report_output)
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
                **fields}

    def report_output(line: str) -> None:
        if not on_output:
            return
        for prefix in ("[GAME_NOTE] ", "[GAME_FILE] ", "[GAME_STAGE] "):
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
                    if prefix == "[GAME_FILE] ":
                        _append_timing(timing_path, {
                            "event": "dev_loop.file_output_handled",
                            "time_utc": _utc_now(), "path": payload.get("path", ""),
                            "status": payload.get("status", ""),
                            "duration_ms": handler_ms,
                            "build_attempt_id": attempt_id,
                            "revision": payload.get("revision", ""),
                        })
                    return
                break
        on_output(line)
    manifest_path = root / "build" / "source-manifest.json"
    output_root = root / "build" / "game"
    change_report_path = root / "build" / "hot-reload" / "change-report.json"
    ready_path = output_root / "ready.json"

    for script in ("resident_state_codegen.py", "packet_registry_codegen.py"):
        result = run_stage(f"codegen:{script}",
                           [sys.executable, str(root / "devscripts" / script), "--check"])
        if result != 0:
            return fail_attempt(f"codegen check failed: {script}", exit_code=result)

    set_stage("manifest_snapshot", "started")
    manifest_started = time.monotonic()
    old_manifest = _read_json(manifest_path)
    manifest = build_manifest(root)
    snapshot_duration_ms = round((time.monotonic() - manifest_started) * 1000, 3)
    set_stage("manifest_snapshot", "completed", duration_ms=snapshot_duration_ms,
              source_count=len(manifest.get("sources", [])))
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
    return_code = _run_checked(root, command, report_output)
    current = _read_json(output_root / "current.json")
    success = (return_code == 0 and str(current.get("status", "")) == "ok"
               and str(current.get("revision", "")) == revision)
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
    _append_timing(timing_path, {
        "event": "build.completed" if success else "build.failed",
        "time_utc": _utc_now(), "elapsed_ms": round(elapsed * 1000, 3),
        "build_attempt_id": attempt_id, "source_generation": source_generation,
        "revision": revision, "exit_code": return_code,
        "generation": current.get("generation"), "dll": current.get("dll"),
        "error": "" if success else (current.get("error") or f"game build failed with exit code {return_code}"),
    })
    worker_progress = _read_json(progress_path)
    if worker_progress.get("build_attempt_id") == attempt_id:
        progress.update(worker_progress)
    summary = {
        "schema_version": 1, "build_attempt_id": attempt_id,
        "source_generation": source_generation, "revision": revision,
        "build_mode": str(mode).lower(),
        "request_ids": request_ids, "notes": notes,
        "changed_paths": progress["changed_paths"],
        "started_utc": attempt_started_utc, "completed_utc": _utc_now(),
        "total_seconds": elapsed, "success": success,
        "generation": current.get("generation"), "dll": current.get("dll"),
        "code_hash": current.get("code_hash"),
        "timeline_jsonl": str(timing_path.relative_to(root)),
        "worker_metrics": {key: current.get(key) for key in
                           ("source_count", "compiled", "skipped", "jobs",
                            "object_scan_seconds", "dependency_check_ms",
                            "cache_record_overhead_ms", "compile_seconds",
                            "link_seconds", "publish_seconds", "total_seconds",
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
    progress["status"] = "published" if success else "failed"
    progress["phase"] = "published" if success else "failed"
    progress["updated_utc"] = _utc_now()
    progress["elapsed_s"] = elapsed
    progress["build_attempt_id"] = attempt_id
    progress["source_generation"] = source_generation
    progress["generation"] = current.get("generation")
    progress["dev_loop_output_handler_ms"] = driver_handler_ms
    _write_json(progress_path, progress)
    try:
        context_path.unlink()
    except OSError:
        pass
    if not success:
        return {
            "success": False,
            "error": current.get("error") or f"game build failed with exit code {return_code}",
            "revision": revision,
            "request_ids": request_ids,
        }

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
        "revision": revision,
        "generation": current.get("generation"),
        "dll": current.get("dll"),
        "code_hash": current.get("code_hash"),
        "request_ids": request_ids,
        "ready": ready,
    }
