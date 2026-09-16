# 09 16 2026
# purpose
# COLD BUILD entry for MiMITA (agents). Compiles and links a NEW, uniquely named
# executable each run: mimita-<UTC timestamp>.exe (e.g. mimita-20260916T173758.exe).
# Multiple agents (and humans via buildv3.py) can therefore build and test their
# own changes without being blocked by another running mimita executable.
# Serializes builds through a visible lock file to avoid object-file races.
# Writes build/changelog.txt and build/build-result.json after each build.
# Does NOT run mimita.exe, open graphics windows, or deploy builds.
# Does NOT kill, close, restart, or unlock a running mimita.exe.
# Does NOT relink an executable that is currently running.

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

BUILD_HISTORY_FILE = os.path.join(ROOT, "build", "build-history.txt")


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


def _name_is_running(exe_name):
    """True when an executable with this image name is running."""
    if not exe_name:
        return False
    try:
        result = subprocess.run(
            ["tasklist", "/FI", f"IMAGENAME eq {exe_name}", "/FO", "CSV", "/NH"],
            capture_output=True,
            text=True,
            timeout=5,
        )
        return exe_name.lower() in (result.stdout or "").lower()
    except Exception:
        # If detection fails, assume it might be running and protect it.
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
    if not os.path.exists(BUILD_HISTORY_FILE):
        return []
    with open(BUILD_HISTORY_FILE, "r") as f:
        lines = [line.strip() for line in f.readlines() if line.strip()]
    return lines[-5:]


def save_build_history(timestamp):
    history = load_build_history()
    history.append(timestamp)
    history = history[-5:]
    os.makedirs(os.path.dirname(BUILD_HISTORY_FILE), exist_ok=True)
    with open(BUILD_HISTORY_FILE, "w") as f:
        for h in history:
            f.write(h + "\n")


def timestamp_now():
    return datetime.datetime.now().strftime("%m%d%Y %H%M%S")


def make_exe_name():
    """A new, uniquely named executable for every build (local time)."""
    return "mimita-" + datetime.datetime.now().strftime("%Y%m%dT%H%M%S") + ".exe"


def enforce_target_not_running(exe_name):
    """Never relink an executable that is currently running.

    A build writes a NEW uniquely named file, so an unrelated running mimita
    executable is fine (each agent/human tests their own build). Only the exact
    target name being live would be a real conflict."""
    if not _name_is_running(exe_name):
        return
    if os.environ.get("MIMITA_FORCE_COLD") == "1":
        print("[COLD BUILD] MIMITA_FORCE_COLD=1: relinking a RUNNING executable.", flush=True)
        return
    print("HOT_RELOAD_BOUNDARY_VIOLATION", flush=True)
    print(f"{exe_name} is running. Refusing to relink the running executable.", flush=True)
    print("Run the build again to get a new uniquely-named executable.", flush=True)
    sys.exit(3)


def main():
    if not acquire_build_lock():
        sys.exit(2)
    atexit.register(release_build_lock)

    # Each build produces a new executable so concurrent agents/humans are not
    # blocked by another running mimita executable.
    exe_name = os.environ.get("MIMITA_EXE_NAME") or make_exe_name()
    os.environ["MIMITA_EXE_NAME"] = exe_name
    enforce_target_not_running(exe_name)

    start = datetime.datetime.now()

    build_args = [sys.executable, "build.py", "build-only"]
    if "release" in sys.argv:
        build_args.append("release")

    print(f"[BUILD] starting build.py build-only -> {exe_name}", flush=True)
    # Stream compiler output live so a slow compile is visible instead of
    # appearing to hang until the subprocess exits.
    result = subprocess.run(build_args, text=True)

    elapsed = (datetime.datetime.now() - start).total_seconds()

    if result.returncode == 0:
        if "Nothing changed" in (result.stdout or ""):
            status = "NOTHING_CHANGED"
        else:
            status = "SUCCESS"
    else:
        status = "FAILED"

    current_ts = timestamp_now()
    history = load_build_history()

    log_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "build")
    os.makedirs(log_dir, exist_ok=True)
    log_path = os.path.join(log_dir, "changelog.txt")
    exe_path = os.path.join(ROOT, exe_name)

    with open(log_path, "w") as f:
        f.write("=== BUILD CHANGELOG ===\n")
        f.write(f"Current time: {current_ts}\n")
        f.write(f"Last 5 run times: {', '.join(history) if history else '(none)'}\n")
        f.write(f"Time: {start.strftime('%Y-%m-%d %H:%M:%S')}\n")
        f.write(f"Status: {status}\n")
        f.write(f"Executable: {exe_name}\n")
        f.write(f"Return Code: {result.returncode}\n")
        f.write(f"Duration: {elapsed:.2f}s\n")
        f.write("\n--- Build Output ---\n")
        f.write(result.stdout or "")
        if result.stderr:
            f.write("\n--- Stderr ---\n")
            f.write(result.stderr)

    # Machine-readable result for agents (includes the new executable path).
    try:
        with open(os.path.join(log_dir, "build-result.json"), "w") as f:
            json.dump(
                {
                    "status": status,
                    "return_code": result.returncode,
                    "duration_s": round(elapsed, 2),
                    "utc": datetime.datetime.now(datetime.timezone.utc)
                    .strftime("%Y-%m-%dT%H:%M:%S.%f")[:-3] + "Z",
                    "executable": exe_name,
                    "executable_path": exe_path,
                },
                f,
                indent=2,
            )
    except OSError:
        pass

    save_build_history(current_ts)

    print("=== BUILD CHANGELOG ===")
    print(f"Current time: {current_ts}")
    print(f"Status: {status}")
    print(f"Executable: {exe_name}")
    print(f"Path: {exe_path}")
    print(f"Duration: {elapsed:.2f}s")
    print(f"Log: {log_path}")
    print(f"Return Code: {result.returncode}")
    print()

    sys.exit(result.returncode)


if __name__ == "__main__":
    main()
