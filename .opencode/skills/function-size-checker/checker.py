#!/usr/bin/env python3
"""
Function Size Checker

Rules:
  - Functions over 50 lines: Warning
  - Functions over 100 lines: High Risk
  - Functions over 200 lines: Critical

Scans src/*.cpp for function definitions and measures their line count.
"""

import os
import re
import sys

REPO_ROOT = os.path.dirname(os.path.abspath(__file__))
while not os.path.isfile(os.path.join(REPO_ROOT, "overseer.py")):
    REPO_ROOT = os.path.dirname(REPO_ROOT)
    if REPO_ROOT == os.path.dirname(REPO_ROOT):
        sys.exit(1)

MAX_WARNING = 50
MAX_HIGH = 100
MAX_CRITICAL = 200

EXCLUDE_DIRS = {"node_modules", ".opencode", "build", ".git", "__pycache__"}


def find_cpp_files():
    src_dir = os.path.join(REPO_ROOT, "src")
    files = []
    for root, dirs, names in os.walk(src_dir):
        dirs[:] = [d for d in dirs if d not in EXCLUDE_DIRS]
        for name in names:
            if name.endswith(".cpp"):
                files.append(os.path.join(root, name))
    return files


def extract_functions(filepath):
    """Find function definitions in a .cpp file and return (name, start_line, end_line)."""
    with open(filepath, "r", errors="replace") as f:
        lines = f.readlines()

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
        functions = extract_functions(filepath)
        for name, start, end, count in functions:
            sev = severity(count)
            all_issues.append((relpath, name, start, count, sev))

    if not all_issues:
        print("No oversized functions found.")
        sys.exit(0)

    all_issues.sort(key=lambda x: -x[3])
    print(f"Found {len(all_issues)} oversized function(s):\n")
    for relpath, name, start, count, sev in all_issues:
        print(f"  [{sev:8}] {relpath}:{start}")
        print(f"            Function '{name}()' — {count} lines (max {MAX_WARNING})")

    sys.exit(1)


if __name__ == "__main__":
    main()
