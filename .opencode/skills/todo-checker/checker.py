#!/usr/bin/env python3
"""
Todo Checker — Scan the repo for case-insensitive TODO/FIXME/HACK/XXX comments.

Exit code: 0 if none found, 1 if any found.
"""

import os
import re
import sys

REPO_ROOT = os.path.dirname(os.path.abspath(__file__))
for _ in range(3):
    REPO_ROOT = os.path.dirname(REPO_ROOT)

PATTERNS = [
    r'TODO',
    r'FIXME',
    r'HACK',
    r'XXX',
    r'WORKAROUND',
]

SCAN_DIRS = {
    'src',
    'config',
    'assets',
    'shaders',
}

ALLOWED_EXTENSIONS = {
    '.py', '.cpp', '.h', '.hpp', '.c',
    '.json', '.md', '.txt', '.sh', '.bat', '.ps1',
    '.cmake', '.mk', '.frag', '.vert', '.glsl',
}

MAX_FILE_SIZE = 1 * 1024 * 1024
COMPILED = [(p, re.compile(r'\b' + p + r'\b', re.IGNORECASE)) for p in PATTERNS]

def should_scan(path):
    rel = os.path.relpath(path, REPO_ROOT)
    parts = rel.split(os.sep)
    # Only scan files inside SCAN_DIRS
    if not any(parts[0] == d for d in SCAN_DIRS):
        return False
    ext = os.path.splitext(path)[1].lower()
    if ext not in ALLOWED_EXTENSIONS:
        return False
    try:
        if os.path.getsize(path) > MAX_FILE_SIZE or os.path.getsize(path) == 0:
            return False
    except OSError:
        return False
    return True

def scan_file(path):
    results = []
    try:
        with open(path, 'r', encoding='utf-8', errors='replace') as f:
            for i, line in enumerate(f, 1):
                if i > 50000:
                    break
                for pname, regex in COMPILED:
                    if regex.search(line):
                        results.append((i, line.strip(), pname))
                        break
    except Exception:
        pass
    return results

def main():
    findings = []
    scanned = 0
    dir_count = 0
    for scan_dir in SCAN_DIRS:
        scan_path = os.path.join(REPO_ROOT, scan_dir)
        if not os.path.isdir(scan_path):
            continue
        for root, dirs, files in os.walk(scan_path):
            for fname in files:
                fpath = os.path.join(root, fname)
                if not should_scan(fpath):
                    continue
                matches = scan_file(fpath)
                scanned += 1
                for line_num, line_text, pattern in matches:
                    rel_path = os.path.relpath(fpath, REPO_ROOT)
                    findings.append((rel_path, line_num, line_text, pattern))

    if not findings:
        sys.stdout.write(f"Todo Checker: PASS\n")
        sys.stdout.write(f"Scanned {scanned} files. No TODO/FIXME/HACK/XXX/WORKAROUND comments found.\n")
        return 0

    by_file = {}
    for rel_path, line_num, line_text, pattern in findings:
        by_file.setdefault(rel_path, []).append((line_num, line_text, pattern))

    sys.stdout.write(f"Todo Checker: BLOCKER\n")
    sys.stdout.write(f"Scanned {scanned} files. Found {len(findings)} comment(s):\n\n")

    for fpath in sorted(by_file.keys()):
        entries = by_file[fpath]
        sys.stdout.write(f"  {fpath} ({len(entries)} match(es))\n")
        for line_num, line_text, pattern in sorted(entries, key=lambda x: x[0]):
            display = line_text[:100].strip()
            sys.stdout.write(f"    L{line_num}: [{pattern}] {display}\n")
        sys.stdout.write("\n")

    return 1

if __name__ == "__main__":
    sys.exit(main())
