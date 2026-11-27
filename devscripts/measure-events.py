#!/usr/bin/env python3
"""Automatic measurement extraction from MiMITA run journals.

Why this exists
---------------
Agents were hand-writing PowerShell/`ConvertFrom-Json` one-liners to pull
`authored_ms`/`npc_ms` out of `events.jsonl` for a before/after claim. That
worked, but it was re-invented per task and easy to leave unpinned. This tool
pins the identity (run id, generation, mode, map, journal paths) and prints the
same summary every time: count, min, mean, p50, p95, max, sum, plus optional
A->B latency and a two-run comparison.

It reads journals only. It does not prove compilation, visuals, or gameplay
acceptance; it makes the runtime numbers easy to cite and compare.

Examples
--------
    python devscripts/measure-events.py 20261127_021425 \
        --preset authored-block-parity

    python devscripts/measure-events.py 20261127_001718 \
        --event map.geometry.block-delta.applied \
        --field authored_ms --field npc_ms --field duration_ms

    python devscripts/measure-events.py 20261127_001718 \
        --compare 20261127_021425 --preset authored-block-parity
"""

from __future__ import annotations

import argparse
import fnmatch
import json
import math
import re
import sys
import tempfile
from dataclasses import dataclass, field
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Iterable, Iterator

ROOT = Path(__file__).resolve().parents[1]
LOGS = ROOT / "logs"

PRESETS: dict[str, dict[str, Any]] = {
    "authored-block-parity": {
        "events": ["map.geometry.block-delta.applied", "map-authored-geometry.applied"],
        "fields": [
            "authored_ms",
            "npc_ms",
            "duration_ms",
            "triangles",
            "npc_triangles",
            "collision_triangles",
        ],
        "latency_a": "map-editor.object.deleted",
        "latency_b": "map.geometry.block-delta.applied",
        "latency_window_ms": 60_000,
    },
    "server-tick": {
        "events": ["performance.server-tick-window", "performance.server-tick-stall"],
        "fields": [
            "total_avg_ms",
            "total_p95_ms",
            "total_max_ms",
            "npc_max_ms",
            "snapshot_max_ms",
        ],
    },
    "frame": {
        "events": ["performance.frame-slice", "PERFORMANCE_FRAME"],
        "fields": ["frame_p95_ms", "frame_p99_ms", "frame_max_ms", "p95_frame_ms", "p99_frame_ms"],
    },
}

SQLITE_EPOCH = datetime(1970, 1, 1, tzinfo=timezone.utc)


@dataclass
class RunIdentity:
    run_id: str
    run_dir: Path
    journals: list[Path] = field(default_factory=list)
    generation: Any = ""
    game_dll: str = ""
    build_id: str = ""
    mode: str = ""
    map: str = ""
    profile: str = ""


@dataclass
class EventRow:
    instance: str
    name: str
    wall_ms: float
    correlation: str
    fields: dict[str, Any]


def parse_wall_time(value: Any) -> float:
    """Return epoch milliseconds for an ISO-8601 wall_time, or NaN."""
    if not value:
        return math.nan
    text = str(value).strip()
    if text.endswith("Z"):
        text = text[:-1] + "+00:00"
    try:
        parsed = datetime.fromisoformat(text)
    except ValueError:
        return math.nan
    if parsed.tzinfo is None:
        parsed = parsed.replace(tzinfo=timezone.utc)
    return (parsed - SQLITE_EPOCH).total_seconds() * 1000.0


def percentile(sorted_values: list[float], fraction: float) -> float:
    if not sorted_values:
        return 0.0
    if len(sorted_values) == 1:
        return sorted_values[0]
    position = max(0.0, min(1.0, fraction)) * (len(sorted_values) - 1)
    lower = int(math.floor(position))
    upper = int(math.ceil(position))
    if lower == upper:
        return sorted_values[lower]
    weight = position - lower
    return sorted_values[lower] * (1.0 - weight) + sorted_values[upper] * weight


def summarize(values: list[float]) -> dict[str, Any]:
    if not values:
        return {
            "n": 0,
            "min": 0.0,
            "mean": 0.0,
            "p50": 0.0,
            "p95": 0.0,
            "max": 0.0,
            "sum": 0.0,
        }
    ordered = sorted(values)
    return {
        "n": len(ordered),
        "min": ordered[0],
        "mean": sum(ordered) / len(ordered),
        "p50": percentile(ordered, 0.50),
        "p95": percentile(ordered, 0.95),
        "max": ordered[-1],
        "sum": sum(ordered),
    }


def unit_for(field_name: str) -> str:
    if field_name.endswith("_ms"):
        return "ms"
    if field_name.endswith("_s"):
        return "s"
    if field_name.endswith("_bytes"):
        return "bytes"
    if field_name.endswith("_pct") or field_name.endswith("_percent"):
        return "%"
    return ""


def resolve_run(argument: str | Path | None) -> RunIdentity | None:
    """Resolve a run id, run directory, or events.jsonl path."""
    if argument is None:
        candidates = [path for path in LOGS.rglob("events.jsonl") if path.is_file()]
        if not candidates:
            return None
        chosen = max(candidates, key=lambda path: path.stat().st_mtime)
        return build_identity(chosen)

    raw = str(argument)
    candidate = Path(raw)
    if not candidate.is_absolute():
        candidate = (Path.cwd() / candidate).resolve()
    if candidate.exists():
        if candidate.is_file():
            return build_identity(candidate)
        return build_identity(candidate)

    matches: list[Path] = []
    for path in LOGS.rglob(raw):
        if not path.is_dir():
            continue
        if (path / "server-01").is_dir() or (path / "client-01").is_dir() or \
                any(path.glob("*/events.jsonl")):
            matches.append(path)
    if not matches:
        return None
    chosen = max(matches, key=lambda path: path.stat().st_mtime)
    return build_identity(chosen)


def build_identity(path: Path) -> RunIdentity | None:
    if path.is_file():
        journal = path
        run_dir = journal.parent.parent if journal.parent.name.startswith("client") or \
            journal.parent.name.startswith("server") else journal.parent
    else:
        run_dir = path
        journal = None

    journals: list[Path] = []
    if journal is not None:
        journals.append(journal)
    else:
        journals.extend(sorted(run_dir.glob("*/events.jsonl")))

    if not journals:
        return None

    identity = RunIdentity(run_id=run_dir.name, run_dir=run_dir, journals=journals)
    run_json = run_dir / "run.json"
    if run_json.exists():
        try:
            data = json.loads(run_json.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError):
            data = {}
        identity.run_id = str(data.get("run_id", "") or identity.run_id)
        build = data.get("build_identity", {}) or {}
        identity.generation = build.get("game_generation", "") or data.get(
            "game_generation", ""
        )
        if not identity.generation:
            match = re.search(r"generation-(\d+)", str(data.get("game_dll", "")))
            if match:
                identity.generation = match.group(1)
        dll = str(build.get("game_dll_sha256", "") or "")
        identity.game_dll = dll[:12]
        identity.build_id = str(data.get("build_id", "") or "")
        identity.mode = str(
            build.get("mode", "")
            or data.get("mode", "")
            or data.get("launch_mode_name", "")
            or data.get("launch_mode", "")
        )
        identity.map = str(data.get("map", "") or "")
        identity.profile = str(data.get("profile", "") or "")
    return identity


def instance_of(path: Path) -> str:
    return path.parent.name if path.parent.name else "?"


def iter_events(identity: RunIdentity, instance_filter: str) -> Iterator[EventRow]:
    for journal in identity.journals:
        instance = instance_of(journal)
        if instance_filter == "server" and not instance.startswith("server"):
            continue
        if instance_filter == "client" and not instance.startswith("client"):
            continue
        if instance == "dev-loop":
            continue
        try:
            handle = journal.open("r", encoding="utf-8", errors="replace")
        except OSError:
            continue
        with handle:
            for line in handle:
                line = line.strip()
                if not line:
                    continue
                try:
                    event = json.loads(line)
                except json.JSONDecodeError:
                    continue
                name = str(event.get("event", "") or "")
                if not name:
                    continue
                yield EventRow(
                    instance=instance,
                    name=name,
                    wall_ms=parse_wall_time(event.get("wall_time")),
                    correlation=str(event.get("correlation_id", "") or ""),
                    fields=event.get("fields") or {},
                )


def name_matches(name: str, patterns: list[str]) -> bool:
    for pattern in patterns:
        if pattern == name:
            return True
        if "*" in pattern and fnmatch.fnmatchcase(name, pattern):
            return True
    return False


def numeric(value: Any) -> float | None:
    if isinstance(value, bool):
        return None
    if isinstance(value, (int, float)):
        number = float(value)
        return number if math.isfinite(number) else None
    if isinstance(value, str):
        try:
            number = float(value)
        except ValueError:
            return None
        return number if math.isfinite(number) else None
    return None


def collect_fields(
    identity: RunIdentity,
    events: list[str],
    fields: list[str],
    instance_filter: str,
    since_ms: float | None,
    until_ms: float | None,
) -> tuple[dict[tuple[str, str], list[float]], dict[str, int], dict[str, list[float]]]:
    """Return (event,field)->values, event counts, and latency rows."""
    values: dict[tuple[str, str], list[float]] = {}
    counts: dict[str, int] = {}
    auto_fields: dict[str, set[str]] = {}

    for row in iter_events(identity, instance_filter):
        if not name_matches(row.name, events):
            continue
        if since_ms is not None and (math.isnan(row.wall_ms) or row.wall_ms < since_ms):
            continue
        if until_ms is not None and (math.isnan(row.wall_ms) or row.wall_ms > until_ms):
            continue
        counts[row.name] = counts.get(row.name, 0) + 1
        if fields:
            selected = fields
        else:
            detected = auto_fields.setdefault(row.name, set())
            for key, raw in row.fields.items():
                if numeric(raw) is not None:
                    detected.add(str(key))
            selected = sorted(detected)
        for key in selected:
            number = numeric(row.fields.get(key))
            if number is None:
                continue
            values.setdefault((row.name, key), []).append(number)
    return values, counts, {}


def latency(
    identity: RunIdentity,
    event_a: str,
    event_b: str,
    instance_filter: str,
    window_ms: float,
) -> dict[str, Any]:
    """Measure A->B latency using same-host wall_time."""
    a_events: list[tuple[float, str]] = []
    b_events: list[tuple[float, str]] = []
    for row in iter_events(identity, instance_filter):
        if math.isnan(row.wall_ms):
            continue
        if name_matches(row.name, [event_a]):
            a_events.append((row.wall_ms, row.correlation))
        elif name_matches(row.name, [event_b]):
            b_events.append((row.wall_ms, row.correlation))
    a_events.sort(key=lambda item: item[0])
    b_events.sort(key=lambda item: item[0])

    by_correlation: dict[str, list[float]] = {}
    for wall, correlation in a_events:
        if correlation:
            by_correlation.setdefault(correlation, []).append(wall)

    deltas: list[float] = []
    matched_correlations = 0
    unmatched = 0
    for wall_b, correlation_b in b_events:
        chosen: float | None = None
        if correlation_b and correlation_b in by_correlation:
            candidates = [wall for wall in by_correlation[correlation_b] if wall <= wall_b]
            if candidates:
                chosen = max(candidates)
                matched_correlations += 1
        if chosen is None:
            candidates = [wall for wall, _ in a_events if wall <= wall_b]
            if candidates:
                chosen = max(candidates)
        if chosen is None:
            unmatched += 1
            continue
        delta = wall_b - chosen
        if delta < 0 or delta > window_ms:
            unmatched += 1
            continue
        deltas.append(delta)
    return {
        "a": event_a,
        "b": event_b,
        "window_ms": window_ms,
        "summary": summarize(deltas),
        "matched_by_correlation": matched_correlations,
        "unmatched_b": unmatched,
    }


def identity_lines(identity: RunIdentity) -> list[str]:
    lines = [
        f"- run: `{identity.run_id}`",
        f"- game generation: `{identity.generation or '?'}`"
        + (f" (dll `{identity.game_dll}`)" if identity.game_dll else ""),
        f"- mode/map/profile: `{identity.mode or '?'}` / `{identity.map or '?'}` / "
        f"`{identity.profile or '?'}`",
        f"- run dir: `{identity.run_dir}`",
        "- journals:",
    ]
    for journal in identity.journals:
        lines.append(f"  - `{journal}`")
    return lines


def print_metrics(
    identity: RunIdentity,
    values: dict[tuple[str, str], list[float]],
    counts: dict[str, int],
    latency_result: dict[str, Any] | None,
) -> None:
    print("## measurement identity")
    print()
    print("\n".join(identity_lines(identity)))
    print()
    print("## event counts")
    print()
    print("| event | rows |")
    print("|---|---:|")
    for name, count in sorted(counts.items(), key=lambda item: (-item[1], item[0])):
        print(f"| `{name}` | {count} |")
    print()

    if values:
        print("## numeric fields")
        print()
        print("| event | field | unit | n | min | mean | p50 | p95 | max | sum |")
        print("|---|---|---|---:|---:|---:|---:|---:|---:|---:|")
        for (name, key), numbers in sorted(values.items()):
            stats = summarize(numbers)
            unit = unit_for(key)
            print(
                f"| `{name}` | `{key}` | {unit or '-'} | {stats['n']} | "
                f"{stats['min']:.3f} | {stats['mean']:.3f} | {stats['p50']:.3f} | "
                f"{stats['p95']:.3f} | {stats['max']:.3f} | {stats['sum']:.3f} |"
            )
        print()

    if latency_result:
        stats = latency_result["summary"]
        print("## latency")
        print()
        print(
            f"`{latency_result['a']}` -> `{latency_result['b']}` "
            f"(window {latency_result['window_ms']:.0f} ms, same-host wall clock)"
        )
        print()
        print("| n | min | mean | p50 | p95 | max | matched by correlation | unmatched B |")
        print("|---:|---:|---:|---:|---:|---:|---:|---:|")
        print(
            f"| {stats['n']} | {stats['min']:.3f} | {stats['mean']:.3f} | "
            f"{stats['p50']:.3f} | {stats['p95']:.3f} | {stats['max']:.3f} | "
            f"{latency_result['matched_by_correlation']} | {latency_result['unmatched_b']} |"
        )
        print()
        print(
            "Note: cross-process latency uses `wall_time`; it is valid on one host and "
            "must not be compared across machines with unsynchronized clocks."
        )
        print()


def print_comparison(
    before: RunIdentity,
    after: RunIdentity,
    before_values: dict[tuple[str, str], list[float]],
    after_values: dict[tuple[str, str], list[float]],
    before_latency: dict[str, Any] | None,
    after_latency: dict[str, Any] | None,
) -> None:
    print("## comparison")
    print()
    print("### before")
    print()
    print("\n".join(identity_lines(before)))
    print()
    print("### after")
    print()
    print("\n".join(identity_lines(after)))
    print()

    keys = sorted(set(before_values) | set(after_values))
    if keys:
        print("| event | field | unit | before n | before mean | after n | after mean | delta | delta % |")
        print("|---|---|---|---:|---:|---:|---:|---:|---:|")
        for name, key in keys:
            before_stats = summarize(before_values.get((name, key), []))
            after_stats = summarize(after_values.get((name, key), []))
            delta = after_stats["mean"] - before_stats["mean"]
            if abs(before_stats["mean"]) > 1e-9:
                percent = delta / before_stats["mean"] * 100.0
                percent_text = f"{percent:+.1f}%"
            else:
                percent_text = "-"
            print(
                f"| `{name}` | `{key}` | {unit_for(key) or '-'} | {before_stats['n']} | "
                f"{before_stats['mean']:.3f} | {after_stats['n']} | "
                f"{after_stats['mean']:.3f} | {delta:+.3f} | {percent_text} |"
            )
        print()

    if before_latency and after_latency:
        print("| latency | before n | before mean | after n | after mean | delta |")
        print("|---|---:|---:|---:|---:|---:|")
        b = before_latency["summary"]
        a = after_latency["summary"]
        print(
            f"| `{before_latency['a']}` -> `{before_latency['b']}` | {b['n']} | "
            f"{b['mean']:.3f} ms | {a['n']} | {a['mean']:.3f} ms | "
            f"{a['mean'] - b['mean']:+.3f} ms |"
        )
        print()


def parse_bound(text: str | None) -> float | None:
    if not text:
        return None
    value = parse_wall_time(text)
    if not math.isnan(value):
        return value
    # Accept HH:MM:SS against today's UTC date.
    try:
        today = datetime.now(timezone.utc).strftime("%Y-%m-%dT")
        return parse_wall_time(today + text + "Z")
    except Exception:
        return None


def run_self_test() -> int:
    """Synthetic journal round-trip; no repository journals touched."""
    with tempfile.TemporaryDirectory() as temp:
        run_dir = Path(temp) / "20260101_000000"
        (run_dir / "server-01").mkdir(parents=True)
        (run_dir / "client-01").mkdir(parents=True)
        (run_dir / "run.json").write_text(
            json.dumps(
                {
                    "run_id": "20260101_000000",
                    "mode": "selftest",
                    "map": "selftest",
                    "build_identity": {"game_generation": 1, "game_dll_sha256": "a" * 64},
                }
            ),
            encoding="utf-8",
        )
        server = run_dir / "server-01" / "events.jsonl"
        client = run_dir / "client-01" / "events.jsonl"
        server.write_text(
            "\n".join(
                [
                    json.dumps(
                        {
                            "event": "map.geometry.block-delta.applied",
                            "wall_time": "2026-01-01T00:00:01.500Z",
                            "correlation_id": "map",
                            "fields": {"authored_ms": 10.0, "npc_ms": 20.0, "duration_ms": 30.0},
                        }
                    ),
                    json.dumps(
                        {
                            "event": "map.geometry.block-delta.applied",
                            "wall_time": "2026-01-01T00:00:02.500Z",
                            "correlation_id": "map",
                            "fields": {"authored_ms": 30.0, "npc_ms": 40.0, "duration_ms": 70.0},
                        }
                    ),
                ]
            ),
            encoding="utf-8",
        )
        client.write_text(
            json.dumps(
                {
                    "event": "map-editor.object.deleted",
                    "wall_time": "2026-01-01T00:00:01.000Z",
                    "correlation_id": "block",
                    "fields": {},
                }
            ),
            encoding="utf-8",
        )
        identity = build_identity(run_dir)
        assert identity is not None
        values, counts, _ = collect_fields(
            identity,
            ["map.geometry.block-delta.applied"],
            ["authored_ms", "npc_ms", "duration_ms"],
            "all",
            None,
            None,
        )
        stats = summarize(values[("map.geometry.block-delta.applied", "authored_ms")])
        assert stats["n"] == 2 and abs(stats["mean"] - 20.0) < 1e-6, stats
        assert counts["map.geometry.block-delta.applied"] == 2
        measured = latency(
            identity, "map-editor.object.deleted", "map.geometry.block-delta.applied",
            "all", 60_000,
        )
        assert measured["summary"]["n"] == 2, measured
        assert abs(measured["summary"]["mean"] - 1000.0) < 1e-6, measured
    print("measure-events self-test: PASS")
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("run", nargs="?", help="run id, run directory, or events.jsonl")
    parser.add_argument("--event", action="append", default=[], help="event name or glob (repeatable)")
    parser.add_argument("--field", action="append", default=[], help="numeric field to summarize (repeatable)")
    parser.add_argument("--instance", choices=["all", "server", "client"], default="all")
    parser.add_argument("--since", help="UTC ISO time or HH:MM:SS")
    parser.add_argument("--until", help="UTC ISO time or HH:MM:SS")
    parser.add_argument("--latency-a", help="earlier event name")
    parser.add_argument("--latency-b", help="later event name")
    parser.add_argument("--latency-window-ms", type=float, default=30_000)
    parser.add_argument("--compare", help="second run id/dir/journal for a before/after table")
    parser.add_argument("--preset", choices=sorted(PRESETS), help="predefined event/field set")
    parser.add_argument("--json", action="store_true", help="emit a JSON object instead of markdown")
    parser.add_argument(
        "--out",
        help="also write the rendered report here (persist metrics before retention prunes journals)",
    )
    parser.add_argument("--self-test", action="store_true", help="run a synthetic round-trip")
    args = parser.parse_args(argv)

    if args.self_test:
        return run_self_test()

    preset = PRESETS.get(args.preset or "", {})
    events = list(args.event) or list(preset.get("events", []))
    fields = list(args.field) or list(preset.get("fields", []))
    latency_a = args.latency_a or preset.get("latency_a")
    latency_b = args.latency_b or preset.get("latency_b")
    window_ms = args.latency_window_ms or preset.get("latency_window_ms", 30_000)

    if not events:
        parser.error("at least one --event or --preset is required")

    identity = resolve_run(args.run)
    if identity is None:
        print(
            "No matching run journal found. Pass a run id, a run directory, or an "
            "events.jsonl path. Retention may have removed old journals; persist "
            "metrics with --out when the run is still live.",
            file=sys.stderr,
        )
        return 2

    since_ms = parse_bound(args.since)
    until_ms = parse_bound(args.until)

    values, counts, _ = collect_fields(
        identity, events, fields, args.instance, since_ms, until_ms
    )
    latency_result = None
    if latency_a and latency_b:
        latency_result = latency(identity, latency_a, latency_b, args.instance, window_ms)

    import contextlib
    import io

    buffer = io.StringIO()
    with contextlib.redirect_stdout(buffer):
        if args.compare:
            other = resolve_run(args.compare)
            if other is None:
                print(
                    f"No matching comparison run found: {args.compare}. Retention may "
                    "have removed its journals.",
                    file=sys.stderr,
                )
                return 2
            other_values, other_counts, _ = collect_fields(
                other, events, fields, args.instance, since_ms, until_ms
            )
            other_latency = None
            if latency_a and latency_b:
                other_latency = latency(other, latency_a, latency_b, args.instance, window_ms)
            if args.json:
                payload = {
                    "before": {
                        "identity": identity_lines(identity),
                        "counts": counts,
                        "values": {
                            f"{name}:{key}": summarize(rows)
                            for (name, key), rows in values.items()
                        },
                    },
                    "after": {
                        "identity": identity_lines(other),
                        "counts": other_counts,
                        "values": {
                            f"{name}:{key}": summarize(rows)
                            for (name, key), rows in other_values.items()
                        },
                    },
                    "latency_before": latency_result,
                    "latency_after": other_latency,
                }
                print(json.dumps(payload, indent=2, sort_keys=True))
            else:
                print_comparison(
                    identity, other, values, other_values, latency_result, other_latency
                )
        elif args.json:
            payload = {
                "identity": identity_lines(identity),
                "counts": counts,
                "values": {
                    f"{name}:{key}": summarize(rows) for (name, key), rows in values.items()
                },
                "latency": latency_result,
            }
            print(json.dumps(payload, indent=2, sort_keys=True))
        else:
            print_metrics(identity, values, counts, latency_result)

    text = buffer.getvalue()
    if args.out:
        out_path = Path(args.out)
        if not out_path.is_absolute():
            out_path = (Path.cwd() / out_path).resolve()
        out_path.parent.mkdir(parents=True, exist_ok=True)
        out_path.write_text(text, encoding="utf-8")
        print(f"[measure-events] wrote {out_path}")
    print(text, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
