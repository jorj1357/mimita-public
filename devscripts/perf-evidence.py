"""Summarize a MiMITA events.jsonl run for performance evidence.

Usage:
    python devscripts/perf-evidence.py
    python devscripts/perf-evidence.py logs/2026-10-27/20261027_220226/events.jsonl

The report separates source/build/runtime claims: it reads the journal only.
It does not prove visual quality or play feel.
"""

from __future__ import annotations

import argparse
import json
from collections import Counter, defaultdict
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]


def newest_events() -> Path | None:
    files = list((ROOT / "logs").rglob("events.jsonl"))
    return max(files, key=lambda p: p.stat().st_mtime) if files else None


def number(value: Any, default: float = 0.0) -> float:
    try:
        return float(value)
    except (TypeError, ValueError):
        return default


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("events", nargs="?", type=Path)
    parser.add_argument("--top", type=int, default=8)
    args = parser.parse_args()

    path = args.events or newest_events()
    if not path or not path.exists():
        print("No events.jsonl found.")
        return 2
    path = path.resolve()

    counts: Counter[str] = Counter()
    process_counts: Counter[str] = Counter()
    baselines: list[dict[str, Any]] = []
    frames: list[dict[str, Any]] = []
    slices: list[dict[str, Any]] = []
    growth: list[dict[str, Any]] = []
    session_growth: list[dict[str, Any]] = []
    audits: list[dict[str, Any]] = []
    spikes: list[dict[str, Any]] = []
    movement_corrections: list[dict[str, Any]] = []
    bounce_events: list[dict[str, Any]] = []
    npc_traces: list[dict[str, Any]] = []
    mace_traces: list[dict[str, Any]] = []
    contacts = 0
    clashes = 0
    state_changes = 0

    with path.open("r", encoding="utf-8", errors="replace") as handle:
        for line in handle:
            try:
                event = json.loads(line)
            except json.JSONDecodeError:
                continue
            name = str(event.get("event", ""))
            counts[name] += 1
            process_counts[str(event.get("process", "?"))] += 1
            fields = event.get("fields") or {}
            if name == "performance.baseline":
                baselines.append(fields)
            elif name == "PERFORMANCE_FRAME":
                frames.append(fields)
            elif name == "performance.frame-slice":
                slices.append(fields)
            elif name == "performance.combat-growth":
                growth.append(fields)
            elif name == "performance.session-growth":
                session_growth.append(fields)
            elif name == "dev-loop.process.audit":
                audits.append(fields)
            elif name in {"PERFORMANCE_SPIKE", "PERFORMANCE_SPIKE_REPORT"}:
                spikes.append(fields)
            elif name == "movement.server-position-corrected":
                movement_corrections.append(fields)
            elif name == "collision.bounce.applied":
                bounce_events.append(fields)
            elif name.startswith("server.npc.lifecycle.trace") or \
                    name == "client.npc.trace" or name == "client.npc.damage-visible":
                npc_traces.append(fields | {"_event": name})
            elif name == "mace.anim.trace":
                mace_traces.append(fields)
            elif name == "melee.contact.fx":
                contacts += 1
            elif name == "weapon.clash":
                clashes += 1
            elif name == "anim.state.changed":
                state_changes += 1

    print(f"journal: {path}")
    print(f"size_bytes: {path.stat().st_size}")
    print("processes:", dict(sorted(process_counts.items())))
    print(
        "events:",
        f"performance_frames={len(frames)}",
        f"frame_slices={len(slices)}",
        f"combat_growth={len(growth)}",
        f"session_growth={len(session_growth)}",
        f"movement_corrections={len(movement_corrections)}",
        f"bounce_events={len(bounce_events)}",
        f"npc_traces={len(npc_traces)}",
        f"mace_traces={len(mace_traces)}",
        f"melee_contacts={contacts}",
        f"weapon_clashes={clashes}",
        f"anim_state_changes={state_changes}",
    )

    if audits:
        print("\nprocess audit:")
        seen_more_than_one = False
        for audit in audits:
            found = int(audit.get("found", 0) or 0)
            killed = audit.get("killed") or []
            cap = audit.get("max_mimita_processes")
            seen_more_than_one = seen_more_than_one or found > int(cap or 1)
            print(f"  max={cap} found={found} killed={len(killed)}")
        if seen_more_than_one:
            print("  WARNING: launch found more mimita.exe processes than the profile cap")
    else:
        print("\nprocess audit: (none recorded)")

    if baselines:
        print("\nbaseline:")
        for baseline in baselines[:3]:
            print(
                "  "
                f"build={baseline.get('build_id', '')} "
                f"pid={baseline.get('process_id', '')} "
                f"max_fps={baseline.get('target_fps_max', '')} "
                f"vsync={baseline.get('vsync', '')} "
                f"npcs={baseline.get('npc_count', '')} "
                f"effects={baseline.get('effects_alive', '')} "
                f"corpses={baseline.get('corpses_active', '')}"
            )
    else:
        print("\nbaseline: (none recorded)")

    if frames:
        tail = frames[-min(10, len(frames)):]
        p99 = [number(f.get("p99_frame_ms")) for f in tail if "p99_frame_ms" in f]
        p95 = [number(f.get("p95_frame_ms")) for f in tail if "p95_frame_ms" in f]
        over = sum(int(f.get("frames_over_budget", 0) or 0) for f in tail)
        max_sim = max((number(f.get("max_simulation_ms")) for f in frames), default=0.0)
        print("\nrecent frame windows:")
        print(
            f"  count={len(tail)} "
            f"p95_avg={(sum(p95) / len(p95)) if p95 else 0.0:.3f}ms "
            f"p99_avg={(sum(p99) / len(p99)) if p99 else 0.0:.3f}ms "
            f"over_budget={over} "
            f"max_simulation={max_sim:.3f}ms"
        )
        logger_delta = sum(int(f.get("logger_events_delta", 0) or 0) for f in tail)
        logger_bytes = sum(int(f.get("logger_bytes_delta", 0) or 0) for f in tail)
        print(f"  logger_last_windows: events={logger_delta} bytes={logger_bytes}")

    if slices:
        print("\nrecent 200ms slices:")
        for item in slices[-min(5, len(slices)):]:
            scopes = item.get("alloc_scopes") or item.get("scopes") or []
            top_alloc = max(scopes, key=lambda scope: number(scope.get("alloc_bytes")),
                            default={})
            print(
                "  "
                f"window={number(item.get('window_ms')):.1f}ms "
                f"frames={item.get('frames', '')} "
                f"p95={number(item.get('frame_p95_ms')):.3f}ms "
                f"p99={number(item.get('frame_p99_ms')):.3f}ms "
                f"max={number(item.get('frame_max_ms')):.3f}ms "
                f"private={item.get('private_bytes', '')} "
                f"anims={item.get('animation_set_cache', '')}/"
                f"{item.get('animation_clip_cache', '')} "
                f"avatars={item.get('avatar_cache_entries', '')} "
                f"pending={item.get('avatar_pending_loads', '')} "
                f"top_alloc={top_alloc.get('name', '')}:"
                f"{top_alloc.get('alloc_bytes', '')}"
            )

    if session_growth:
        print("\nsession growth:")
        for item in session_growth[-min(5, len(session_growth)):]:
            print(
                "  "
                f"elapsed={number(item.get('session_elapsed_s')):.1f}s "
                f"private_delta={item.get('private_delta_bytes', '')} "
                f"working_delta={item.get('working_set_delta_bytes', '')} "
                f"allocation_delta={item.get('allocation_delta', '')} "
                f"npcs={item.get('npcs', '')} "
                f"effects={item.get('effects', '')} "
                f"corpses={item.get('corpses', '')}"
            )

    if movement_corrections:
        print("\nmovement corrections:")
        for item in movement_corrections[-min(5, len(movement_corrections)):]:
            print(
                "  "
                f"reason={item.get('reason', '')} "
                f"class={item.get('correction_class', '')} "
                f"error={number(item.get('position_error')):.3f} "
                f"reported={item.get('reported_position', '')} "
                f"accepted={item.get('accepted_position', '')} "
                f"input={item.get('move_axes', '')}"
            )

    if bounce_events:
        print("\nbounce responses:")
        for item in bounce_events[-min(5, len(bounce_events)):]:
            print(
                "  "
                f"part={item.get('actor_part', '')} "
                f"dynamic={item.get('dynamic_entity', '')} "
                f"impact={number(item.get('impact')):.3f} "
                f"strength={number(item.get('strength')):.3f} "
                f"delta={item.get('velocity_delta', '')}"
            )

    if npc_traces:
        print("\nNPC trace:")
        for item in npc_traces[-min(8, len(npc_traces)):]:
            print(
                "  "
                f"event={item.get('_event', '')} "
                f"id={item.get('actor_id', '')} "
                f"present={item.get('present_in_remote_npcs', item.get('snapshot_included', ''))} "
                f"alive={item.get('alive', '')} "
                f"model={item.get('model_loaded', '')} "
                f"dead={item.get('dead', '')}"
            )

    if mace_traces:
        print("\nMace animation trace:")
        for item in mace_traces[-min(8, len(mace_traces)):]:
            print(
                "  "
                f"selected={item.get('selected_clip', '')} "
                f"locked={item.get('action_locked_clip', '')} "
                f"pose={item.get('selected_weapon_pose', '')} "
                f"moving={item.get('moving', '')} "
                f"weapon_profile={item.get('active_clip_in_weapon_profile', '')}"
            )

    if growth:
        print("\ncombat growth (last 5s windows):")
        for item in growth[-min(5, len(growth)):]:
            print(
                "  "
                f"npcs={item.get('npcs_active', '')} "
                f"effects={item.get('effects_alive', '')} "
                f"corpses={item.get('corpses_active', '')} "
                f"blood={item.get('blood_particles', '')} "
                f"p99={number(item.get('p99_frame_ms')):.3f}ms "
                f"log_events+={item.get('logger_events_delta', '')} "
                f"log_bytes+={item.get('logger_bytes_delta', '')}"
            )

    if spikes:
        print("\nspikes:")
        by_name: Counter[str] = Counter()
        for spike in spikes:
            for contributor in spike.get("contributors") or []:
                by_name[str(contributor.get("name", "?"))] += 1
        for name, count in by_name.most_common(max(1, args.top)):
            print(f"  {name}: {count}")

    print("\nnote: journal evidence proves runtime events, not visual or gameplay acceptance.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
