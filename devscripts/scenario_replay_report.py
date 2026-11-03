#!/usr/bin/env python3
"""Run/report a human-recorded collision scenario from canonical journals.

The recording is a scenario definition JSON:

    {"journal": "logs/.../events.jsonl", "scenario_id": "scn-...",
     "invariants": ["no_collision_speed_collapse"]}

This tool never authors routes. It runs the same recording on the current
build and reports the exact journal evidence, first divergence, checkpoint
restores, and a verdict. Uncovered or unreproduced runs are not passes.
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def journal_segments(path: Path) -> list[Path]:
    sealed = sorted(
        p for p in path.parent.glob("events-*.jsonl")
        if p.is_file() and len(p.stem) > len("events-")
    )
    return sealed + ([path] if path.is_file() else [])


def read_records(path: Path, scenario_id: str, definition_path: Path | None = None):
    for segment in journal_segments(path):
        try:
            lines = segment.read_text(encoding="utf-8", errors="replace").splitlines()
        except OSError:
            continue
        for line_number, line in enumerate(lines, 1):
            try:
                record = json.loads(line)
            except (ValueError, TypeError):
                continue
            fields = record.get("fields") or {}
            record_scenario = str(fields.get("scenario_id") or record.get("correlation_id") or "")
            record_path = str(fields.get("path") or "")
            path_match = False
            if definition_path is not None and record_path:
                try:
                    path_match = (ROOT / record_path).resolve() == definition_path.resolve()
                except OSError:
                    path_match = Path(record_path).name == definition_path.name
            if record_scenario != scenario_id and not path_match:
                continue
            yield segment, line_number, record


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("definition", help="scenario definition JSON")
    parser.add_argument("--play", action="store_true",
                        help="send scenario_play through the dev loop before reporting")
    parser.add_argument("--require-no-divergence", action="store_true")
    parser.add_argument("--wait-seconds", type=float, default=120.0,
                        help="runner guard while waiting for completion; not a gameplay verdict")
    parser.add_argument("--playback-journal",
                        help="journal that receives playback events (default: source journal)")
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()

    definition_path = Path(args.definition)
    definition = json.loads(definition_path.read_text(encoding="utf-8"))
    journal = Path(definition.get("journal", ""))
    scenario_id = str(definition.get("scenario_id", ""))
    if not journal or not scenario_id:
        print("definition requires journal and scenario_id", file=sys.stderr)
        return 2
    if not journal.is_absolute():
        journal = (ROOT / journal).resolve()

    playback_journal = Path(args.playback_journal) if args.playback_journal else journal
    if not playback_journal.is_absolute():
        playback_journal = (ROOT / playback_journal).resolve()

    play_result = None
    if args.play:
        status = subprocess.run(
            [sys.executable, str(ROOT / "devscripts" / "dev-loop.py"),
             "--status", "--json"],
            cwd=str(ROOT), text=True, capture_output=True,
        )
        try:
            current_status = json.loads(status.stdout)
            live_events = current_status.get("events_path")
            if live_events:
                playback_journal = Path(live_events)
        except (ValueError, TypeError):
            pass
        play_result = subprocess.run(
            [sys.executable, str(ROOT / "devscripts" / "dev-loop.py"),
             "--command", f"scenario_play {definition_path}", "--json"],
            cwd=str(ROOT), text=True, capture_output=True,
        )
        deadline = time.monotonic() + max(0.0, args.wait_seconds)
        while time.monotonic() < deadline:
            try:
                current_events = list(read_records(playback_journal, scenario_id, definition_path))
            except OSError:
                current_events = []
            names = {item[2].get("event") for item in current_events}
            if names & {"scenario.playback.completed", "scenario.playback.failed",
                        "scenario.playback.stopped", "scenario.record.invalid"}:
                break
            time.sleep(0.25)

    events = list(read_records(journal, scenario_id, definition_path))
    if playback_journal != journal:
        events.extend(read_records(playback_journal, scenario_id, definition_path))
    by_event: dict[str, list] = {}
    for item in events:
        by_event.setdefault(item[2].get("event", ""), []).append(item)

    first_divergence = by_event.get("scenario.playback.divergence", [])
    restores = by_event.get("scenario.replay.restore", [])
    completed = by_event.get("scenario.playback.completed", [])
    failed = by_event.get("scenario.playback.failed", [])
    invalid = (by_event.get("scenario.record.invalid", []) +
               by_event.get("scenario.server.record.invalid", []))
    started = by_event.get("scenario.record.started", [])
    record_ticks = by_event.get("scenario.record.tick", [])
    checkpoints = by_event.get("scenario.record.checkpoint", [])

    verdict = "PENDING"
    reason = "no playback completion or failure event observed"
    if not started:
        verdict = "INVALID"
        reason = "scenario.record.started missing"
    elif invalid:
        verdict = "INVALID"
        reason = "scenario recording is incomplete"
    elif failed:
        verdict = "FAIL"
        reason = "scenario.playback.failed observed"
    elif completed:
        if args.require_no_divergence and first_divergence:
            verdict = "FAIL"
            reason = "playback completed but divergence evidence exists"
        elif definition.get("require_checkpoint") and not checkpoints:
            verdict = "INVALID"
            reason = "checkpoint required but missing"
        else:
            verdict = "PASS"
            reason = "playback completed; review cited event evidence"

    def cite(items):
        if not items:
            return None
        segment, line_number, record = items[0]
        return {
            "journal": str(segment),
            "line": line_number,
            "event": record.get("event"),
            "event_id": record.get("event_id"),
            "tick": record.get("tick"),
            "fields": record.get("fields") or {},
        }

    report = {
        "definition": str(definition_path),
        "scenario_id": scenario_id,
        "journal": str(journal),
        "playback_journal": str(playback_journal),
        "verdict": verdict,
        "reason": reason,
        "record_started": cite(started),
        "record_tick_count": len(record_ticks),
        "checkpoint_count": len(checkpoints),
        "first_divergence": cite(first_divergence),
        "restores": [cite(item) for item in restores],
        "playback_completed": cite(completed),
        "playback_failed": cite(failed),
        "record_invalid": cite(invalid),
        "play_command": None if play_result is None else {
            "returncode": play_result.returncode,
            "stdout": play_result.stdout[-4000:],
            "stderr": play_result.stderr[-4000:],
        },
    }
    if args.json:
        print(json.dumps(report, indent=2))
    else:
        print(f"[SCENARIO] {verdict}: {reason}")
        print(f"  scenario={scenario_id}")
        print(f"  journal={journal}")
        print(f"  record_ticks={len(record_ticks)} checkpoints={len(checkpoints)}")
        if report["first_divergence"]:
            first = report["first_divergence"]
            print(f"  first_divergence={first['journal']}:{first['line']} "
                  f"event_id={first['event_id']} tick={first['tick']}")
        for restore in report["restores"]:
            print(f"  restore={restore['journal']}:{restore['line']} "
                  f"event_id={restore['event_id']} tick={restore['tick']}")
        if play_result is not None:
            print(f"  play_returncode={play_result.returncode}")
    return 0 if verdict == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
