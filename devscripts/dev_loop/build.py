"""Canonical incremental build worker for the persistent dev-loop daemon."""

from __future__ import annotations

import os
import subprocess
import sys
import time
from pathlib import Path


def run_build(loop, requested_generation: int, request_ids: list[str], mode: str,
              *, root: Path, make_artifact_id, publish_build, find_ccache,
              creationflags: int, hidden_startupinfo) -> None:
    started = time.monotonic()
    command = [sys.executable, str(root / "build.py"), "build-only"]
    if mode == "release":
        command.append("release")
    environment = os.environ.copy()
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
            artifact_id, folder_name = make_artifact_id()
            destination = publish_build(artifact_id, folder_name,
                                       requested_generation, mode)
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
        "folder_name": folder_name,
        "destination": str(destination) if destination else "",
        "error": error_message,
        "duration_seconds": max(0.0, time.monotonic() - started),
        "output_tail": [line.rstrip()[-240:] for line in output[-10:]],
    })
