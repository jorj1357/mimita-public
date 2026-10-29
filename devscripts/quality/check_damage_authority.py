#!/usr/bin/env python3
"""Report combat health/knockback writes outside the shared damage sink.

Report-only unless --gate is passed.
"""

from __future__ import annotations

import argparse
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SCAN_DIRS = (ROOT / "src",)
SUFFIXES = {".cpp", ".h", ".hpp", ".cc", ".cxx"}

# These files are allowed to own authoritative health/knockback mutation.
ALLOWLIST = {
    "src/network/server-damage.cpp",
}

# Lifecycle/spawn/reset and tests are expected to assign health directly.
ALLOW_PREFIXES = (
    "src/devtools/",
    "tests/",
)

HEALTH_WRITE = re.compile(r"\.health\s*(?:[-+*/]?=)(?!=)")
KNOCKBACK_WRITE = re.compile(r"\.knockbackImpulse\s*[-+*/]?=(?!=)")


def is_allowed(rel: str) -> bool:
    if rel in ALLOWLIST:
        return True
    return any(rel.startswith(prefix) for prefix in ALLOW_PREFIXES)


def scan() -> list[dict]:
    findings: list[dict] = []
    for base in SCAN_DIRS:
        if not base.exists():
            continue
        for path in base.rglob("*"):
            if not path.is_file() or path.suffix.lower() not in SUFFIXES:
                continue
            rel = path.relative_to(ROOT).as_posix()
            if is_allowed(rel):
                continue
            try:
                lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
            except OSError:
                continue
            for number, line in enumerate(lines, 1):
                stripped = line.strip()
                if stripped.startswith("//") or stripped.startswith("*"):
                    continue
                if HEALTH_WRITE.search(line) or KNOCKBACK_WRITE.search(line):
                    findings.append(
                        {
                            "file": rel,
                            "line": number,
                            "text": stripped,
                        }
                    )
    return findings


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--gate", action="store_true", help="exit nonzero on findings")
    args = parser.parse_args()

    findings = scan()
    print(f"[damage-authority] findings={len(findings)}")
    for item in findings[:120]:
        print(f"  {item['file']}:{item['line']}: {item['text']}")
    if len(findings) > 120:
        print(f"  ... {len(findings) - 120} more")
    if args.gate and findings:
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
