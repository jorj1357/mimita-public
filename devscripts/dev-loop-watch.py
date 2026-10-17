# 10 17 2026, 00 40
"""Always-on dev-loop watcher/supervisor.

Keeps exactly one checkout-owned dev-loop daemon alive and picks up edited
`devscripts/dev-loop.py` code. This implements the watcher described in
`docs/specs/dev-loop/dev-loop.md` section 19.4a, using the controlled
replacement rules in section 19.3.

Contract:
- If the checkout-owned daemon exits, promptly restart it from the current
  `devscripts/dev-loop.py`.
- If the running daemon's recorded script hash differs from the on-disk hash,
  perform a controlled replacement (stop the stale daemon, start a fresh one).
- Never spawn a competing/duplicate daemon for the same checkout; the daemon
  lock is respected and a stale lock is cleared only after its PID is dead.
- Record `dev-loop.watcher.started` / `dev-loop.watcher.restarted` (old/new
  daemon PID and script hash) in the active journal, and surface a visible error
  if it cannot keep a daemon alive.
- While a live game run is present, a source-only change is deferred by default
  so a healthy game pair is not replaced; pass `--force-source-change` to
  replace anyway.

This script does not build, launch, or own the game; it only supervises the
dev-loop daemon process.
"""

from __future__ import annotations

import argparse
import datetime
import hashlib
import json
import os
import socket
import subprocess
import sys
import time
import uuid
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEV_ROOT = ROOT / ".dev"
DEV_LOOP_SCRIPT = ROOT / "devscripts" / "dev-loop.py"
LOCK_PATH = DEV_ROOT / "dev-loop.lock"
CURRENT_RUN_PATH = DEV_ROOT / "current-run.json"
WATCH_LOCK_PATH = DEV_ROOT / "dev-loop-watch.lock"
WATCH_STATE_PATH = DEV_ROOT / "dev-loop-watch.json"
WATCH_LOG_PATH = DEV_ROOT / "dev-loop-watch.log"


def utc_now() -> str:
    return datetime.datetime.now(datetime.timezone.utc).isoformat(
        timespec="milliseconds").replace("+00:00", "Z")


def log(message: str) -> None:
    line = f"[{utc_now()}] {message}"
    print(line, flush=True)
    try:
        with open(WATCH_LOG_PATH, "a", encoding="utf-8", newline="\n") as handle:
            handle.write(line + "\n")
    except OSError:
        pass


def read_json(path: Path):
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return None


def write_json(path: Path, value) -> None:
    try:
        path.write_text(json.dumps(value, indent=2), encoding="utf-8")
    except OSError as error:
        log(f"unable to write {path}: {error}")


def script_sha256() -> str:
    try:
        return hashlib.sha256(DEV_LOOP_SCRIPT.read_bytes()).hexdigest()
    except OSError:
        return ""


def pid_alive(pid: int) -> bool:
    """Cross-platform liveness that never signals the target process."""
    if not pid or pid <= 0:
        return False
    if os.name == "nt":
        import ctypes

        SYNCHRONIZE = 0x00100000
        PROCESS_QUERY_LIMITED_INFORMATION = 0x1000
        kernel32 = ctypes.windll.kernel32
        handle = kernel32.OpenProcess(
            SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, False, int(pid))
        if not handle:
            return False
        try:
            code = ctypes.c_ulong()
            if kernel32.GetExitCodeProcess(handle, ctypes.byref(code)):
                return code.value == 259  # STILL_ACTIVE
            return False
        finally:
            kernel32.CloseHandle(handle)
    try:
        os.kill(pid, 0)
        return True
    except OSError:
        return False


def lock_pid() -> int:
    try:
        return int(LOCK_PATH.read_text(encoding="ascii").strip() or "0")
    except (OSError, ValueError):
        return 0


def append_journal_event(events_path: str, event: str, fields: dict, reason: str) -> None:
    if not events_path:
        return
    record = {
        "category": "GENERAL",
        "event": event,
        "event_id": f"WATCHER_{uuid.uuid4().hex[:12]}",
        "fields": fields,
        "level": "IMPORTANT",
        "process": "dev-loop-watch",
        "process_role": "dev-loop-watcher",
        "pid": os.getpid(),
        "reason": reason,
        "wall_time": utc_now(),
    }
    try:
        with open(events_path, "a", encoding="utf-8", newline="\n") as journal:
            journal.write(json.dumps(record, separators=(",", ":")) + "\n")
    except OSError as error:
        log(f"unable to append journal event {event}: {error}")


def current_events_path() -> str:
    run = read_json(CURRENT_RUN_PATH) or {}
    return str(run.get("events_path") or "")


def start_daemon(profile: str, launch_mode: str, visibility: str) -> int:
    # --daemon marks this as the build/game/control owner; the role marker keeps
    # Task Manager's Command Line column unambiguous (the image name stays
    # python.exe).
    args = [sys.executable, str(DEV_LOOP_SCRIPT), "--daemon",
            "--dev-loop-role=daemon", "--profile", profile,
            "--launch-mode", str(launch_mode)]
    args.append("--visible" if visibility == "visible" else "--headless")
    creationflags = 0
    if os.name == "nt":
        creationflags = (getattr(subprocess, "DETACHED_PROCESS", 0)
                         | getattr(subprocess, "CREATE_NEW_PROCESS_GROUP", 0))
    try:
        with open(WATCH_LOG_PATH, "a", encoding="utf-8") as sink:
            process = subprocess.Popen(
                args, cwd=str(ROOT), stdout=sink, stderr=subprocess.STDOUT,
                creationflags=creationflags, close_fds=True)
    except OSError as error:
        log(f"VISIBLE ERROR: unable to start dev-loop daemon: {error}")
        return 0
    return process.pid


def request_stop(port: int, token: str) -> None:
    if not port or not token:
        return
    try:
        with socket.create_connection(("127.0.0.1", int(port)), timeout=5.0) as connection:
            payload = {"token": token, "action": "stop",
                       "request_id": f"watch-{uuid.uuid4().hex[:8]}"}
            connection.sendall((json.dumps(payload) + "\n").encode("utf-8"))
            connection.recv(65536)
    except (OSError, ValueError, json.JSONDecodeError) as error:
        log(f"stop request not delivered ({error}); will clear lock if PID is dead")


def wait_for_exit(pid: int, timeout: float) -> bool:
    deadline = time.time() + timeout
    while time.time() < deadline:
        if not pid_alive(pid):
            return True
        time.sleep(0.25)
    return not pid_alive(pid)


def do_restart(profile: str, launch_mode: str, visibility: str, reason: str,
               old_pid: int, old_hash: str, force: bool) -> bool:
    run = read_json(CURRENT_RUN_PATH) or {}
    events_path = str(run.get("events_path") or "")
    port = int(run.get("control_port") or 0)
    token = str(run.get("control_token") or "")
    run_alive = bool(run.get("run_alive"))

    if run_alive and not force:
        append_journal_event(events_path, "dev-loop.watcher.source-change-deferred", {
            "reason": reason, "old_daemon_pid": old_pid, "old_script_hash": old_hash,
            "disk_script_hash": script_sha256(),
        }, "source changed while a live run is present; deferring replacement")
        log("source change detected but a live run is present; deferring "
            "(pass --force-source-change to replace)")
        return False

    current_pid = lock_pid()
    if current_pid and pid_alive(current_pid):
        request_stop(port, token)
        if not wait_for_exit(current_pid, 20.0):
            append_journal_event(events_path, "dev-loop.watcher.restart-failed", {
                "reason": reason, "stuck_daemon_pid": current_pid,
            }, "stale daemon did not exit; refusing to spawn a duplicate")
            log(f"VISIBLE ERROR: daemon PID {current_pid} did not exit; not starting a duplicate")
            return False
    # Clear a stale lock now that its owner is dead, so the new daemon starts
    # cleanly without the daemon's own stale-lock handling.
    stale = lock_pid()
    if stale and not pid_alive(stale):
        try:
            LOCK_PATH.unlink()
        except OSError:
            pass

    new_pid = start_daemon(profile, launch_mode, visibility)
    if not new_pid:
        append_journal_event(events_path, "dev-loop.watcher.restart-failed", {
            "reason": reason, "new_daemon_pid": 0,
        }, "unable to start a replacement dev-loop daemon")
        log("VISIBLE ERROR: failed to start a replacement dev-loop daemon")
        return False

    # Wait for the replacement to take the lock.
    deadline = time.time() + 30.0
    while time.time() < deadline:
        if lock_pid() and pid_alive(lock_pid()):
            break
        time.sleep(0.5)
    new_hash = script_sha256()
    append_journal_event(current_events_path() or events_path,
                         "dev-loop.watcher.restarted", {
                             "reason": reason,
                             "old_daemon_pid": old_pid,
                             "new_daemon_pid": lock_pid() or new_pid,
                             "old_script_hash": old_hash,
                             "new_script_hash": new_hash,
                         }, "dev-loop daemon replaced by watcher")
    log(f"restarted dev-loop daemon: old_pid={old_pid} new_pid={lock_pid() or new_pid} "
        f"reason={reason}")
    state = read_json(WATCH_STATE_PATH) or {}
    state.update({
        "watcher_pid": os.getpid(),
        "last_daemon_pid": lock_pid() or new_pid,
        "last_script_hash": new_hash,
        "profile": profile,
        "launch_mode": str(launch_mode),
        "visibility": visibility,
        "restarts": int(state.get("restarts", 0)) + 1,
        "updated_at": utc_now(),
    })
    write_json(WATCH_STATE_PATH, state)
    return True


def acquire_watch_lock() -> bool:
    DEV_ROOT.mkdir(parents=True, exist_ok=True)
    try:
        descriptor = os.open(str(WATCH_LOCK_PATH), os.O_CREAT | os.O_EXCL | os.O_WRONLY)
    except FileExistsError:
        owner = lock_pid_file(WATCH_LOCK_PATH)
        if owner and pid_alive(owner):
            log(f"another watcher is already running (PID {owner}); exiting")
            return False
        try:
            WATCH_LOCK_PATH.unlink()
        except OSError as error:
            log(f"VISIBLE ERROR: stale watcher lock not removable: {error}")
            return False
        return acquire_watch_lock()
    os.write(descriptor, str(os.getpid()).encode("ascii"))
    os.close(descriptor)
    return True


def lock_pid_file(path: Path) -> int:
    try:
        return int(path.read_text(encoding="ascii").strip() or "0")
    except (OSError, ValueError):
        return 0


def release_watch_lock() -> None:
    try:
        if lock_pid_file(WATCH_LOCK_PATH) == os.getpid():
            WATCH_LOCK_PATH.unlink()
    except OSError:
        pass


def run_watch(args: argparse.Namespace) -> int:
    if not acquire_watch_lock():
        return 1
    profile = args.profile
    launch_mode = args.launch_mode
    visibility = args.visibility
    # Adopt the last-used launch configuration if the caller did not override.
    run = read_json(CURRENT_RUN_PATH) or {}
    if args.profile == "npc-navigation" and run.get("profile"):
        profile = str(run.get("profile"))
    if args.launch_mode is None and run.get("launch_mode"):
        launch_mode = str(run.get("launch_mode"))
    if args.visibility is None and run.get("launch_visibility"):
        visibility = str(run.get("launch_visibility"))
    launch_mode = str(launch_mode if launch_mode is not None else "1")
    visibility = str(visibility or "visible")

    state = read_json(WATCH_STATE_PATH) or {}
    state.update({"watcher_pid": os.getpid(), "profile": profile,
                  "launch_mode": launch_mode, "visibility": visibility,
                  "started_at": utc_now(), "restarts": int(state.get("restarts", 0))})
    write_json(WATCH_STATE_PATH, state)
    append_journal_event(current_events_path(), "dev-loop.watcher.started", {
        "watcher_pid": os.getpid(), "profile": profile,
        "launch_mode": launch_mode, "visibility": visibility,
        "script_hash": script_sha256(),
    }, "dev-loop watcher started; keeping exactly one daemon alive")
    log(f"watcher started pid={os.getpid()} profile={profile} mode={launch_mode} "
        f"visibility={visibility}")

    deferred_hash = ""
    try:
        while True:
            run = read_json(CURRENT_RUN_PATH) or {}
            events_path = str(run.get("events_path") or "")
            disk_hash = script_sha256()
            running_hash = str(run.get("daemon_script_hash") or "")
            current_pid = lock_pid()
            daemon_alive = bool(current_pid and pid_alive(current_pid))

            if not daemon_alive:
                if current_pid:
                    log(f"daemon PID {current_pid} is not alive; restarting")
                else:
                    log("no dev-loop daemon lock present; starting one")
                if do_restart(profile, launch_mode, visibility, "daemon-not-alive",
                              current_pid, running_hash, force=True):
                    deferred_hash = ""
            elif running_hash and disk_hash and running_hash != disk_hash:
                if deferred_hash != disk_hash:
                    deferred_hash = disk_hash
                    log(f"on-disk dev-loop.py hash {disk_hash[:12]} differs from running "
                        f"{running_hash[:12]}")
                do_restart(profile, launch_mode, visibility, "source-hash-changed",
                           current_pid, running_hash, force=args.force_source_change)
            else:
                deferred_hash = ""

            if args.once:
                return 0
            time.sleep(max(1.0, float(args.interval)))
    except KeyboardInterrupt:
        log("watcher interrupted; releasing watch lock")
        return 0
    finally:
        release_watch_lock()


def main() -> int:
    parser = argparse.ArgumentParser(description="Always-on MiMITA dev-loop watcher")
    parser.add_argument("--profile", default="npc-navigation")
    parser.add_argument("--launch-mode", default=None)
    parser.add_argument("--visible", dest="visibility", action="store_const",
                        const="visible", default=None)
    parser.add_argument("--headless", dest="visibility", action="store_const",
                        const="headless")
    parser.add_argument("--interval", default=5.0, type=float,
                        help="seconds between checks (default 5)")
    parser.add_argument("--force-source-change", action="store_true",
                        help="replace a daemon whose script hash changed even while a run is live")
    parser.add_argument("--once", action="store_true",
                        help="run a single check and exit (for testing)")
    parser.add_argument("--dev-loop-role", default="watchdog",
                        choices=("", "watchdog", "daemon", "dashboard"),
                        help="stable role marker recorded in the process command line")
    args = parser.parse_args()
    return run_watch(args)


if __name__ == "__main__":
    raise SystemExit(main())
