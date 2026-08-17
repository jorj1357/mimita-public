#!/usr/bin/env python3
"""Cross-platform git push helper (replaces git-push-v2.bat)."""
import subprocess
import sys
from datetime import datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parent


def prompt(text):
    try:
        return input(text).strip()
    except EOFError:
        return ""


def run(args, check=False):
    print()
    print("$ git " + " ".join(args))
    result = subprocess.run(
        ["git"] + args,
        cwd=ROOT,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    if result.stdout:
        print(result.stdout.rstrip())
    if check and result.returncode != 0:
        raise RuntimeError(f"git {' '.join(args)} failed (exit {result.returncode})")
    return result


def main():
    print()
    print("=====================================")
    print("Current Branch:")
    current = run(["branch", "--show-current"]).stdout.strip()
    print("   " + current)
    print("=====================================")

    print()
    print("Recent Branches:")
    print()
    lines = run(
        ["for-each-ref", "--sort=-committerdate",
         "--format=%(refname:short)", "refs/heads"]
    ).stdout.splitlines()
    branches = []
    for i, branch in enumerate(lines[:9], start=1):
        branches.append(branch)
        print(f"{i}) {branch}")

    print()
    pick = prompt("Pick branch (1-9) or press Enter to stay on current: ")
    if pick:
        try:
            target = branches[int(pick) - 1]
        except (ValueError, IndexError):
            target = ""
        if target:
            print()
            print(f"Switching to {target}")
            run(["checkout", target])
            current = target

    print()
    print(f"Active branch: {current}")
    print()
    msg = prompt("Commit message: ")
    if not msg:
        msg = "Auto commit " + datetime.now().strftime("%m-%d-%Y-%H-%M-%S")

    run(["add", "."])
    run(["commit", "-m", msg])
    run(["push", "-u", "origin", "HEAD"])

    print()
    print("Done.")


if __name__ == "__main__":
    try:
        main()
    except RuntimeError as exc:
        print()
        print(f"ERROR: {exc}")
    finally:
        prompt("Press Enter to exit...")
