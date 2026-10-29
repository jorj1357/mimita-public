#!/usr/bin/env python3
"""Report dead-code candidates using cppcheck when available plus local markers."""

from __future__ import annotations

import argparse
import shutil
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MARKERS = ("TODO-DELETE", "#if 0")
SCAN_DIRS = (ROOT / "src" / "combat", ROOT / "src" / "network")
SUFFIXES = {".cpp", ".h", ".hpp", ".cc", ".cxx"}


def scan_markers() -> list[dict]:
    findings: list[dict] = []
    for base in SCAN_DIRS:
        if not base.exists():
            continue
        for path in base.rglob("*"):
            if not path.is_file() or path.suffix.lower() not in SUFFIXES:
                continue
            rel = path.relative_to(ROOT).as_posix()
            try:
                lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
            except OSError:
                continue
            for number, line in enumerate(lines, 1):
                for marker in MARKERS:
                    if marker in line:
                        findings.append(
                            {"file": rel, "line": number, "marker": marker, "text": line.strip()}
                        )
    return findings


def run_cppcheck() -> str:
    exe = shutil.which("cppcheck")
    if not exe:
        return "cppcheck not installed"
    command = [
        exe,
        "--enable=unusedFunction",
        "--std=c++20",
        "--quiet",
        "--template={file}:{line}: {message} [{id}]",
        str(ROOT / "src" / "combat"),
        str(ROOT / "src" / "network"),
    ]
    try:
        result = subprocess.run(command, capture_output=True, text=True, timeout=300)
    except (OSError, subprocess.TimeoutExpired) as error:
        return f"cppcheck failed: {error}"
    return (result.stdout or "").strip() or "cppcheck: no unusedFunction findings"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--gate", action="store_true")
    args = parser.parse_args()

    markers = scan_markers()
    print(f"[dead-code] markers={len(markers)}")
    for item in markers[:120]:
        print(f"  {item['file']}:{item['line']}: {item['marker']} {item['text']}")
    if len(markers) > 120:
        print(f"  ... {len(markers) - 120} more")

    cppcheck = run_cppcheck()
    print("[dead-code] cppcheck:")
    for line in cppcheck.splitlines()[:80]:
        print(f"  {line}")

    if args.gate and markers:
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
