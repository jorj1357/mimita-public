#!/usr/bin/env python3
"""
Multi-Agent Coordination System

A lightweight, file-based coordination protocol so multiple AI coding agents
do not fight each other, overwrite outputs, or build simultaneously.

Usage (from any agent script):

    from devscripts.agent_coordination import AgentCoordinator

    coord = AgentCoordinator("MyTaskName")
    coord.register()

    if coord.acquire_lock("build"):
        # do build work
        coord.release_lock("build")

    coord.update_state("ANALYZING", "Analyzing collision code...")
    # ... do work ...
    coord.update_state("PASSED", results="All tests pass")

    coord.unregister()  # optional cleanup
"""

import datetime
import json
import os
import socket
import sys
import tempfile
import time
import traceback

# ── Configuration ────────────────────────────────────────────

PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
STATE_DIR = os.path.join(PROJECT_ROOT, "project_state")
STATUS_FILE = os.path.join(STATE_DIR, "agent_status.json")
HISTORY_FILE = os.path.join(STATE_DIR, "history.log")

HEARTBEAT_INTERVAL = 20  # seconds between heartbeats
STALE_TIMEOUT = 600      # seconds before assuming agent crashed (10 min)
LOCK_RETRY_INTERVAL = 3  # seconds between lock acquisition retries

# ── Helpers ──────────────────────────────────────────────────

def _now():
    return datetime.datetime.now()

def _timestamp(dt=None):
    if dt is None:
        dt = _now()
    return dt.strftime("%Y-%m-%d %H:%M:%S")

def _hostname():
    try:
        return socket.gethostname()
    except Exception:
        return "unknown"

def _pid():
    try:
        return os.getpid()
    except Exception:
        return 0

def _branch():
    try:
        import subprocess
        result = subprocess.run(
            ["git", "rev-parse", "--abbrev-ref", "HEAD"],
            capture_output=True, text=True, cwd=PROJECT_ROOT, timeout=5
        )
        return result.stdout.strip() if result.returncode == 0 else "unknown"
    except Exception:
        return "unknown"

# ── Atomic file write ────────────────────────────────────────

def _atomic_write(data, path):
    """Write data to path atomically using a temp file + rename."""
    fd, tmp_path = tempfile.mkstemp(dir=os.path.dirname(path), suffix=".tmp")
    try:
        with os.fdopen(fd, "w", encoding="utf-8") as f:
            f.write(data)
        os.replace(tmp_path, path)
    except Exception:
        try:
            os.unlink(tmp_path)
        except Exception:
            pass
        raise

def _read_json(path):
    """Read and parse a JSON file, returning None if missing or invalid."""
    if not os.path.isfile(path):
        return None
    try:
        with open(path, "r", encoding="utf-8") as f:
            return json.load(f)
    except (json.JSONDecodeError, IOError):
        return None

def _file_lock(path, block=True):
    """
    Acquire an exclusive lock using atomic directory creation.
    os.mkdir is atomic on all platforms (Windows, Linux, macOS).
    Returns a path string (the lock dir) on success, None on failure.
    """
    lock_dir = path + ".lockdir"
    while True:
        try:
            os.mkdir(lock_dir)
            return lock_dir
        except FileExistsError:
            if not block:
                return None
            time.sleep(0.1)
        except Exception:
            return None

def _file_unlock(fd):
    """Release a directory-based lock."""
    if fd is not None:
        try:
            os.rmdir(fd)
        except Exception:
            pass

# ── Status file management ───────────────────────────────────

def _ensure_state_dir():
    os.makedirs(STATE_DIR, exist_ok=True)

def _read_status():
    """Read the full agent status dict."""
    _ensure_state_dir()
    return _read_json(STATUS_FILE) or {}

def _write_status(data, held_lock=None):
    """
    Write the full agent status dict atomically.

    If held_lock is provided, it must be a lock fd (directory path) that
    is already held by the caller. Otherwise acquires its own lock.
    """
    _ensure_state_dir()
    own_lock = None
    if held_lock is None:
        own_lock = _file_lock(STATUS_FILE)
    try:
        _atomic_write(json.dumps(data, indent=2), STATUS_FILE)
    finally:
        if own_lock:
            _file_unlock(own_lock)

def _update_agent(agent_id, updater, lock_fd=None):
    """
    Read status, apply updater function, write back.
    Handles locking once at the outer level.
    """
    own_lock = None
    if lock_fd is None:
        own_lock = _file_lock(STATUS_FILE)
        if own_lock is None:
            return
    try:
        data = _read_status()
        data = _remove_stale_entries(data)
        updater(data)
        _write_status(data, held_lock=lock_fd or own_lock)
    finally:
        if own_lock:
            _file_unlock(own_lock)

def _append_history(entry):
    """Append a line to the append-only history log."""
    _ensure_state_dir()
    try:
        with open(HISTORY_FILE, "a", encoding="utf-8") as f:
            f.write(entry + "\n")
            f.flush()
    except Exception:
        pass

def _remove_stale_entries(data):
    """Remove agent entries that haven't heartbeated within STALE_TIMEOUT."""
    now_ts = time.time()
    to_remove = []
    for agent_id, info in data.items():
        last_hb = info.get("lastHeartbeatEpoch", 0)
        if now_ts - last_hb > STALE_TIMEOUT:
            to_remove.append(agent_id)
    for agent_id in to_remove:
        entry = data.pop(agent_id, {})
        task = entry.get("task", "unknown")
        _append_history(
            f"[{_timestamp()}] Stale agent {agent_id} removed "
            f"(task='{task}', no heartbeat for >{STALE_TIMEOUT}s)"
        )
    # Also release stale locks
    for agent_id in list(data.keys()):
        info = data[agent_id]
        for lock in list(info.get("locks", [])):
            # Check if the lock owner is stale
            if agent_id in to_remove:
                _append_history(
                    f"[{_timestamp()}] Recovered stale lock '{lock}' from {agent_id}"
                )
    return data


# ── Coordinator class ────────────────────────────────────────

class AgentCoordinator:
    """
    Coordination handle for one AI agent instance.

    Usage:
        coord = AgentCoordinator("Implement replay", agent_id="Agent 4")
        coord.register()
        coord.update_state("ANALYZING", "Searching replay code...")
        coord.acquire_lock("build")
        # ... do build ...
        coord.release_lock("build")
        coord.update_state("PASSED", results="Build passed")
    """

    def __init__(self, task_description, agent_id=None):
        self.task = task_description
        self.agent_id = agent_id or f"{_hostname()}-{_pid()}"
        self._heartbeat_time = 0
        self._fd_lock = None  # file lock fd for agent_status.json

    # ── Registration ──────────────────────────────────────

    def register(self):
        """Read status, remove stale entries, register this agent."""
        def updater(data):
            data[self.agent_id] = {
                "agentId": self.agent_id,
                "hostname": _hostname(),
                "pid": _pid(),
                "task": self.task,
                "branch": _branch(),
                "startTime": _timestamp(),
                "startTimeEpoch": time.time(),
                "lastHeartbeat": _timestamp(),
                "lastHeartbeatEpoch": time.time(),
                "state": "STARTING",
                "details": "",
                "locks": [],
                "result": "",
                "resultTimestamp": "",
            }
        lock_fd = _file_lock(STATUS_FILE)
        if lock_fd is None:
            print("[COORD] WARNING: Could not acquire lock for registration")
            return
        try:
            data = _read_status()
            data = _remove_stale_entries(data)
            updater(data)
            _write_status(data, held_lock=lock_fd)
            _append_history(
                f"[{_timestamp()}] {self.agent_id} registered (task='{self.task}')"
            )
        finally:
            _file_unlock(lock_fd)
        self._heartbeat_time = time.time()
        print(f"[COORD] Registered as {self.agent_id}")

    def unregister(self):
        """Remove this agent from the status file."""
        lock_fd = _file_lock(STATUS_FILE)
        if lock_fd is None:
            return
        try:
            data = _read_status()
            if self.agent_id in data:
                held_locks = data[self.agent_id].get("locks", [])
                for lock in held_locks:
                    _append_history(
                        f"[{_timestamp()}] {self.agent_id} released lock '{lock}' "
                        f"(unregister)"
                    )
                del data[self.agent_id]
                _write_status(data, held_lock=lock_fd)
                _append_history(
                    f"[{_timestamp()}] {self.agent_id} unregistered"
                )
        finally:
            _file_unlock(lock_fd)
        print(f"[COORD] Unregistered {self.agent_id}")

    # ── State update ──────────────────────────────────────

    def update_state(self, state, details="", results=""):
        """
        Update this agent's state.

        Standard states:
            STARTING, ANALYZING, EDITING, COMPILING, RUNNING_TESTS,
            WAITING, BLOCKED, FAILED, PASSED, DONE
        """
        lock_fd = _file_lock(STATUS_FILE)
        if lock_fd is None:
            return
        try:
            data = _read_status()
            if self.agent_id not in data:
                data[self.agent_id] = {
                    "agentId": self.agent_id,
                    "hostname": _hostname(),
                    "pid": _pid(),
                    "task": self.task,
                    "branch": _branch(),
                    "startTime": _timestamp(),
                    "startTimeEpoch": time.time(),
                    "lastHeartbeat": _timestamp(),
                    "lastHeartbeatEpoch": time.time(),
                    "state": state,
                    "details": details,
                    "locks": [],
                    "result": results,
                    "resultTimestamp": _timestamp() if results else "",
                }
            else:
                data[self.agent_id]["state"] = state
                data[self.agent_id]["details"] = details
                data[self.agent_id]["lastHeartbeat"] = _timestamp()
                data[self.agent_id]["lastHeartbeatEpoch"] = time.time()
                if results:
                    data[self.agent_id]["result"] = results
                    data[self.agent_id]["resultTimestamp"] = _timestamp()
            _write_status(data, held_lock=lock_fd)
        finally:
            _file_unlock(lock_fd)
        self._heartbeat_time = time.time()

        # Print state change
        state_str = state.ljust(15)
        print(f"[COORD] [{state_str}] {self.task}")
        if details:
            print(f"         {details}")
        if results:
            print(f"         Result: {results}")

    # ── Heartbeat ─────────────────────────────────────────

    def heartbeat(self, force=False):
        """Send heartbeat if HEARTBEAT_INTERVAL has elapsed."""
        now = time.time()
        if not force and now - self._heartbeat_time < HEARTBEAT_INTERVAL:
            return
        self._heartbeat_time = now
        lock_fd = _file_lock(STATUS_FILE)
        if lock_fd is None:
            return
        try:
            data = _read_status()
            info = data.get(self.agent_id)
            if info is None:
                return
            info["lastHeartbeat"] = _timestamp()
            info["lastHeartbeatEpoch"] = time.time()
            _write_status(data, held_lock=lock_fd)
        finally:
            _file_unlock(lock_fd)

    # ── Resource locks ────────────────────────────────────

    def acquire_lock(self, lock_name, block=True, timeout=300):
        """
        Acquire a named resource lock.

        Args:
            lock_name: Name of the lock (e.g., "build", "replay", "physics").
            block: If True, block until lock is available.
            timeout: Maximum seconds to wait (0 = forever).

        Returns:
            True if lock acquired, False otherwise.
        """
        start = time.time()
        while True:
            lock_fd = _file_lock(STATUS_FILE)
            if lock_fd is None:
                if not block:
                    return False
                time.sleep(LOCK_RETRY_INTERVAL)
                continue

            try:
                data = _read_status()
                data = _remove_stale_entries(data)

                # Check if any other agent holds this lock
                owner = None
                for aid, info in data.items():
                    if aid == self.agent_id:
                        continue
                    if lock_name in info.get("locks", []):
                        owner = aid
                        break

                if owner is None:
                    if self.agent_id not in data:
                        data[self.agent_id] = {
                            "agentId": self.agent_id,
                            "hostname": _hostname(),
                            "pid": _pid(),
                            "task": self.task,
                            "branch": _branch(),
                            "startTime": _timestamp(),
                            "startTimeEpoch": time.time(),
                            "lastHeartbeat": _timestamp(),
                            "lastHeartbeatEpoch": time.time(),
                            "state": "WAITING",
                            "details": "",
                            "locks": [],
                            "result": "",
                            "resultTimestamp": "",
                        }
                    locks = data[self.agent_id].get("locks", [])
                    if lock_name not in locks:
                        locks.append(lock_name)
                    data[self.agent_id]["locks"] = locks
                    data[self.agent_id]["state"] = "RUNNING_TESTS" if lock_name == "build" else data[self.agent_id].get("state", "RUNNING_TESTS")
                    data[self.agent_id]["lastHeartbeat"] = _timestamp()
                    data[self.agent_id]["lastHeartbeatEpoch"] = time.time()
                    _write_status(data, held_lock=lock_fd)
                    _append_history(
                        f"[{_timestamp()}] {self.agent_id} acquired lock '{lock_name}'"
                    )
                    self._heartbeat_time = time.time()
                    print(f"[COORD] Acquired lock: {lock_name}")
                    return True
                else:
                    owner_info = data.get(owner, {})
                    elapsed = time.time() - start
                    remaining = timeout - elapsed

                    if not block:
                        print(f"[COORD] Lock '{lock_name}' held by {owner}")
                        return False

                    if timeout > 0 and remaining <= 0:
                        print(f"[COORD] Timed out waiting for lock '{lock_name}' from {owner}")
                        return False

                    print(f"\n{'─' * 50}")
                    print(f"Lock '{lock_name}' already held.")
                    print(f"  Owner:  {owner}")
                    print(f"  Task:   {owner_info.get('task', '?')}")
                    print(f"  State:  {owner_info.get('state', '?')}")
                    print(f"  Since:  {owner_info.get('startTime', '?')}")
                    if timeout > 0:
                        print(f"  Timeout: {int(remaining)}s remaining")
                    print(f"  Waiting...")
                    print(f"{'─' * 50}\n")
                    _append_history(
                        f"[{_timestamp()}] {self.agent_id} waiting for lock "
                        f"'{lock_name}' (held by {owner})"
                    )
            finally:
                _file_unlock(lock_fd)

            time.sleep(LOCK_RETRY_INTERVAL)

    def release_lock(self, lock_name):
        """Release a named resource lock."""
        lock_fd = _file_lock(STATUS_FILE)
        if lock_fd is None:
            return
        try:
            data = _read_status()
            info = data.get(self.agent_id)
            if info is None:
                return
            locks = info.get("locks", [])
            if lock_name in locks:
                locks.remove(lock_name)
            info["locks"] = locks
            info["lastHeartbeat"] = _timestamp()
            info["lastHeartbeatEpoch"] = time.time()
            _write_status(data, held_lock=lock_fd)
            _append_history(
                f"[{_timestamp()}] {self.agent_id} released lock '{lock_name}'"
            )
            print(f"[COORD] Released lock: {lock_name}")
        finally:
            _file_unlock(lock_fd)

    def is_locked(self, lock_name):
        """Check if a lock is held by any agent (including this one)."""
        fd = _file_lock(STATUS_FILE)
        if fd is None:
            return False
        try:
            data = _read_status()
            data = _remove_stale_entries(data)
            for aid, info in data.items():
                if lock_name in info.get("locks", []):
                    return True
            return False
        finally:
            _file_unlock(fd)

    def lock_owner(self, lock_name):
        """Return the agent_id that holds a lock, or None."""
        fd = _file_lock(STATUS_FILE)
        if fd is None:
            return None
        try:
            data = _read_status()
            for aid, info in data.items():
                if lock_name in info.get("locks", []):
                    return aid
            return None
        finally:
            _file_unlock(fd)

    # ── Status query ──────────────────────────────────────

    def list_agents(self):
        """Return a dict of all registered agents."""
        data = _read_status()
        return _remove_stale_entries(data)

    def find_agent(self, task_substring=None, state=None):
        """Find agents matching optional filters."""
        data = self.list_agents()
        results = []
        for aid, info in data.items():
            if task_substring and task_substring.lower() not in info.get("task", "").lower():
                continue
            if state and info.get("state") != state:
                continue
            results.append((aid, info))
        return results

    def wait_for_lock(self, lock_name, timeout=600):
        """Block until a lock becomes free. Returns True when free, False on timeout."""
        start = time.time()
        while True:
            owner = self.lock_owner(lock_name)
            if owner is None or owner == self.agent_id:
                return True
            if timeout > 0 and time.time() - start > timeout:
                return False
            time.sleep(2)

    # ── Result reporting ──────────────────────────────────

    def report_failure(self, reason, details=""):
        """Report a failure with details."""
        self.update_state("FAILED", details=details, results=reason)
        _append_history(
            f"[{_timestamp()}] {self.agent_id} FAILED: {reason}"
        )

    def report_success(self, summary):
        """Report a success with summary."""
        self.update_state("PASSED", results=summary)
        _append_history(
            f"[{_timestamp()}] {self.agent_id} PASSED: {summary}"
        )


# ── Print status (human readable) ────────────────────────────

def print_status():
    """Print the current agent status in human-readable format."""
    data = _read_status()
    if not data:
        print("No agents registered.")
        return

    print("\n" + "=" * 60)
    print("AGENT COORDINATION STATUS")
    print("=" * 60)

    for agent_id, info in data.items():
        state = info.get("state", "?")
        task = info.get("task", "?")
        details = info.get("details", "")
        locks = info.get("locks", [])
        heartbeat = info.get("lastHeartbeat", "?")
        result = info.get("result", "")
        hostname = info.get("hostname", "?")
        pid = info.get("pid", 0)
        branch = info.get("branch", "?")

        age = time.time() - info.get("lastHeartbeatEpoch", 0)
        stale = " [STALE]" if age > STALE_TIMEOUT else ""

        print(f"\n  {agent_id}{stale}")
        print(f"  {'─' * 40}")
        print(f"  State:     {state}")
        print(f"  Task:      {task}")
        if details:
            print(f"  Details:   {details}")
        print(f"  Host:      {hostname} (PID {pid})")
        print(f"  Branch:    {branch}")
        print(f"  Heartbeat: {heartbeat}")
        if locks:
            print(f"  Locks:     {', '.join(locks)}")
        if result:
            print(f"  Result:    {result}")

    print("=" * 60 + "\n")


# ── CLI entry point ──────────────────────────────────────────

def main():
    """CLI for agent coordination."""
    import argparse
    parser = argparse.ArgumentParser(description="Multi-Agent Coordination")
    parser.add_argument("action", nargs="?",
                        choices=["status", "register", "unregister", "lock", "unlock"],
                        help="Action to perform")
    parser.add_argument("--task", help="Task description")
    parser.add_argument("--agent", help="Agent ID")
    parser.add_argument("--lock", help="Lock name")
    parser.add_argument("--state", default="ANALYZING", help="State to set")
    parser.add_argument("--result", help="Result message")
    parser.add_argument("--detail", help="Detail message", default="")
    args = parser.parse_args()

    if args.action == "status" or not args.action:
        print_status()
        return

    if args.action == "register":
        if not args.task:
            print("ERROR: --task is required for register")
            sys.exit(1)
        coord = AgentCoordinator(args.task, agent_id=args.agent)
        coord.register()
        coord.update_state(args.state, detail=args.detail)
        return

    if args.action == "unregister":
        coord = AgentCoordinator("unregister", agent_id=args.agent)
        coord.unregister()
        return

    if args.action == "lock":
        if not args.lock:
            print("ERROR: --lock is required for lock")
            sys.exit(1)
        coord = AgentCoordinator("lock", agent_id=args.agent)
        acquired = coord.acquire_lock(args.lock, block=False)
        if acquired:
            print(f"[COORD] Acquired lock '{args.lock}'")
        else:
            owner = coord.lock_owner(args.lock)
            print(f"[COORD] Lock '{args.lock}' is held by {owner}")
        return

    if args.action == "unlock":
        if not args.lock:
            print("ERROR: --lock is required for unlock")
            sys.exit(1)
        coord = AgentCoordinator("unlock", agent_id=args.agent)
        coord.release_lock(args.lock)
        return


if __name__ == "__main__":
    main()
