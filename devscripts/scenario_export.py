"""List or export canonical scenarios from a MiMITA journal.

The canonical journal stores scenario rows selected by scenario_id. This tool
writes the same recording back into a legacy schema-2 .jsonl under
devscripts/scenarios/user so ScenarioPlayer can load it directly.

Usage:
    python devscripts/scenario_export.py --journal logs/2026-11-03/20261103_195321/events.jsonl --list
    python devscripts/scenario_export.py --journal ... --scenario scn-...
    python devscripts/scenario_export.py --journal ... --scenario scn-... --out devscripts/scenarios/user/x.jsonl
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any, Iterable

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_OUT_DIR = ROOT / "devscripts" / "scenarios" / "user"


def segments_for(journal: Path) -> list[Path]:
    folder = journal.parent
    segments = sorted(p for p in folder.glob("events*.jsonl") if p.is_file())
    if journal.exists() and journal not in segments:
        segments.append(journal)
    return segments


def records(journal: Path) -> Iterable[dict[str, Any]]:
    for segment in segments_for(journal):
        with segment.open("r", encoding="utf-8", errors="replace") as handle:
            for line in handle:
                if "scenario_id" not in line:
                    continue
                try:
                    value = json.loads(line)
                except json.JSONDecodeError:
                    continue
                fields = value.get("fields")
                if isinstance(fields, dict) and fields.get("scenario_id"):
                    yield value


def summarize(journal: Path) -> list[dict[str, Any]]:
    by_id: dict[str, dict[str, Any]] = {}
    for value in records(journal):
        fields = value["fields"]
        sid = str(fields["scenario_id"])
        event = str(value.get("event", ""))
        item = by_id.setdefault(
            sid,
            {
                "scenario_id": sid,
                "journal": str(journal),
                "started_utc": "",
                "map": "",
                "mode": "",
                "frames": 0,
                "last_tick": 0,
                "valid": None,
                "failure": "",
                "path": "",
            },
        )
        if event == "scenario.record.started":
            item["started_utc"] = str(value.get("wall_time", ""))
            item["map"] = str(fields.get("map", ""))
            item["mode"] = str(fields.get("mode", ""))
            item["path"] = str(fields.get("path", ""))
        elif event == "scenario.record.client_tick":
            item["frames"] = max(item["frames"], int(fields.get("frame", 0)) + 1)
            item["last_tick"] = max(item["last_tick"], int(fields.get("client_tick", 0)))
        elif event in {"scenario.record.completed", "scenario.record.invalid"}:
            invalid = event == "scenario.record.invalid" or not bool(
                fields.get("valid", True)
            )
            if invalid:
                item["valid"] = False
                failure = str(fields.get("capture_failure", "")) or str(
                    fields.get("reason", "")
                )
                if failure:
                    item["failure"] = failure
            elif item["valid"] is None:
                item["valid"] = True
            item["frames"] = max(item["frames"], int(fields.get("frames", 0)))
    return sorted(by_id.values(), key=lambda row: row["started_utc"] or row["scenario_id"])


def vec3(value: Any) -> list[float]:
    if isinstance(value, list) and len(value) >= 3:
        try:
            return [float(value[0]), float(value[1]), float(value[2])]
        except (TypeError, ValueError):
            pass
    return [0.0, 0.0, 0.0]


def export(journal: Path, scenario_id: str, out: Path | None) -> Path:
    start: dict[str, Any] | None = None
    ticks: list[dict[str, Any]] = []
    end: dict[str, Any] | None = None
    end_event = ""
    start_wall = ""

    for value in records(journal):
        fields = value["fields"]
        if str(fields.get("scenario_id")) != scenario_id:
            continue
        event = str(value.get("event", ""))
        if event == "scenario.record.started":
            start = fields
            start_wall = str(value.get("wall_time", ""))
        elif event == "scenario.record.client_tick":
            ticks.append(fields)
        elif event in {"scenario.record.completed", "scenario.record.invalid"}:
            end = fields
            end_event = event

    if start is None:
        raise SystemExit(f"scenario not found in journal: {scenario_id}")
    if not ticks:
        raise SystemExit(f"scenario has no client ticks: {scenario_id}")

    source_path = str(start.get("path") or "")
    if out is None:
        name = Path(source_path).name if source_path else f"{scenario_id}.jsonl"
        if not name.endswith(".jsonl"):
            name = f"{scenario_id}.jsonl"
        out = DEFAULT_OUT_DIR / name
    out.parent.mkdir(parents=True, exist_ok=True)

    start_block = start.get("start") or {}
    first_actor = (ticks[0].get("actor") or {}) if ticks else {}
    header = {
        "kind": "header",
        "schema": 2,
        "scenario_id": scenario_id,
        "source_journal": str(journal),
        "created_utc": start.get("created_utc") or start_wall,
        "tick_rate": start.get("tick_rate", 60),
        "map": start.get("map", ""),
        "mode": start.get("mode", ""),
        "weapon_set": start.get("weapon_set", 0),
        "player": first_actor.get("name", ""),
        "avatar": first_actor.get("avatar", ""),
        "networked": True,
        "tracks": ["client_input", "client_actors", "client_collision"],
        "start": start_block,
    }

    with out.open("w", encoding="utf-8", newline="\n") as handle:
        handle.write(json.dumps(header, separators=(",", ":")) + "\n")
        for fields in ticks:
            actor = fields.get("actor") or {}
            inp = fields.get("input") or {}
            contacts = actor.get("contacts") or []
            tick = {
                "kind": "tick",
                "frame": fields.get("frame", 0),
                "client_tick": fields.get("client_tick", 0),
                "server_tick": fields.get("server_tick", 0),
                "wish": inp.get("wish", [0.0, 0.0]),
                "move_world": inp.get("move_world", [0.0, 0.0]),
                "jump": inp.get("jump_held", False),
                "jump_pressed": inp.get("jump_pressed", False),
                "dash_pressed": inp.get("dash_pressed", False),
                "down_dash_pressed": inp.get("down_dash_pressed", False),
                "freeze_held": inp.get("freeze_held", False),
                "freeze_pressed": inp.get("freeze_pressed", False),
                "crouch_held": inp.get("crouch_held", False),
                "interact_held": inp.get("interact_held", False),
                "reload_pressed": inp.get("reload_pressed", False),
                "primary_held": inp.get("primary_held", False),
                "secondary_held": inp.get("secondary_held", False),
                "shield_held": inp.get("shield_held", False),
                "grab_held": inp.get("grab_held", False),
                "look_yaw": inp.get("look_yaw", 0.0),
                "look_pitch": inp.get("look_pitch", 0.0),
                "cam_forward": vec3(inp.get("cam_forward")),
                "equipped_weapon": actor.get("equipped_weapon", ""),
                "equipped_slot": actor.get("equipped_slot", 0),
                "position": vec3(actor.get("position")),
                "velocity": vec3(actor.get("velocity")),
                "health": actor.get("health", 0),
                "grounded": actor.get("grounded", False),
                "spawn_generation": actor.get("spawn_generation", 0),
                "transform_epoch": actor.get("transform_epoch", 0),
                "collision": {
                    "on_ground": actor.get("grounded", False),
                    "stable_on_ground": actor.get("stable_grounded", False),
                    "has_world_contact": actor.get("world_contact", False),
                    "contact_count": len(contacts),
                    "contacts": contacts,
                },
            }
            handle.write(json.dumps(tick, separators=(",", ":")) + "\n")
        footer = {
            "kind": "footer",
            "scenario_id": scenario_id,
            "source_journal": str(journal),
            "ended_utc": "",
            "frames": len(ticks),
            "stop_reason": (end or {}).get("stop_reason", "exported"),
            "valid": end_event == "scenario.record.completed"
            and bool((end or {}).get("valid", True)),
            "capture_failure": (end or {}).get("capture_failure", "")
            or (end or {}).get("reason", ""),
            "schema": 2,
        }
        handle.write(json.dumps(footer, separators=(",", ":")) + "\n")
    return out


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--journal", required=True, type=Path)
    parser.add_argument("--list", action="store_true")
    parser.add_argument("--scenario")
    parser.add_argument("--out", type=Path)
    args = parser.parse_args()

    journal = args.journal
    if not journal.exists():
        raise SystemExit(f"journal not found: {journal}")

    if args.list or not args.scenario:
        for row in summarize(journal):
            valid = "unknown" if row["valid"] is None else ("valid" if row["valid"] else "invalid")
            print(
                f"{row['scenario_id']}  {valid:7s}  frames={row['frames']}  "
                f"tick={row['last_tick']}  map={row['map']}  path={row['path']}"
            )
        return 0

    out = export(journal, args.scenario, args.out)
    print(f"exported: {out}")
    print(f"play: scenario_play {out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
