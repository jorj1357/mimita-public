#!/usr/bin/env python3
"""Read-only evidence scanner for MiMITA events*.jsonl journals.

This is the first AI/human proof scanner slice. It does not close issues and
does not invent runtime proof; it extracts the facts an agent can attach to an
issue or use to choose the next bounded test.
"""

from __future__ import annotations

import argparse
import json
from collections import Counter
from pathlib import Path


def _journals(run_dir: Path) -> list[Path]:
    paths = []
    server = run_dir / "server-01"
    if server.is_dir():
        paths.extend(sorted(server.glob("events*.jsonl")))
    for client in sorted(run_dir.glob("client-*")):
        if client.is_dir():
            paths.extend(sorted(client.glob("events*.jsonl")))
    return paths


def _iter_rows(paths: list[Path]):
    for path in paths:
        try:
            with path.open("r", encoding="utf-8", errors="replace") as handle:
                for line in handle:
                    line = line.strip()
                    if not line:
                        continue
                    try:
                        row = json.loads(line)
                    except json.JSONDecodeError:
                        continue
                    if isinstance(row, dict):
                        yield path, row
        except OSError:
            continue


def _fields(row: dict) -> dict:
    value = row.get("fields")
    return value if isinstance(value, dict) else {}


def scan(run_dir: Path) -> dict:
    paths = _journals(run_dir)
    events = Counter()
    errors = Counter()
    room = {
        "stages": Counter(),
        "timeouts": 0,
        "literal_error_room_code": 0,
        "last_startup_stage": "",
    }
    melee = {
        "claims": Counter(),
        "accepted": Counter(),
        "rejected": Counter(),
        "damage": Counter(),
        "kills": Counter(),
    }
    ai = {
        "states": Counter(),
        "targets": Counter(),
        "explore": 0,
        "fallback_spawn": 0,
        "dash": 0,
        "down_dash": 0,
        "freeze": 0,
        "last_state": {},
    }
    perf = {
        "fps": [],
        "zone_ms": [],
        "frame_p99_ms": [],
        "server_tick": [],
    }

    latest_wall_time = ""
    for path, row in _iter_rows(paths):
        event = str(row.get("event") or "")
        fields = _fields(row)
        level = str(row.get("level") or "").upper()
        events[event] += 1
        latest_wall_time = str(row.get("wall_time") or latest_wall_time)
        if level == "ERRORS" or level == "ERROR":
            errors[event] += 1

        if event == "server.startup.stage":
            room["last_startup_stage"] = (
                str(fields.get("stage") or "") + ":" +
                str(fields.get("result") or ""))
        if event == "network.room-code.wait.timed-out":
            room["timeouts"] += 1
        if event in {
                "network.room-code.wait.started",
                "network.room-code.wait.completed",
                "network.room-code.wait.timed-out",
                "network.connect.stage",
                "network.gui-server.launch",
                "network.gui-server.startup-failed",
                "network.gui-server.startup-stalled"}:
            stage = fields.get("stage")
            if stage:
                room["stages"][str(stage)] += 1
        if fields.get("room_code") == "Room creation timed out":
            room["literal_error_room_code"] += 1

        if event == "melee.client.contact_claim":
            melee["claims"][str(fields.get("weapon") or "?")] += 1
        elif event == "melee.contact.claim.accepted":
            melee["accepted"][str(fields.get("weapon") or "?")] += 1
        elif event == "melee.contact.claim.rejected":
            melee["rejected"][str(fields.get("stage") or "unknown")] += 1
        elif event == "server.damage.applied" and fields.get("source") == "physical_contact":
            melee["damage"][str(fields.get("attacker_type") or "?")] += 1
            if fields.get("killed"):
                melee["kills"][str(fields.get("attacker_type") or "?")] += 1
        elif event == "kill.reward":
            melee["kills"][str(fields.get("weapon") or "?")] += 1

        if event == "player.ai.state":
            ai["last_state"] = fields
            ai["states"][str(row.get("reason") or "")] += 1
            ai["targets"][str(fields.get("target_id") or 0)] += 1
        elif event == "player.ai.explore":
            ai["explore"] += 1
        elif event == "player.ai.fallback_spawn":
            ai["fallback_spawn"] += 1
        elif event == "player.ai.action":
            ai["dash"] += 1 if fields.get("dash") else 0
            ai["down_dash"] += 1 if fields.get("down_dash") else 0
            ai["freeze"] += 1 if fields.get("freeze_pressed") else 0

        if event == "PERFORMANCE_FRAME":
            fps = fields.get("avg_fps")
            if isinstance(fps, (int, float)):
                perf["fps"].append(float(fps))
            p99 = fields.get("p99_frame_ms")
            if isinstance(p99, (int, float)):
                perf["frame_p99_ms"].append(float(p99))
        elif event == "performance.resource-window":
            raw = fields.get("raw")
            if isinstance(raw, str):
                try:
                    raw = json.loads(raw)
                except json.JSONDecodeError:
                    raw = None
            if isinstance(raw, dict):
                value = raw.get("frame_p99_ms")
                if isinstance(value, (int, float)):
                    perf["frame_p99_ms"].append(float(value))
        elif event == "performance.server-tick-window":
            value = fields.get("total_p99_ms")
            if isinstance(value, (int, float)):
                perf["server_tick"].append(float(value))

    def _bounds(values: list[float]) -> dict:
        if not values:
            return {}
        return {
            "count": len(values),
            "min": round(min(values), 3),
            "max": round(max(values), 3),
            "avg": round(sum(values) / len(values), 3),
        }

    return {
        "run_dir": str(run_dir),
        "journals": [str(path) for path in paths],
        "latest_wall_time": latest_wall_time,
        "events": dict(events.most_common()),
        "errors": dict(errors.most_common()),
        "room": room,
        "melee": melee,
        "ai": ai,
        "performance": {
            "fps": _bounds(perf["fps"]),
            "client_frame_p99_ms": _bounds(perf["frame_p99_ms"]),
            "server_tick_p99_ms": _bounds(perf["server_tick"]),
        },
    }


def _print_summary(result: dict) -> None:
    room = result["room"]
    melee = result["melee"]
    ai = result["ai"]
    print(f"run: {result['run_dir']}")
    print(f"journals: {len(result['journals'])}")
    print(f"room timeouts={room['timeouts']} literal_error_room_code={room['literal_error_room_code']} "
          f"last_startup={room['last_startup_stage'] or '-'}")
    print(f"melee claims={dict(melee['claims'])} accepted={dict(melee['accepted'])} "
          f"rejected={dict(melee['rejected'])}")
    print(f"melee physical damage={dict(melee['damage'])} kills={dict(melee['kills'])}")
    last = ai["last_state"]
    print(f"ai explore={ai['explore']} fallback_spawn={ai['fallback_spawn']} "
          f"dash={ai['dash']} down_dash={ai['down_dash']} freeze={ai['freeze']} "
          f"target={last.get('target_id', 0)} distance={last.get('target_distance', 0)}")
    print(f"performance fps={result['performance']['fps']} "
          f"frame_p99={result['performance']['client_frame_p99_ms']} "
          f"server_p99={result['performance']['server_tick_p99_ms']}")
    if result["errors"]:
        print(f"errors: {result['errors']}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("run_dir", type=Path)
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()
    if not args.run_dir.is_dir():
        raise SystemExit(f"run directory not found: {args.run_dir}")
    result = scan(args.run_dir.resolve())
    if args.json:
        print(json.dumps(result, indent=2, sort_keys=True))
    else:
        _print_summary(result)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
