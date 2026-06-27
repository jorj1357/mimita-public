#!/usr/bin/env python3
"""
overseer.py — Final quality gate for the Mimita repository.

Auto-discovers all skills under .opencode/skills/<name>/checker.py,
runs each one, collects results, and produces a combined PASS/FAIL report.

Usage:
    python overseer.py

Exit code: 0 if all checks pass, 1 if any check fails.
"""

import glob
import os
import shutil
import subprocess
import sys
import time
from datetime import datetime


REPO_ROOT = os.path.dirname(os.path.abspath(__file__))
SKILLS_DIR = os.path.join(REPO_ROOT, ".opencode", "skills")
LOG_DIR = os.path.join(REPO_ROOT, "logs")
MAX_LOGS = 30


def discover_checkers():
    """Find all skill directories that contain a checker.py."""
    checkers = []
    if not os.path.isdir(SKILLS_DIR):
        return checkers
    for name in sorted(os.listdir(SKILLS_DIR)):
        skill_dir = os.path.join(SKILLS_DIR, name)
        checker_path = os.path.join(skill_dir, "checker.py")
        if os.path.isdir(skill_dir) and os.path.isfile(checker_path):
            checkers.append((name, checker_path))
    return checkers


def run_checker(name, checker_path):
    """Execute a single checker.py, capture output and exit code."""
    start = time.time()
    try:
        proc = subprocess.run(
            [sys.executable, checker_path],
            cwd=REPO_ROOT,
            capture_output=True,
            text=True,
            timeout=120,
        )
        duration = time.time() - start
        passed = proc.returncode == 0
        output = proc.stdout
        if proc.stderr:
            if output:
                output += "\n"
            output += "[STDERR]\n" + proc.stderr
        return {
            "name": name,
            "passed": passed,
            "output": output.strip(),
            "duration": duration,
            "returncode": proc.returncode,
        }
    except subprocess.TimeoutExpired:
        return {
            "name": name,
            "passed": False,
            "output": "TIMED OUT (120s)",
            "duration": 120.0,
            "returncode": -1,
        }
    except Exception as e:
        return {
            "name": name,
            "passed": False,
            "output": f"ERROR: {e}",
            "duration": 0.0,
            "returncode": -1,
        }


def format_name(name):
    """Convert kebab-case-name to Title Case Name."""
    return name.replace("-", " ").replace("_", " ").title()


def build_report_text(results):
    """Build the full report as a string."""
    lines = []
    separator = "=" * 56
    lines.append("")
    lines.append(separator)
    lines.append("OVERSEER REPORT")
    lines.append(separator)
    lines.append("")

    max_name_len = max(len(format_name(r["name"])) for r in results) if results else 30
    all_passed = True

    for r in results:
        label = format_name(r["name"]).ljust(max_name_len)
        status = "PASS" if r["passed"] else "FAIL"
        duration_str = f"({r['duration']:.1f}s)"

        if r["passed"]:
            lines.append(f"  {label}  .... {status}  {duration_str}")
        else:
            lines.append(f"  {label}  .... {status}  {duration_str}")
            lines.append("")
            for line in r["output"].split("\n"):
                if line.strip():
                    lines.append(f"      {line}")
            lines.append("")

        if not r["passed"]:
            all_passed = False

    lines.append("-" * 56)
    lines.append("")
    if all_passed:
        lines.append("  Overall Status:  PASS")
    else:
        lines.append("  Overall Status:  FAILED")
    lines.append("")
    lines.append(separator)
    lines.append("")

    return "\n".join(lines), all_passed


def print_report(results):
    """Print the combined report to stdout."""
    report_text, all_passed = build_report_text(results)
    print(report_text, end="")
    return 0 if all_passed else 1


def save_log(report_text):
    """Save report to rolling log directory (max MAX_LOGS files)."""
    os.makedirs(LOG_DIR, exist_ok=True)

    # Format: mm-dd-yyyy-hh-mm-ss-overseer.txt
    ts = datetime.now().strftime("%m-%d-%Y-%H-%M-%S")
    log_path = os.path.join(LOG_DIR, f"{ts}-overseer.txt")

    with open(log_path, "w") as f:
        f.write(report_text)

    # Prune old logs — keep only MAX_LOGS most recent
    pattern = os.path.join(LOG_DIR, "*-overseer.txt")
    existing = sorted(glob.glob(pattern))
    while len(existing) > MAX_LOGS:
        oldest = existing.pop(0)
        os.remove(oldest)


def main():
    checkers = discover_checkers()

    if not checkers:
        msg = "No skills with checker.py found under .opencode/skills/"
        print(msg)
        sys.exit(1)

    results = []
    for name, checker_path in checkers:
        result = run_checker(name, checker_path)
        results.append(result)

    report_text, all_passed = build_report_text(results)

    # Print to stdout
    print(report_text, end="")

    # Save to rolling log
    save_log(report_text)

    exit_code = 0 if all_passed else 1
    sys.exit(exit_code)


if __name__ == "__main__":
    main()
