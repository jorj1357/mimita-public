#!/usr/bin/env python3
"""
File Size Checker

Rules:
  - Files over 300 lines: WARNING
  - Files over 500 lines: HIGH RISK
  - Files over 1000 lines: CRITICAL
  - src/main.cpp must be <= 100 lines
"""

import os
import sys
import fnmatch

REPO_ROOT = os.path.dirname(os.path.abspath(__file__))
while not os.path.isfile(os.path.join(REPO_ROOT, "overseer.py")):
    REPO_ROOT = os.path.dirname(REPO_ROOT)
    if REPO_ROOT == os.path.dirname(REPO_ROOT):
        sys.exit(1)

# Directories to scan for source files
SCAN_DIRS = ["src", "website/server", "launcher", "installer"]
# File patterns to check
INCLUDE_PATTERNS = ["*.cpp", "*.h", "*.hpp", "*.c", "*.js", "*.py", "*.iss"]
# Directories to exclude
EXCLUDE_DIRS = ["node_modules", ".opencode", "build", ".git", "__pycache__"]
# Known library/auto-generated files to skip (single-file libs, codegen output)
LIBRARY_FILES = {
    "src/glad.c",
    "src/utils/stb_image_impl.cpp",
    "src/tiny_obj_loader_declare.cpp",
    "src/tinygltf_declare.cpp",
}


def is_library_file(relpath):
    rel = relpath.replace("\\", "/")
    for lib in LIBRARY_FILES:
        if rel == lib or rel.endswith("/" + lib):
            return True
    return False


def should_include(filepath):
    rel = os.path.relpath(filepath, REPO_ROOT)
    for exc in EXCLUDE_DIRS:
        if f"{os.sep}{exc}{os.sep}" in rel or rel.startswith(exc + os.sep):
            return False
    if is_library_file(rel):
        return False
    return True


def collect_files():
    files = []
    for scan_dir in SCAN_DIRS:
        full_path = os.path.join(REPO_ROOT, scan_dir)
        if not os.path.isdir(full_path):
            continue
        for root, dirs, names in os.walk(full_path):
            dirs[:] = [d for d in dirs if d not in EXCLUDE_DIRS]
            for name in names:
                if any(fnmatch.fnmatch(name, pat) for pat in INCLUDE_PATTERNS):
                    filepath = os.path.join(root, name)
                    if should_include(filepath):
                        files.append(filepath)
    return files


def check_file(filepath):
    relpath = os.path.relpath(filepath, REPO_ROOT)
    try:
        with open(filepath, "r", errors="replace") as f:
            lines = f.readlines()
    except Exception as e:
        return (relpath, 0, "error", str(e))

    count = len(lines)
    is_main = relpath == "src" + os.sep + "main.cpp"

    if is_main:
        if count > 100:
            return (relpath, count, "CRITICAL", f"main.cpp has {count} lines (max 100)")
        return (relpath, count, "ok", "")

    if count > 1000:
        return (relpath, count, "CRITICAL", f"{count} lines (max 1000)")
    if count > 500:
        return (relpath, count, "HIGH", f"{count} lines (max 500)")
    if count > 300:
        return (relpath, count, "WARNING", f"{count} lines (max 300)")

    return (relpath, count, "ok", "")


def main():
    files = collect_files()
    issues = []

    for f in files:
        result = check_file(f)
        if result[2] != "ok":
            issues.append(result)

    if not issues:
        largest = sorted(
            [(os.path.relpath(f, REPO_ROOT), len(open(f, errors="replace").readlines()))
             for f in files if os.path.isfile(f)],
            key=lambda x: -x[1]
        )[:3]
        print(f"Checked {len(files)} files. No oversized files.")
        print(f"Largest: {', '.join(f'{n} ({c}L)' for n, c in largest)}")
        sys.exit(0)

    print(f"Checked {len(files)} files. Found {len(issues)} issue(s):\n")
    for relpath, count, severity, msg in issues:
        print(f"  [{severity:8}] {relpath}")
        print(f"            {msg}")

    sys.exit(1)


if __name__ == "__main__":
    main()
