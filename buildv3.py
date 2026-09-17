# 09 16 2026
# purpose
# COLD BUILD entry for MiMITA (humans). Builds the same way as build_agent.py but
# is the human-facing command. Every run links a NEW uniquely named executable,
# mimita-<local timestamp>.exe (e.g. mimita-20260916T173758.exe), so several
# people/agents can each build and test their own changes without being blocked
# by another running mimita executable.
# It shares the same build lock and build.py pipeline as build_agent.py, so no
# two builds compile into the same object files at once.
# Launches the newly built executable after a successful build. It does not
# kill, close, or unlock any running mimita executable.

import json
import os
import subprocess

import build_agent

if __name__ == "__main__":
    try:
        build_agent.main()
    except SystemExit as build_exit:
        if build_exit.code not in (0, None):
            raise

        result_path = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                   "build", "build-result.json")
        try:
            with open(result_path, "r", encoding="utf-8") as result_file:
                result = json.load(result_file)
        except (OSError, ValueError) as error:
            print(f"[BUILD] unable to launch: could not read {result_path}: {error}")
            raise

        executable_path = result.get("executable_path")
        if result.get("status") not in ("SUCCESS", "NOTHING_CHANGED"):
            print(f"[BUILD] not launching: status={result.get('status')}")
        elif not executable_path or not os.path.isfile(executable_path):
            print(f"[BUILD] not launching: executable is missing: {executable_path}")
        else:
            print(f"[BUILD] launching {executable_path}")
            subprocess.Popen([executable_path], cwd=os.path.dirname(executable_path))
