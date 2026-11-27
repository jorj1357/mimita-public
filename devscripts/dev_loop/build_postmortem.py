"""Turn one game-DLL build summary into a why/time/postmortem record.

The dev-loop already records detailed phase durations, worker metrics, resource
sampling, notes, and changed paths. This module condenses that into a stable
record every build can carry in `game-generation.ready` and in
`.dev/build-timings/<attempt>.json`.

It is intentionally dependency-free so the builder can call it inline.
"""

from __future__ import annotations

import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent


def _number(value, default=0.0) -> float:
    try:
        if value is None or value == "":
            return default
        return float(value)
    except (TypeError, ValueError):
        return default


def _first_number(*values, default=0.0) -> float:
    for value in values:
        if value is None or value == "":
            continue
        number = _number(value, None)
        if number is not None:
            return number
    return default


def _read_timeline_queue(summary: dict) -> tuple[float, float]:
    """Return (max, mean) file queue-wait seconds from the attempt timeline."""
    timeline = str(summary.get("timeline_jsonl") or "")
    if not timeline:
        return 0.0, 0.0
    path = (ROOT / timeline).resolve()
    try:
        path.relative_to(ROOT)
    except ValueError:
        return 0.0, 0.0
    waits: list[float] = []
    try:
        with path.open("r", encoding="utf-8") as handle:
            for line in handle:
                try:
                    row = json.loads(line)
                except json.JSONDecodeError:
                    continue
                if row.get("event") != "translation_unit.completed":
                    continue
                wait = _number(row.get("queue_wait_ms"), None)
                if wait is not None:
                    waits.append(wait / 1000.0)
    except OSError:
        return 0.0, 0.0
    if not waits:
        return 0.0, 0.0
    return max(waits), sum(waits) / len(waits)


def build_postmortem(summary: dict) -> dict:
    notes = summary.get("notes") or []
    note = notes[0] if notes and isinstance(notes[0], dict) else {}
    why = " ".join(str(note.get("why") or "").split())
    why_missing = not why
    changed = [str(p) for p in (summary.get("changed_paths") or [])]
    headers = [p for p in changed if p.endswith((".h", ".hpp", ".inl"))]
    fanout = note.get("header_fanout")
    try:
        fanout = int(fanout) if fanout is not None else None
    except (TypeError, ValueError):
        fanout = None

    wm = summary.get("worker_metrics") or {}
    total = _first_number(summary.get("total_seconds"),
                          wm.get("total_seconds"))
    worker_total = _first_number(summary.get("worker_total_seconds"),
                                 wm.get("worker_total_seconds"))
    compile_s = _first_number(wm.get("compile_seconds"),
                              _number(summary.get("compile_sum_ms")) / 1000.0)
    link_s = _first_number(wm.get("link_seconds"))
    publish_s = _first_number(wm.get("publish_seconds"))
    scan_s = _first_number(summary.get("object_scan_seconds"),
                           wm.get("object_scan_seconds"))
    queue_max_s, queue_mean_s = _read_timeline_queue(summary)
    compiled = int(_number(wm.get("compiled"), 0))
    skipped = int(_number(wm.get("skipped"), 0))
    invalidations = summary.get("cache_invalidations") or []
    slowest = wm.get("slowest_compiles") or []
    if isinstance(slowest, list):
        slowest = slowest[:5]

    res = summary.get("resource_sampling") or {}
    mean_cpu = _number(res.get("mean_host_cpu_percent"), 0.0)
    peak_cpu = _number(res.get("peak_host_cpu_percent"), 0.0)
    min_mem_mb = _number(res.get("minimum_available_memory_bytes"), 0.0) / (
        1024.0 * 1024.0)

    suggestions: list[str] = []
    if why_missing:
        suggestions.append(
            "Add --why \"...\" to devscripts/build-note.py so this build's "
            "purpose is recorded in the note and game-build events.")
    if headers and fanout is None:
        suggestions.append(
            "A header changed without a recorded fanout. Run "
            "`python devscripts/dependency_graph.py --affected <header>` and "
            "pass --header-fanout to the build note.")
    if fanout is not None and fanout > 50:
        suggestions.append(
            f"Header fanout is {fanout} sources. Split the header, move "
            "implementation-only types out of it, or batch the dependent "
            "changes in one build.")
    if compile_s > 60.0:
        top = slowest[0]["path"] if slowest and isinstance(slowest[0], dict) \
            else "the slowest TU"
        suggestions.append(
            f"Compile dominates ({compile_s:.1f}s). Inspect {top} and the "
            "top 5 slowest TUs; reduce includes/PCH fanout or split the TU.")
    if link_s > 20.0:
        suggestions.append(
            f"Link is {link_s:.1f}s. Check link_input_count and consider "
            "fewer/lighter object inputs or a targeted link path.")
    if total > 0.0 and scan_s > max(5.0, total * 0.10):
        suggestions.append(
            f"Object scan is {scan_s:.1f}s of {total:.1f}s. Check object-cache "
            "freshness, index load, and stat prewarm; quarantine only per the "
            "build self-repair runbook.")
    if compiled > 500:
        suggestions.append(
            f"{compiled} TUs compiled. This is effectively a full rebuild; "
            "batch related changes and check cache invalidations.")
    if peak_cpu >= 90.0 or (min_mem_mb and min_mem_mb < 256.0):
        suggestions.append(
            f"Resource pressure: peak host CPU {peak_cpu:.1f}%, minimum free "
            f"memory {min_mem_mb:.0f} MB. Move concurrent builds/tests off the "
            "critical path or schedule off-peak.")
    if queue_max_s > 30.0:
        suggestions.append(
            f"Max per-TU queue wait is {queue_max_s:.1f}s. The worker pool is "
            "starved or the build slot is contended; check jobs and concurrent "
            "runs.")
    if not suggestions:
        suggestions.append(
            "No dominant phase: keep watching slowest TUs, queue wait, header "
            "fanout, cache invalidations, and CPU/memory.")

    return {
        "schema_version": 1,
        "why": why,
        "why_missing": why_missing,
        "agent": note.get("agent") or "",
        "attempt_id": summary.get("build_attempt_id") or "",
        "generation": summary.get("generation"),
        "changed_paths": changed,
        "headers": headers,
        "header_fanout": fanout,
        "seconds": {
            "total": round(total, 3),
            "worker_total": round(worker_total, 3),
            "compile": round(compile_s, 3),
            "link": round(link_s, 3),
            "publish": round(publish_s, 3),
            "object_scan": round(scan_s, 3),
            "queue_wait_max": round(queue_max_s, 3),
            "queue_wait_mean": round(queue_mean_s, 3),
        },
        "cache": {
            "compiled": compiled,
            "skipped": skipped,
            "invalidations": invalidations,
        },
        "slowest_compiles": slowest,
        "resource": {
            "mean_host_cpu_percent": round(mean_cpu, 2),
            "peak_host_cpu_percent": round(peak_cpu, 2),
            "minimum_available_memory_mb": round(min_mem_mb, 1),
        },
        "suggestions": suggestions,
        "metrics_to_watch": [
            "why (build-note --why)",
            "header_fanout (dependency_graph --affected)",
            "compile/link/publish/scan seconds",
            "slowest_compiles top 5",
            "per-TU queue_wait_ms",
            "cache_invalidations",
            "resource mean/peak CPU and minimum free memory",
        ],
    }
