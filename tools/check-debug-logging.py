"""Find diagnostic output that bypasses the central StructuredLogger.

This is intentionally a small inventory check, not a replacement logging
framework.  It reports likely diagnostic printf calls and direct log-file
writers so new bypasses remain visible during development.
"""

from __future__ import annotations

import argparse
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SKIP_DIRS = {".git", "build", "logs", "external", "include"}
SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hpp"}
DIAGNOSTIC_MARKERS = (
    "[CAMERA]", "[MAIN MENU]", "[ANIM", "[NPC", "[PROJECTILE",
    "[SERVER", "[CLIENT", "[ICE", "[SOUND]", "[PERFORMANCE]",
    "[SPAWN", "[AIM]", "[FRAME TIMING]",
)


def source_files() -> list[Path]:
    roots = [ROOT / "src", ROOT / "launcher", ROOT / "website"]
    return [
        p for base in roots if base.exists() for p in base.rglob("*")
        if p.is_file() and p.suffix.lower() in SOURCE_SUFFIXES
    ]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--strict", action="store_true",
                        help="return failure when bypasses are found")
    args = parser.parse_args()

    raw_call = re.compile(r"\b(?:std::)?(?:v?printf|puts|fputs|fprintf)\s*\(")
    direct_file = re.compile(
        r"(?:fopen|CreateFileA|std::ofstream|ofstream)\s*\([^\n]*(?:log|debug|diag|trace)",
        re.IGNORECASE,
    )

    violations: list[tuple[str, int, str]] = []
    for path in source_files():
        text = path.read_text(encoding="utf-8", errors="replace")
        lines = text.splitlines()
        for number, line in enumerate(lines, 1):
            start = max(0, text.find(line))
            call_end = min(len(text), start + 1200)
            call_window = text[start:call_end]
            if raw_call.search(line) and any(marker in call_window for marker in DIAGNOSTIC_MARKERS):
                violations.append((str(path.relative_to(ROOT)), number, "raw diagnostic output"))
            if direct_file.search(call_window):
                violations.append((str(path.relative_to(ROOT)), number, "direct diagnostic file"))

    for path, number, reason in violations:
        print(f"{path}:{number}: {reason}")
    print(f"diagnostic_bypasses={len(violations)}")
    return 1 if args.strict and violations else 0


if __name__ == "__main__":
    raise SystemExit(main())
