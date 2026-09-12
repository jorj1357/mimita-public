# 09 12 2026
# purpose
# Verify the live-development invariant:
#   * a LIVE BUILD never writes or relinks mimita.exe;
#   * it produces an immutable generation file;
#   * the cold-build tooling refuses to relink a running game and contains no
#     taskkill/auto-kill path.
# Does NOT build or link the executable itself.
# Does NOT require mimita.exe to be running.

import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXE = os.path.join(ROOT, "mimita.exe")
LIVE_BUILD = os.path.join(ROOT, "devscripts", "live-build.py")
BUILD_AGENT = os.path.join(ROOT, "build_agent.py")


def signature(path):
    if not os.path.exists(path):
        return None
    stat = os.stat(path)
    return (stat.st_size, int(stat.st_mtime))


def main():
    ok = True

    def check(condition, name):
        nonlocal ok
        print(("[ok] " if condition else "[FAIL] ") + name)
        ok = ok and condition

    # Static invariant: the cold build must not kill the game.
    with open(BUILD_AGENT, "r", encoding="utf-8", errors="replace") as handle:
        agent = handle.read().lower()
    check("taskkill" not in agent, "cold build has no taskkill/auto-kill")
    check("hot_reload_boundary_violation" in agent, "cold build reports boundary violation")
    check("mimita_force_cold" in agent, "cold build has an explicit force override")

    # Live build must leave the executable byte-for-byte in place.
    before = signature(EXE)
    generation = 990001
    output = os.path.join(ROOT, "build", "hotreload", f"mimita-live-g{generation:06d}.dll")
    if os.path.exists(output):
        os.remove(output)

    completed = subprocess.run(
        [sys.executable, LIVE_BUILD, "--generation", str(generation)],
        cwd=ROOT,
    )
    after = signature(EXE)

    check(completed.returncode == 0, "live build succeeds")
    check(os.path.exists(output), "live build emits immutable generation file")
    check(before == after, "live build leaves mimita.exe unchanged")

    if not ok:
        print("[LIVE BUILD INVARIANT SELFTEST] FAIL")
        return 1
    print("[LIVE BUILD INVARIANT SELFTEST] PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
