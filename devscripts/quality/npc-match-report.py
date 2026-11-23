#!/usr/bin/env python3
"""Bounded NPC match report from canonical server journals.

Reads one or more server ``events.jsonl`` files (or a run directory) and
reports per-NPC and per-round movement, combat, objective, and wall-contact
statistics. It is a read-only analysis tool: it never starts a runtime, edits
a journal, or changes gameplay state.

Scale: newer ``npc.movement-metric`` records carry both raw world-unit values
and ``meters_per_world_unit``. Older records used the legacy 1.0 scale. This
tool honours the scale recorded in each event so mixed journals stay
comparable instead of silently mixing two unit systems.
"""

from __future__ import annotations

import argparse
import json
import math
import re
import statistics
import sys
from collections import Counter, defaultdict
from pathlib import Path
from typing import Any, Iterable


def rotation_key(path: Path) -> tuple[int, int]:
    name = path.name
    if name == "events.jsonl":
        return (1, 0)
    match = re.search(r"events-(\d+)\.jsonl$", name)
    if match:
        return (0, int(match.group(1)))
    return (2, 0)


def expand_journals(paths: Iterable[str]) -> list[Path]:
    found: list[Path] = []
    for raw in paths:
        path = Path(raw)
        if path.is_dir():
            server_dirs = list(path.glob("**/server-01"))
            search_dirs = server_dirs if server_dirs else [path]
            for directory in search_dirs:
                segment = sorted(directory.glob("events*.jsonl"), key=rotation_key)
                found.extend(segment)
        else:
            found.append(path)
    unique: list[Path] = []
    seen: set[Path] = set()
    for path in found:
        resolved = path.resolve()
        if resolved in seen:
            continue
        seen.add(resolved)
        unique.append(path)
    return unique


def iter_events(files: Iterable[Path]) -> Iterable[dict[str, Any]]:
    for path in files:
        with path.open("r", encoding="utf-8", errors="replace") as handle:
            for line in handle:
                line = line.strip()
                if not line:
                    continue
                try:
                    event = json.loads(line)
                except json.JSONDecodeError:
                    continue
                if isinstance(event, dict):
                    yield event


def fields_of(event: dict[str, Any]) -> dict[str, Any]:
    fields = event.get("fields")
    return fields if isinstance(fields, dict) else {}


def event_name(event: dict[str, Any]) -> str:
    name = event.get("event")
    return name if isinstance(name, str) else ""


def event_time(event: dict[str, Any]) -> float | None:
    value = event.get("t")
    return float(value) if isinstance(value, (int, float)) else None


def event_actor(event: dict[str, Any]) -> int | None:
    fields = fields_of(event)
    for key in ("actor", "npc", "actor_id"):
        value = fields.get(key)
        if isinstance(value, int):
            return value
    value = event.get("correlation_id")
    try:
        return int(value)
    except (TypeError, ValueError):
        return None


def number(value: Any, default: float = 0.0) -> float:
    return float(value) if isinstance(value, (int, float)) else default


def integer(value: Any, default: int = 0) -> int:
    return int(value) if isinstance(value, (int, float)) else default


def vec3(value: Any) -> tuple[float, float, float] | None:
    if isinstance(value, (list, tuple)) and len(value) >= 3:
        try:
            return (float(value[0]), float(value[1]), float(value[2]))
        except (TypeError, ValueError):
            return None
    return None


def distance(a: tuple[float, float, float], b: tuple[float, float, float]) -> float:
    return math.sqrt((a[0] - b[0]) ** 2 + (a[1] - b[1]) ** 2 + (a[2] - b[2]) ** 2)


def planar_distance(a: tuple[float, float, float], b: tuple[float, float, float]) -> float:
    return math.hypot(a[0] - b[0], a[1] - b[1])


def percentile(values: list[float], fraction: float) -> float | None:
    if not values:
        return None
    ordered = sorted(values)
    if len(ordered) == 1:
        return ordered[0]
    position = fraction * (len(ordered) - 1)
    low = int(math.floor(position))
    high = int(math.ceil(position))
    if low == high:
        return ordered[low]
    blend = position - low
    return ordered[low] * (1.0 - blend) + ordered[high] * blend


def mean(values: list[float]) -> float | None:
    return statistics.fmean(values) if values else None


def fmt(value: Any, digits: int = 2) -> str:
    if value is None:
        return "-"
    if isinstance(value, float):
        if not math.isfinite(value):
            return "-"
        return f"{value:.{digits}f}"
    return str(value)


class LifeAccumulator:
    __slots__ = (
        "life_seconds",
        "distance_units",
        "planar_distance_units",
        "scale",
        "dash",
        "air_dash",
        "down_dash",
        "jump",
        "bhop",
        "freeze",
        "air_seconds",
        "air_move_seconds",
        "wall_contact_seconds",
        "near_wall_avoid_seconds",
        "blocked_motion_seconds",
        "max_speed_mps",
    )

    def __init__(self) -> None:
        self.life_seconds = 0.0
        self.distance_units = 0.0
        self.planar_distance_units = 0.0
        self.scale = 1.0
        self.dash = 0
        self.air_dash = 0
        self.down_dash = 0
        self.jump = 0
        self.bhop = 0
        self.freeze = 0
        self.air_seconds = 0.0
        self.air_move_seconds = 0.0
        self.wall_contact_seconds = 0.0
        self.near_wall_avoid_seconds = 0.0
        self.blocked_motion_seconds = 0.0
        self.max_speed_mps = 0.0


def life_from_metric(fields: dict[str, Any]) -> LifeAccumulator:
    life = LifeAccumulator()
    life.life_seconds = number(fields.get("life_elapsed_seconds"))
    life.scale = number(fields.get("meters_per_world_unit"), 1.0) or 1.0
    life.distance_units = number(
        fields.get("life_distance_3d_units"),
        number(fields.get("life_distance_3d_m")),
    )
    life.planar_distance_units = number(
        fields.get("life_planar_distance_units"),
        number(fields.get("life_planar_distance_m")),
    )
    life.dash = integer(fields.get("dash_count"))
    life.air_dash = integer(fields.get("air_dash_count"))
    life.down_dash = integer(fields.get("down_dash_count"))
    life.jump = integer(fields.get("jump_count"))
    life.bhop = integer(fields.get("bhop_count"))
    life.freeze = integer(fields.get("freeze_count"))
    life.air_seconds = number(fields.get("air_seconds"))
    life.air_move_seconds = number(fields.get("air_movement_seconds"))
    life.wall_contact_seconds = number(fields.get("wall_contact_seconds"))
    life.near_wall_avoid_seconds = number(
        fields.get("near_wall_avoid_seconds")
    )
    life.blocked_motion_seconds = number(fields.get("blocked_motion_seconds"))
    life.max_speed_mps = number(
        fields.get("max_planar_speed_mps"),
        number(fields.get("current_planar_speed_mps")),
    )
    return life


def summarize_lives(lives: list[LifeAccumulator]) -> dict[str, Any]:
    total_seconds = sum(life.life_seconds for life in lives)
    total_units = sum(life.distance_units for life in lives)
    calibrated = sum(life.distance_units * life.scale for life in lives)
    return {
        "lives": len(lives),
        "life_seconds": total_seconds,
        "distance_m": calibrated,
        "average_speed_mps": (calibrated / total_seconds) if total_seconds > 0 else None,
        "max_speed_mps": max((life.max_speed_mps for life in lives), default=0.0),
        "dash": sum(life.dash for life in lives),
        "air_dash": sum(life.air_dash for life in lives),
        "down_dash": sum(life.down_dash for life in lives),
        "jump": sum(life.jump for life in lives),
        "bhop": sum(life.bhop for life in lives),
        "freeze": sum(life.freeze for life in lives),
        "air_seconds": sum(life.air_seconds for life in lives),
        "air_move_seconds": sum(life.air_move_seconds for life in lives),
        "wall_contact_seconds": sum(life.wall_contact_seconds for life in lives),
        "near_wall_avoid_seconds": sum(
            life.near_wall_avoid_seconds for life in lives
        ),
        "blocked_motion_seconds": sum(
            life.blocked_motion_seconds for life in lives
        ),
        "_raw_distance_units": total_units,
    }


def per_minute(count: int, seconds: float) -> float | None:
    return (count * 60.0 / seconds) if seconds > 0 else None


def build_report(files: list[Path]) -> dict[str, Any]:
    name_counts: Counter[str] = Counter()
    actor_team: dict[int, int] = {}
    actor_lives: dict[int, list[LifeAccumulator]] = defaultdict(list)
    actor_current: dict[int, LifeAccumulator] = {}
    actor_events: dict[int, Counter[str]] = defaultdict(Counter)

    attack_positions: dict[int, list[tuple[float, tuple[float, float, float]]]] = defaultdict(list)

    round_states: dict[int, dict[str, Any]] = {}
    current_round: int | None = None

    first_damage: dict[int, float] = {}
    spawn_time: dict[int, float] = {}
    npc_npc_ttks: list[float] = []
    npc_npc_kill_times: list[float] = []
    kills_per_round: Counter[int] = Counter()
    damage_kills = 0
    reward_kills = 0
    npc_deaths = 0

    first_t: float | None = None
    last_t: float | None = None
    observed_scale: float | None = None

    def close_life(actor: int) -> None:
        life = actor_current.pop(actor, None)
        if life is not None and life.life_seconds > 0.0:
            actor_lives[actor].append(life)

    def round_state(number_value: int) -> dict[str, Any]:
        state = round_states.get(number_value)
        if state is None:
            state = {
                "round": number_value,
                "start_t": None,
                "go_t": None,
                "plant_start_t": None,
                "planted_t": None,
                "plant_site": None,
                "defuse_assigned_t": None,
                "defuse_start_t": None,
                "defused_t": None,
                "exploded_t": None,
                "result_t": None,
                "winner_team": None,
                "reason": None,
                "plant_interruptions": 0,
            }
            round_states[number_value] = state
        return state

    for event in iter_events(files):
        name = event_name(event)
        if not name:
            continue
        name_counts[name] += 1
        timestamp = event_time(event)
        if timestamp is not None:
            first_t = timestamp if first_t is None else min(first_t, timestamp)
            last_t = timestamp if last_t is None else max(last_t, timestamp)

        if name == "npc.movement-metric":
            fields = fields_of(event)
            actor = integer(fields.get("actor"))
            if actor <= 0:
                continue
            team = integer(fields.get("team"), -1)
            if team in (0, 1):
                actor_team[actor] = team
            scale = number(fields.get("meters_per_world_unit"), 0.0)
            if scale > 0.0 and observed_scale is None:
                observed_scale = scale
            life_seconds = number(fields.get("life_elapsed_seconds"))
            previous = actor_current.get(actor)
            if previous is not None and life_seconds + 0.25 < previous.life_seconds:
                close_life(actor)
            actor_current[actor] = life_from_metric(fields)
            continue

        if name == "npc.attack-gate":
            actor = event_actor(event)
            position = vec3(fields_of(event).get("pos"))
            if actor is not None and position is not None and timestamp is not None:
                attack_positions[actor].append((timestamp, position))
            continue

        if name in (
            "npc.dash",
            "npc.jump",
            "npc.freeze",
            "npc.stuck-episode",
            "npc.stuck-recovery",
            "npc.movement-progress-failed",
            "npc.nav-plan-failed",
            "npc.nav-objective-fallback",
            "npc.area-escape",
        ):
            actor = event_actor(event)
            if actor is not None:
                actor_events[actor][name] += 1
            continue

        if name == "npc.death":
            npc_deaths += 1
            continue

        if name in ("npc.spawn.resolved", "npc.respawn"):
            actor = event_actor(event)
            if actor is not None and timestamp is not None:
                spawn_time[actor] = timestamp
                first_damage.pop(actor, None)
            continue

        if name == "server.damage.applied":
            fields = fields_of(event)
            attacker = fields.get("attacker_id")
            target = fields.get("target_actor_id")
            attacker_type = fields.get("attacker_type")
            target_type = fields.get("target_actor_type")
            if not (isinstance(attacker, int) and isinstance(target, int)):
                continue
            if attacker_type != "npc" or target_type != "npc":
                continue
            if timestamp is None:
                continue
            first_damage.setdefault(target, timestamp)
            if fields.get("killed") is True:
                damage_kills += 1
                start = first_damage.get(target)
                if start is not None:
                    npc_npc_ttks.append(max(0.0, timestamp - start))
                first_damage.pop(target, None)
                npc_npc_kill_times.append(timestamp)
                if current_round is not None:
                    kills_per_round[current_round] += 1
                actor_events[attacker]["npc-vs-npc-kill"] += 1
            continue

        if name == "kill.reward":
            fields = fields_of(event)
            if fields.get("killer_is_npc") is True and fields.get("victim_is_npc") is True:
                reward_kills += 1
            continue

        if name == "round.start":
            round_number = integer(fields_of(event).get("round"), -1)
            if round_number >= 0:
                current_round = round_number
                state = round_state(round_number)
                if timestamp is not None:
                    state["start_t"] = timestamp
                # A new round invalidates per-actor damage engagements.
                first_damage.clear()
            continue

        if name == "round.go":
            round_number = integer(fields_of(event).get("round"), -1)
            if round_number >= 0:
                state = round_state(round_number)
                if timestamp is not None:
                    state["go_t"] = timestamp
            continue

        if name == "objective.plant-start":
            round_number = current_round
            fields = fields_of(event)
            if round_number is None or round_number < 0:
                continue
            state = round_state(round_number)
            if state["plant_start_t"] is None and timestamp is not None:
                state["plant_start_t"] = timestamp
                state["plant_site"] = fields.get("site")
            continue

        if name == "objective.plant-interrupted":
            if current_round is not None:
                round_state(current_round)["plant_interruptions"] += 1
            continue

        if name == "objective.planted":
            if current_round is not None:
                state = round_state(current_round)
                if timestamp is not None:
                    state["planted_t"] = timestamp
                state["plant_site"] = fields_of(event).get("site", state["plant_site"])
            continue

        if name == "objective.defuse-assigned":
            if current_round is not None:
                state = round_state(current_round)
                if timestamp is not None and state["defuse_assigned_t"] is None:
                    state["defuse_assigned_t"] = timestamp
            continue

        if name == "objective.defuse-start":
            if current_round is not None:
                state = round_state(current_round)
                if timestamp is not None and state["defuse_start_t"] is None:
                    state["defuse_start_t"] = timestamp
            continue

        if name == "objective.defused":
            if current_round is not None:
                state = round_state(current_round)
                if timestamp is not None:
                    state["defused_t"] = timestamp
            continue

        if name == "objective.exploded":
            if current_round is not None:
                state = round_state(current_round)
                if timestamp is not None:
                    state["exploded_t"] = timestamp
            continue

        if name == "round.result":
            round_number = integer(fields_of(event).get("round"), -1)
            if round_number >= 0:
                state = round_state(round_number)
                state["result_t"] = timestamp
                state["winner_team"] = fields_of(event).get("winner_team")
                state["reason"] = fields_of(event).get("reason")
            continue

    for actor in list(actor_current):
        close_life(actor)

    actor_summaries: dict[str, Any] = {}
    for actor, lives in actor_lives.items():
        summary = summarize_lives(lives)
        total_seconds = summary["life_seconds"]
        summary["team"] = actor_team.get(actor)
        summary["dash_counters"] = summary["dash"]
        summary["dash_events"] = actor_events[actor].get("npc.dash", 0)
        summary["dash_source"] = "counter" if summary["dash"] else "event"
        if not summary["dash"]:
            summary["dash"] = summary["dash_events"]
        summary["jump_counters"] = summary["jump"]
        summary["jump_events"] = actor_events[actor].get("npc.jump", 0)
        if not summary["jump"]:
            summary["jump"] = summary["jump_events"]
        summary["freeze_counters"] = summary["freeze"]
        summary["freeze_events"] = actor_events[actor].get("npc.freeze", 0)
        if not summary["freeze"]:
            summary["freeze"] = summary["freeze_events"]
        summary["dash_per_minute"] = per_minute(summary["dash"], total_seconds)
        summary["jump_per_minute"] = per_minute(summary["jump"], total_seconds)
        summary["bhop_per_minute"] = per_minute(summary["bhop"], total_seconds)
        summary["down_dash_per_minute"] = per_minute(summary["down_dash"], total_seconds)
        summary["freeze_per_minute"] = per_minute(summary["freeze"], total_seconds)
        summary["air_ratio"] = (
            summary["air_seconds"] / total_seconds if total_seconds > 0 else None
        )
        summary["wall_contact_ratio"] = (
            summary["wall_contact_seconds"] / total_seconds
            if total_seconds > 0
            else None
        )
        summary["near_wall_avoid_ratio"] = (
            summary["near_wall_avoid_seconds"] / total_seconds
            if total_seconds > 0
            else None
        )
        summary["blocked_motion_ratio"] = (
            summary["blocked_motion_seconds"] / total_seconds
            if total_seconds > 0
            else None
        )
        summary["stuck_episodes"] = actor_events[actor].get("npc.stuck-episode", 0)
        summary["stuck_per_minute"] = per_minute(
            summary["stuck_episodes"], total_seconds
        )
        summary["progress_failed"] = actor_events[actor].get(
            "npc.movement-progress-failed", 0
        )
        attack_samples = attack_positions.get(actor, [])
        attack_samples.sort(key=lambda item: item[0])
        segment_speeds: list[float] = []
        segment_distance = 0.0
        segment_seconds = 0.0
        for index in range(1, len(attack_samples)):
            t0, p0 = attack_samples[index - 1]
            t1, p1 = attack_samples[index]
            delta_t = t1 - t0
            if delta_t < 0.05 or delta_t > 2.0:
                continue
            segment = distance(p0, p1)
            speed = segment / delta_t
            if speed > 150.0:
                continue
            segment_distance += segment
            segment_seconds += delta_t
            segment_speeds.append(speed)
        speed_scale = observed_scale if observed_scale is not None else 1.0
        summary["attack_gate_segments"] = len(segment_speeds)
        summary["attack_gate_average_units_per_second"] = (
            segment_distance / segment_seconds if segment_seconds > 0 else None
        )
        summary["attack_gate_average_mps"] = (
            summary["attack_gate_average_units_per_second"] * speed_scale
            if summary["attack_gate_average_units_per_second"] is not None
            else None
        )
        summary["attack_gate_median_mps"] = (
            statistics.median(segment_speeds) * speed_scale
            if segment_speeds
            else None
        )
        summary["attack_gate_p90_mps"] = (
            (percentile(segment_speeds, 0.90) or 0.0) * speed_scale
            if segment_speeds
            else None
        )
        actor_summaries[str(actor)] = summary

    team_summaries: dict[str, Any] = {}
    for team in (0, 1):
        team_actors = [
            summary
            for summary in actor_summaries.values()
            if summary.get("team") == team
        ]
        total_seconds = sum(summary["life_seconds"] for summary in team_actors)
        total_distance = sum(summary["distance_m"] for summary in team_actors)
        team_summaries[str(team)] = {
            "actors": len(team_actors),
            "life_seconds": total_seconds,
            "average_speed_mps": (
                total_distance / total_seconds if total_seconds > 0 else None
            ),
            "dash": sum(summary["dash"] for summary in team_actors),
            "dash_per_minute": per_minute(
                sum(summary["dash"] for summary in team_actors), total_seconds
            ),
            "down_dash": sum(summary["down_dash"] for summary in team_actors),
            "down_dash_per_minute": per_minute(
                sum(summary["down_dash"] for summary in team_actors), total_seconds
            ),
            "bhop": sum(summary["bhop"] for summary in team_actors),
            "bhop_per_minute": per_minute(
                sum(summary["bhop"] for summary in team_actors), total_seconds
            ),
            "jump": sum(summary["jump"] for summary in team_actors),
            "freeze": sum(summary["freeze"] for summary in team_actors),
            "air_ratio": (
                sum(summary["air_seconds"] for summary in team_actors) / total_seconds
                if total_seconds > 0
                else None
            ),
            "wall_contact_ratio": (
                sum(summary["wall_contact_seconds"] for summary in team_actors)
                / total_seconds
                if total_seconds > 0
                else None
            ),
            "near_wall_avoid_ratio": (
                sum(summary["near_wall_avoid_seconds"] for summary in team_actors)
                / total_seconds
                if total_seconds > 0
                else None
            ),
            "blocked_motion_ratio": (
                sum(summary["blocked_motion_seconds"] for summary in team_actors)
                / total_seconds
                if total_seconds > 0
                else None
            ),
            "stuck_episodes": sum(summary["stuck_episodes"] for summary in team_actors),
            "stuck_per_minute": per_minute(
                sum(summary["stuck_episodes"] for summary in team_actors),
                total_seconds,
            ),
        }

    round_summaries: list[dict[str, Any]] = []
    for round_number in sorted(round_states):
        state = dict(round_states[round_number])
        start = state.get("go_t") or state.get("start_t")
        if start is not None and state.get("plant_start_t") is not None:
            state["plant_latency_seconds"] = state["plant_start_t"] - start
        if state.get("plant_start_t") is not None and state.get("planted_t") is not None:
            state["plant_duration_seconds"] = state["planted_t"] - state["plant_start_t"]
        if state.get("planted_t") is not None and state.get("defuse_start_t") is not None:
            state["defuse_latency_seconds"] = state["defuse_start_t"] - state["planted_t"]
        if state.get("defuse_start_t") is not None and state.get("defused_t") is not None:
            state["defuse_duration_seconds"] = state["defused_t"] - state["defuse_start_t"]
        state["npc_npc_kills"] = kills_per_round.get(round_number, 0)
        round_summaries.append(state)

    kill_intervals = [
        b - a for a, b in zip(npc_npc_kill_times, npc_npc_kill_times[1:])
    ]
    combat = {
        "npc_npc_damage_kills": damage_kills,
        "npc_npc_reward_kills": reward_kills,
        "npc_deaths": npc_deaths,
        "ttk_samples": len(npc_npc_ttks),
        "ttk_mean_seconds": mean(npc_npc_ttks),
        "ttk_median_seconds": statistics.median(npc_npc_ttks) if npc_npc_ttks else None,
        "ttk_p90_seconds": percentile(npc_npc_ttks, 0.90),
        "kill_interval_mean_seconds": mean(kill_intervals),
        "kill_interval_median_seconds": (
            statistics.median(kill_intervals) if kill_intervals else None
        ),
        "kills_per_round_mean": (
            statistics.fmean(kills_per_round.values()) if kills_per_round else None
        ),
        "rounds_with_kills": len(kills_per_round),
    }

    diagnostics = {
        name: count
        for name, count in sorted(name_counts.items())
        if name.startswith("npc.")
        and any(
            token in name
            for token in (
                "dash",
                "jump",
                "freeze",
                "stuck",
                "progress",
                "nav-plan",
                "nav-objective",
                "area-escape",
                "defuse",
                "movement-metric",
            )
        )
    }

    return {
        "journals": [str(path) for path in files],
        "meters_per_world_unit": observed_scale,
        "first_t": first_t,
        "last_t": last_t,
        "event_counts": dict(name_counts),
        "diagnostics": diagnostics,
        "actors": actor_summaries,
        "teams": team_summaries,
        "rounds": round_summaries,
        "combat": combat,
    }


def print_text(report: dict[str, Any], top: int) -> None:
    print("Journals:")
    for journal in report["journals"]:
        print(f"  {journal}")
    first_t = report["first_t"]
    last_t = report["last_t"]
    span = (last_t - first_t) if first_t is not None and last_t is not None else None
    print(f"Time span: {fmt(span)} s")
    print()

    actors = report["actors"]
    rows = sorted(
        actors.items(),
        key=lambda item: (
            item[1].get("team") if item[1].get("team") is not None else 9,
            -item[1]["life_seconds"],
        ),
    )
    limit = len(rows) if top <= 0 else min(top, len(rows))
    print("Per-NPC movement (life-weighted):")
    header = (
        "actor   team lives  life_s  avg_mps  max_mps  dash/min  down/min  "
        "bhop/min  jump/min  frz/min  air%  wall%  wallavoid%  blocked%  stuck/min"
    )
    print(header)
    for actor, summary in rows[:limit]:
        print(
            f"{actor:>6}  {summary.get('team', '-'):>4}  "
            f"{summary['lives']:>5}  {fmt(summary['life_seconds'], 1):>6}  "
            f"{fmt(summary['average_speed_mps']):>7}  "
            f"{fmt(summary['max_speed_mps']):>7}  "
            f"{fmt(summary['dash_per_minute']):>8}  "
            f"{fmt(summary['down_dash_per_minute']):>8}  "
            f"{fmt(summary['bhop_per_minute']):>8}  "
            f"{fmt(summary['jump_per_minute']):>8}  "
            f"{fmt(summary['freeze_per_minute']):>7}  "
            f"{fmt((summary['air_ratio'] or 0.0) * 100.0, 1):>5}  "
            f"{fmt((summary['wall_contact_ratio'] or 0.0) * 100.0, 1):>5}  "
            f"{fmt((summary['near_wall_avoid_ratio'] or 0.0) * 100.0, 1):>9}  "
            f"{fmt((summary['blocked_motion_ratio'] or 0.0) * 100.0, 1):>7}  "
            f"{fmt(summary['stuck_per_minute']):>9}"
        )
    print()

    print("Team summary:")
    for team, summary in report["teams"].items():
        label = {0: "CT", 1: "T"}.get(int(team), team)
        print(
            f"  {label:>2}: actors={summary['actors']} life_s={fmt(summary['life_seconds'], 1)} "
            f"avg_mps={fmt(summary['average_speed_mps'])} "
            f"dash/min={fmt(summary['dash_per_minute'])} "
            f"down/min={fmt(summary['down_dash_per_minute'])} "
            f"bhop/min={fmt(summary['bhop_per_minute'])} "
            f"air%={fmt((summary['air_ratio'] or 0.0) * 100.0, 1)} "
            f"wall%={fmt((summary['wall_contact_ratio'] or 0.0) * 100.0, 1)} "
            f"wallavoid%={fmt((summary['near_wall_avoid_ratio'] or 0.0) * 100.0, 1)} "
            f"blocked%={fmt((summary['blocked_motion_ratio'] or 0.0) * 100.0, 1)} "
            f"stuck/min={fmt(summary['stuck_per_minute'])}"
        )
    print()

    combat = report["combat"]
    print("NPC-vs-NPC combat:")
    print(
        f"  damage_kills={combat['npc_npc_damage_kills']} "
        f"reward_kills={combat['npc_npc_reward_kills']} "
        f"npc_deaths={combat['npc_deaths']} "
        f"ttk_n={combat['ttk_samples']} "
        f"ttk_mean_s={fmt(combat['ttk_mean_seconds'])} "
        f"ttk_median_s={fmt(combat['ttk_median_seconds'])} "
        f"ttk_p90_s={fmt(combat['ttk_p90_seconds'])} "
        f"kill_interval_median_s={fmt(combat['kill_interval_median_seconds'])} "
        f"kills_per_round_mean={fmt(combat['kills_per_round_mean'])}"
    )
    print()

    print("Round objective timing:")
    for state in report["rounds"]:
        if not any(
            state.get(key) is not None
            for key in ("plant_start_t", "planted_t", "defused_t", "exploded_t")
        ):
            continue
        print(
            f"  round {state['round']:>3}: "
            f"site={state.get('plant_site') or '-':>2} "
            f"plant_latency_s={fmt(state.get('plant_latency_seconds'))} "
            f"plant_dur_s={fmt(state.get('plant_duration_seconds'))} "
            f"defuse_latency_s={fmt(state.get('defuse_latency_seconds'))} "
            f"defuse_dur_s={fmt(state.get('defuse_duration_seconds'))} "
            f"interruptions={state.get('plant_interruptions', 0)} "
            f"kills={state.get('npc_npc_kills', 0)} "
            f"winner={state.get('winner_team')} "
            f"reason={state.get('reason')}"
        )
    print()

    diagnostics = report["diagnostics"]
    if diagnostics:
        print("Diagnostics:")
        for name, count in sorted(
            diagnostics.items(), key=lambda item: -item[1]
        ):
            print(f"  {count:>7}  {name}")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Report NPC movement, combat, objective, and wall metrics from canonical server journals."
    )
    parser.add_argument(
        "paths",
        nargs="+",
        help="server events.jsonl file(s) or a run directory (server-01 segments are used)",
    )
    parser.add_argument("--json", action="store_true", help="emit JSON instead of text")
    parser.add_argument(
        "--out",
        help="write the report to this file instead of stdout",
    )
    parser.add_argument(
        "--top",
        type=int,
        default=40,
        help="maximum actor rows in text output (0 = all)",
    )
    args = parser.parse_args(argv)

    files = expand_journals(args.paths)
    files = [path for path in files if path.is_file()]
    if not files:
        print("No events journal found.", file=sys.stderr)
        return 2

    report = build_report(files)
    if args.json:
        payload = json.dumps(report, indent=2, sort_keys=True)
    else:
        import io

        buffer = io.StringIO()
        with _redirect_stdout(buffer):
            print_text(report, args.top)
        payload = buffer.getvalue()

    if args.out:
        Path(args.out).write_text(payload, encoding="utf-8")
    else:
        sys.stdout.write(payload)
    return 0


class _redirect_stdout:
    def __init__(self, buffer):
        self._buffer = buffer
        self._original = None

    def __enter__(self):
        self._original = sys.stdout
        sys.stdout = self._buffer
        return self

    def __exit__(self, exc_type, exc, tb):
        sys.stdout = self._original
        return False


if __name__ == "__main__":
    raise SystemExit(main())
