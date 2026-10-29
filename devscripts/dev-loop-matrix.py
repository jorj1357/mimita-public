#!/usr/bin/env python3
"""Validate, plan, and run the one-server/N-client contact matrix.

This is the matrix runner companion to dev-loop.py. It reuses the dev-loop
server, stops the dev-loop's single client, then launches N clients with unique
journals and command queues.
"""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import time
import uuid
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEV_LOOP = ROOT / "devscripts" / "dev-loop.py"
DEFAULT_MATRIX = ROOT / "devscripts" / "scenarios" / "matrix" / "actor-weapon-matrix.json"
STATE_PATH = ROOT / ".dev" / "current-run.json"
OWNER_PATH = ROOT / ".dev" / "server-owner.json"


def load(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def validate(value: dict) -> list[str]:
    errors: list[str] = []
    if value.get("schema") != 1:
        errors.append("schema must be 1")
    pairings = value.get("actor_pairings")
    if not isinstance(pairings, list) or len(pairings) != 4:
        errors.append("actor_pairings must contain the four required pairings")
    for key in ("weapons", "assertions"):
        if not isinstance(value.get(key), list) or not value[key]:
            errors.append(f"{key} must be a non-empty list")
    server = value.get("server")
    if not isinstance(server, dict) or not server.get("profiles"):
        errors.append("server.profiles must be a non-empty list")
    return errors


def run_control(*args: str, timeout: int = 180) -> subprocess.CompletedProcess:
    return subprocess.run(
        [sys.executable, str(DEV_LOOP), *args],
        cwd=ROOT,
        capture_output=True,
        text=True,
        timeout=timeout,
    )


def read_state() -> dict:
    try:
        return load(STATE_PATH)
    except (OSError, json.JSONDecodeError):
        return {}


def wait_for(predicate, timeout: float, interval: float = 0.5) -> bool:
    deadline = time.time() + timeout
    while time.time() < deadline:
        if predicate():
            return True
        time.sleep(interval)
    return False


def ensure_server(launch_mode: str) -> dict:
    state = read_state()
    latest = str(state.get("latest_successful_build", ""))
    server_build = str(state.get("server_build_id", ""))
    stale_server = bool(
        state.get("server_alive") and latest and
        not server_build.startswith(latest + ":")
    )
    if stale_server:
        print(f"[matrix] server build is stale ({server_build or 'unknown'} != {latest}); restarting")
        run_control("--request", "stop-server")
        wait_for(lambda: not read_state().get("server_alive"), 30.0)
        state = read_state()
    if not (state.get("server_alive") and state.get("server_ready")):
        result = run_control("--launch-mode", launch_mode, "--request", "launch")
        print(result.stdout.strip())
        if not wait_for(lambda: read_state().get("server_alive"), 120.0):
            raise SystemExit("[matrix] server did not start")
    state = read_state()
    if not state.get("server_alive"):
        raise SystemExit("[matrix] server is not alive")
    return state


def stop_dev_loop_client() -> None:
    result = run_control("--request", "stop-client")
    print(result.stdout.strip())
    wait_for(lambda: not read_state().get("client_pids"), 30.0)


def server_port(state: dict) -> int:
    try:
        owner = load(OWNER_PATH)
        return int(owner.get("local_port", 0))
    except (OSError, json.JSONDecodeError, TypeError, ValueError):
        return int(state.get("local_port", 0) or 0)


def journal_has(path: Path, needle: str) -> bool:
    try:
        return needle in path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return False


def launch_client(state: dict, client: dict, run_dir: Path, port: int) -> dict:
    client_id = str(client["id"])
    name = str(client.get("name", client_id))
    journal = run_dir / f"client-{client_id}.jsonl"
    queue = run_dir / f"client-{client_id}.commands.jsonl"
    queue.write_text("", encoding="utf-8")
    exe = Path(state.get("server_executable", ROOT / ".dev" / "current" / "mimita.exe"))
    args = [
        str(exe),
        "--connect", f"127.0.0.1:{port}",
        "--map", str(state.get("map", "")),
        "--name", name,
        "--unlock-cursor",
        "--agent-command-file", str(queue),
    ]
    env = os.environ.copy()
    env["MIMITA_EVENTS_FILE"] = str(journal)
    env["MIMITA_RUN_ID"] = str(state.get("run_id", ""))
    env["MIMITA_BUILD_ID"] = str(state.get("server_build_id", ""))
    flags = getattr(subprocess, "CREATE_NO_WINDOW", 0)
    process = subprocess.Popen(
        args,
        cwd=ROOT,
        env=env,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        creationflags=flags,
    )
    print(f"[matrix] client {client_id} pid={process.pid} journal={journal.relative_to(ROOT)}")
    return {
        "id": client_id,
        "name": name,
        "process": process,
        "journal": journal,
        "queue": queue,
    }


def wait_client_ready(client: dict, timeout: float = 45.0) -> bool:
    journal = client["journal"]
    return wait_for(
        lambda: journal_has(journal, '"event":"logger.started"') and
                journal_has(journal, '"event":"client.actor.spawn_activated.received"'),
        timeout,
    )


def write_steps(client: dict, steps: list[dict]) -> float:
    start_ms = int(time.time() * 1000)
    offset = 0
    lines = []
    for index, step in enumerate(steps):
        command = str(step.get("command", "")).strip()
        if not command:
            continue
        args = step.get("args", [])
        text = " ".join([command, *(str(value) for value in args)]).strip()
        lines.append({
            "request_id": f"matrix-{client['id']}-{uuid.uuid4().hex[:8]}:{index}",
            "command": text,
            "scenario_id": f"matrix-{client['id']}",
            "step_id": str(step.get("id", index)),
            "step_index": index,
            "step_count": len(steps),
            "not_before_ms": start_ms + offset,
        })
        offset += max(0, int(step.get("delay_ms_after", 0) or 0))
    with client["queue"].open("a", encoding="utf-8", newline="\n") as queue:
        for line in lines:
            queue.write(json.dumps(line, separators=(",", ":")) + "\n")
    return offset / 1000.0


def read_events(path: Path) -> list[dict]:
    events = []
    try:
        lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
    except OSError:
        return events
    for line in lines:
        try:
            value = json.loads(line)
        except json.JSONDecodeError:
            continue
        if isinstance(value, dict):
            events.append(value)
    return events


def evaluate_assertions(client: dict, assertions: list[dict]) -> list[dict]:
    events = read_events(client["journal"])
    results = []
    for assertion in assertions:
        kind = assertion.get("type")
        event_name = assertion.get("event")
        matched = [event for event in events if event.get("event") == event_name]
        passed = False
        actual = {"count": len(matched)}
        if kind == "event_present":
            passed = bool(matched)
        elif kind == "event_absent":
            passed = not matched
        elif kind == "event_field_equals":
            field = assertion.get("field")
            expected = assertion.get("equals")
            values = [event.get("fields", {}).get(field) for event in matched]
            actual = {"count": len(matched), "values": values[:5]}
            passed = any(value == expected for value in values)
        results.append({
            "client": client["id"],
            "type": kind,
            "event": event_name,
            "passed": passed,
            "actual": actual,
        })
    return results


def terminate_clients(clients: list[dict]) -> None:
    for client in clients:
        process = client["process"]
        if process.poll() is None:
            process.terminate()
    deadline = time.time() + 8.0
    for client in clients:
        process = client["process"]
        try:
            process.wait(timeout=max(0.1, deadline - time.time()))
        except subprocess.TimeoutExpired:
            process.kill()


def run_matrix(matrix: dict, plan: dict, launch_mode: str) -> int:
    state = ensure_server(launch_mode)
    stop_dev_loop_client()
    state = read_state()
    port = server_port(state)
    if port <= 0:
        raise SystemExit("[matrix] no local server port")
    run_dir = Path(state.get("events_path", "")).parent
    run_dir.mkdir(parents=True, exist_ok=True)

    clients = []
    results = []
    try:
        for client_plan in plan.get("clients", []):
            clients.append(launch_client(state, client_plan, run_dir, port))
        for client in clients:
            if not wait_client_ready(client):
                print(f"[matrix] client {client['id']} did not become ready")
                return 1
        for client, client_plan in zip(clients, plan.get("clients", [])):
            seconds = write_steps(client, client_plan.get("steps", []))
            print(f"[matrix] client {client['id']} steps={len(client_plan.get('steps', []))} wait={seconds:.2f}s")
        max_wait = max(
            [sum(max(0, int(step.get("delay_ms_after", 0) or 0)) for step in c.get("steps", []))
             for c in plan.get("clients", [])] + [0]
        ) / 1000.0
        time.sleep(max_wait + 3.0)
        for client, client_plan in zip(clients, plan.get("clients", [])):
            results.extend(evaluate_assertions(client, client_plan.get("assertions", [])))
    finally:
        terminate_clients(clients)

    manifest = {
        "run_id": state.get("run_id"),
        "server_pid": state.get("server_pid"),
        "port": port,
        "clients": [
            {"id": c["id"], "name": c["name"],
             "journal": str(c["journal"].relative_to(ROOT)),
             "queue": str(c["queue"].relative_to(ROOT))}
            for c in clients
        ],
        "results": results,
    }
    (run_dir / "matrix-run.json").write_text(
        json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    failed = [result for result in results if not result["passed"]]
    print(f"[matrix] assertions={len(results)} failed={len(failed)}")
    for result in failed:
        print(f"  FAIL {result['client']} {result['type']} {result['event']} actual={result['actual']}")
    print(f"[matrix] manifest={run_dir / 'matrix-run.json'}")
    return 1 if failed else 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--matrix", default=str(DEFAULT_MATRIX))
    parser.add_argument("--plan", help="JSON plan with clients/steps/assertions")
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--print-plan", action="store_true")
    parser.add_argument("--run", action="store_true")
    parser.add_argument("--launch-mode", default=None)
    args = parser.parse_args()

    path = Path(args.matrix)
    if not path.is_absolute():
        path = ROOT / path
    try:
        value = load(path)
    except (OSError, json.JSONDecodeError) as error:
        print(f"[matrix] invalid: {error}")
        return 1

    errors = validate(value)
    if errors:
        for error in errors:
            print(f"[matrix] error: {error}")
        return 1
    print(f"[matrix] valid: {path.relative_to(ROOT)}")
    if args.print_plan:
        for profile in value["server"]["profiles"]:
            for pairing in value["actor_pairings"]:
                for weapon in value["weapons"]:
                    print(
                        f"  {profile}: {pairing['id']} weapon={weapon} "
                        f"attacker={pairing['attacker']} target={pairing['target']}"
                    )
    if args.run:
        if not args.plan:
            print("[matrix] --run requires --plan")
            return 1
        plan_path = Path(args.plan)
        if not plan_path.is_absolute():
            plan_path = ROOT / plan_path
        try:
            plan = load(plan_path)
        except (OSError, json.JSONDecodeError) as error:
            print(f"[matrix] plan invalid: {error}")
            return 1
        launch_mode = str(args.launch_mode or value.get("server", {}).get("launch_mode", "1"))
        return run_matrix(value, plan, launch_mode)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
