"""Build artifact naming, discovery, and sidecar identity checks."""

from __future__ import annotations

import datetime
import json
from pathlib import Path
from typing import Callable


def folder_name_for_id(artifact_id: str) -> str:
    """Map a canonical UTC artifact ID to its Windows-safe directory name."""
    return artifact_id.replace(":", "-")


def build_artifact_id(now: datetime.datetime | None, build_root: Path) -> tuple[str, str]:
    timestamp = now or datetime.datetime.now(datetime.timezone.utc)
    if timestamp.tzinfo is None:
        timestamp = timestamp.replace(tzinfo=datetime.timezone.utc)
    timestamp = timestamp.astimezone(datetime.timezone.utc)
    while True:
        artifact_id = timestamp.isoformat(timespec="microseconds").replace("+00:00", "Z")
        folder_name = folder_name_for_id(artifact_id)
        if not (build_root / folder_name).exists():
            return artifact_id, folder_name
        timestamp += datetime.timedelta(microseconds=1)


def _sort_key(path: Path) -> tuple[int, str]:
    try:
        timestamp = datetime.datetime.strptime(path.name, "%Y-%m-%dT%H-%M-%S.%fZ")
        return (1, timestamp.strftime("%Y%m%d%H%M%S%f"))
    except ValueError:
        return (0, path.name.zfill(20) if path.name.isdigit() else "")


def published_builds(build_root: Path) -> list[Path]:
    if not build_root.is_dir():
        return []
    return sorted(
        (path for path in build_root.iterdir()
         if path.is_dir() and (path.name.isdigit() or _sort_key(path)[1])),
        key=_sort_key,
        reverse=True,
    )


def identity_for_folder(
    folder: Path,
    executable_identity: Callable[[Path], dict],
) -> dict | None:
    exe = folder / "mimita.exe"
    identity = executable_identity(exe)
    if not identity.get("valid"):
        return None
    identity = dict(identity)
    sidecar = folder / "build-identity.json"
    try:
        metadata = json.loads(sidecar.read_text(encoding="utf-8"))
    except (OSError, ValueError, json.JSONDecodeError):
        if not folder.name.isdigit():
            return None
        identity.update({
            "artifact_id": folder.name,
            "build_number": int(folder.name),
            "build_id": f"{folder.name}:{identity.get('sha256', '')}",
            "path": str(exe),
            "valid": True,
            "validation": "valid legacy numbered artifact",
        })
        return identity

    if not isinstance(metadata, dict):
        return None
    artifact_id = str(metadata.get("artifact_id") or "")
    digest = str(identity.get("sha256") or "")
    valid = bool(
        artifact_id
        and folder_name_for_id(artifact_id) == folder.name
        and metadata.get("sha256") == digest
        and metadata.get("build_id") == f"{artifact_id}:{digest}"
        and isinstance(metadata.get("source_generation"), int)
    )
    identity.update(metadata)
    identity.update({
        "path": str(exe),
        "valid": valid,
        "validation": "valid" if valid else "artifact identity metadata mismatch",
        "build_number": None,
    })
    return identity if valid else None


def newest_valid_build(
    build_root: Path,
    executable_identity: Callable[[Path], dict],
) -> tuple[str, str] | None:
    for folder in published_builds(build_root):
        identity = identity_for_folder(folder, executable_identity)
        if identity:
            return str(identity["artifact_id"]), folder.name
    return None
