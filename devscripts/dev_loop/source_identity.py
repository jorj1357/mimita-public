"""Git identity for the exact source tree behind a lane build."""

from __future__ import annotations

import hashlib
import os
import subprocess
from pathlib import Path
from typing import Any


def source_identity(root: Path) -> dict[str, Any]:
    def git(*args: str) -> bytes:
        kwargs = {
            "cwd": root,
            "check": True,
            "stdout": subprocess.PIPE,
            "stderr": subprocess.PIPE,
        }
        if os.name == "nt":
            kwargs["creationflags"] = subprocess.CREATE_NO_WINDOW
            startupinfo = subprocess.STARTUPINFO()
            startupinfo.dwFlags |= subprocess.STARTF_USESHOWWINDOW
            startupinfo.wShowWindow = 0
            kwargs["startupinfo"] = startupinfo
            kwargs["close_fds"] = True
        return subprocess.run(["git", *args], **kwargs).stdout

    revision = git("rev-parse", "HEAD").decode("ascii", errors="replace").strip()
    status = git("status", "--porcelain", "--untracked-files=all")
    hasher = hashlib.sha256(git("diff", "--binary", "HEAD"))
    for submodule in ("external/libjuice", "external/manifold", "external/recastnavigation"):
        tree_entry = git("ls-tree", "HEAD", "--", submodule).decode(
            "utf-8", errors="replace").strip()
        hasher.update(tree_entry.encode("utf-8", errors="replace"))
    manifold_library = root / "external" / "manifold-prebuilt" / "lib" / "libmanifold.a"
    if manifold_library.is_file():
        hasher.update(b"external/manifold-prebuilt/lib/libmanifold.a\0")
        with manifold_library.open("rb") as handle:
            for chunk in iter(lambda: handle.read(1024 * 1024), b""):
                hasher.update(chunk)
    untracked = git("ls-files", "--others", "--exclude-standard", "-z")
    for entry in untracked.decode("utf-8", errors="replace").split("\0"):
        if not entry:
            continue
        hasher.update(entry.encode("utf-8", errors="replace"))
        try:
            hasher.update((root / entry).read_bytes())
        except OSError:
            hasher.update(b"<unreadable>")
    return {
        "revision": revision,
        "dirty": bool(status),
        "worktree_sha256": hasher.hexdigest(),
    }
