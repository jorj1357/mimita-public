#!/usr/bin/env python3
"""
Function Size Checker (informational only)

Reports function sizes but NEVER blocks a build. A function may be as long
as it needs to be to fulfill its single responsibility.
"""

import os
import re
import subprocess
import sys

REPO_ROOT = os.path.dirname(os.path.abspath(__file__))
while not os.path.isfile(os.path.join(REPO_ROOT, "overseer.py")):
    REPO_ROOT = os.path.dirname(REPO_ROOT)
    if REPO_ROOT == os.path.dirname(REPO_ROOT):
        sys.exit(1)

MAX_WARNING = 10000  # informational only — never blocks
MAX_HIGH = 20000
MAX_CRITICAL = 50000

EXCLUDE_DIRS = {"node_modules", ".opencode", "build", ".git", "__pycache__"}
SEVERITY_RANK = {"WARNING": 1, "HIGH": 2, "CRITICAL": 3}


def find_cpp_files():
    changed = changed_paths()
    if changed is not None:
        files = []
        for rel in changed:
            if rel.endswith(".cpp"):
                full = os.path.join(REPO_ROOT, rel)
                if os.path.isfile(full):
                    files.append(full)
        return files

    src_dir = os.path.join(REPO_ROOT, "src")
    files = []
    for root, dirs, names in os.walk(src_dir):
        dirs[:] = [d for d in dirs if d not in EXCLUDE_DIRS]
        for name in names:
            if name.endswith(".cpp"):
                files.append(os.path.join(root, name))
    return files


def changed_paths():
    try:
        paths = []
        tracked = subprocess.run(
            ["git", "diff", "--name-only", "--diff-filter=ACMRT", "HEAD", "--", "src"],
            cwd=REPO_ROOT, capture_output=True, text=True, check=False)
        paths.extend(p.strip() for p in tracked.stdout.splitlines() if p.strip())
        untracked = subprocess.run(
            ["git", "ls-files", "--others", "--exclude-standard", "src"],
            cwd=REPO_ROOT, capture_output=True, text=True, check=False)
        paths.extend(p.strip() for p in untracked.stdout.splitlines() if p.strip())
        return sorted(set(paths))
    except Exception:
        return None


def extract_functions_from_lines(lines):
    functions = []
    # Match function definitions: return_type name(...) {
    # Handles multi-line returns, templates, namespaces
    pattern = re.compile(
        r'(?:^|\n)\s*'                        # start of line
        r'(?:virtual\s+|static\s+|inline\s+)?'  # optional keywords
        r'(?:[\w:]+\s+)*'                       # return type (possibly qualified)
        r'(\w+)\s*\([^)]*\)\s*(?:const\s*)?(?:override\s*)?(?:final\s*)?'  # name(params)
        r'(?:\s*=\s*0)?\s*'                     # pure virtual
        r'(?:\{|\;)',                           # opening brace or semicolon
        re.MULTILINE
    )

    # Simpler approach: find lines that look like function starts
    func_pattern = re.compile(
        r'^\s*(?:static\s+|inline\s+|virtual\s+)?'
        r'(?:void|bool|int|float|double|char|std::\w+|glm::\w+|'
        r'unsigned|size_t|auto|\w+::\w+|\w+)\s+'  # return type
        r'(\w+)\s*\('                              # function name
    )

    brace_depth = 0
    current_func = None
    func_start = 0

    i = 0
    while i < len(lines):
        line = lines[i]

        if current_func is None:
            m = func_pattern.search(line)
            if m:
                # Skip obvious non-function lines
                name = m.group(1)
                if name not in ("if", "for", "while", "switch", "catch", "return",
                                "else", "case", "sizeof", "throw", "delete", "new"):
                    current_func = name
                    func_start = i + 1  # 1-indexed
                    # Count opening braces on this line
                    brace_depth = line.count("{") - line.count("}")
        else:
            brace_depth += line.count("{") - line.count("}")
            if brace_depth <= 0:
                func_end = i + 1
                line_count = func_end - func_start + 1
                if line_count > MAX_WARNING:
                    functions.append((current_func, func_start, func_end, line_count))
                current_func = None
                brace_depth = 0
        i += 1

    return functions


def extract_functions(filepath):
    """Find function definitions in a .cpp file and return (name, start_line, end_line)."""
    with open(filepath, "r", errors="replace") as f:
        return extract_functions_from_lines(f.readlines())


def base_functions(relpath):
    try:
        git_path = relpath.replace(os.sep, "/")
        proc = subprocess.run(
            ["git", "show", f"HEAD:{git_path}"],
            cwd=REPO_ROOT, capture_output=True, text=True, check=False)
        if proc.returncode != 0:
            return {}
        lines = proc.stdout.splitlines(True)
        result = {}
        for name, start, end, count in extract_functions_from_lines(lines):
            result[name] = count
        return result
    except Exception:
        return {}


def severity(line_count):
    if line_count > MAX_CRITICAL:
        return "CRITICAL"
    if line_count > MAX_HIGH:
        return "HIGH"
    return "WARNING"


def main():
    files = find_cpp_files()
    all_issues = []

    for filepath in files:
        relpath = os.path.relpath(filepath, REPO_ROOT)
        baseline = base_functions(relpath)
        functions = extract_functions(filepath)
        for name, start, end, count in functions:
            sev = severity(count)
            base_count = baseline.get(name)
            if base_count is not None and base_count > MAX_WARNING:
                base_sev = severity(base_count)
                if count <= base_count or SEVERITY_RANK[sev] <= SEVERITY_RANK[base_sev]:
                    continue
            all_issues.append((relpath, name, start, count, sev))

    if all_issues:
        all_issues.sort(key=lambda x: -x[3])
        print(f"Note: {len(all_issues)} function(s) over {MAX_WARNING} lines (informational, not enforced):")
        for relpath, name, start, count, sev in all_issues[:5]:
            print(f"  [{sev:8}] {relpath}:{start} '{name}()' — {count} lines")
    else:
        print("No oversized functions found.")

    print("Function size check: informational only (no limits enforced).")
    sys.exit(0)


if __name__ == "__main__":
    main()
