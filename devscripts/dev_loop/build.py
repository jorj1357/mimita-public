"""Canonical incremental build worker for the persistent dev-loop daemon."""

from __future__ import annotations

import os
import json
import subprocess
import sys
import time
from pathlib import Path

from dev_loop.source_identity import source_identity
from devscripts.build_authority import write_authority


def run_build(loop, requested_generation: int, request_ids: list[str], mode: str,
              *, root: Path, make_artifact_id, publish_build, find_ccache,
              creationflags: int, hidden_startupinfo) -> None:
    started = time.monotonic()
    try:
        source_at_start = source_identity(root)
    except (OSError, subprocess.SubprocessError):
        source_at_start = {"revision": "unknown", "dirty": True,
                           "worktree_sha256": "unknown"}
    command = [sys.executable, str(root / "build.py"), "build-only"]
    if mode == "release":
        command.append("release")
    environment = os.environ.copy()
    environment["MIMITA_DEV_LOOP_BUILD"] = "1"
    environment["MIMITA_BUILD_AUTHORITY"] = write_authority(
        root, "exe", "dev-loop",
        {"lane": environment.get("MIMITA_LANE_ID", ""),
         "root": str(root)})
    try:
        environment["MIMITA_LANE_ID"] = root.name
    except AttributeError:
        pass
    # Machine-wide policy is one EXE build and two module builds. Keep this
    # builder at half the logical processors or less; 16 GB RAM must also host
    # the game and the editor.
    try:
        jobs = int(environment.get("MIMITA_BUILD_JOBS", "4"))
    except ValueError:
        jobs = 4
    environment["MIMITA_BUILD_JOBS"] = str(max(1, min(4, jobs)))
    if os.name == "nt":
        creationflags |= 0x00004000  # BELOW_NORMAL_PRIORITY_CLASS
    # Mode 2 lanes share only ccache's content-addressed results. Their object
    # files, PCH, executable, dev-loop state, and journals remain checkout-local.
    lane_job_config = root / ".dev" / "agent-lane-build.json"
    try:
        lane_jobs = json.loads(lane_job_config.read_text(encoding="utf-8")).get("jobs")
        if isinstance(lane_jobs, int) and 1 <= lane_jobs <= 256:
            environment["MIMITA_BUILD_JOBS"] = str(max(1, min(4, lane_jobs)))
        shared_ccache = root.parent / ".dev" / "shared-agent-ccache"
        shared_ccache.mkdir(parents=True, exist_ok=True)
        environment["CCACHE_DIR"] = str(shared_ccache)
        environment["CCACHE_BASEDIR"] = str(root.parent)
    except (OSError, ValueError, AttributeError, json.JSONDecodeError):
        pass
    ccache = find_ccache()
    if ccache:
        environment["MIMITA_CCACHE"] = ccache
        print(f"[DEV] ccache: {ccache}")
    else:
        print("[DEV] ccache: not found; build will still work without it")

    print("[DEV] starting incremental build")
    output: list[str] = []
    result = 2
    destination: Path | None = None
    artifact_id: str | None = None
    folder_name: str | None = None
    error_message = ""
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
        result = process.wait()
    except OSError as error:
        error_message = f"could not start canonical build: {error}"
        loop._record_build_output(error_message)

    successful = result == 0 and (
        "BUILD SUCCESS" in "".join(output)
        or "Nothing changed." in "".join(output)
    )
    if successful:
        try:
            try:
                source_at_publish = source_identity(root)
            except (OSError, subprocess.SubprocessError):
                source_at_publish = {"revision": "unknown", "dirty": True,
                                     "worktree_sha256": "unknown"}
            artifact_id, folder_name = make_artifact_id()
            destination = publish_build(artifact_id, folder_name,
                                       requested_generation, mode,
                                       source_at_start, source_at_publish)
        except Exception as error:
            successful = False
            error_message = f"publish failed: {error}"
            loop._record_build_output(error_message)

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
        "source_changed_during_build": source_at_start != locals().get("source_at_publish", source_at_start),
        "folder_name": folder_name,
        "destination": str(destination) if destination else "",
        "error": error_message,
        "duration_seconds": max(0.0, time.monotonic() - started),
        "output_tail": [line.rstrip()[-240:] for line in output[-10:]],
    })
