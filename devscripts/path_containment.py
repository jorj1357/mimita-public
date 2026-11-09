"""Repository path containment for agent lanes and build tooling.

Every agent-created or agent-read project path must resolve inside this
checkout. This module is the single owner for that decision so lane metadata,
scheduler state, caches, temporary files, builds, and journals fail closed
instead of silently using a sibling folder, a user-profile directory, or the
system temporary directory.
"""

from __future__ import annotations

import os
import re
from pathlib import Path


_SAFE_NAME = re.compile(r"[^A-Za-z0-9_.-]+")


def _resolved(path: Path | str) -> Path:
    return Path(os.path.realpath(os.fspath(path)))


def repository_root(start: Path | str | None = None) -> Path:
    """Return the resolved repository root for a source file or directory."""
    if start is None:
        start = Path(__file__).resolve().parents[1]
    return _resolved(start)


def is_within(root: Path | str, candidate: Path | str) -> bool:
    root_path = _resolved(root)
    candidate_path = _resolved(candidate)
    try:
        candidate_path.relative_to(root_path)
        return True
    except ValueError:
        return False


def resolve_within(root: Path | str, candidate: Path | str, *,
                   label: str = "path", must_exist: bool = False) -> Path:
    """Resolve a path and reject it unless it stays inside ``root``."""
    root_path = _resolved(root)
    candidate_path = Path(candidate)
    if not candidate_path.is_absolute():
        candidate_path = root_path / candidate_path
    resolved = _resolved(candidate_path)
    if must_exist and not resolved.exists():
        raise FileNotFoundError(f"{label} does not exist: {resolved}")
    if not is_within(root_path, resolved):
        raise RuntimeError(f"{label} escapes repository {root_path}: {resolved}")
    return resolved


def resolve_coordinator_root(build_root: Path | str) -> Path:
    """Resolve the coordinator repository that owns a build root.

    The normal checkout is its own coordinator. A lane worktree must be an
    exact child of ``<coordinator>/.dev/agent-lanes``. Any other configured
    coordinator is rejected so a build cannot read or write another tree.
    """
    root = _resolved(build_root)
    configured = os.environ.get("MIMITA_COORDINATOR_ROOT", "").strip()
    if not configured:
        return root
    coordinator = _resolved(configured)
    if coordinator == root:
        return coordinator
    lane_root = _resolved(coordinator / ".dev" / "agent-lanes")
    if root.parent == lane_root and is_within(lane_root, root):
        return coordinator
    raise RuntimeError(
        "MIMITA_COORDINATOR_ROOT is not the repository that owns this build "
        f"root ({root}): {coordinator}")


def safe_name(value: str, *, fallback: str = "main") -> str:
    cleaned = _SAFE_NAME.sub("_", str(value or "").strip()).strip("._-")
    return cleaned[:48] or fallback


def repository_temp_dir(root: Path | str, name: str) -> Path:
    """Create and return ``<repo>/.dev/tmp/<safe-name>``."""
    root_path = _resolved(root)
    path = resolve_within(
        root_path, root_path / ".dev" / "tmp" / safe_name(name),
        label="temporary directory")
    path.mkdir(parents=True, exist_ok=True)
    return path


def apply_temp_environment(root: Path | str, name: str,
                           environment: dict[str, str]) -> Path:
    """Point child-process temporary-file variables inside the repository."""
    path = repository_temp_dir(root, name)
    for variable in ("TMP", "TEMP", "TMPDIR"):
        environment[variable] = str(path)
    return path
