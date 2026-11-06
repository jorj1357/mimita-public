#!/usr/bin/env python3
"""Small offline project/version store for authored MiMITA map JSON files.

This is a local foundation only. It does not claim to provide multiplayer
authority, distributed transport, or encrypted account backups.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shutil
import sys
import tempfile
from datetime import datetime, timezone
from pathlib import Path


ID_RE = re.compile(r"^[a-zA-Z0-9][a-zA-Z0-9._-]{0,63}$")


def store_root() -> Path:
    base = os.environ.get("LOCALAPPDATA") or os.environ.get("APPDATA")
    if not base:
        base = str(Path.home() / ".local" / "share")
    return Path(base) / "Mimita" / "WorldProjects"


def require_id(value: str) -> str:
    if not ID_RE.fullmatch(value):
        raise ValueError("project id must be 1-64 letters, digits, '.', '_' or '-' ")
    return value


def canonical_json(value: object) -> bytes:
    return (json.dumps(value, ensure_ascii=False, sort_keys=True,
                       separators=(",", ":")) + "\n").encode("utf-8")


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def write_atomic(path: Path, data: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, temp_name = tempfile.mkstemp(prefix=".write-", dir=path.parent)
    try:
        with os.fdopen(fd, "wb") as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temp_name, path)
    finally:
        try:
            os.unlink(temp_name)
        except FileNotFoundError:
            pass


def project_dir(project_id: str) -> Path:
    return store_root() / require_id(project_id)


def read_manifest(project_id: str) -> dict:
    path = project_dir(project_id) / "project.json"
    if not path.is_file():
        raise ValueError(f"project not found: {project_id}")
    data = json.loads(path.read_text(encoding="utf-8"))
    if data.get("format") != 1 or data.get("project_id") != project_id:
        raise ValueError(f"unsupported or invalid project manifest: {path}")
    return data


def save_version(project_id: str, author: str, title: str | None = None) -> str:
    directory = project_dir(project_id)
    manifest = read_manifest(project_id)
    working = directory / "working" / "map.json"
    data = json.loads(working.read_text(encoding="utf-8"))
    if not isinstance(data, dict):
        raise ValueError("map JSON root must be an object")
    payload = canonical_json(data)
    version_hash = digest(payload)
    version_dir = directory / "versions" / version_hash
    map_path = version_dir / "map.json"
    if not map_path.exists():
        write_atomic(map_path, payload)
        version = {
            "version": version_hash,
            "parent": manifest.get("head"),
            "map_sha256": version_hash,
            "created_utc": datetime.now(timezone.utc).isoformat().replace("+00:00", "Z"),
            "author_account_id": author,
        }
        write_atomic(version_dir / "version.json", canonical_json(version))
    if title:
        manifest["title"] = title
    manifest["head"] = version_hash
    manifest["updated_utc"] = datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")
    write_atomic(directory / "project.json", canonical_json(manifest))
    return version_hash


def cmd_create(args: argparse.Namespace) -> None:
    project_id = require_id(args.project_id)
    directory = project_dir(project_id)
    if directory.exists():
        raise ValueError(f"project already exists: {project_id}")
    source = Path(args.map).resolve()
    data = json.loads(source.read_text(encoding="utf-8"))
    if not isinstance(data, dict):
        raise ValueError("map JSON root must be an object")
    directory.mkdir(parents=True)
    write_atomic(directory / "working" / "map.json", canonical_json(data))
    manifest = {
        "format": 1,
        "project_id": project_id,
        "owner_account_id": args.owner,
        "title": args.title or project_id,
        "visibility": args.visibility,
        "license": {
            "code_default": "MIT",
            "media_default": "CC-BY-4.0",
            "dependency_licenses": [],
            "status": "creator-confirmation-required",
        },
        "parent": None,
        "head": None,
        "created_utc": datetime.now(timezone.utc).isoformat().replace("+00:00", "Z"),
    }
    write_atomic(directory / "project.json", canonical_json(manifest))
    version = save_version(project_id, args.owner)
    print(f"created {project_id} version={version} path={directory}")


def cmd_fork(args: argparse.Namespace) -> None:
    source = read_manifest(args.from_project)
    parent_id = source["project_id"]
    parent_version = args.version or source.get("head")
    if not parent_version or not re.fullmatch(r"[0-9a-f]{64}", parent_version):
        raise ValueError("source project has no saved version")
    source_version = project_dir(parent_id) / "versions" / parent_version
    if not source_version.is_dir():
        raise ValueError(f"source version not found: {parent_version}")
    fork_id = require_id(args.project_id)
    target = project_dir(fork_id)
    if target.exists():
        raise ValueError(f"project already exists: {fork_id}")
    shutil.copytree(source_version, target / "versions" / parent_version)
    (target / "working").mkdir(parents=True)
    shutil.copyfile(source_version / "map.json", target / "working" / "map.json")
    manifest = {
        "format": 1,
        "project_id": fork_id,
        "owner_account_id": args.owner,
        "title": args.title or fork_id,
        "visibility": args.visibility,
        "license": source.get("license", {
            "code_default": "MIT",
            "media_default": "CC-BY-4.0",
            "dependency_licenses": [],
            "status": "creator-confirmation-required",
        }),
        "parent": {"project_id": parent_id, "version": parent_version},
        "head": parent_version,
        "created_utc": datetime.now(timezone.utc).isoformat().replace("+00:00", "Z"),
    }
    write_atomic(target / "project.json", canonical_json(manifest))
    print(f"forked {parent_id}@{parent_version} as {fork_id}")


def cmd_save(args: argparse.Namespace) -> None:
    manifest = read_manifest(args.project_id)
    source = Path(args.map).resolve() if args.map else project_dir(args.project_id) / "working" / "map.json"
    data = json.loads(source.read_text(encoding="utf-8"))
    if not isinstance(data, dict):
        raise ValueError("map JSON root must be an object")
    write_atomic(project_dir(args.project_id) / "working" / "map.json", canonical_json(data))
    version = save_version(args.project_id, args.author, args.title)
    print(f"saved {args.project_id}@{version} (parent={manifest.get('head') or 'none'})")


def cmd_list(_: argparse.Namespace) -> None:
    root = store_root()
    for path in sorted(root.iterdir()) if root.is_dir() else []:
        try:
            manifest = read_manifest(path.name)
            print(f"{path.name}\t{manifest.get('head') or 'unsaved'}\t{manifest.get('visibility')}\t{manifest.get('title')}")
        except (ValueError, OSError, json.JSONDecodeError):
            continue


def cmd_show(args: argparse.Namespace) -> None:
    manifest = read_manifest(args.project_id)
    print(json.dumps(manifest, indent=2, ensure_ascii=False))
    versions = project_dir(args.project_id) / "versions"
    for version in sorted(versions.iterdir()) if versions.is_dir() else []:
        print(version.name)


def cmd_open(args: argparse.Namespace) -> None:
    manifest = read_manifest(args.project_id)
    version = args.version or manifest.get("head")
    if not version or not re.fullmatch(r"[0-9a-f]{64}", version):
        raise ValueError("project has no saved version")
    map_path = project_dir(args.project_id) / "versions" / version / "map.json"
    data = map_path.read_bytes()
    if digest(data) != version:
        raise ValueError("version content hash mismatch")
    print(map_path)


def parser() -> argparse.ArgumentParser:
    root = argparse.ArgumentParser(description="Local versioned store for MiMITA map projects")
    sub = root.add_subparsers(dest="command", required=True)
    create = sub.add_parser("create", help="import a map JSON as a new project")
    create.add_argument("project_id"); create.add_argument("map")
    create.add_argument("--owner", required=True); create.add_argument("--title")
    create.add_argument("--visibility", choices=("public", "private"), default="public")
    create.set_defaults(run=cmd_create)
    fork = sub.add_parser("fork", help="fork a saved project version")
    fork.add_argument("from_project"); fork.add_argument("project_id")
    fork.add_argument("--version"); fork.add_argument("--owner", required=True)
    fork.add_argument("--title"); fork.add_argument("--visibility", choices=("public", "private"), default="public")
    fork.set_defaults(run=cmd_fork)
    save = sub.add_parser("save", help="save working map JSON as an immutable version")
    save.add_argument("project_id"); save.add_argument("--map")
    save.add_argument("--author", required=True); save.add_argument("--title")
    save.set_defaults(run=cmd_save)
    show = sub.add_parser("show", help="show project metadata and version hashes")
    show.add_argument("project_id"); show.set_defaults(run=cmd_show)
    open_cmd = sub.add_parser("open", help="verify a version and print its map JSON path")
    open_cmd.add_argument("project_id"); open_cmd.add_argument("--version")
    open_cmd.set_defaults(run=cmd_open)
    listing = sub.add_parser("list", help="list local projects")
    listing.set_defaults(run=cmd_list)
    return root


def main() -> int:
    args = parser().parse_args()
    try:
        args.run(args)
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"world-project: {error}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
