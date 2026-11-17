#!/usr/bin/env python3
"""Read-only network connection trace and assertion tool.

Reads the canonical per-instance events.jsonl files plus networkingconfig.json
and validates one followable reconnect/health chain. It does not start a build,
launch a process, or write gameplay state.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_CONFIG = ROOT / "config" / "networkingconfig.json"
LOGS_ROOT = ROOT / "logs"

NETWORK_PREFIXES = (
    "network.",
    "game.generation.swap_",
    "game.generation.candidate_loaded",
    "game.generation.old_shutdown",
    "game.generation.new_init",
    "game.generation.first_tick",
    "game.generation.first_snapshot",
)


def strip_json_comments(text: str) -> str:
    out: list[str] = []
    i = 0
    in_string = False
    escape = False
    while i < len(text):
        ch = text[i]
        if in_string:
            out.append(ch)
            if escape:
                escape = False
            elif ch == "\\":
                escape = True
            elif ch == '"':
                in_string = False
            i += 1
            continue
        if ch == '"':
            in_string = True
            out.append(ch)
            i += 1
            continue
        if ch == "/" and i + 1 < len(text) and text[i + 1] == "/":
            while i < len(text) and text[i] not in "\r\n":
                i += 1
            continue
        out.append(ch)
        i += 1
    return "".join(out)


def load_config(path: Path) -> dict:
    text = path.read_text(encoding="utf-8")
    return json.loads(strip_json_comments(text))


def resolve_run(value: str | None) -> Path:
    if value:
        candidate = Path(value)
        if candidate.is_file():
            return candidate.parent
        if candidate.is_dir():
            return candidate
        for run_json in LOGS_ROOT.glob(f"**/{value}/run.json"):
            return run_json.parent
        raise FileNotFoundError(f"run not found: {value}")
    candidates = sorted(LOGS_ROOT.glob("**/run.json"), key=lambda p: p.stat().st_mtime)
    if not candidates:
        raise FileNotFoundError("no run.json under logs/")
    return candidates[-1].parent


def read_run(run_dir: Path) -> tuple[dict, list[Path], list[Path]]:
    run = json.loads((run_dir / "run.json").read_text(encoding="utf-8"))
    client_journals: list[Path] = []
    server_journals: list[Path] = []
    for instance in run.get("instances", []):
        source = Path(instance.get("journal_path", ""))
        if not source.name:
            continue
        directory = source.parent
        if not directory.is_dir():
            continue
        journals = sorted(directory.glob("events-*.jsonl")) + [directory / "events.jsonl"]
        journals = [p for p in journals if p.is_file()]
        if instance.get("role") == "client":
            client_journals.extend(journals)
        elif instance.get("role") == "server":
            server_journals.extend(journals)
    return run, client_journals, server_journals


def iter_events(paths: list[Path]):
    for path in paths:
        with path.open("r", encoding="utf-8", errors="replace") as handle:
            for line in handle:
                if not line:
                    continue
                if '"event":"network.' not in line and '"event":"game.generation.' not in line:
                    continue
                try:
                    record = json.loads(line)
                except json.JSONDecodeError:
                    continue
                name = str(record.get("event", ""))
                if name.startswith(NETWORK_PREFIXES):
                    yield record


def event_time(record: dict) -> float:
    value = record.get("wall_time")
    if value:
        try:
            return datetime.fromisoformat(value.replace("Z", "+00:00")).timestamp()
        except ValueError:
            pass
    return 0.0


def field(record: dict, name: str, default=None):
    fields = record.get("fields")
    return fields.get(name, default) if isinstance(fields, dict) else default


def event_context(record: dict) -> tuple[int, int, int]:
    connection = field(record, "connection_id", 0)
    player = int(field(record, "local_player_id", 0) or 0)
    if not connection:
        connection = player
    return (
        int(connection or 0),
        int(field(record, "attempt_id", 0) or 0),
        player,
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--run", help="run id or run directory; default latest")
    parser.add_argument("--connection", type=int, help="filter by connection_id")
    parser.add_argument("--json", action="store_true", help="machine-readable summary")
    args = parser.parse_args()

    config = load_config(DEFAULT_CONFIG)
    retries = config.get("retries", {})
    interval_ms = float(retries.get("reconnect_interval_ms", 0))
    grace_ms = float(retries.get("reconnect_grace_ms", 0))
    max_attempts = int(retries.get("reconnect_max_attempts", 0))

    run_dir = resolve_run(args.run)
    run, client_paths, server_paths = read_run(run_dir)
    paths = client_paths + server_paths
    records = sorted(iter_events(paths), key=event_time)

    if args.connection is not None:
        records = [r for r in records if event_context(r)[0] == args.connection]

    errors: list[str] = []
    attempts: dict[tuple[int, int], list[dict]] = {}
    swap_windows: list[tuple[float, float]] = []
    swap_open: float | None = None
    terminal_seen = False
    recovery_seen = False
    generation_activations = 0
    rejected_slots: list[dict] = []

    for record in records:
        name = record.get("event", "")
        if name == "game.generation.swap_begin":
            swap_open = event_time(record)
        elif name == "game.generation.swap_end":
            if swap_open is not None:
                swap_windows.append((swap_open, event_time(record)))
            swap_open = None
        elif name == "network.client.reconnect.attempt":
            if str(field(record, "result", "")) == "ok":
                key = event_context(record)[:2]
                attempts.setdefault(key, []).append(record)
        elif name == "network.client.reconnect.terminal":
            terminal_seen = True
            elapsed = float(field(record, "elapsed_ms", 0) or 0)
            if grace_ms > 0 and elapsed + 1.0 < grace_ms:
                errors.append(
                    f"terminal before grace at seq {record.get('seq')}: "
                    f"elapsed_ms={elapsed} grace_ms={grace_ms}"
                )
        elif name == "network.client.snapshot.recovery":
            recovery_seen = True
        elif name == "game.generation.activated":
            generation_activations += 1
        elif name == "network.server.reconnect.rejected":
            if not field(record, "slot_state"):
                errors.append(
                    f"server reconnect rejection lacks slot_state at seq {record.get('seq')}"
                )
            rejected_slots.append(record)

    # Compatibility with pre-contract journals: attempts and server rejections
    # were previously folded into network.session.health.
    if not attempts:
        for record in records:
            if record.get("event") != "network.session.health":
                continue
            trigger = str(field(record, "trigger", ""))
            attempt = int(field(record, "reconnect_attempt", 0) or 0)
            if trigger == "reconnect_attempt" and attempt > 0:
                key = (int(field(record, "local_player_id", 0) or 0), 0)
                attempts.setdefault(key, []).append(record)
            elif trigger == "reconnect_rejected":
                rejected_slots.append(record)

    for (connection_id, attempt_id), rows in attempts.items():
        for previous, current in zip(rows, rows[1:]):
            delta_ms = (event_time(current) - event_time(previous)) * 1000.0
            if interval_ms > 0 and delta_ms + 50.0 < interval_ms:
                errors.append(
                    f"connection {connection_id} attempt {attempt_id}: "
                    f"spacing {delta_ms:.0f}ms < configured {interval_ms:.0f}ms"
                )
        numbers = [int(field(row, "attempt", 0) or 0) for row in rows]
        if max_attempts > 0 and max(numbers or [0]) > max_attempts:
            errors.append(
                f"connection {connection_id}: attempt {max(numbers)} > max {max_attempts}"
            )

    for begin, end in swap_windows:
        for record in records:
            if record.get("event") != "network.client.reconnect.attempt":
                continue
            when = event_time(record)
            if begin <= when <= end:
                errors.append(
                    f"reconnect attempt during intentional swap at seq {record.get('seq')}"
                )

    summary = {
        "run_id": run.get("run_id"),
        "journals": [str(p) for p in paths],
        "network_events": len(records),
        "connections": sorted({event_context(r)[0] for r in records}),
        "attempts": sum(len(v) for v in attempts.values()),
        "swap_windows": len(swap_windows),
        "generation_activations": generation_activations,
        "terminal_seen": terminal_seen,
        "recovery_seen": recovery_seen,
        "server_rejections": len(rejected_slots),
        "errors": errors,
    }

    if args.json:
        print(json.dumps(summary, indent=2))
    else:
        print(f"run={summary['run_id']} events={summary['network_events']} "
              f"connections={summary['connections']} attempts={summary['attempts']}")
        print(f"swap_windows={summary['swap_windows']} "
              f"generation_activations={generation_activations} "
              f"terminal={terminal_seen} recovery={recovery_seen} "
              f"server_rejections={summary['server_rejections']}")
        for error in errors:
            print(f"ERROR: {error}")
        if not records:
            print("ERROR: no network events found in this run")
        elif not errors:
            print("PASS: connection event chain is internally consistent")
    if not records:
        return 2
    return 1 if errors else 0


if __name__ == "__main__":
    raise SystemExit(main())
