#!/usr/bin/env python3
"""
Physics Architecture Checker

Enforces that main.cpp does NOT contain:
  - movement logic
  - collision logic
  - rendering logic
  - terminal command registration
  - networking logic

main.cpp should only do initialization, registration, update loop, and shutdown.
"""

import os
import re
import sys

REPO_ROOT = os.path.dirname(os.path.abspath(__file__))
while not os.path.isfile(os.path.join(REPO_ROOT, "overseer.py")):
    REPO_ROOT = os.path.dirname(REPO_ROOT)
    if REPO_ROOT == os.path.dirname(REPO_ROOT):
        sys.exit(1)

MAIN_CPP = os.path.join(REPO_ROOT, "src", "main.cpp")

# Keywords and patterns that should NOT be in main.cpp
FORBIDDEN_PATTERNS = [
    # Movement logic
    (r'\bdoWalk\b', "movement logic (doWalk)"),
    (r'\bdoJump\b', "movement logic (doJump)"),
    (r'\bdoDash\b', "movement logic (doDash)"),
    (r'\bdoGravity\b', "physics logic (doGravity)"),
    (r'\bdoFriction\b', "movement logic (doFriction)"),
    (r'\bdoMovement\b', "movement logic (doMovement)"),
    (r'\bdoCollision\b', "collision logic (doCollision)"),
    (r'\bphysicsTick\b', "physics tick logic"),
    (r'\bmovePlayer\b', "movement logic (movePlayer)"),

    # Collision logic
    (r'\bcheckCollision\b', "collision logic (checkCollision)"),
    (r'\bresolveCollision\b', "collision logic (resolveCollision)"),
    (r'\bcollide\b', "collision logic (collide)"),
    (r'\bglbCollide\b', "collision logic (glbCollide)"),
    (r'\bcontactSolver\b', "collision logic (contactSolver)"),

    # Rendering logic (not orchestration)
    (r'\brenderWorld\b', "rendering logic (renderWorld)"),
    (r'\brenderPlayer\b', "rendering logic (renderPlayer)"),
    (r'\bdrawMesh\b', "rendering logic (drawMesh)"),
    (r'\bpostProcess\b', "rendering logic (postProcess)"),
    (r'\bdepthPass\b', "rendering logic (depthPass)"),
    (r'\bshadowPass\b', "rendering logic (shadowPass)"),

    # Terminal command registration (should be in feature-specific files)
    (r'\bregisterReplayCommands\b', "terminal command registration (registerReplayCommands)"),
    (r'\bregisterWeaponCommands\b', "terminal command registration (registerWeaponCommands)"),
    (r'\bregisterNpcCommands\b', "terminal command registration (registerNpcCommands)"),
    (r'\bregisterDuelCommands\b', "terminal command registration (registerDuelCommands)"),
    (r'\bregisterDebugCommands\b', "terminal command registration (registerDebugCommands)"),

    # Networking logic
    (r'\bsendPacket\b', "networking logic (sendPacket)"),
    (r'\brecvPacket\b', "networking logic (recvPacket)"),
    (r'\bparsePacket\b', "networking logic (parsePacket)"),
    (r'\bnetTick\b', "networking logic (netTick)"),
    (r'\bmpTick\b', "networking logic (mpTick)"),
    (r'\bserverTick\b', "networking logic (serverTick)"),
    (r'\bclientTick\b', "networking logic (clientTick)"),
    (r'\bhandlePacket\b', "networking logic (handlePacket)"),
    (r'\bflushPackets\b', "networking logic (flushPackets)"),
]


def main():
    if not os.path.isfile(MAIN_CPP):
        print(f"main.cpp not found at {MAIN_CPP}")
        sys.exit(1)

    with open(MAIN_CPP, "r", errors="replace") as f:
        content = f.read()
        lines = content.split("\n")

    violations = []
    for i, line in enumerate(lines, 1):
        stripped = line.strip()
        if stripped.startswith("//") or stripped.startswith("/*") or stripped.startswith("*"):
            continue
        for pattern, description in FORBIDDEN_PATTERNS:
            if re.search(pattern, line):
                violations.append((i, line.strip(), description))

    if not violations:
        print(f"main.cpp is clean — no forbidden patterns found.")
        sys.exit(0)

    print(f"Found {len(violations)} architecture violation(s) in main.cpp:\n")
    for line_no, code, description in violations:
        print(f"  Line {line_no}: {description}")
        print(f"    {code[:120]}")

    sys.exit(1)


if __name__ == "__main__":
    main()
