#!/usr/bin/env python3
"""
Duplicate State Checker

Detects competing variable names that likely represent the same state concept.

Known duplicates to detect:
  - grounded / onGround / stableOnGround / groundedThisFrame
  - dashAvailable / dashReady / canDash
  - jumpAvailable / jumpReady / canJump
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

SRC_DIR = os.path.join(REPO_ROOT, "src")

SKIP_FILE_PATTERNS = [
    r"physics-mini\.cpp",  # grounded/onGround/groundedThisFrame are intentionally distinct
    r"physics-gravity\.cpp",
    r"physics-collision\.cpp",  # phase-specific ground state tracking
    r"physics-collision-glb-main\.cpp",
    r"devtools\\terminal\.cpp",  # health used in command descriptions/strings only
    r"engine\\engine-tick-ui-game-hud\.cpp",  # health is ReplayActorState field read via .health
    r"gui\\hud\\player-nameplates\.cpp",  # health only in comments
    r"engine\\engine-tick-ui-replay-hud\.cpp",  # health is ReplayActorState field read via .health
    r"replay\\replay-scene\.h",  # ReplayActorState has its own health field (different struct from Player)
    r"replay\\replay-recorder\.cpp",  # replay-only health field
    r"replay\\replay-player-interp\.cpp",  # replay-only health field
    r"engine\\engine-tick-replay\.cpp",  # sets actor.health from player.currentHp (intentional mapping)
    r"engine\\engine-tick-render\.cpp",  # reads actorState.health (replay struct field)
    r"engine\\engine-tick-net\.cpp",  # interpolation struct field read via .health
    r"replay\\replay-io\.cpp",  # replay serialization of health field
    r"replay\\replay-io-save\.cpp",  # replay serialization of health field
]

DUPLICATE_GROUPS = [
    ["grounded", "onGround", "stableOnGround", "groundedThisFrame", "isGrounded", "mGrounded"],
    ["dashAvailable", "dashReady", "canDash", "mDashReady", "mCanDash"],
    ["jumpAvailable", "jumpReady", "canJump", "mJumpReady", "mCanJump", "jumpEnabled"],
    ["mCanDoubleJump", "canDoubleJump", "doubleJumpReady"],
    ["mSlideReady", "canSlide", "slideReady"],
    ["mWalkSpeed", "walkSpeed", "mMoveSpeed", "moveSpeed"],
    ["mHealth", "health", "currentHp", "mCurrentHp", "mHp"],
    ["mMaxHealth", "maxHealth", "maxHp", "mMaxHp"],
    ["isDead", "mDead", "dead", "bDead"],
]


def find_files():
    changed = changed_files()
    if changed is not None:
        return changed

    files = []
    for root, dirs, names in os.walk(SRC_DIR):
        dirs[:] = [d for d in dirs if d not in ("node_modules", ".opencode", "build", ".git", "__pycache__")]
        for name in names:
            if name.endswith((".cpp", ".h")):
                files.append(os.path.join(root, name))
    return files


def changed_files():
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
        files = []
        for rel in sorted(set(paths)):
            if rel.endswith((".cpp", ".h")):
                full = os.path.join(REPO_ROOT, rel)
                if os.path.isfile(full):
                    files.append(full)
        return files
    except Exception:
        return None


def main():
    files = find_files()
    all_findings = []

    for group in DUPLICATE_GROUPS:
        # Search all files for each variable name in the group
        usages = {}  # varname -> [(filepath, line)]

        for filepath in files:
            relpath = os.path.relpath(filepath, REPO_ROOT)
            skip = False
            for pat in SKIP_FILE_PATTERNS:
                if re.search(pat, relpath.replace("\\", "/")):
                    skip = True
                    break
            if skip:
                continue

            with open(filepath, "r", errors="replace") as f:
                try:
                    lines = f.readlines()
                except:
                    continue

            for varname in group:
                for i, line in enumerate(lines, 1):
                    if re.search(r'\b' + re.escape(varname) + r'\b', line):
                        # Skip struct member declarations (e.g. "bool onGround = false;")
                        member_decl = re.match(r'^\s+\w+\s+' + re.escape(varname) + r'\b.*;\s*$', line)
                        if member_decl:
                            continue
                        # Skip struct member access (e.g. "actor.grounded" or "a[\"health\"]")
                        member_access = re.search(r'\.\s*' + re.escape(varname) + r'\b', line)
                        if member_access:
                            continue
                        usages.setdefault(varname, []).append((relpath, i))

        # Count how many different variables from this group are actually used
        active_vars = {v: locs for v, locs in usages.items() if len(locs) >= 2}
        if len(active_vars) >= 2:
            all_findings.append((group, active_vars))

    if not all_findings:
        print("No duplicate state variables detected.")
        sys.exit(0)

    print(f"Found {len(all_findings)} potential duplicate state group(s):\n")
    for group, active_vars in all_findings:
        print(f"  Group: {', '.join(group)}")
        for varname, locations in sorted(active_vars.items()):
            locs_str = ", ".join(f"{f}:{l}" for f, l in locations[:5])
            if len(locations) > 5:
                locs_str += f" ... (+{len(locations) - 5} more)"
            print(f"    '{varname}' — {len(locations)} occurrence(s)")
            print(f"      e.g. {locs_str}")
        print()

    sys.exit(1)


if __name__ == "__main__":
    main()
