# build_agent.py
# Build script for AI agents.
# Builds without running the exe, so agents never hang.
# Writes build/changelog.txt after each build so AI agents
# can verify whether a real build happened.

import subprocess
import sys
import os
import datetime
import time

# ── Build history tracker ─────────────────────────────────────────────
BUILD_HISTORY_FILE = os.path.join(
    os.path.dirname(os.path.abspath(__file__)), "build", "build-history.txt"
)

def load_build_history():
    """Return list of last 5 timestamp strings."""
    if not os.path.exists(BUILD_HISTORY_FILE):
        return []
    with open(BUILD_HISTORY_FILE, "r") as f:
        lines = [line.strip() for line in f.readlines() if line.strip()]
    return lines[-5:]

def save_build_history(timestamp):
    """Append timestamp, keep last 5."""
    history = load_build_history()
    history.append(timestamp)
    history = history[-5:]
    os.makedirs(os.path.dirname(BUILD_HISTORY_FILE), exist_ok=True)
    with open(BUILD_HISTORY_FILE, "w") as f:
        for h in history:
            f.write(h + "\n")

def timestamp_now():
    return datetime.datetime.now().strftime("%m%d%Y %H%M%S")

# Auto-kill running mimita.exe so linker can overwrite it
kill_result = subprocess.run(
    ["taskkill", "/f", "/im", "mimita.exe"],
    capture_output=True, text=True
)
time.sleep(0.5)

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

    current_ts = timestamp_now()
    history = load_build_history()

    # Write changelog
    log_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "build")
    os.makedirs(log_dir, exist_ok=True)
    log_path = os.path.join(log_dir, "changelog.txt")

    with open(log_path, "w") as f:
        f.write("=== BUILD CHANGELOG ===\n")
        f.write(f"Current time: {current_ts}\n")
        f.write(f"Last 5 run times: {', '.join(history) if history else '(none)'}\n")
        f.write(f"Time: {start.strftime('%Y-%m-%d %H:%M:%S')}\n")
        f.write(f"Status: {status}\n")
        f.write(f"Return Code: {result.returncode}\n")
        f.write(f"Duration: {elapsed:.2f}s\n")
        f.write("\n--- Build Output ---\n")
        f.write(result.stdout)
        if result.stderr:
            f.write("\n--- Stderr ---\n")
            f.write(result.stderr)

    save_build_history(current_ts)

    # Also print changelog to stdout for immediate AI inspection
    print(f"=== BUILD CHANGELOG ===")
    print(f"Current time: {current_ts}")
    print(f"Last 5 run times: {', '.join(history) if history else '(none)'}")
    print(f"Status: {status}")
    print(f"Duration: {elapsed:.2f}s")
    print(f"Log: {log_path}")
    print(f"Return Code: {result.returncode}")
    print()
    print(result.stdout)
    if result.stderr:
        print(result.stderr)

    sys.exit(result.returncode)
