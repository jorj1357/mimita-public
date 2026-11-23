"""Helpers for resolving the net source delta in a sealed Mode 3 batch."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path


def _relative_key(root: Path, value: str) -> str | None:
    candidate = Path(value)
    if not candidate.is_absolute():
        candidate = root / candidate
    try:
        relative = candidate.resolve(strict=False).relative_to(
            root.resolve(strict=False))
    except (OSError, ValueError):
        return None
    return relative.as_posix().casefold()


def _manifest_hashes(path: Path) -> dict[str, str] | None:
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError):
        return None
    if not isinstance(data, dict):
        return None

    hashes: dict[str, str] = {}
    for collection in (data.get("sources"), data.get("headers")):
        if not isinstance(collection, list):
            continue
        for row in collection:
            if not isinstance(row, dict):
                continue
            relative = row.get("path")
            digest = row.get("sha256")
            if isinstance(relative, str) and isinstance(digest, str) and digest:
                hashes[Path(relative).as_posix().casefold()] = digest.casefold()
    config_hashes = data.get("config_hashes")
    if isinstance(config_hashes, dict):
        for relative, digest in config_hashes.items():
            if isinstance(relative, str) and isinstance(digest, str) and digest:
                hashes[Path(relative).as_posix().casefold()] = digest.casefold()
    return hashes or None


def restore_pending_batch(state: object) -> dict | None:
    """Validate a persisted, unsealed Mode 3 batch for daemon startup.

    An active batch belongs to a build worker and is not replayed after a
    daemon restart. Pending path records are safe to restore because the
    normal seal-time manifest/hash check runs again before scheduling.
    """
    if not isinstance(state, dict) or not state.get("batch_pending"):
        return None
    if state.get("active_batch"):
        return None
    try:
        generation = int(state.get("batch_generation", 0))
    except (TypeError, ValueError):
        return None
    raw_targets = state.get("batch_targets")
    raw_paths = state.get("batch_changed_paths")
    if not isinstance(raw_targets, list) or not isinstance(raw_paths, list):
        return None
    targets = {str(value) for value in raw_targets
               if value in {"cold", "game"}}
    rows: dict[str, dict] = {}
    for row in raw_paths:
        if not isinstance(row, dict):
            continue
        path = row.get("path")
        target = row.get("target")
        if not isinstance(path, str) or target not in {"cold", "game"}:
            continue
        rows[path] = dict(row)
    if not targets or not rows:
        return None
    row_targets = {str(row.get("target")) for row in rows.values()}
    if not row_targets.issubset(targets):
        return None
    return {
        "generation": max(0, generation),
        "targets": targets,
        "changed_paths": rows,
        "started_utc": (state.get("batch_started_utc")
                        if isinstance(state.get("batch_started_utc"), str)
                        else None),
    }


def filter_reverted_batch_paths(
        root: Path, rows: list[dict],
        baseline_manifests: dict[str, Path]) -> tuple[list[dict], list[str]]:
    """Drop only queued files whose bytes equal their target's published source.

    Missing manifests, unmapped paths, unreadable files, and deleted files fail
    open: their rows remain in the batch. Only changed paths are hashed; this
    does not scan the repository or dependency graph.
    """
    root = root.resolve(strict=False)
    indexes = {
        target: index
        for target, path in baseline_manifests.items()
        if (index := _manifest_hashes(Path(path))) is not None
    }

    retained: list[dict] = []
    reverted: list[str] = []
    for row in rows:
        if not isinstance(row, dict):
            continue
        relative = row.get("path")
        target = str(row.get("target") or "")
        if not isinstance(relative, str) or target not in indexes:
            retained.append(row)
            continue

        key = _relative_key(root, relative)
        expected = indexes[target].get(key or "")
        if not expected:
            retained.append(row)
            continue

        candidate = Path(relative)
        if not candidate.is_absolute():
            candidate = root / candidate
        try:
            resolved = candidate.resolve(strict=False)
            resolved.relative_to(root)
            if not resolved.is_file():
                retained.append(row)
                continue
            digest = hashlib.sha256(resolved.read_bytes()).hexdigest()
        except (OSError, ValueError):
            retained.append(row)
            continue

        if digest.casefold() == expected:
            reverted.append(Path(relative).as_posix())
        else:
            retained.append(row)

    return retained, reverted


def can_share_source_generation_baseline(
        exe_source_generation: object, game_source_generation: object,
        current_revision: object, attempt_revision: object,
        exe_request_ids: object, game_request_ids: object) -> bool:
    """Whether the current game snapshot can stand in for an EXE baseline.

    Older EXE artifacts may lack a copied source manifest. A game manifest is
    usable for cold-target paths only when both published targets identify the
    same nonzero root source generation, were published by the same Mode 3
    batch, and the game pointer matches its last successful attempt. Generations
    alone are insufficient because they reset when the daemon restarts.
    """
    try:
        exe_generation = int(exe_source_generation)
        game_generation = int(game_source_generation)
    except (TypeError, ValueError):
        return False
    if exe_generation <= 0 or game_generation != exe_generation:
        return False
    def batch_ids(value: object, suffix: str) -> set[str]:
        if not isinstance(value, list):
            return set()
        return {
            str(item)[:-len(suffix)] for item in value
            if str(item or "").endswith(suffix)
        }

    exe_batches = batch_ids(exe_request_ids, ":mode3:cold")
    game_batches = batch_ids(game_request_ids, ":mode3:game")
    if not exe_batches or not (exe_batches & game_batches):
        return False
    current = str(current_revision or "").strip()
    attempt = str(attempt_revision or "").strip()
    return bool(current and attempt and current == attempt)
