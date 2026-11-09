"""Canonical incremental build worker for the persistent dev-loop daemon."""

from __future__ import annotations

import os
import subprocess
import sys
import time
from pathlib import Path

from dev_loop.source_identity import source_identity
from devscripts.build_authority import write_authority
from devscripts.path_containment import (
    apply_temp_environment,
    resolve_coordinator_root,
    resolve_within,
)


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
    environment["CCACHE_BASEDIR"] = str(coordinator)
    apply_temp_environment(
        coordinator, environment.get("MIMITA_LANE_ID", root.name), environment)
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
