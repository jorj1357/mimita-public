# 07 19 2026, 11 10
# purpose
# Build script for AI agents that compiles MiMITA without launching the game.
# Serializes agent builds through a visible lock file to avoid object-file races.
# Writes build/changelog.txt after each build so agents can verify status.
# Does NOT run mimita.exe, open graphics windows, or deploy builds.
# Does NOT modify source code other than normal compiler outputs.
# Does NOT hide failed builds or lock ownership from callers.

# build_agent.py

import subprocess
import sys
import os
import datetime
import time
import json
import socket
import atexit

ROOT = os.path.dirname(os.path.abspath(__file__))
BUILD_DIR = os.path.join(ROOT, "build")
LOCK_FILE = os.path.join(BUILD_DIR, "build-agent.lock")
LOCK_WAIT_SECONDS = 900
LOCK_POLL_SECONDS = 2

# ── Build history tracker ─────────────────────────────────────────────
BUILD_HISTORY_FILE = os.path.join(
    ROOT, "build", "build-history.txt"
)


def _process_is_running(pid):
    if not pid or pid <= 0:
        return False
    try:
        result = subprocess.run(
            ["tasklist", "/FI", f"PID eq {pid}", "/FO", "CSV", "/NH"],
            capture_output=True,
            text=True,
            timeout=5,
        )
        return f'"{pid}"' in result.stdout or f',{pid},' in result.stdout
    except Exception:
        return True


def _read_lock_info():
    try:
        with open(LOCK_FILE, "r", encoding="utf-8", errors="replace") as f:
            return json.load(f)
    except Exception:
        return {}


def _format_lock_info(info):
    if not info:
        return "unknown owner"
    return (
        f"pid={info.get('pid', '?')} "
        f"host={info.get('host', '?')} "
        f"started={info.get('started', '?')} "
        f"cmd={info.get('cmd', '?')}"
    )


def acquire_build_lock():
    os.makedirs(BUILD_DIR, exist_ok=True)
    deadline = time.time() + LOCK_WAIT_SECONDS
    warned_owner = None
    payload = {
        "pid": os.getpid(),
        "host": socket.gethostname(),
        "started": datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
        "cmd": " ".join([sys.executable] + sys.argv),
    }

    while True:
        try:
            fd = os.open(LOCK_FILE, os.O_CREAT | os.O_EXCL | os.O_WRONLY)
            with os.fdopen(fd, "w", encoding="utf-8") as f:
                json.dump(payload, f, indent=2)
            print(f"[BUILD LOCK] acquired pid={payload['pid']} file={LOCK_FILE}")
            return True
        except FileExistsError:
            info = _read_lock_info()
            owner = _format_lock_info(info)
            owner_pid = int(info.get("pid", 0) or 0)
            if not _process_is_running(owner_pid):
                print(f"[BUILD LOCK] removing stale lock: {owner}")
                try:
                    os.remove(LOCK_FILE)
                    continue
                except OSError:
                    pass

            if owner != warned_owner:
                print(f"[BUILD LOCK] waiting for active build: {owner}")
                warned_owner = owner
            if time.time() >= deadline:
                print(f"[BUILD LOCK] timeout waiting for active build: {owner}")
                return False
            time.sleep(LOCK_POLL_SECONDS)


def release_build_lock():
    info = _read_lock_info()
    if int(info.get("pid", 0) or 0) == os.getpid():
        try:
            os.remove(LOCK_FILE)
            print("[BUILD LOCK] released")
        except FileNotFoundError:
            pass

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
    if not acquire_build_lock():
        sys.exit(2)
    atexit.register(release_build_lock)

    start = datetime.datetime.now()

    build_args = [sys.executable, "build.py", "build-only"]
    if "release" in sys.argv:
        build_args.append("release")

    result = subprocess.run(
        build_args,
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
