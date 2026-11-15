#!/usr/bin/env python3
"""Fail a hot-client run unless it proves the normal hot frame is live.

Checks a canonical client journal for:
- a loaded generation (startup probe or live activation),
- render path `dll`,
- connection state `Connected`,
- frame progression,
- no watchdog stalls after the post-activation bootstrap.
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def latest_client_journal() -> Path | None:
    logs = ROOT / "logs"
    if not logs.is_dir():
        return None
    journals = [path for path in logs.rglob("client*/events.jsonl")
                if path.is_file()]
    if not journals:
        return None
    return max(journals, key=lambda path: path.stat().st_mtime)


def evaluate(journal: Path, min_frames: int, bootstrap_frames: int,
             require_swap: bool, min_activations: int) -> dict:
    started = False
    activated = False
    activation_count = 0
    connection_state = ""
    max_frames = 0
    render_events = 0
    bootstrap_cutoff: float | None = None
    stalls: list[float] = []
    dll = ""
    error = ""

    with journal.open("r", encoding="utf-8", errors="replace") as handle:
        for line in handle:
            try:
                event = json.loads(line)
            except json.JSONDecodeError:
                continue
            name = event.get("event")
            fields = event.get("fields") or {}
            try:
                timestamp = float(event.get("t", 0.0))
            except (TypeError, ValueError):
                timestamp = 0.0

            if name == "game.host.probe.started":
                started = True
                bootstrap_cutoff = None
                dll = str(fields.get("dll") or dll)
            elif name == "game.host.probe.failed":
                error = "generation rejected: " + str(fields.get("path", ""))
            elif name == "game.generation.activated":
                activated = True
                activation_count += 1
                bootstrap_cutoff = None
                dll = str(fields.get("dll") or dll)
            elif name == "game.render.path":
                if fields.get("path") != "dll":
                    continue
                render_events += 1
                try:
                    frames = int(fields.get("frames", 0))
                except (TypeError, ValueError):
                    frames = 0
                max_frames = max(max_frames, frames)
                if frames >= bootstrap_frames:
                    bootstrap_cutoff = timestamp
            elif name == "network.connection-state.changed":
                connection_state = str(fields.get("to") or "")
            elif name == "performance.stall-watchdog":
                stalls.append(timestamp)

    post_stalls = [stamp for stamp in stalls
                   if bootstrap_cutoff is not None and stamp > bootstrap_cutoff]
    checks = {
        "generation_loaded": started or activated,
        "activation_required": activated if require_swap else True,
        "activation_count_met": activation_count >= min_activations,
        "render_path_dll": render_events > 0,
        "connected": connection_state == "Connected",
        "frame_progression": max_frames >= min_frames,
        "no_post_bootstrap_stalls": not post_stalls,
    }
    return {
        "journal": str(journal),
        "dll": dll,
        "started": started,
        "activated": activated,
        "activation_count": activation_count,
        "max_frames": max_frames,
        "render_events": render_events,
        "connection_state": connection_state,
        "bootstrap_cutoff_t": bootstrap_cutoff,
        "post_bootstrap_stalls": post_stalls,
        "error": error,
        "checks": checks,
        "passed": all(checks.values()),
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Validate a live hot-client run")
    parser.add_argument("journal", nargs="?", help="client events.jsonl path")
    parser.add_argument("--latest", action="store_true",
                        help="use the newest client journal")
    parser.add_argument("--min-frames", type=int, default=120)
    parser.add_argument("--bootstrap-frames", type=int, default=30)
    parser.add_argument("--require-swap", action="store_true",
                        help="require a live different-DLL activation")
    parser.add_argument("--min-activations", type=int, default=0,
                        help="require at least this many live activations")
    parser.add_argument("--json", dest="json_path",
                        help="write the result as JSON")
    args = parser.parse_args(argv)

    journal = Path(args.journal) if args.journal else (
        latest_client_journal() if args.latest else None)
    if journal is None or not journal.is_file():
        print("[HOT GATE] no client journal found", file=sys.stderr)
        return 2

    result = evaluate(journal, args.min_frames, args.bootstrap_frames,
                      args.require_swap, args.min_activations)
    if args.json_path:
        output = Path(args.json_path)
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n",
                          encoding="utf-8")

    print(f"[HOT GATE] journal={journal}")
    print(f"[HOT GATE] dll={result['dll'] or '-'} "
          f"frames={result['max_frames']} "
          f"connected={result['connection_state'] or '-'} "
          f"activations={result['activation_count']}")
    for name, passed in result["checks"].items():
        print(f"[HOT GATE] {'PASS' if passed else 'FAIL'} {name}")
    if result["post_bootstrap_stalls"]:
        print("[HOT GATE] post-bootstrap stall times: "
              + ", ".join(f"{stamp:.3f}" for stamp in result["post_bootstrap_stalls"]))
    if result["error"]:
        print(f"[HOT GATE] error={result['error']}")
    print(f"[HOT GATE] {'PASS' if result['passed'] else 'FAIL'}")
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())

