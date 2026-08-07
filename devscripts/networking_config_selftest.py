#!/usr/bin/env python3
# 08 03 2026, 20 32
# purpose
# Validates networkingconfig.json ownership for remote timeline and retry policy.
# Checks source invariants that keep remote presentation on the render snapshot.
# Reports focused failures for agent and CI troubleshooting.
# Does NOT launch mimita.exe or require a graphics window.
# Does NOT prove live packet delivery, ICE negotiation, or manual visual quality.
# Does NOT mutate config files or networking presets.

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CONFIG = ROOT / "config" / "networkingconfig.json"


def fail(message):
    print(f"FAIL: {message}")
    return 1


def require_keys(obj, path, keys):
    missing = [key for key in keys if key not in obj]
    if missing:
        return fail(f"{path} missing keys: {', '.join(missing)}")
    return 0


def main():
    try:
        data = json.loads(CONFIG.read_text(encoding="utf-8"))
    except Exception as exc:
        return fail(f"could not parse {CONFIG}: {exc}")

    checks = [
        ("root", data, [
            "remote_player_interpolation",
            "adaptive_snapshot_buffer",
            "event_timeline",
            "runtime_rates",
            "retries",
            "reliable_gameplay_events",
            "buffer_limits",
            "badconn",
        ]),
        ("remote_player_interpolation", data.get("remote_player_interpolation", {}), [
            "enabled",
            "direct_render",
            "interpolation_delay_ms",
            "minimum_snapshots_before_rendering",
        ]),
        ("adaptive_snapshot_buffer", data.get("adaptive_snapshot_buffer", {}), [
            "enabled",
            "minimum_delay_ms",
            "maximum_delay_ms",
            "jitter_multiplier",
            "arrival_jitter_smoothing",
        ]),
        ("event_timeline", data.get("event_timeline", {}), [
            "enabled",
            "remote_effect_maximum_hold_ms",
        ]),
        ("retries", data.get("retries", {}), [
            "attack_retry_interval_ms",
            "attack_retry_max_attempts",
            "attack_request_timeout_ms",
            "reconnect_initial_backoff_ms",
            "reconnect_max_attempts",
            "reconnect_max_backoff_ms",
        ]),
        ("reliable_gameplay_events", data.get("reliable_gameplay_events", {}), [
            "max_pending_per_player",
            "retry_ms",
            "ttl_ms",
            "max_attempts",
        ]),
        ("buffer_limits", data.get("buffer_limits", {}), [
            "server_position_history_ticks",
            "server_broadcast_sample_limit",
        ]),
        ("badconn", data.get("badconn", {}), ["presets"]),
    ]
    for path, obj, keys in checks:
        if require_keys(obj, path, keys):
            return 1

    remote = data["remote_player_interpolation"]
    adaptive = data["adaptive_snapshot_buffer"]
    event_timeline = data["event_timeline"]
    if remote["direct_render"]:
        return fail("remote_player_interpolation.direct_render must be false for adaptive timeline tests")
    if int(remote["minimum_snapshots_before_rendering"]) < 2:
        return fail("minimum_snapshots_before_rendering must be at least 2")
    if not adaptive["enabled"]:
        return fail("adaptive_snapshot_buffer.enabled must be true")
    if adaptive["maximum_delay_ms"] < adaptive["minimum_delay_ms"]:
        return fail("adaptive maximum_delay_ms is smaller than minimum_delay_ms")
    if not event_timeline["enabled"]:
        return fail("event_timeline.enabled must be true")
    motion = data.get("remote_motion_smoothing", {})
    linear_delay = int(motion.get("linear_delay_ticks", 0))
    linear_max = int(motion.get("linear_max_delay_ticks", 0))
    if linear_delay <= 0:
        return fail("remote_motion_smoothing.linear_delay_ticks must be positive")
    if linear_max > 0 and linear_max < linear_delay:
        return fail("linear_max_delay_ticks silently clamps linear_delay_ticks")
    if motion.get("render_filter") == "linear" and \
            motion.get("linear_clock_source") != "wall_time":
        return fail("linear mode must use wall_time clock source")
    if "1" not in data["badconn"]["presets"]:
        return fail("badconn preset 1 is missing")
    if (ROOT / "config" / "badconnconfig.json").exists():
        return fail("config/badconnconfig.json exists; badconn must live in networkingconfig.json")

    interpolation_src = (ROOT / "src" / "network" / "multiplayer-interpolation.cpp").read_text(
        encoding="utf-8", errors="replace")
    forbidden = [
        "interpolation.target.dashSerial",
        "interpolation.target.groundJumpSerial",
        "interpolation.target.airJumpSerial",
        "interpolation.target.downDashSerial",
        "interpolation.target.freezeSerial",
        "interpolation.target.directionChangeSerial",
        "player.spawnGeneration = interpolation.target.spawnGeneration",
    ]
    for needle in forbidden:
        if needle in interpolation_src:
            return fail(f"remote presentation still uses newest snapshot: {needle}")

    shots_src = (ROOT / "src" / "network" / "multiplayer-shots.cpp").read_text(
        encoding="utf-8", errors="replace")
    required_shot_terms = [
        "pendingShotEvents",
        "pendingPelletBlastEvents",
        "visualTimelineReady",
        "mpReleaseTimelineEvents",
    ]
    for needle in required_shot_terms:
        if needle not in shots_src:
            return fail(f"timeline event queue missing source marker: {needle}")

    print("PASS: networking config/timeline invariants")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
