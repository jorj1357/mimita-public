#!/usr/bin/env python3
"""Report repeated normalized source blocks without external dependencies."""

from __future__ import annotations

import argparse
import hashlib
import re
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SCAN_DIRS = (ROOT / "src" / "combat", ROOT / "src" / "network", ROOT / "src" / "physics" / "movement")
SUFFIXES = {".cpp", ".h", ".hpp", ".cc", ".cxx"}
WINDOW = 10
MIN_CHARS = 20


def normalize(line: str) -> str:
    line = line.strip()
    line = re.sub(r"//.*$", "", line)
    line = re.sub(r"\s+", " ", line)
    return line


def blocks() -> dict[str, list[tuple[str, int, int]]]:
    found: dict[str, list[tuple[str, int, int]]] = defaultdict(list)
    for base in SCAN_DIRS:
        if not base.exists():
            continue
        for path in base.rglob("*"):
            if not path.is_file() or path.suffix.lower() not in SUFFIXES:
                continue
            rel = path.relative_to(ROOT).as_posix()
            try:
                raw = path.read_text(encoding="utf-8", errors="replace").splitlines()
            except OSError:
                continue
            lines = [normalize(line) for line in raw]
            useful = [
                (number, text)
                for number, text in enumerate(lines, 1)
                if text and not text.startswith(("#include", "#pragma"))
            ]
            for start in range(0, len(useful) - WINDOW + 1):
                chunk = useful[start : start + WINDOW]
                text = "\n".join(item[1] for item in chunk)
                if len(text) < MIN_CHARS * WINDOW:
                    continue
                digest = hashlib.sha1(text.encode("utf-8")).hexdigest()
                found[digest].append((rel, chunk[0][0], chunk[-1][0]))
    return found


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--gate", action="store_true")
    parser.add_argument("--limit", type=int, default=30)
    args = parser.parse_args()

    repeated = {key: value for key, value in blocks().items() if len(value) > 1}
    print(f"[duplicate-report] repeated_windows={len(repeated)}")
    shown = 0
    for _digest, locations in sorted(repeated.items(), key=lambda item: -len(item[1])):
        files = {file for file, _, _ in locations}
        if len(files) < 2 and len(locations) < 2:
            continue
        print(f"  x{len(locations)} " + ", ".join(f"{f}:{a}-{b}" for f, a, b in locations[:6]))
        shown += 1
        if shown >= args.limit:
            break
    if args.gate and repeated:
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
