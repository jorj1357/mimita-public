#!/usr/bin/env python3
"""Prune old game-DLL generations and dated log runs.

Keeps the published/current generation, the configured number of newest
generations, and the configured number of newest log date folders. Locked or
in-use files are skipped and reported; the previous generation is never removed.
"""

from __future__ import annotations

import argparse
import json
import shutil
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
GAME_DIR = ROOT / "build" / "game"
LOGS_DIR = ROOT / "logs"


def _read_json(path: Path) -> dict:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return {}
    return value if isinstance(value, dict) else {}


def _dir_size(path: Path) -> int:
    total = 0
    for item in path.rglob("*"):
        try:
            if item.is_file():
                total += item.stat().st_size
        except OSError:
            pass
    return total


def _remove(path: Path, dry_run: bool) -> tuple[bool, int]:
    size = _dir_size(path)
    if dry_run:
        return True, size
    try:
        shutil.rmtree(path)
        return True, size
    except OSError as error:
        print(f"[PRUNE] skipped {path.name}: {error}", file=sys.stderr)
        return False, 0


def prune_generations(keep: int, dry_run: bool) -> int:
    dirs = []
    for path in GAME_DIR.glob("generation-*"):
        if not path.is_dir():
            continue
        try:
            number = int(path.name.split("-", 1)[1])
        except (IndexError, ValueError):
            continue
        dirs.append((number, path))
    if not dirs:
        return 0
    dirs.sort(key=lambda item: item[0], reverse=True)
    protected = {number for number, _ in dirs[:max(1, keep)]}
    for name in ("ready.json", "current.json"):
        generation = _read_json(GAME_DIR / name).get("generation")
        try:
            protected.add(int(generation))
        except (TypeError, ValueError):
            pass
    freed = 0
    removed = 0
    for number, path in dirs:
        if number in protected:
            continue
        ok, size = _remove(path, dry_run)
        if ok:
            freed += size
            removed += 1
    print(f"[PRUNE] generations kept={sorted(protected, reverse=True)} "
          f"removed={removed} freed_gb={freed / (1024 ** 3):.2f}"
          f"{' (dry-run)' if dry_run else ''}")
    return freed


def prune_logs(keep: int, dry_run: bool) -> int:
    if keep <= 0:
        return 0
    dates = sorted((path for path in LOGS_DIR.glob("*")
                    if path.is_dir() and _looks_like_date(path.name)),
                   key=lambda path: path.name, reverse=True)
    if not dates:
        return 0
    keep_set = {path.name for path in dates[:keep]}
    keep_set.add(dates[0].name)
    freed = 0
    removed = 0
    for path in dates:
        if path.name in keep_set:
            continue
        ok, size = _remove(path, dry_run)
        if ok:
            freed += size
            removed += 1
    print(f"[PRUNE] log_dates kept={sorted(keep_set)} "
          f"removed={removed} freed_gb={freed / (1024 ** 3):.2f}"
          f"{' (dry-run)' if dry_run else ''}")
    return freed


def _looks_like_date(name: str) -> bool:
    parts = name.split("-")
    return (len(parts) == 3 and len(parts[0]) == 4 and
            all(part.isdigit() for part in parts))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--keep-generations", type=int, default=5)
    parser.add_argument("--keep-log-days", type=int, default=3)
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--generations-only", action="store_true")
    parser.add_argument("--logs-only", action="store_true")
    args = parser.parse_args()

    freed = 0
    if not args.logs_only:
        freed += prune_generations(max(1, args.keep_generations), args.dry_run)
    if not args.generations_only:
        freed += prune_logs(max(0, args.keep_log_days), args.dry_run)
    print(f"[PRUNE] total_freed_gb={freed / (1024 ** 3):.2f}"
          f"{' (dry-run)' if args.dry_run else ''}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
