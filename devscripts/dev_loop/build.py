"""Canonical incremental build worker for the persistent dev-loop daemon."""

from __future__ import annotations

import os
import hashlib
import json
import subprocess
import sys
import time
import uuid
from pathlib import Path

from devscripts.dev_loop.build_resource_sampler import BuildResourceSampler
from dev_loop.source_identity import source_identity
from devscripts.build_authority import write_authority
from devscripts.path_containment import (
    apply_temp_environment,
    resolve_coordinator_root,
    resolve_within,
)


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def read_progress(root: Path) -> dict:
    try:
        value = json.loads((root / "build" / "progress.json").read_text(
            encoding="utf-8"))
        return value if isinstance(value, dict) else {}
    except (OSError, ValueError, json.JSONDecodeError):
        return {}


def latest_artifact_matches(root: Path, loop) -> tuple[str, str, Path] | None:
    folder = getattr(loop, "latest_build_folder", "") or ""
    if not folder:
        return None
    artifact = root / ".dev" / "builds" / folder
    identity_path = artifact / "build-identity.json"
    executable = artifact / "mimita.exe"
    current = root / "mimita.exe"
    try:
        identity = json.loads(identity_path.read_text(encoding="utf-8"))
    except (OSError, ValueError, json.JSONDecodeError):
        return None
    if not executable.is_file() or not current.is_file():
        return None
    try:
        if _sha256(current) != _sha256(executable):
            return None
    except OSError:
        return None
    artifact_id = str(identity.get("artifact_id") or "")
    if not artifact_id:
        return None
    return artifact_id, folder, artifact


def published_source_identity(root: Path, loop) -> tuple[dict, int] | None:
    """Return the source identity recorded for the latest published EXE.

    Game-DLL-only edits advance the shared source generation without making
    the EXE stale. The identity is provenance for that artifact, not a claim
    about the current worktree. The canonical build result decides whether the
    EXE inputs are still a no-op; reusing provenance avoids several full-tree
    Git scans in that common case.
    """
    folder = getattr(loop, "latest_build_folder", "") or ""
    if not folder:
        return None
    artifact = root / ".dev" / "builds" / folder
    try:
        identity = json.loads((artifact / "build-identity.json").read_text(
            encoding="utf-8"))
    except (OSError, ValueError, json.JSONDecodeError):
        return None
    if not isinstance(identity, dict):
        return None
    if str(identity.get("artifact_id") or "") != str(
            getattr(loop, "latest_build", "") or ""):
        return None
    source = identity.get("source_at_publish") or identity.get(
        "source_at_build_start")
    if not isinstance(source, dict) or not source.get("revision") or not source.get(
            "worktree_sha256"):
        return None
    try:
        source_generation = int(identity.get("source_generation"))
    except (TypeError, ValueError):
        return None
    return dict(source), source_generation


def _run_build_attempt(loop, requested_generation: int, request_ids: list[str],
                       mode: str, *, root: Path, make_artifact_id,
                       publish_build, find_ccache, creationflags: int,
                       hidden_startupinfo,
                       resource_sampler: BuildResourceSampler | None = None,
                       attempt_id: str = "",
                       attempt_started: float | None = None) -> None:
    started = attempt_started if attempt_started is not None else time.monotonic()
    with loop.lock:
        source_generation_at_start = int(loop.generation)
    identity_phase_started = time.monotonic()
    loop._record_build_progress({
        "phase": "source_identity",
        "elapsed_s": 0.0,
        "message": "checking published EXE source identity",
    })
    try:
        source_at_start = None
        source_identity_source_generation = source_generation_at_start
        published_identity = published_source_identity(root, loop)
        if published_identity is not None:
            source_at_start, source_identity_source_generation = published_identity
        source_identity_start_basis = (
            "published_exe_artifact" if source_at_start is not None
            else "full_worktree_git")
        if source_at_start is None:
            source_at_start = source_identity(root)
        source_identity_start_seconds = time.monotonic() - identity_phase_started
    except (OSError, subprocess.SubprocessError):
        source_at_start = {"revision": "unknown", "dirty": True,
                           "worktree_sha256": "unknown"}
        source_identity_start_basis = "unavailable"
        source_identity_start_seconds = time.monotonic() - identity_phase_started
    setup_started = time.monotonic()
    command = [sys.executable, "-u", str(root / "build.py"), "build-only"]
    if mode == "release":
        command.append("release")
    environment = os.environ.copy()
    repo_tmp = root / ".dev" / "tmp"
    repo_tmp.mkdir(parents=True, exist_ok=True)
    environment["TMP"] = str(repo_tmp)
    environment["TEMP"] = str(repo_tmp)
    environment["TMPDIR"] = str(repo_tmp)
    environment["MIMITA_DEV_LOOP_BUILD"] = "1"
    environment["PYTHONUNBUFFERED"] = "1"
    environment["MIMITA_BUILD_AUTHORITY"] = write_authority(
        root, "exe", "dev-loop",
        {"lane": environment.get("MIMITA_LANE_ID", ""),
         "root": str(root)})
    try:
        environment["MIMITA_LANE_ID"] = root.name
    except AttributeError:
        pass
    # Machine-wide policy allows one active EXE build, which uses all logical
    # processors.
    if os.name == "nt":
        creationflags |= 0x00000080  # HIGH_PRIORITY_CLASS
    # Mode 2 lanes share only ccache's content-addressed results. Their object
    # files, PCH, executable, dev-loop state, and journals remain checkout-local.
    coordinator = resolve_coordinator_root(root)
    environment["MIMITA_COORDINATOR_ROOT"] = str(coordinator)
    shared_ccache = resolve_within(
        coordinator, coordinator / ".dev" / "ccache", label="shared ccache")
    shared_ccache.mkdir(parents=True, exist_ok=True)
    environment["CCACHE_DIR"] = str(shared_ccache)
    # Normalize each worktree to its own root so lane and coordinator builds
    # produce the same relative compiler inputs and cache keys.
    environment["CCACHE_BASEDIR"] = str(root)
    environment["CCACHE_NOHASHDIR"] = "1"
    apply_temp_environment(
        coordinator, environment.get("MIMITA_LANE_ID", root.name), environment)
    ccache = find_ccache()
    if ccache:
        environment["MIMITA_CCACHE"] = ccache
        print(f"[DEV] ccache: {ccache}")
    else:
        print("[DEV] ccache: not found; build will still work without it")

    print("[DEV] starting incremental build")
    loop._record_build_progress({
        "phase": "waiting_for_slot",
        "elapsed_s": 0,
        "message": "waiting for the EXE build slot",
    })
    output: list[str] = []
    result = 2
    destination: Path | None = None
    artifact_id: str | None = None
    folder_name: str | None = None
    error_message = ""
    build_script_seconds = 0.0
    post_build_processing_seconds = 0.0
    artifact_validation_seconds = 0.0
    artifact_publish_seconds = 0.0
    source_identity_publish_seconds = 0.0
    source_identity_publish_basis = "not_applicable"
    build_script_started = time.monotonic()
    try:
        process = subprocess.Popen(
            command,
            cwd=root,
            env=environment,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1,
            creationflags=creationflags,
            **({"startupinfo": hidden_startupinfo()} if os.name == "nt" else {}),
        )
        assert process.stdout is not None
        for line in process.stdout:
            print(line, end="")
            output.append(line)
            loop._record_build_output(line)
            progress = read_progress(root)
            if progress:
                loop._record_build_progress(progress)
        result = process.wait()
        build_script_seconds = time.monotonic() - build_script_started
    except OSError as error:
        build_script_seconds = time.monotonic() - build_script_started
        error_message = f"could not start canonical build: {error}"
        loop._record_build_output(error_message)

    post_build_started = time.monotonic()
    successful = result == 0 and (
        "BUILD SUCCESS" in "".join(output)
        or "Nothing changed." in "".join(output)
    )
    if successful:
        try:
            build_text = "".join(output)
            with loop.lock:
                source_generation_at_publish = int(loop.generation)
            source_changed_during_build = (
                source_generation_at_publish != source_generation_at_start)
            source_identity_publish_basis = source_identity_start_basis
            if source_changed_during_build:
                publish_identity_started = time.monotonic()
                try:
                    source_at_publish = source_identity(root)
                    source_identity_source_generation = source_generation_at_publish
                    source_identity_publish_basis = "full_worktree_git"
                except (OSError, subprocess.SubprocessError):
                    source_at_publish = {"revision": "unknown", "dirty": True,
                                         "worktree_sha256": "unknown"}
                    source_identity_publish_basis = "unavailable"
                source_identity_publish_seconds = (
                    time.monotonic() - publish_identity_started)
            elif (source_identity_start_basis == "published_exe_artifact"
                  and "Nothing changed." not in build_text):
                # The compiler found a cold input that the watcher did not
                # classify as stale. Refresh once after the build, then use
                # that identity at both ends because the watched generation
                # stayed stable for the whole attempt.
                publish_identity_started = time.monotonic()
                try:
                    source_at_publish = source_identity(root)
                    source_at_start = source_at_publish
                    source_identity_source_generation = source_generation_at_publish
                    source_identity_start_basis = "full_worktree_git_after_stable_build"
                    source_identity_publish_basis = "full_worktree_git"
                except (OSError, subprocess.SubprocessError):
                    source_at_publish = {"revision": "unknown", "dirty": True,
                                         "worktree_sha256": "unknown"}
                    source_identity_publish_basis = "unavailable"
                source_identity_publish_seconds = (
                    time.monotonic() - publish_identity_started)
            else:
                source_at_publish = source_at_start
            artifact_validation_started = time.monotonic()
            reused = latest_artifact_matches(root, loop)
            artifact_validation_seconds = (
                time.monotonic() - artifact_validation_started)
            if reused and ("Nothing changed." in build_text or
                           source_at_start == source_at_publish):
                artifact_id, folder_name, destination = reused
                print(f"[DEV] reusing artifact {folder_name} (EXE unchanged)")
            else:
                loop._record_build_progress({
                    "phase": "publishing",
                    "message": "publishing the validated executable",
                })
                artifact_id, folder_name = make_artifact_id()
                artifact_publish_started = time.monotonic()
                destination = publish_build(artifact_id, folder_name,
                                           requested_generation, mode,
                                           source_at_start, source_at_publish,
                                           request_ids)
                artifact_publish_seconds = (
                    time.monotonic() - artifact_publish_started)
            loop._record_build_progress({
                "phase": "completed",
                "artifact_id": artifact_id,
                "folder_name": folder_name,
                "message": "build artifact published",
            })
        except Exception as error:
            successful = False
            error_message = f"publish failed: {error}"
            loop._record_build_output(error_message)

    post_build_processing_seconds = time.monotonic() - post_build_started

    output_log_started = time.monotonic()
    try:
        (root / ".dev" / "build-output.log").write_text(
            "".join(output), encoding="utf-8", errors="replace")
    except OSError:
        pass
    output_log_seconds = time.monotonic() - output_log_started

    with loop.lock:
        source_generation_at_completion = int(loop.generation)
    source_changed_during_build = (
        source_generation_at_completion != source_generation_at_start)
    event_started = time.monotonic()
    try:
        loop.record_runtime_event(
            "dev-loop.build.source_identity",
            {"request_ids": request_ids,
             "source_generation_start": source_generation_at_start,
             "source_generation_end": locals().get(
                 "source_generation_at_publish", source_generation_at_completion),
             "identity_source_generation": source_identity_source_generation,
             "source_changed_during_build": source_changed_during_build,
             "start_seconds": round(source_identity_start_seconds, 3),
             "publish_seconds": round(locals().get(
                 "source_identity_publish_seconds", 0.0), 3),
             "start_basis": source_identity_start_basis,
             "publish_basis": locals().get(
                 "source_identity_publish_basis", "not_applicable")},
            "EXE build source identity timing",
        )
    except Exception:
        pass

    runtime_event_seconds = time.monotonic() - event_started
    resource_summary = resource_sampler.stop() if resource_sampler else None
    total_worker_seconds = time.monotonic() - started
    setup_seconds = max(0.0, build_script_started - setup_started)
    accounted_seconds = sum((
        source_identity_start_seconds, setup_seconds, build_script_seconds,
        post_build_processing_seconds, output_log_seconds, runtime_event_seconds,
    ))
    worker_timing = {
        "total_worker_seconds": round(total_worker_seconds, 3),
        "source_identity_start_seconds": round(source_identity_start_seconds, 3),
        "source_identity_publish_seconds": round(source_identity_publish_seconds, 3),
        "source_identity_start_basis": source_identity_start_basis,
        "source_identity_publish_basis": source_identity_publish_basis,
        "source_identity_source_generation": source_identity_source_generation,
        "setup_seconds": round(setup_seconds, 3),
        "build_script_seconds": round(build_script_seconds, 3),
        "post_build_processing_seconds": round(post_build_processing_seconds, 3),
        "artifact_validation_seconds": round(artifact_validation_seconds, 3),
        "artifact_publish_seconds": round(artifact_publish_seconds, 3),
        "output_log_seconds": round(output_log_seconds, 3),
        "runtime_event_seconds": round(runtime_event_seconds, 3),
        "unattributed_seconds": round(max(0.0, total_worker_seconds - accounted_seconds), 3),
        "source_generation_start": source_generation_at_start,
        "source_generation_end": source_generation_at_completion,
        "source_changed_during_build": source_changed_during_build,
        "build_attempt_id": attempt_id,
        "resource_sampling": resource_summary,
    }

    loop.build_completions.put({
        "generation": requested_generation,
        "mode": mode,
        "request_ids": request_ids,
        "success": successful,
        "return_code": result,
        "artifact_id": artifact_id,
        "source_revision": source_at_start.get("revision"),
        "source_dirty": source_at_start.get("dirty"),
        "source_worktree_sha256": source_at_start.get("worktree_sha256"),
        "source_changed_during_build": source_changed_during_build,
        "folder_name": folder_name,
        "destination": str(destination) if destination else "",
        "error": error_message,
        "duration_seconds": total_worker_seconds,
        "worker_timing": worker_timing,
        "source_identity": {
            "start_seconds": round(source_identity_start_seconds, 3),
            "publish_seconds": round(locals().get(
                "source_identity_publish_seconds", 0.0), 3),
            "start_basis": source_identity_start_basis,
            "publish_basis": locals().get(
                "source_identity_publish_basis", "not_applicable"),
            "source_generation_start": source_generation_at_start,
            "source_generation_end": locals().get(
                "source_generation_at_publish", source_generation_at_start),
            "identity_source_generation": source_identity_source_generation,
        },
        "output_tail": [line.rstrip()[-240:] for line in output[-10:]],
    })


def run_build(loop, requested_generation: int, request_ids: list[str], mode: str,
              *, root: Path, make_artifact_id, publish_build, find_ccache,
              creationflags: int, hidden_startupinfo) -> None:
    """Run one EXE build with the same resource sidecar used for game DLLs."""
    root = Path(root)
    attempt_id = (f"exe-build-{time.strftime('%Y%m%dT%H%M%S')}-"
                  f"{uuid.uuid4().hex[:8]}")
    attempt_started = time.monotonic()
    sampler = BuildResourceSampler(
        root, attempt_id, progress_path=root / "build" / "progress.json")
    try:
        sampler.start()
    except Exception as error:  # best-effort telemetry must not block the build
        sampler._errors.append(
            f"sampler start failed: {type(error).__name__}: {error}"[:500])
        sampler._status = "unavailable"
    try:
        loop._record_build_progress({
            "phase": "setup", "build_attempt_id": attempt_id,
            "message": "starting EXE build with resource sampling",
        })
        _run_build_attempt(
            loop, requested_generation, request_ids, mode, root=root,
            make_artifact_id=make_artifact_id, publish_build=publish_build,
            find_ccache=find_ccache, creationflags=creationflags,
            hidden_startupinfo=hidden_startupinfo,
            resource_sampler=sampler, attempt_id=attempt_id,
            attempt_started=attempt_started)
    finally:
        summary = sampler.stop()
        # A normal attempt carries the summary in its completion record. This
        # state update also exposes it if orchestration raised before enqueue.
        try:
            progress = read_progress(root)
            phase = str(progress.get("phase") or "setup")
            loop._record_build_progress({
                **progress,
                "phase": phase,
                "build_attempt_id": attempt_id,
                "resource_sampling": summary,
                "message": progress.get("message") or
                           "EXE build resource sampling complete",
            })
        except Exception:
            pass
