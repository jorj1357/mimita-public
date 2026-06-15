# build_agent.py
# Build script for AI agents.
# Builds without running the exe, so agents never hang.
# Writes build/changelog.txt after each build so AI agents
# can verify whether a real build happened.

import subprocess
import sys
import os
import datetime

if __name__ == "__main__":
    start = datetime.datetime.now()

    result = subprocess.run(
        [sys.executable, "build.py", "build-only"],
        capture_output=True,
        text=True,
    )

    elapsed = (datetime.datetime.now() - start).total_seconds()

    # Determine build result
    if result.returncode == 0:
        if "Nothing changed" in result.stdout:
            status = "NOTHING_CHANGED"
        else:
            status = "SUCCESS"
    else:
        status = "FAILED"

    # Write changelog
    log_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "build")
    os.makedirs(log_dir, exist_ok=True)
    log_path = os.path.join(log_dir, "changelog.txt")

    with open(log_path, "w") as f:
        f.write("=== BUILD CHANGELOG ===\n")
        f.write(f"Time: {start.strftime('%Y-%m-%d %H:%M:%S')}\n")
        f.write(f"Status: {status}\n")
        f.write(f"Return Code: {result.returncode}\n")
        f.write(f"Duration: {elapsed:.2f}s\n")
        f.write("\n--- Build Output ---\n")
        f.write(result.stdout)
        if result.stderr:
            f.write("\n--- Stderr ---\n")
            f.write(result.stderr)

    # Also print changelog to stdout for immediate AI inspection
    print(f"=== BUILD CHANGELOG ===")
    print(f"Status: {status}")
    print(f"Duration: {elapsed:.2f}s")
    print(f"Log: {log_path}")
    print(f"Return Code: {result.returncode}")
    print()
    print(result.stdout)
    if result.stderr:
        print(result.stderr)

    sys.exit(result.returncode)
