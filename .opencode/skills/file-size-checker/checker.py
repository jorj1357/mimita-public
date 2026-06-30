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
import subprocess

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

SEVERITY_RANK = {
    "ok": 0,
    "WARNING": 1,
    "HIGH": 2,
    "CRITICAL": 3,
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
    changed = changed_paths()
    if changed is not None:
        files = []
        for rel in changed:
            if not any(fnmatch.fnmatch(os.path.basename(rel), pat) for pat in INCLUDE_PATTERNS):
                continue
            filepath = os.path.join(REPO_ROOT, rel)
            if os.path.isfile(filepath) and should_include(filepath):
                files.append(filepath)
        return files

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


def changed_paths():
    try:
        paths = []
        tracked = subprocess.run(
            ["git", "diff", "--name-only", "--diff-filter=ACMRT", "HEAD", "--"],
            cwd=REPO_ROOT, capture_output=True, text=True, check=False)
        paths.extend(p.strip() for p in tracked.stdout.splitlines() if p.strip())
        untracked = subprocess.run(
            ["git", "ls-files", "--others", "--exclude-standard"],
            cwd=REPO_ROOT, capture_output=True, text=True, check=False)
        paths.extend(p.strip() for p in untracked.stdout.splitlines() if p.strip())
        return sorted(set(paths))
    except Exception:
        return None


def base_line_count(relpath):
    try:
        git_path = relpath.replace(os.sep, "/")
        proc = subprocess.run(
            ["git", "show", f"HEAD:{git_path}"],
            cwd=REPO_ROOT, capture_output=True, text=True, check=False)
        if proc.returncode != 0:
            return None
        return len(proc.stdout.splitlines())
    except Exception:
        return None


def severity_for(relpath, count):
    is_main = relpath == "src" + os.sep + "main.cpp"
    if is_main:
        if count > 100:
            return ("CRITICAL", f"main.cpp has {count} lines (max 100)")
        return ("ok", "")

    if count > 1000:
        return ("CRITICAL", f"{count} lines (max 1000)")
    if count > 500:
        return ("HIGH", f"{count} lines (max 500)")
    if count > 300:
        return ("WARNING", f"{count} lines (max 300)")
    return ("ok", "")


def check_file(filepath):
    relpath = os.path.relpath(filepath, REPO_ROOT)
    try:
        with open(filepath, "r", errors="replace") as f:
            lines = f.readlines()
    except Exception as e:
        return (relpath, 0, "error", str(e))

    count = len(lines)
    severity, msg = severity_for(relpath, count)
    return (relpath, count, severity, msg)


def main():
    files = collect_files()
    issues = []

    for f in files:
        result = check_file(f)
        if result[2] == "ok":
            continue

        base_count = base_line_count(result[0])
        if base_count is not None:
            base_severity, _ = severity_for(result[0], base_count)
            if SEVERITY_RANK[result[2]] <= SEVERITY_RANK[base_severity]:
                continue
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
