#!/usr/bin/env python3
# 2026-11-12
# purpose
# One-command live coordinator for the whole-game hot-reload loop.
#
#   python devscripts/live.py
#
# It watches the canonical source tree, debounces completed saves, pins one
# coherent source revision through devscripts/source_manifest.py, and spawns
# devscripts/game_build_worker.py in the background. A successful worker writes
# build/game/current.json and build/game/ready.json; the probe host now polls
# ready.json and activates the generation at a safe boundary.
#
# It never blocks the simulation thread, never relaunches the game, and never
# requests an EXE build. Unknown ownership stops with a diagnostic.
#
# The probe host polls ready.json. The production whole-game cutover is still
# pending.

from __future__ import annotations

import argparse
import json
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from devscripts.source_manifest import build_manifest, diff_manifests, validate  # noqa: E402

WATCH_ROOTS = (
    "src",
    "include",
    "external/recastnavigation",
    "external/libjuice",
    "external/manifold-prebuilt/include",
)
WATCH_FILES = (
    "build.py",
    "build_game_dll.py",
    "CMakeLists.txt",
    "config/build/source-owners.json",
    "config/modules/modules.json",
)
WATCH_SUFFIXES = (".cpp", ".h", ".hpp", ".hh", ".hxx", ".inl", ".ipp", ".json", ".rc")
EXCLUDE_DIRS = {".git", ".dev", "build", "dist", "node_modules", ".venv", "__pycache__"}


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="milliseconds").replace("+00:00", "Z")


def read_json(path: Path) -> dict | None:
    try:
        with open(path, "r", encoding="utf-8") as handle:
            return json.load(handle)
    except (OSError, json.JSONDecodeError):
        return None


def write_json_atomic(path: Path, payload: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    temporary.replace(path)


def watch_files() -> list[Path]:
    files: list[Path] = []
    for name in WATCH_FILES:
        path = ROOT / name
        if path.is_file():
            files.append(path)
    for directory in WATCH_ROOTS:
        base = ROOT / directory
        if not base.is_dir():
            continue
        for path in base.rglob("*"):
            if not path.is_file() or path.suffix.lower() not in WATCH_SUFFIXES:
                continue
            if set(path.relative_to(ROOT).parts) & EXCLUDE_DIRS:
                continue
            files.append(path)
    return files


def snapshot_signature() -> tuple:
    signature = []
    for path in watch_files():
        try:
            stat = path.stat()
        except OSError:
            continue
        signature.append((str(path), stat.st_mtime_ns, stat.st_size))
    return tuple(sorted(signature))


def snapshot_path(out_root: Path, revision: str) -> Path:
    return out_root / "snapshots" / revision / "manifest.json"


def spawn_build(revision: str, out_root: Path) -> subprocess.Popen:
    command = [
        sys.executable,
        str(ROOT / "devscripts" / "game_build_worker.py"),
        "--manifest", str(snapshot_path(out_root, revision)),
        "--out", str(out_root),
        "--revision", revision,
    ]
    process = subprocess.Popen(command, cwd=str(ROOT))
    print(f"[LIVE] build start revision={revision} pid={process.pid}")
    return process


def publish_ready(out_root: Path, revision: str) -> bool:
    current = read_json(out_root / "current.json")
    if not current or current.get("revision") != revision or current.get("status") != "ok":
        return False
    write_json_atomic(out_root / "ready.json", {
        "status": "ready",
        "revision": revision,
        "generation": current.get("generation"),
        "dll": current.get("dll"),
        "code_hash": current.get("code_hash"),
        "published_utc": utc_now(),
    })
    print(f"[LIVE] ready revision={revision} generation={current.get('generation')} "
          f"dll={current.get('dll')}")
    return True


def run_cycle(out_root: Path, last_manifest_path: Path, dry_run: bool) -> dict | None:
    manifest = build_manifest(ROOT)
    errors, warnings = validate(manifest)
    for warning in warnings:
        print(f"[LIVE] WARN {warning}")
    if errors:
        for error in errors:
            print(f"[LIVE] ERROR {error}", file=sys.stderr)
        print("[LIVE] unknown or duplicate ownership; no build requested", file=sys.stderr)
        return None

    previous = read_json(last_manifest_path) if last_manifest_path.is_file() else None
    if previous and previous.get("revision") == manifest["revision"]:
        return manifest

    if previous:
        report = diff_manifests(previous, manifest)
        print(f"[LIVE] change {report['old_revision']} -> {report['new_revision']} "
              f"added={len(report['added'])} removed={len(report['removed'])} "
              f"changed={len(report['changed'])} headers={len(report['changed_headers'])}")
        for path in report["added"]:
            print(f"[LIVE]   added {path}")
        for path in report["removed"]:
            print(f"[LIVE]   removed {path}")
        for entry in report["renamed"]:
            print(f"[LIVE]   renamed {entry['from']} -> {entry['to']}")
    else:
        print(f"[LIVE] initial snapshot revision={manifest['revision']}")

    destination = snapshot_path(out_root, manifest["revision"])
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")

    if dry_run:
        print(f"[LIVE] dry-run: would build revision={manifest['revision']}")
        return manifest

    process = spawn_build(manifest["revision"], out_root)
    return {"manifest": manifest, "process": process}


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="MiMITA live whole-game coordinator")
    parser.add_argument("--once", action="store_true", help="scan once and exit")
    parser.add_argument("--dry-run", action="store_true", help="detect changes but do not build")
    parser.add_argument("--out", default="build/game")
    parser.add_argument("--poll", type=float, default=0.4, help="watch poll seconds")
    parser.add_argument("--settle", type=float, default=0.6, help="save debounce seconds")
    parser.add_argument("--status", action="store_true")
    args = parser.parse_args(argv)

    out_root = (ROOT / args.out).resolve() if not Path(args.out).is_absolute() else Path(args.out)
    last_manifest_path = out_root / "last-manifest.json"

    if args.status:
        current = read_json(out_root / "current.json") or {}
        ready = read_json(out_root / "ready.json") or {}
        print(f"[LIVE] current generation={current.get('generation')} "
              f"revision={current.get('revision')} dll={current.get('dll')}")
        print(f"[LIVE] ready generation={ready.get('generation')} "
              f"revision={ready.get('revision')}")
        return 0

    if args.once:
        result = run_cycle(out_root, last_manifest_path, args.dry_run)
        if isinstance(result, dict) and result.get("process"):
            result["process"].wait()
            if result["process"].returncode == 0:
                publish_ready(out_root, result["manifest"]["revision"])
                write_json_atomic(last_manifest_path, result["manifest"])
        return 0

    print(f"[LIVE] watching {ROOT}; poll={args.poll}s settle={args.settle}s out={out_root}")
    signature = snapshot_signature()
    processed_signature = None
    stable_since = time.monotonic()
    build_process: subprocess.Popen | None = None
    build_revision: str | None = None

    while True:
        time.sleep(args.poll)
        current_signature = snapshot_signature()

        if build_process is not None and build_process.poll() is not None:
            success = build_process.returncode == 0
            if success and build_revision:
                publish_ready(out_root, build_revision)
                manifest = read_json(snapshot_path(out_root, build_revision))
                if manifest:
                    write_json_atomic(last_manifest_path, manifest)
            else:
                print(f"[LIVE] build failed revision={build_revision} "
                      f"exit={build_process.returncode}; active generation unchanged",
                      file=sys.stderr)
            build_process = None
            build_revision = None

        if current_signature != signature:
            signature = current_signature
            stable_since = time.monotonic()
            continue

        if time.monotonic() - stable_since < args.settle:
            continue

        if current_signature == processed_signature:
            continue

        if build_process is not None:
            # Coalesce: the newest stable revision is built after the current one.
            continue

        processed_signature = current_signature
        result = run_cycle(out_root, last_manifest_path, args.dry_run)
        if isinstance(result, dict) and result.get("process"):
            build_process = result["process"]
            build_revision = result["manifest"]["revision"]


if __name__ == "__main__":
    raise SystemExit(main())
