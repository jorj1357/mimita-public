# 07 19 2026, 11 10
# purpose
# COLD BUILD entry for MiMITA. Compiles and relinks mimita.exe.
# Refuses to run while mimita.exe is open: the running executable must remain
# running, and live development uses the live build path instead.
# Serializes agent builds through a visible lock file to avoid object-file races.
# Writes build/changelog.txt after each build so agents can verify status.
# Does NOT run mimita.exe, open graphics windows, or deploy builds.
# Does NOT kill, close, restart, or unlock a running mimita.exe.
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
    payload = {
        "pid": os.getpid(),
        "host": socket.gethostname(),
        "started": datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
        "cmd": " ".join([sys.executable] + sys.argv),
    }

    try:
        fd = os.open(LOCK_FILE, os.O_CREAT | os.O_EXCL | os.O_WRONLY)
        with os.fdopen(fd, "w", encoding="utf-8") as f:
            json.dump(payload, f, indent=2)
        print(f"[BUILD LOCK] acquired pid={payload['pid']} file=build/build-agent.lock", flush=True)
        return True
    except FileExistsError:
        info = _read_lock_info()
        owner = _format_lock_info(info)
        owner_pid = int(info.get("pid", 0) or 0)
        if not _process_is_running(owner_pid):
            print(f"[BUILD LOCK] removing stale lock: {owner}", flush=True)
            try:
                os.remove(LOCK_FILE)
                return acquire_build_lock()
            except OSError:
                pass
        print(f"[BUILD LOCK] another build already running: {owner}", flush=True)
        print("[BUILD LOCK] skipping — build is already in progress", flush=True)
        sys.exit(0)


def release_build_lock():
    info = _read_lock_info()
    if int(info.get("pid", 0) or 0) == os.getpid():
        try:
            os.remove(LOCK_FILE)
            print("[BUILD LOCK] released", flush=True)
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

def mimita_is_running():
    """Return True when any mimita.exe process is currently running."""
    try:
        result = subprocess.run(
            ["tasklist", "/FI", "IMAGENAME eq mimita.exe", "/FO", "CSV", "/NH"],
            capture_output=True,
            text=True,
            timeout=5,
        )
        return "mimita.exe" in (result.stdout or "").lower()
    except Exception:
        # If detection fails, assume it might be running and protect it.
        return True


def enforce_live_invariant():
    """COLD BUILD must never unlock, replace, or kill a running mimita.exe."""
    if not mimita_is_running():
        return
    if os.environ.get("MIMITA_FORCE_COLD") == "1":
        print("[COLD BUILD] " + "=" * 60, flush=True)
        print("[COLD BUILD] MIMITA_FORCE_COLD=1: relinking a RUNNING mimita.exe.", flush=True)
        print("[COLD BUILD] This is the nuclear last resort and is almost never", flush=True)
        print("[COLD BUILD] necessary. The running executable is supposed to stay", flush=True)
        print("[COLD BUILD] alive and receive code edits live. Continuing anyway.", flush=True)
        print("[COLD BUILD] " + "=" * 60, flush=True)
        return
    print("HOT_RELOAD_BOUNDARY_VIOLATION", flush=True)
    print("mimita.exe is running. COLD BUILD refused: it would relink the running", flush=True)
    print("executable, which the live-development invariant forbids.", flush=True)
    print("", flush=True)
    print("Use LIVE BUILD instead (never writes mimita.exe):", flush=True)
    print("    python devscripts/live-build.py", flush=True)
    print("", flush=True)
    print("If a change genuinely cannot be activated live, classify it and move the", flush=True)
    print("behavior behind the stable hot ABI. MIMITA_FORCE_COLD=1 is the loud,", flush=True)
    print("last-resort override for an intentional cold build only.", flush=True)
    sys.exit(3)


# 07 19 2026: the old build path force-killed every running mimita.exe here.
# That is forbidden. A running mimita.exe must never be killed, closed, or
# unlocked.
enforce_live_invariant()

if __name__ == "__main__":
    if not acquire_build_lock():
        sys.exit(2)
    atexit.register(release_build_lock)

    start = datetime.datetime.now()

    build_args = [sys.executable, "build.py", "build-only"]
    if "release" in sys.argv:
        build_args.append("release")

    print("[BUILD] starting build.py build-only", flush=True)
    # Stream compiler output live so a slow compile is visible instead of
    # appearing to hang until the subprocess exits.
    result = subprocess.run(build_args, text=True)

    elapsed = (datetime.datetime.now() - start).total_seconds()

    # Determine build result
    if result.returncode == 0:
        if "Nothing changed" in (result.stdout or ""):
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
        f.write(result.stdout or "")
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
