#!/usr/bin/env python3
"""Report likely duplicate contact/damage owners and weapon-ID special cases."""

from __future__ import annotations

import argparse
import re
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SCAN_DIRS = (ROOT / "src" / "combat", ROOT / "src" / "network")
SUFFIXES = {".cpp", ".h", ".hpp", ".cc", ".cxx"}

WEAPON_ID_BRANCH = re.compile(
    r"(?:def(?:inition)?|stepDef|weaponDef|weapon|wdef)(?:->|\.)id\s*==\s*\""
)
FUNCTION_START = re.compile(
    r"^\s*(?:static\s+|inline\s+|virtual\s+|constexpr\s+|extern\s+)*"
    r"(?:[\w:<>,\s\*&\[\]]+)\s+(\w+)\s*\([^;]*\)\s*(?:const\s*)?\{"
)
IGNORED_FUNCTIONS = {
    "main",
    "if",
    "for",
    "while",
    "switch",
    "catch",
    "return",
    "sizeof",
}


def iter_sources():
    for base in SCAN_DIRS:
        if not base.exists():
            continue
        for path in base.rglob("*"):
            if path.is_file() and path.suffix.lower() in SUFFIXES:
                yield path


def scan_weapon_branches() -> list[dict]:
    findings: list[dict] = []
    for path in iter_sources():
        rel = path.relative_to(ROOT).as_posix()
        try:
            lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
        except OSError:
            continue
        for number, line in enumerate(lines, 1):
            if WEAPON_ID_BRANCH.search(line):
                findings.append({"file": rel, "line": number, "text": line.strip()})
    return findings


def scan_duplicate_functions() -> list[dict]:
    names: dict[str, list[tuple[str, int]]] = defaultdict(list)
    for path in iter_sources():
        rel = path.relative_to(ROOT).as_posix()
        try:
            lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
        except OSError:
            continue
        for number, line in enumerate(lines, 1):
            match = FUNCTION_START.match(line)
            if not match:
                continue
            name = match.group(1)
            if name in IGNORED_FUNCTIONS or len(name) < 4:
                continue
            names[name].append((rel, number))

    findings: list[dict] = []
    for name, locations in sorted(names.items()):
        files = {file for file, _ in locations}
        if len(files) > 1:
            findings.append(
                {
                    "name": name,
                    "locations": [f"{file}:{line}" for file, line in locations],
                }
            )
    return findings


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--gate", action="store_true")
    args = parser.parse_args()

    branches = scan_weapon_branches()
    duplicates = scan_duplicate_functions()

    print(f"[contact-ownership] weapon_id_branches={len(branches)} duplicate_function_names={len(duplicates)}")
    for item in branches[:80]:
        print(f"  branch {item['file']}:{item['line']}: {item['text']}")
    for item in duplicates[:80]:
        print(f"  duplicate {item['name']}: {', '.join(item['locations'])}")

    if args.gate and (branches or duplicates):
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
