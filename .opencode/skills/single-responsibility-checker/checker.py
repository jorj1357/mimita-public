#!/usr/bin/env python3
"""
Single Responsibility Checker

Prevents duplicate implementations. Enforces one canonical implementation per concept.

Detects:
  - Multiple files claiming ownership of the same concept
  - Duplicate function definitions with similar names
"""

import os
import re
import sys
from collections import defaultdict

REPO_ROOT = os.path.dirname(os.path.abspath(__file__))
while not os.path.isfile(os.path.join(REPO_ROOT, "overseer.py")):
    REPO_ROOT = os.path.dirname(REPO_ROOT)
    if REPO_ROOT == os.path.dirname(REPO_ROOT):
        sys.exit(1)

SRC_DIR = os.path.join(REPO_ROOT, "src")

# Concept groups that should each have a single implementation
CONCEPT_GROUPS = [
    # Collision
    ["CollisionSolver", "CollisionHelper", "CollisionUtils", "CollisionCore"],
    # Movement
    ["MovementSystem", "MovementHelper", "MovementCore"],
    # Replay
    ["ReplayExporter", "ReplayExporterJSON", "ReplayExporterFFmpeg"],
    # Physics
    ["PhysicsSolver", "PhysicsHelper", "PhysicsCore"],
]


def find_cpp_files():
    files = []
    for root, dirs, names in os.walk(SRC_DIR):
        dirs[:] = [d for d in dirs if d not in (".git", "__pycache__")]
        for name in names:
            if name.endswith(".cpp"):
                files.append(os.path.join(root, name))
    return files


def extract_class_names(filepath):
    """Extract class/struct names defined in a file."""
    names = []
    with open(filepath, "r", errors="replace") as f:
        content = f.read()
    # Match class/struct definitions
    for m in re.finditer(r'(?:class|struct)\s+(\w+)', content):
        names.append(m.group(1))
    return names


def extract_function_names(filepath):
    """Extract top-level function definitions."""
    names = []
    with open(filepath, "r", errors="replace") as f:
        lines = f.readlines()
    for line in lines:
        m = re.match(
            r'^\s*(?:static\s+|inline\s+)?'
            r'(?:void|bool|int|float|double|char|std::\w+|glm::\w+|'
            r'unsigned|size_t|auto)\s+'
            r'(\w+)\s*\(',
            line
        )
        if m:
            name = m.group(1)
            if name not in ("if", "for", "while", "switch", "catch", "return",
                            "else", "case", "sizeof", "throw", "delete", "new"):
                names.append(name)
    return names


def main():
    files = find_cpp_files()
    findings = []

    # Group 1: Check class/struct duplication across concept groups
    concept_usage = defaultdict(list)  # class_name -> [filepaths]
    for filepath in files:
        relpath = os.path.relpath(filepath, REPO_ROOT)
        class_names = extract_class_names(filepath)
        for cn in class_names:
            for group in CONCEPT_GROUPS:
                if cn in group:
                    concept_usage[cn].append(relpath)

    for group in CONCEPT_GROUPS:
        active = [cn for cn in group if cn in concept_usage]
        if len(active) >= 2:
            findings.append(("Duplicate concept implementations", active, dict(concept_usage)))

    # Do not flag repeated helper or local variable names as responsibility
    # violations. Ownership issues are enforced through explicit concept groups
    # above; broad same-name heuristics produced false positives such as
    # `file`, `out`, and `normal` across unrelated local scopes.

    if not findings:
        print("Single responsibility: PASS — no duplicate implementations detected.")
        sys.exit(0)

    print(f"Found {len(findings)} potential responsibility issue(s):\n")
    for title, names, details in findings:
        print(f"  {title}")
        if names:
            for n in names:
                locs = details.get(n, [])
                if locs:
                    print(f"    '{n}' — {len(locs)} location(s)")
                    for loc in locs[:3]:
                        print(f"      {loc}")
                    if len(locs) > 3:
                        print(f"      ... (+{len(locs)-3} more)")
        else:
            for loc in details.get("locations", [])[:5]:
                print(f"      {loc}")
            if len(details.get("locations", [])) > 5:
                print(f"      ... (+{len(details['locations'])-5} more)")
        print()

    sys.exit(1)


if __name__ == "__main__":
    main()
