"""Whole-game DLL build worker invoked only by the root dev-loop."""

from __future__ import annotations

import json
import os
import subprocess
import sys
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


def _run_checked(root: Path, command: list[str], on_output) -> int:
    process = subprocess.Popen(
        command,
        cwd=root,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        bufsize=1,
        env={**os.environ, "PYTHONUNBUFFERED": "1"},
    )
    assert process.stdout is not None
    for line in process.stdout:
        if on_output:
            on_output(line.rstrip())
    return process.wait()


def run_game_build(root: Path, request_ids: list[str], mode: str = "debug",
                   on_output=None) -> dict:
    """Snapshot, validate, build, and publish one whole-game generation."""
    root = Path(root)
    manifest_path = root / "build" / "source-manifest.json"
    output_root = root / "build" / "game"
    change_report_path = root / "build" / "hot-reload" / "change-report.json"
    ready_path = output_root / "ready.json"

    for script in ("resident_state_codegen.py", "packet_registry_codegen.py"):
        result = _run_checked(
            root,
            [sys.executable, str(root / "devscripts" / script), "--check"],
            on_output,
        )
        if result != 0:
            return {
                "success": False,
                "error": f"codegen check failed: {script}",
                "request_ids": request_ids,
            }

    old_manifest = _read_json(manifest_path)
    manifest = build_manifest(root)
    errors, warnings = validate(manifest)
    if errors:
        return {
            "success": False,
            "error": "source manifest validation failed",
            "errors": errors,
            "warnings": warnings,
            "request_ids": request_ids,
        }

    _write_json(manifest_path, manifest)
    revision = str(manifest.get("revision", ""))
    change_report = diff_manifests(old_manifest, manifest) if old_manifest else {}
    if change_report:
        change_report["generated_utc"] = _utc_now()
        _write_json(change_report_path, change_report)

    command = [
        sys.executable,
        "-u",
        str(root / "devscripts" / "game_build_worker.py"),
        "--manifest", str(manifest_path),
        "--out", str(output_root),
        "--revision", revision,
        "--jobs", "0",
        "--priority", "high",
    ]
    if change_report:
        command.extend(["--change-report", str(change_report_path)])
    if str(mode).lower() == "release":
        command.extend(["--build-mode", "release"])

    if on_output:
        on_output(f"building whole-game revision {revision[:12]}")
    return_code = _run_checked(root, command, on_output)
    current = _read_json(output_root / "current.json")
    if return_code != 0 or str(current.get("status", "")) != "ok":
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
