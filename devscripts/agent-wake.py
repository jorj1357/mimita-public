#!/usr/bin/env python3
"""Durable, model-agnostic wake queue for MiMITA agents."""

from __future__ import annotations

import argparse
import datetime
import json
import os
import shutil
import subprocess
import sys
import time
import uuid
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEV_ROOT = ROOT / ".dev"
STATE_ROOT = DEV_ROOT / "agent-wake"
REQUEST_ROOT = STATE_ROOT / "requests"
LOG_ROOT = STATE_ROOT / "logs"
EVENTS_PATH = STATE_ROOT / "events.jsonl"
BUILD_READY = ROOT / "build" / "game" / "ready.json"
BUILD_CURRENT = ROOT / "build" / "game" / "current.json"
BUILD_ATTEMPT = ROOT / "build" / "game" / "last-attempt.json"
BUILD_PROGRESS = ROOT / "build" / "game" / "progress.json"


def utc_now() -> str:
    return datetime.datetime.now(datetime.timezone.utc).isoformat(
        timespec="milliseconds").replace("+00:00", "Z")


def read_json(path: Path) -> dict:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
        return value if isinstance(value, dict) else {}
    except (OSError, json.JSONDecodeError):
        return {}


def write_json(path: Path, value: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n",
                         encoding="utf-8")
    os.replace(temporary, path)


def append_event(event: str, fields: dict, reason: str = "") -> None:
    STATE_ROOT.mkdir(parents=True, exist_ok=True)
    record = {
        "category": "AGENT_WAKE",
        "event": event,
        "event_id": "WAKE_" + uuid.uuid4().hex,
        "fields": fields,
        "level": "IMPORTANT",
        "pid": os.getpid(),
        "process": "agent-wake",
        "process_role": "agent-wake",
        "reason": reason,
        "wall_time": utc_now(),
    }
    with EVENTS_PATH.open("a", encoding="utf-8", newline="\n") as handle:
        handle.write(json.dumps(record, separators=(",", ":")) + "\n")


def pid_alive(pid: int) -> bool:
    if pid <= 0:
        return False
    if os.name == "nt":
        import ctypes
        kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
        handle = kernel32.OpenProcess(0x1000, False, int(pid))
        if not handle:
            return False
        kernel32.CloseHandle(handle)
        return True
    try:
        os.kill(pid, 0)
    except OSError:
        return False
    return True


def request_paths() -> list[Path]:
    REQUEST_ROOT.mkdir(parents=True, exist_ok=True)
    return sorted(REQUEST_ROOT.glob("*.json"))


def load_requests() -> list[dict]:
    rows = []
    for path in request_paths():
        value = read_json(path)
        if value:
            value["_path"] = str(path)
            rows.append(value)
    return rows


def _request_id(prefix: str = "wake") -> str:
    return f"{prefix}-{uuid.uuid4().hex[:12]}"


def add_request(args: argparse.Namespace) -> dict:
    REQUEST_ROOT.mkdir(parents=True, exist_ok=True)
    prompt = args.prompt or ""
    if args.prompt_file:
        prompt = Path(args.prompt_file).read_text(encoding="utf-8")
    if not prompt.strip():
        raise SystemExit("a prompt or --prompt-file is required")
    condition = {
        "kind": args.when,
        "journal": args.journal,
        "event": args.event,
        "field": args.field,
        "value": args.value,
        "min_generation": int(args.min_generation or 0),
        "revision": args.revision,
        "attempt_id": args.attempt_id,
        "timeout_seconds": float(args.timeout_seconds or 0),
    }
    now = utc_now()
    request = {
        "id": _request_id(),
        "agent": args.agent,
        "issue": args.issue,
        "adapter": args.adapter,
        "model": args.model,
        "opencode_agent": args.opencode_agent,
        "command": args.adapter_command,
        "prompt": prompt,
        "condition": condition,
        "fallbacks": list(args.fallback_adapter or []),
        "status": "queued",
        "attempts": 0,
        "created_utc": now,
        "updated_utc": now,
        "claimed_utc": "",
        "launched_utc": "",
        "child_pid": 0,
        "log_path": str(LOG_ROOT / f"{now.replace(':', '-')}-{args.agent}.log"),
        "offset": 0,
        "last_condition": "",
    }
    path = REQUEST_ROOT / f"{request['id']}.json"
    write_json(path, request)
    append_event("wake.queued", {"wake": request["id"], "agent": args.agent,
                                 "issue": args.issue, "condition": condition},
                 "wake request queued")
    return request


def _journal_match(request: dict) -> tuple[bool, str]:
    condition = request.get("condition") or {}
    journal = Path(str(condition.get("journal") or ""))
    if not journal.is_absolute():
        journal = ROOT / journal
    if not journal.is_file():
        return False, "journal missing"
    offset = int(request.get("offset") or 0)
    try:
        with journal.open("rb") as handle:
            handle.seek(offset)
            for raw in handle:
                offset = handle.tell()
                try:
                    row = json.loads(raw.decode("utf-8", errors="replace"))
                except json.JSONDecodeError:
                    continue
                if row.get("event") != condition.get("event"):
                    continue
                field = str(condition.get("field") or "")
                expected = condition.get("value", "")
                if not field or str((row.get("fields") or {}).get(field)) == str(expected):
                    request["offset"] = offset
                    return True, f"matched event {condition.get('event')}"
            request["offset"] = offset
    except OSError as error:
        return False, f"journal read failed: {error}"
    return False, "event not seen"


def condition_met(request: dict) -> tuple[bool, str]:
    condition = request.get("condition") or {}
    kind = str(condition.get("kind") or "")
    now = time.monotonic()
    created = request.get("created_utc", "")
    timeout_seconds = float(condition.get("timeout_seconds") or 0)
    if kind == "build-published":
        ready = read_json(BUILD_READY)
        current = read_json(BUILD_CURRENT)
        generation = int(current.get("generation") or ready.get("generation") or 0)
        revision = str(current.get("revision") or ready.get("revision") or "")
        min_generation = int(condition.get("min_generation") or 0)
        expected_revision = str(condition.get("revision") or "")
        if ready.get("status") == "ready" and generation >= min_generation:
            if not expected_revision or revision == expected_revision:
                return True, f"build generation {generation} published"
        return False, f"waiting for build generation >= {min_generation}"
    if kind == "build-failed":
        attempt = read_json(BUILD_ATTEMPT)
        progress = read_json(BUILD_PROGRESS)
        expected = str(condition.get("attempt_id") or "")
        failed = (str(attempt.get("status") or "") == "failed" or
                  str(progress.get("status") or "") == "failed")
        if expected and expected not in {
                str(attempt.get("build_attempt_id") or ""),
                str(progress.get("build_attempt_id") or "")}:
            return False, f"waiting for failed attempt {expected}"
        if failed:
            return True, "build attempt failed"
        return False, "waiting for build failure"
    if kind == "journal-event":
        return _journal_match(request)
    if kind == "timeout":
        created_epoch = 0.0
        try:
            created_epoch = datetime.datetime.fromisoformat(
                created.replace("Z", "+00:00")).timestamp()
        except ValueError:
            pass
        if timeout_seconds > 0 and time.time() - created_epoch >= timeout_seconds:
            return True, "wake timeout reached"
        return False, "waiting for timeout"
    if kind == "manual":
        return True, "manual wake"
    return False, f"unknown condition {kind!r}"


def _prompt(request: dict) -> str:
    issue = request.get("issue") or "(none)"
    return (
        "You were woken by MiMITA agent-wake. This is a bounded test activation.\n\n"
        f"Wake request: {request.get('id')}\n"
        f"Coordination agent: {request.get('agent')}\n"
        f"Issue: {issue}\n\n"
        "First run:\n"
        "  python devscripts/agent-coord.py begin --agent "
        f"{request.get('agent')} --task \"wake test\"\n"
        "  python devscripts/agent-coord.py sync --agent "
        f"{request.get('agent')}\n"
        f"  python devscripts/agent-coord.py observe {issue} "
        f"--agent {request.get('agent')}\n\n"
        "The sync result includes a brain_check with your current issue and "
        "claimable work. Review it before acting. When you send, ask, or reply, "
        "review the returned brain_check; if the message discusses an issue, "
        "pass --issue <id> so the discussion is appended to that issue. Use "
        "`python devscripts/agent-coord.py issue suggest <id> \"<suggestion>\" "
        f"--agent {request.get('agent')}` "
        "to append a standalone idea. Avoid duplicate issues and unrelated "
        "status notes. Claim work only when you choose to own it, using "
        "`python devscripts/agent-coord.py next --claim "
        f"--agent {request.get('agent')}`.\n\n"
        "Do not invoke a compiler, build.py, a module worker, or a second "
        "dev-loop. The root dev-loop is the only builder. Use isolated test "
        "runs and the normal game commands. Inspect the exact run's "
        "events.jsonl for required and forbidden events. Report the first "
        "divergence, evidence, and confidence back to the issue. Do not edit "
        "code or commit without human approval.\n\n"
        "Requested work:\n" + request.get("prompt", "")
    )


def _adapter_command(request: dict, prompt: str, prompt_path: Path) -> list[str]:
    adapter = str(request.get("adapter") or "opencode")
    agent = str(request.get("agent") or "agent")
    if adapter == "opencode":
        executable = shutil.which("opencode") or "opencode"
        command = [executable, "run", "--dir", str(ROOT), "--format", "json",
                   "--auto", "--title", f"wake {request.get('id')}"]
        if request.get("model"):
            command.extend(["--model", str(request["model"])])
        if request.get("opencode_agent"):
            command.extend(["--agent", str(request["opencode_agent"])])
        command.append(prompt)
        return command
    if adapter == "codex":
        executable = shutil.which("codex") or "codex"
        command = [executable, "exec", "--cd", str(ROOT),
                   "--sandbox", "workspace-write", "--json", prompt]
        if request.get("model"):
            command.extend(["--model", str(request["model"])])
        return command
    command = str(request.get("command") or "")
    if not command:
        raise RuntimeError("generic adapter requires --command")
    command = command.replace("{prompt_file}", str(prompt_path))
    command = command.replace("{issue}", str(request.get("issue") or ""))
    command = command.replace("{agent}", agent)
    return [command]


def launch(request: dict, *, dry_run: bool = False) -> dict:
    subject = _prompt(request)
    log_path = Path(str(request.get("log_path") or LOG_ROOT / "wake.log"))
    log_path.parent.mkdir(parents=True, exist_ok=True)
    prompt_path = log_path.with_suffix(".prompt.txt")
    prompt_path.write_text(subject, encoding="utf-8")
    command = _adapter_command(request, subject, prompt_path)
    request["attempts"] = int(request.get("attempts") or 0) + 1
    request["status"] = "claimed"
    request["claimed_utc"] = utc_now()
    request["updated_utc"] = utc_now()
    write_json(Path(request["_path"]), request)
    append_event("wake.claimed", {"wake": request["id"],
                                  "agent": request.get("agent"),
                                  "adapter": request.get("adapter")},
                 "wake condition met")
    if dry_run:
        return {"wake": request["id"], "command": command,
                "prompt_file": str(prompt_path), "dry_run": True}
    env = dict(os.environ)
    env["MIMITA_AGENT_ID"] = str(request.get("agent") or "agent")
    env["MIMITA_WAKE_ID"] = str(request.get("id") or "")
    env["MIMITA_WAKE_ISSUE"] = str(request.get("issue") or "")
    with log_path.open("ab") as log:
        process = subprocess.Popen(
            command, cwd=ROOT, env=env, stdout=log, stderr=subprocess.STDOUT,
            shell=(str(request.get("adapter") or "") == "generic"),
            creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
    request["status"] = "launched"
    request["child_pid"] = process.pid
    request["launched_utc"] = utc_now()
    request["updated_utc"] = utc_now()
    write_json(Path(request["_path"]), request)
    append_event("wake.launched", {"wake": request["id"],
                                   "agent": request.get("agent"),
                                   "adapter": request.get("adapter"),
                                   "pid": process.pid,
                                   "log": str(log_path)},
                 "wake adapter started")
    return {"wake": request["id"], "pid": process.pid,
            "command": command, "log": str(log_path)}


def run_once(*, dry_run: bool = False) -> list[dict]:
    results = []
    for request in load_requests():
        if request.get("status") == "launched":
            child = int(request.get("child_pid") or 0)
            if child and not pid_alive(child):
                fallbacks = list(request.get("fallbacks") or [])
                if fallbacks:
                    request["adapter"] = fallbacks.pop(0)
                    request["fallbacks"] = fallbacks
                    request["status"] = "queued"
                    request["child_pid"] = 0
                    request["updated_utc"] = utc_now()
                    write_json(Path(request["_path"]), request)
                    append_event("wake.retry",
                                 {"wake": request["id"],
                                  "adapter": request["adapter"]},
                                 "previous adapter process exited; trying fallback")
                else:
                    request["status"] = "failed"
                    request["error"] = "adapter process exited"
                    request["updated_utc"] = utc_now()
                    write_json(Path(request["_path"]), request)
                    append_event("wake.failed",
                                 {"wake": request["id"],
                                  "error": request["error"]},
                                 "adapter process exited")
            continue
        if request.get("status") not in {"queued", "claimed"}:
            continue
        if request.get("status") == "claimed":
            claimed_at = str(request.get("claimed_utc") or "")
            try:
                age = time.time() - datetime.datetime.fromisoformat(
                    claimed_at.replace("Z", "+00:00")).timestamp()
            except ValueError:
                age = 9999
            if age < 300:
                continue
        met, reason = condition_met(request)
        request["last_condition"] = reason
        request["updated_utc"] = utc_now()
        write_json(Path(request["_path"]), request)
        if not met:
            continue
        try:
            results.append(launch(request, dry_run=dry_run))
        except Exception as error:  # noqa: BLE001 - queue must survive adapters
            request["status"] = "failed"
            request["error"] = str(error)
            request["updated_utc"] = utc_now()
            write_json(Path(request["_path"]), request)
            append_event("wake.failed", {"wake": request["id"],
                                         "error": str(error)},
                         "wake adapter failed to launch")
    return results


def main() -> int:
    parser = argparse.ArgumentParser()
    sub = parser.add_subparsers(dest="command", required=True)
    add = sub.add_parser("add")
    add.add_argument("--agent", required=True)
    add.add_argument("--issue", default="")
    add.add_argument("--when", required=True,
                     choices=("build-published", "build-failed",
                              "journal-event", "timeout", "manual"))
    add.add_argument("--adapter", default="opencode",
                     choices=("opencode", "codex", "generic"))
    add.add_argument("--fallback-adapter", action="append", default=[],
                     choices=("opencode", "codex", "generic"))
    add.add_argument("--model", default="")
    add.add_argument("--opencode-agent", default="")
    add.add_argument("--command", dest="adapter_command", default="")
    add.add_argument("--prompt", default="")
    add.add_argument("--prompt-file", default="")
    add.add_argument("--journal", default="")
    add.add_argument("--event", default="")
    add.add_argument("--field", default="")
    add.add_argument("--value", default="")
    add.add_argument("--min-generation", type=int, default=0)
    add.add_argument("--revision", default="")
    add.add_argument("--attempt-id", default="")
    add.add_argument("--timeout-seconds", type=float, default=0)
    sub.add_parser("list")
    run = sub.add_parser("run")
    run.add_argument("--once", action="store_true")
    run.add_argument("--dry-run", action="store_true")
    daemon = sub.add_parser("daemon")
    daemon.add_argument("--interval", type=float, default=2.0)
    daemon.add_argument("--dry-run", action="store_true")
    cancel = sub.add_parser("cancel")
    cancel.add_argument("wake_id")
    args = parser.parse_args()
    if args.command == "add":
        print(json.dumps(add_request(args), indent=2, sort_keys=True))
        return 0
    if args.command == "list":
        print(json.dumps(load_requests(), indent=2, sort_keys=True))
        return 0
    if args.command == "cancel":
        for path in request_paths():
            request = read_json(path)
            if request.get("id") == args.wake_id:
                request["status"] = "cancelled"
                request["updated_utc"] = utc_now()
                write_json(path, request)
                append_event("wake.cancelled", {"wake": args.wake_id})
                print(json.dumps(request, indent=2, sort_keys=True))
                return 0
        raise SystemExit(f"unknown wake request {args.wake_id}")
    if args.command == "run":
        results = run_once(dry_run=args.dry_run)
        print(json.dumps(results, indent=2, sort_keys=True))
        return 0
    if args.command == "daemon":
        append_event("wake.daemon.started", {"interval": args.interval})
        while True:
            try:
                run_once(dry_run=args.dry_run)
            except Exception as error:  # noqa: BLE001 - daemon must stay alive
                append_event("wake.daemon.error", {"error": str(error)},
                             "wake daemon iteration failed")
            time.sleep(max(0.5, args.interval))
    return 0


if __name__ == "__main__":
    sys.exit(main())
