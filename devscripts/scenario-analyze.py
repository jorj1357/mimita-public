#!/usr/bin/env python3
"""Offline collision/stall analysis for MiMITA scenario recordings.

This reads a schema 1/2/3 scenario JSONL without needing a server. It never
mutates the recording and never claims gameplay acceptance. Its job is to tell
an AI where to look in events.jsonl or which recorded ticks need server-backed
replay.
"""

from __future__ import annotations

import argparse
import json
import math
from pathlib import Path
from typing import Any


def _horizontal(value: Any) -> float:
    if not isinstance(value, (list, tuple)) or len(value) < 2:
        return 0.0
    return math.hypot(float(value[0]), float(value[1]))


def _hdist(a: Any, b: Any) -> float:
    if not isinstance(a, (list, tuple)) or not isinstance(b, (list, tuple)):
        return 0.0
    return math.hypot(float(a[0]) - float(b[0]), float(a[1]) - float(b[1]))


def _input_frozen(tick: dict) -> bool:
    return bool(tick.get("freeze_held") or tick.get("freeze_pressed"))


def _is_stationary(tick: dict, prev: dict, *, grounded: bool) -> bool:
    wish = tick.get("wish") or [0.0, 0.0]
    move = tick.get("move_world") or [0.0, 0.0]
    wants_move = _horizontal(wish) > 0.05 or _horizontal(move) > 0.05
    speed = _horizontal(tick.get("velocity"))
    progress = _hdist(tick.get("position"), prev.get("position"))
    return (wants_move and not _input_frozen(tick) and
            bool(tick.get("grounded", True)) is grounded and
            speed < 0.5 and progress < 0.002)


def _wants_move(tick: dict) -> bool:
    return (_horizontal(tick.get("wish")) > 0.05 or
            _horizontal(tick.get("move_world")) > 0.05)


def _intended_direction(tick: dict) -> tuple[float, float]:
    for key in ("move_world", "wish"):
        value = tick.get(key)
        if isinstance(value, (list, tuple)) and len(value) >= 2:
            length = math.hypot(float(value[0]), float(value[1]))
            if length > 0.05:
                return (float(value[0]) / length, float(value[1]) / length)
    return (0.0, 0.0)


def _direction_error_deg(tick: dict) -> float:
    intended = _intended_direction(tick)
    velocity = tick.get("velocity")
    if intended == (0.0, 0.0) or not isinstance(velocity, (list, tuple)) or \
            len(velocity) < 2:
        return 0.0
    speed = _horizontal(velocity)
    if speed <= 0.001:
        return 180.0
    dot = (intended[0] * float(velocity[0]) +
           intended[1] * float(velocity[1])) / speed
    return math.degrees(math.acos(max(-1.0, min(1.0, dot))))


def _windows(ticks: list[dict], predicate, minimum: int = 5) -> list[tuple[int, int]]:
    found: list[tuple[int, int]] = []
    start: int | None = None
    for i in range(1, len(ticks)):
        matched = predicate(ticks[i], ticks[i - 1])
        if matched and start is None:
            start = i
        if not matched and start is not None:
            if i - start >= minimum:
                found.append((start, i - 1))
            start = None
    if start is not None and len(ticks) - start >= minimum:
        found.append((start, len(ticks) - 1))
    return found


def analyze(path: Path) -> dict:
    header: dict | None = None
    footer: dict | None = None
    ticks: list[dict] = []
    kinds: dict[str, int] = {}
    with path.open(encoding="utf-8") as fh:
        for line_number, line in enumerate(fh, 1):
            line = line.strip()
            if not line:
                continue
            try:
                row = json.loads(line)
            except json.JSONDecodeError as error:
                return {
                    "path": str(path),
                    "valid": False,
                    "error": f"line {line_number}: {error}",
                }
            kind = str(row.get("kind", ""))
            kinds[kind] = kinds.get(kind, 0) + 1
            if kind == "header":
                header = row
            elif kind == "footer":
                footer = row
            elif kind == "tick":
                ticks.append(row)

    stalls = [
        _stall_record(ticks, start, end)
        for start, end in _windows(
            ticks, lambda tick, prev: _is_stationary(tick, prev, grounded=True))
    ]
    airborne_blocks = [
        _stall_record(ticks, start, end)
        for start, end in _windows(
            ticks, lambda tick, prev: _is_stationary(tick, prev, grounded=False))
    ]

    freezes = []
    for i in range(1, len(ticks)):
        tick = ticks[i]
        prev = ticks[i - 1]
        if tick.get("position") == prev.get("position") and \
                _horizontal(tick.get("velocity")) > 5.0:
            freezes.append({
                "index": i,
                "frame": tick.get("frame"),
                "client_tick": tick.get("client_tick"),
                "position": tick.get("position"),
                "velocity": tick.get("velocity"),
                "wish": tick.get("wish"),
                "move_world": tick.get("move_world"),
                "grounded": tick.get("grounded"),
                "collision": tick.get("collision"),
            })

    speed_collapses = []
    for i in range(1, len(ticks)):
        tick = ticks[i]
        prev = ticks[i - 1]
        if _input_frozen(tick) or not _wants_move(tick):
            continue
        before = _horizontal(prev.get("velocity"))
        after = _horizontal(tick.get("velocity"))
        if before > 1.0 and after < before * 0.1:
            speed_collapses.append({
                "index": i,
                "frame": tick.get("frame"),
                "client_tick": tick.get("client_tick"),
                "horizontal_speed_before": before,
                "horizontal_speed_after": after,
                "position": tick.get("position"),
                "velocity": tick.get("velocity"),
                "wish": tick.get("wish"),
                "move_world": tick.get("move_world"),
                "grounded": tick.get("grounded"),
                "collision": tick.get("collision"),
            })

    direction_deviations = []
    for start, end in _windows(
            ticks,
            lambda tick, prev: _wants_move(tick) and
            not _input_frozen(tick) and
            _horizontal(tick.get("velocity")) > 1.0 and
            _direction_error_deg(tick) > 70.0,
            minimum=3):
        direction_deviations.append({
            "start_frame": ticks[start].get("frame"),
            "end_frame": ticks[end].get("frame"),
            "ticks": end - start + 1,
            "position": ticks[start].get("position"),
            "wish": ticks[start].get("wish"),
            "move_world": ticks[start].get("move_world"),
        })

    tracks = header.get("tracks", []) if header else []
    return {
        "path": str(path),
        "valid": header is not None and footer is not None,
        "schema": header.get("schema") if header else None,
        "map": header.get("map") if header else None,
        "mode": header.get("mode") if header else None,
        "networked": header.get("networked") if header else None,
        "track_count": len(ticks),
        "kinds": kinds,
        "tracks": tracks,
        "has_collision_track": "client_collision" in tracks,
        "footer": footer,
        "stall_windows": stalls,
        "airborne_blocked_windows": airborne_blocks,
        "freeze_frames": freezes,
        "speed_collapses": speed_collapses,
        "direction_deviation_windows": direction_deviations,
    }


def _stall_record(ticks: list[dict], start: int, end: int) -> dict:
    first = ticks[start]
    last = ticks[end]
    return {
        "start_index": start,
        "end_index": end,
        "ticks": end - start + 1,
        "start_frame": first.get("frame"),
        "end_frame": last.get("frame"),
        "start_client_tick": first.get("client_tick"),
        "end_client_tick": last.get("client_tick"),
        "start_position": first.get("position"),
        "end_position": last.get("position"),
        "wish": first.get("wish"),
        "move_world": first.get("move_world"),
        "grounded": first.get("grounded"),
        "collision": first.get("collision"),
    }


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Analyze a MiMITA scenario recording offline for collision stalls")
    parser.add_argument("path", help="scenario JSONL path")
    parser.add_argument("--json", action="store_true", help="print full JSON result")
    parser.add_argument(
        "--fail-on-stall",
        action="store_true",
        help="return nonzero when any stall window is found",
    )
    parser.add_argument(
        "--fail-on-speed-collapse",
        action="store_true",
        help="return nonzero when a >90%% horizontal speed drop is found",
    )
    args = parser.parse_args()
    path = Path(args.path)
    if not path.is_file():
        print(f"[SCENARIO ANALYZE] missing file: {path}")
        return 2

    result = analyze(path)
    if args.json:
        print(json.dumps(result, indent=2))
    else:
        print(f"[SCENARIO ANALYZE] {path}")
        print(f"  valid={result.get('valid')} schema={result.get('schema')} "
              f"map={result.get('map')} mode={result.get('mode')} "
              f"ticks={result.get('track_count')}")
        print(f"  tracks={result.get('tracks')} "
              f"collision_track={result.get('has_collision_track')}")
        for window in result.get("stall_windows", []):
            print(f"  STALL ticks={window['ticks']} "
                  f"frames={window['start_frame']}..{window['end_frame']} "
                  f"pos={window['start_position']} -> {window['end_position']} "
                  f"wish={window['wish']} grounded={window['grounded']}")
            if window.get("collision"):
                print(f"        collision={json.dumps(window['collision'])}")
        for window in result.get("airborne_blocked_windows", []):
            print(f"  AIRBORNE BLOCK ticks={window['ticks']} "
                  f"frames={window['start_frame']}..{window['end_frame']} "
                  f"pos={window['start_position']} -> {window['end_position']} "
                  f"wish={window['wish']}")
        for collapse in result.get("speed_collapses", [])[:10]:
            print(f"  SPEED COLLAPSE frame={collapse['frame']} "
                  f"speed={collapse['horizontal_speed_before']:.2f} -> "
                  f"{collapse['horizontal_speed_after']:.2f} "
                  f"pos={collapse['position']} grounded={collapse['grounded']}")
        for deviation in result.get("direction_deviation_windows", [])[:10]:
            print(f"  DIRECTION DEVIATION frames={deviation['start_frame']}.."
                  f"{deviation['end_frame']} ticks={deviation['ticks']} "
                  f"wish={deviation['wish']}")
        for freeze in result.get("freeze_frames", [])[:10]:
            print(f"  FREEZE frame={freeze['frame']} "
                  f"pos={freeze['position']} vel={freeze['velocity']} "
                  f"grounded={freeze['grounded']}")
        if not result.get("stall_windows") and not result.get("freeze_frames") \
                and not result.get("speed_collapses"):
            print("  no stall, freeze, or speed-collapse windows detected")

    if not result.get("valid"):
        return 1
    if args.fail_on_stall and result.get("stall_windows"):
        return 1
    if args.fail_on_speed_collapse and result.get("speed_collapses"):
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
