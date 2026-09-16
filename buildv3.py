# 09 16 2026
# purpose
# COLD BUILD entry for MiMITA (humans). Builds the same way as build_agent.py but
# is the human-facing command. Every run links a NEW uniquely named executable,
# mimita-<local timestamp>.exe (e.g. mimita-20260916T173758.exe), so several
# people/agents can each build and test their own changes without being blocked
# by another running mimita executable.
# It shares the same build lock and build.py pipeline as build_agent.py, so no
# two builds compile into the same object files at once.
# Does NOT run, kill, close, or unlock any running mimita executable.

import build_agent

if __name__ == "__main__":
    build_agent.main()
