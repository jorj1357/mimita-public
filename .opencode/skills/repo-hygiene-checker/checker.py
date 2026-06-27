#!/usr/bin/env python3
"""
Repository Hygiene Checker

Prevents repository debris — temporary files, backups, logs, crash dumps, generated artifacts.
"""

import os
import sys
import fnmatch

REPO_ROOT = os.path.dirname(os.path.abspath(__file__))
while not os.path.isfile(os.path.join(REPO_ROOT, "overseer.py")):
    REPO_ROOT = os.path.dirname(REPO_ROOT)
    if REPO_ROOT == os.path.dirname(REPO_ROOT):
        sys.exit(1)

DEBRIS_PATTERNS = [
    "err.txt", "err2.txt", "out.txt", "out2.txt",
    "debug.txt", "debug.log", "crash.log", "trace.log",
    "*.tmp", "*.temp", "*.cache",
    "*.bak", "*.old", "*.backup", "*.copy",
    "test_output.txt", "output.txt", "results.txt", "dump.txt", "report.txt",
    "*.orig", "*.rej",
    "ai_report.txt", "audit_output.txt", "analysis_output.txt",
    "*_copy.cpp", "*_backup.cpp", "*_old.cpp",
]

SKIP_DIRS = {".git", "node_modules", "build", "__pycache__", ".opencode", "assets", "include"}
ALLOWED_FILES = {"build/changelog.txt", "crash-log.txt"}
SCAN_DIRS = ["", "src", "website", "launcher", "installer", "config", "scripts", "devscripts"]


def main():
    issues = []

    # Debris files
    debris = []
    for scan_dir in SCAN_DIRS:
        full_path = os.path.join(REPO_ROOT, scan_dir)
        if not os.path.isdir(full_path):
            continue
        for root, dirs, names in os.walk(full_path):
            dirs[:] = [d for d in dirs if d not in SKIP_DIRS and not d.startswith(".")]
            for name in names:
                relpath = os.path.relpath(os.path.join(root, name), REPO_ROOT)
                if relpath in ALLOWED_FILES:
                    continue
                for pattern in DEBRIS_PATTERNS:
                    if fnmatch.fnmatch(name, pattern):
                        debris.append(relpath)
                        break
    if debris:
        issues.append(("Debris files", debris))

    # Merge conflict markers in src/
    merge_markers = []
    for root, dirs, names in os.walk(os.path.join(REPO_ROOT, "src")):
        dirs[:] = [d for d in dirs if d not in SKIP_DIRS]
        for name in names:
            if name.endswith((".cpp", ".h", ".hpp", ".c", ".py", ".js", ".json")):
                filepath = os.path.join(root, name)
                relpath = os.path.relpath(filepath, REPO_ROOT)
                try:
                    with open(filepath, "r", errors="replace") as f:
                        for i, line in enumerate(f, 1):
                            stripped = line.strip()
                            if stripped in ("<<<<<<<", "=======", ">>>>>>>"):
                                merge_markers.append(f"{relpath}:{i}")
                                break
                except:
                    pass
    if merge_markers:
        issues.append(("Merge conflict markers", merge_markers))

    if not issues:
        print("Repository hygiene: PASS — no issues found.")
        sys.exit(0)

    for category, items in issues:
        print(f"  {category}:")
        for item in items:
            print(f"    {item}")
        print()

    print(f"Total issues: {sum(len(items) for _, items in issues)}")
    sys.exit(1)


if __name__ == "__main__":
    main()
