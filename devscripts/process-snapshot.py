#!/usr/bin/env python3
"""Write a read-only snapshot of MiMITA-related Windows processes and build locks.

Run capture-windows-processes.bat from the repository or double-click it.
The report is saved under .dev/process-snapshots/ and contains no environment
variables, credentials, or full dev-loop state/control tokens.
"""

from __future__ import annotations

import ctypes
import datetime as dt
import json
import os
import re
import subprocess
import sys
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
REPORT_DIR = ROOT / ".dev" / "process-snapshots"
SECRET_OPTION = re.compile(
    r"(?i)(--?[\w-]*(?:password|passwd|token|secret|api[-_]?key|"
    r"authorization|cookie|credential|connection[-_]?string)[\w-]*)"
    r"(?:=|\s+)(?:\"[^\"]*\"|'[^']*'|\S+)"
)
SECRET_ASSIGNMENT = re.compile(
    r"(?i)\b(password|passwd|token|secret|api[-_]?key|authorization|"
    r"cookie|credential|connection[-_]?string)\s*=\s*"
    r"(?:\"[^\"]*\"|'[^']*'|[^\s,;]+)"
)
SECRET_QUERY = re.compile(
    r"(?i)([?&](?:password|passwd|token|secret|api[-_]?key|key)=)[^&\s]+"
)


def utc_now() -> dt.datetime:
    return dt.datetime.now(dt.timezone.utc)


def iso_utc(value: dt.datetime | None = None) -> str:
    return (value or utc_now()).isoformat(timespec="milliseconds").replace(
        "+00:00", "Z")


def redact(value: Any) -> str:
    text = str(value or "")
    text = SECRET_OPTION.sub(r"\1=<REDACTED>", text)
    text = SECRET_ASSIGNMENT.sub(lambda match: match.group(1) + "=<REDACTED>", text)
    return SECRET_QUERY.sub(r"\1<REDACTED>", text)


def parse_json(path: Path) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError, json.JSONDecodeError):
        return {}
    return value if isinstance(value, dict) else {}


def lock_file(path: Path, *, first_byte_locked: bool = False) -> dict[str, Any]:
    result: dict[str, Any] = {"path": str(path), "exists": path.is_file()}
    if not path.is_file():
        return result
    try:
        stat = path.stat()
        result["size_bytes"] = stat.st_size
        result["modified_utc"] = dt.datetime.fromtimestamp(
            stat.st_mtime, dt.timezone.utc).isoformat(timespec="milliseconds").replace(
                "+00:00", "Z")
        with path.open("rb") as handle:
            if first_byte_locked:
                # build_lock.py locks byte zero on Windows. Read the JSON after
                # that byte so the snapshot does not contend with the lock.
                handle.seek(1)
                raw = b"{" + handle.read(8192)
            else:
                raw = handle.read(8192)
        try:
            value = json.loads(raw.decode("utf-8", errors="replace").strip("\x00\r\n "))
            if isinstance(value, dict):
                result["metadata"] = value
        except (ValueError, json.JSONDecodeError):
            result["metadata_read"] = "file is present but metadata was not valid JSON"
    except OSError as error:
        result["read_error"] = f"{type(error).__name__}: {error}"
    return result


def query_processes_powershell() -> tuple[list[dict[str, Any]], str]:
    command = r"""
$ErrorActionPreference = 'Stop'
try {
  Get-CimInstance Win32_Process | ForEach-Object {
    [pscustomobject]@{
      pid = [int]$_.ProcessId
      parent_pid = [int]$_.ParentProcessId
      name = [string]$_.Name
      executable = [string]$_.ExecutablePath
      command_line = [string]$_.CommandLine
      started_utc = if ($_.CreationDate) { $_.CreationDate.ToUniversalTime().ToString('o') } else { $null }
      working_set_bytes = [long]$_.WorkingSetSize
      handle_count = [int]$_.HandleCount
      thread_count = [int]$_.ThreadCount
    }
  } | ConvertTo-Json -Depth 3 -Compress
} catch {
  [Console]::Error.WriteLine($_.Exception.Message)
  exit 3
}
"""
    executable = os.environ.get("SystemRoot", r"C:\Windows") + r"\System32\WindowsPowerShell\v1.0\powershell.exe"
    try:
        completed = subprocess.run(
            [executable, "-NoLogo", "-NoProfile", "-NonInteractive", "-Command", command],
            cwd=ROOT,
            capture_output=True,
            text=True,
            timeout=30,
            check=False,
        )
    except (OSError, subprocess.TimeoutExpired) as error:
        return [], f"PowerShell CIM query failed: {type(error).__name__}: {error}"
    if completed.returncode != 0:
        return [], redact(completed.stderr.strip() or
                          f"PowerShell CIM query exited {completed.returncode}")
    try:
        value = json.loads(completed.stdout.strip() or "null")
    except json.JSONDecodeError as error:
        return [], f"Could not parse PowerShell process output: {error}"
    if value is None:
        return [], "PowerShell returned no process rows"
    return (value if isinstance(value, list) else [value]), ""


def fallback_process_names() -> tuple[list[dict[str, Any]], str]:
    if os.name != "nt":
        return [], "Windows process enumeration is unavailable on this platform"

    class ProcessEntry32W(ctypes.Structure):
        _fields_ = [
            ("dwSize", ctypes.c_ulong),
            ("cntUsage", ctypes.c_ulong),
            ("th32ProcessID", ctypes.c_ulong),
            ("th32DefaultHeapID", ctypes.c_size_t),
            ("th32ModuleID", ctypes.c_ulong),
            ("cntThreads", ctypes.c_ulong),
            ("th32ParentProcessID", ctypes.c_ulong),
            ("pcPriClassBase", ctypes.c_long),
            ("dwFlags", ctypes.c_ulong),
            ("szExeFile", ctypes.c_wchar * 260),
        ]

    kernel32 = ctypes.windll.kernel32
    snapshot = kernel32.CreateToolhelp32Snapshot(0x00000002, 0)
    if snapshot in (0, ctypes.c_void_p(-1).value):
        return [], "Toolhelp32 process snapshot failed"
    rows: list[dict[str, Any]] = []
    try:
        entry = ProcessEntry32W()
        entry.dwSize = ctypes.sizeof(entry)
        if not kernel32.Process32FirstW(snapshot, ctypes.byref(entry)):
            return [], "Toolhelp32 could not enumerate processes"
        while True:
            rows.append({
                "pid": int(entry.th32ProcessID),
                "parent_pid": int(entry.th32ParentProcessID),
                "name": entry.szExeFile,
                "executable": None,
                "command_line": None,
                "started_utc": None,
                "working_set_bytes": None,
                "handle_count": None,
                "thread_count": int(entry.cntThreads),
            })
            if not kernel32.Process32NextW(snapshot, ctypes.byref(entry)):
                break
    finally:
        kernel32.CloseHandle(snapshot)
    return rows, "CIM was unavailable; fallback includes process names/PIDs only"


def state_summary(path: Path) -> dict[str, Any]:
    state = parse_json(path)
    if not state:
        return {"path": str(path), "available": False}
    fields = (
        "status", "build_status", "active_build_started_utc",
        "active_build_elapsed_seconds", "active_build_phase",
        "active_build_detail", "active_build_generation", "daemon_pid",
        "updated_at", "last_build_result", "last_build_duration_seconds",
        "build_authority", "latest_successful_build",
        "latest_successful_build_id", "latest_build_stale",
    )
    result = {key: state[key] for key in fields if key in state}
    result["path"] = str(path)
    result["available"] = True
    tail = state.get("build_output_tail")
    if isinstance(tail, list):
        result["build_output_tail"] = [redact(line) for line in tail[-20:]]
    return result


def relevant_processes(rows: list[dict[str, Any]], known_pids: set[int]) -> list[dict[str, Any]]:
    by_pid: dict[int, dict[str, Any]] = {}
    for row in rows:
        try:
            pid = int(row.get("pid", 0))
        except (TypeError, ValueError):
            continue
        if pid > 0:
            by_pid[pid] = row

    root_text = str(ROOT).lower().replace("/", "\\")
    selected: set[int] = set()
    for pid, row in by_pid.items():
        name = str(row.get("name") or "").lower()
        command = str(row.get("command_line") or "").lower().replace("/", "\\")
        executable = str(row.get("executable") or "").lower().replace("/", "\\")
        is_mimita_exe = name.startswith("mimita")
        is_repo_python = (name in {"python.exe", "pythonw.exe"} and
                          (root_text in command or "dev-loop.py" in command or
                           "build.py" in command))
        if is_mimita_exe or is_repo_python or root_text in executable or pid in known_pids:
            selected.add(pid)

    # Include each selected process's parent chain to show which dev-loop
    # launched the worker. This is read-only and bounded to six generations.
    for pid in list(selected):
        current = pid
        for _ in range(6):
            row = by_pid.get(current)
            if not row:
                break
            try:
                parent = int(row.get("parent_pid", 0))
            except (TypeError, ValueError):
                break
            if parent <= 0 or parent not in by_pid or parent in selected:
                break
            selected.add(parent)
            current = parent

    output = []
    for pid in sorted(selected):
        row = by_pid[pid]
        command = redact(row.get("command_line"))
        name = str(row.get("name") or "")
        role = "parent_or_launcher"
        lower_command = command.lower()
        if name.lower().startswith("mimita"):
            role = "mimita_executable_candidate"
        elif "build.py" in lower_command and "build-only" in lower_command:
            role = "build_worker_candidate"
        elif "dev-loop.py" in lower_command or "dev_loop" in lower_command:
            role = "dev_loop_candidate"
        elif name.lower() in {"python.exe", "pythonw.exe"}:
            role = "python_process_candidate"
        output.append({
            "pid": pid,
            "parent_pid": row.get("parent_pid"),
            "name": name,
            "role_evidence": role,
            "executable": row.get("executable"),
            "command_line_redacted": command or None,
            "started_utc": row.get("started_utc"),
            "working_set_bytes": row.get("working_set_bytes"),
            "handle_count": row.get("handle_count"),
            "thread_count": row.get("thread_count"),
        })
    return output


def main() -> int:
    if os.name != "nt":
        print("This process snapshot is intended to run on Windows.", file=sys.stderr)
        return 2

    report_dir = REPORT_DIR
    try:
        report_dir.resolve().relative_to(ROOT.resolve())
        report_dir.mkdir(parents=True, exist_ok=True)
        report_dir.resolve().relative_to(ROOT.resolve())
    except (OSError, ValueError) as error:
        print(f"Refusing report path outside the repository: {error}", file=sys.stderr)
        return 2

    dev = ROOT / ".dev"
    build_lock = lock_file(dev / "build-serialization.lock", first_byte_locked=True)
    scheduler_lock = lock_file(dev / "build-scheduler" / "exe-0.lock")
    loop_lock_path = dev / "dev-loop.lock"
    try:
        loop_pid = int(loop_lock_path.read_text(encoding="utf-8").strip())
    except (OSError, ValueError):
        loop_pid = 0
    state = state_summary(dev / "state.json")
    progress = parse_json(ROOT / "build" / "progress.json")
    progress_fields = (
        "pid", "phase", "current", "compiled", "skipped", "total",
        "elapsed_s", "updated_utc", "phase_seconds",
    )
    progress_summary = {key: progress[key] for key in progress_fields if key in progress}
    progress_summary["path"] = str(ROOT / "build" / "progress.json")
    if not progress:
        progress_summary["available"] = False

    known_pids = {loop_pid}
    for source in (build_lock, scheduler_lock):
        metadata = source.get("metadata")
        if isinstance(metadata, dict):
            try:
                known_pids.add(int(metadata.get("pid", 0)))
            except (TypeError, ValueError):
                pass
    for candidate in (state.get("daemon_pid"), progress_summary.get("pid")):
        try:
            known_pids.add(int(candidate or 0))
        except (TypeError, ValueError):
            pass
    known_pids.discard(0)

    processes, scan_error = query_processes_powershell()
    scan_method = "PowerShell CIM Win32_Process"
    if scan_error:
        processes, fallback_error = fallback_process_names()
        scan_method = "Toolhelp32Snapshot fallback"
        scan_error = f"{scan_error}; {fallback_error}" if fallback_error else scan_error

    observed_pids = set()
    for process in processes:
        try:
            observed_pids.add(int(process.get("pid", 0)))
        except (TypeError, ValueError):
            continue
    lock_owner_pids = sorted(known_pids)
    matched_lock_owners = [pid for pid in lock_owner_pids if pid in observed_pids]
    missing_lock_owners = [pid for pid in lock_owner_pids if pid not in observed_pids]

    report_time = utc_now()
    report = {
        "schema": 1,
        "captured_utc": iso_utc(report_time),
        "repository_root": str(ROOT),
        "machine": os.environ.get("COMPUTERNAME", "unknown"),
        "python_version": sys.version.split()[0],
        "process_scan": {
            "method": scan_method,
            "error_or_limitation": scan_error or None,
            "lock_owner_pids_found": matched_lock_owners,
            "lock_owner_pids_not_found": missing_lock_owners,
            "candidate_processes": relevant_processes(processes, known_pids),
        },
        "build_locks": {
            "dev_loop_pid_file": {
                "path": str(loop_lock_path),
                "pid": loop_pid or None,
            },
            "checkout_build_lock": build_lock,
            "machine_exe_slot": scheduler_lock,
        },
        "dev_loop_state": state,
        "build_progress": progress_summary,
    }
    filename = "process_snapshot_" + report_time.strftime("%Y%m%d_%H%M%S") + ".json"
    destination = report_dir / filename
    destination.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n",
                           encoding="utf-8", newline="\n")
    print(f"MiMITA process snapshot saved: {destination}")
    print(f"Process scan: {scan_method}; relevant process rows: "
          f"{len(report['process_scan']['candidate_processes'])}")
    if scan_error:
        print("Process scan warning: " + scan_error)
        print("Run this launcher directly in your Windows session for the full CIM scan.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
