#!/usr/bin/env python3
"""
Dependency Checker

Prevent subsystem coupling.

Forbidden dependencies:
  - physics/ -> ui/, menu/, replay/, website/, audio/
"""

import os
import re
import sys

REPO_ROOT = os.path.dirname(os.path.abspath(__file__))
while not os.path.isfile(os.path.join(REPO_ROOT, "overseer.py")):
    REPO_ROOT = os.path.dirname(REPO_ROOT)
    if REPO_ROOT == os.path.dirname(REPO_ROOT):
        sys.exit(1)

SRC_DIR = os.path.join(REPO_ROOT, "src")

# (subsystem_dir, [forbidden_target_prefixes])
FORBIDDEN = [
    ("physics", ["gui/ui", "gui/menu", "replay", "website", "audio"]),
]

# Files in physics/ that are allowed to break subsystem isolation
# because they implement cross-cutting features (persistent projectiles, etc.)
ALLOWLIST = [
    "src/physics/persistent-physics.cpp",
]


def find_cpp_files(directory):
    files = []
    for root, dirs, names in os.walk(directory):
        dirs[:] = [d for d in dirs if d not in ("node_modules", ".opencode", "build", ".git", "__pycache__")]
        for name in names:
            if name.endswith((".cpp", ".h")):
                files.append(os.path.join(root, name))
    return files


def check_includes(filepath, subsystem, forbidden_targets):
    relpath = os.path.relpath(filepath, REPO_ROOT)
    violations = []

    # Check if this file is inside the subsystem directory
    subsystem_path = os.path.join(SRC_DIR, subsystem)
    if not relpath.startswith(os.path.join("src", subsystem)):
        return violations

    # Allowlisted files are exempt
    if relpath in ALLOWLIST:
        return violations

    with open(filepath, "r", errors="replace") as f:
        content = f.read()

    # Find all #include statements with project headers (quoted includes)
    includes = re.findall(r'#include\s+"([^"]+)"', content)

    for inc in includes:
        inc_normalized = inc.replace("\\", "/")
        for target in forbidden_targets:
            if inc_normalized.startswith(target):
                violations.append((relpath, inc_normalized, subsystem, target))

    return violations


def main():
    all_violations = []

    for subsystem, targets in FORBIDDEN:
        files = find_cpp_files(os.path.join(SRC_DIR, subsystem))
        for f in files:
            violations = check_includes(f, subsystem, targets)
            all_violations.extend(violations)

    if not all_violations:
        print("No dependency violations found.")
        sys.exit(0)

    print(f"Found {len(all_violations)} dependency violation(s):\n")
    for relpath, include, subsystem, target in all_violations:
        print(f"  {relpath}")
        print(f"    includes '{include}'")
        print(f"    {subsystem}/ must not depend on {target}/")

    sys.exit(1)


if __name__ == "__main__":
    main()
