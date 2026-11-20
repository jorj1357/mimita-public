#!/usr/bin/env python3
"""Detect build and change-to-evidence friction and optionally file an issue."""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from devscripts.agent_coord import CoordError, CoordStore  # noqa: E402


def _read_json(path: Path) -> dict:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return {}
    return value if isinstance(value, dict) else {}


def _seconds_to_ms(value: object) -> float:
    try:
        return max(0.0, float(value) * 1000.0)
    except (TypeError, ValueError):
        return 0.0


def _cold_findings() -> list[dict]:
    guard = ROOT / "devscripts" / "cold_change_guard.py"
    report = ROOT / ".dev" / "tmp" / "build-friction" / "cold-change.json"
    report.parent.mkdir(parents=True, exist_ok=True)
    result = subprocess.run(
        [sys.executable, str(guard), "--report-only", "--json", str(report)],
        cwd=ROOT, capture_output=True, text=True, check=False)
    if result.returncode != 0:
        return []
    try:
        data = json.loads(report.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return []
    findings: list[dict] = []
    rows = data if isinstance(data, list) else data.get("changes") or data.get("files") or []
    for row in rows if isinstance(rows, list) else []:
        if not isinstance(row, dict):
            continue
        classification = str(row.get("classification") or row.get("class") or "")
        if classification in {"COLD", "COLD_ABI", "HEADER_REVIEW", "UNKNOWN"}:
            findings.append({
                "kind": "cold-change",
                "title": f"Build friction: {classification} change requires EXE work",
                "summary": f"{row.get('path') or '(unknown path)'} is {classification}",
                "evidence": row,
            })
    return findings


def collect_findings(threshold_ms: float, include_cold: bool) -> list[dict]:
    state = _read_json(ROOT / ".dev" / "state.json")
    game = state.get("game_build") or {}
    findings: list[dict] = []
    build_id = str(state.get("latest_successful_build_id") or "")
    cause = str(state.get("build_cause") or game.get("build_cause") or "")
    if cause in {"header_fanout", "rebuild", "identity", "mixed"}:
        findings.append({
            "kind": "build-cause",
            "title": f"Build friction: {cause} build",
            "summary": f"build cause {cause} on {build_id or 'an unpinned build'}",
            "evidence": {
                "build_id": build_id,
                "build_cause": cause,
                "source_generation": state.get("source_generation"),
                "changed_headers": state.get("changed_headers") or [],
                "header_fanout_top": state.get("header_fanout_top") or [],
            },
        })
    for key, label in (
        ("worker_total_seconds", "worker"),
        ("compile_seconds", "compile"),
        ("link_seconds", "link"),
        ("staging_seconds", "staging"),
    ):
        value = game.get(key, state.get(key))
        elapsed_ms = _seconds_to_ms(value)
        if elapsed_ms > threshold_ms:
            findings.append({
                "kind": "phase-budget",
                "title": f"Build friction: slow {label} phase",
                "summary": f"{label} took {elapsed_ms / 1000.0:.2f}s (budget {threshold_ms / 1000.0:.2f}s)",
                "evidence": {
                    "build_id": build_id,
                    "phase": label,
                    "elapsed_ms": elapsed_ms,
                    "budget_ms": threshold_ms,
                    "source_generation": state.get("source_generation"),
                },
            })
    pending = bool(state.get("build_pending"))
    active = state.get("active_build_generation")
    if pending and active is None:
        findings.append({
            "kind": "build-wait",
            "title": "Build friction: build request is waiting",
            "summary": "a build is pending with no active generation",
            "evidence": {
                "build_status": state.get("build_status"),
                "build_pending": pending,
                "active_build_generation": active,
                "source_generation": state.get("source_generation"),
                "message": state.get("message"),
            },
        })
    if include_cold:
        findings.extend(_cold_findings())
    return findings


def _existing(store: CoordStore, title: str) -> bool:
    return any(row.get("title") == title
               for row in store.issue_list(kind="build-friction", limit=200)
               if row.get("state") not in {"done", "cancelled"})


def record(findings: list[dict], agent: str, task: str) -> int:
    store = CoordStore()
    created = 0
    try:
        for finding in findings:
            if _existing(store, str(finding["title"])):
                continue
            store.issue_add(
                agent, str(finding["title"]),
                quote=str(finding["summary"]),
                summary=str(finding["summary"]),
                kind="build-friction", source="build", via_agent=agent,
                severity="normal", priority="normal",
                evidence=finding.get("evidence") or {})
            created += 1
    finally:
        store.close()
    return created


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--agent", default=os.environ.get("MIMITA_AGENT_ID", ""))
    parser.add_argument("--task", default="")
    parser.add_argument("--threshold-ms", type=float, default=5000.0)
    parser.add_argument("--no-cold", action="store_true")
    parser.add_argument("--create", action="store_true",
                        help="file new build-friction issues in the shared brain")
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()

    findings = collect_findings(args.threshold_ms, not args.no_cold)
    if args.json:
        print(json.dumps(findings, indent=2, sort_keys=True))
    else:
        for finding in findings:
            print(f"[BUILD FRICTION] {finding['title']}: {finding['summary']}")
        if not findings:
            print("[BUILD FRICTION] no findings")
    if args.create and findings:
        agent = args.agent.strip()
        if not agent:
            print("[BUILD FRICTION] --agent or MIMITA_AGENT_ID is required with --create",
                  file=sys.stderr)
            return 2
        try:
            created = record(findings, agent, args.task)
        except CoordError as error:
            print(f"[BUILD FRICTION] {error}", file=sys.stderr)
            return error.code
        print(f"[BUILD FRICTION] created={created} existing={len(findings) - created}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
