#!/usr/bin/env python3
"""Detect build and change-to-evidence friction and optionally file an issue."""

from __future__ import annotations

import argparse
import json
import os
import shutil
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


def _wait_issue(store: CoordStore, wait_id: str) -> dict | None:
    """Find an existing issue for one exact wait occurrence."""
    for row in store.issue_list(kind="build-friction", limit=500):
        try:
            evidence = json.loads(row.get("evidence") or "{}")
        except (TypeError, json.JSONDecodeError):
            continue
        if isinstance(evidence, dict) and evidence.get("wait_id") == wait_id:
            return row
    return None


def record_wait_incident(agent: str, wait: dict) -> dict:
    """Create or append one build-friction issue for a concrete wait.

    The wait ID is the uniqueness key. A long wait gets one issue; a new wait
    gets another issue even when its stage and reason are identical.
    """
    wait_id = str(wait.get("wait_id") or "").strip()
    if not wait_id:
        raise ValueError("wait_id is required")
    kind = str(wait.get("kind") or "wait")
    stage = str(wait.get("stage") or "unknown")
    summary = str(wait.get("summary") or
                  f"{kind} wait at {stage} exceeded the wait budget")
    store = CoordStore()
    try:
        existing = _wait_issue(store, wait_id)
        if existing is not None:
            store.issue_occurrence(agent, str(existing["id"]), summary,
                                   evidence=wait)
            return {"created": False, "issue_id": str(existing["id"]),
                    "wait_id": wait_id}
        issue = store.issue_add(
            agent,
            f"Wait over budget: {kind} at {stage}",
            quote=summary,
            summary=summary,
            kind="build-friction",
            source="dev-loop",
            via_agent=agent,
            severity="normal",
            priority="high",
            evidence=wait,
            reproduction=(
                f"Observe dev-loop {kind} wait {wait_id} at stage {stage}; "
                f"elapsed={wait.get('elapsed_seconds')}s."),
            expected_behavior=(
                "A build/test wait over one minute creates one visible "
                "build-friction issue and wakes one reviewer automatically."),
            acceptance=(
                "The issue carries wait_id, stage, reason, elapsed, build/run "
                "IDs, and evidence; a reviewer wake is queued exactly once for "
                "this wait occurrence."),
            paths=["devscripts/dev-loop.py", "devscripts/build_friction.py",
                   "devscripts/agent-wake.py"],
            proof_expectation=(
                "Dev-loop wait event plus the issue record and reviewer wake "
                "record."),
        )
        return {"created": True, "issue_id": str(issue["id"]),
                "wait_id": wait_id}
    finally:
        store.close()


def wake_reviewer(issue_id: str, wait: dict) -> dict:
    """Queue one bounded reviewer activation for an over-budget wait."""
    script = ROOT / "devscripts" / "agent-wake.py"
    adapters = [name for name in ("codex", "opencode")
                if shutil.which(name)]
    if not adapters:
        return {"queued": False, "reason": "no wake adapter available"}
    primary = adapters[0]
    fallbacks: list[str] = []
    for adapter in ("opencode", "codex"):
        if adapter != primary and shutil.which(adapter):
            fallbacks.append(adapter)
    prompt = (
        f"Review build/test wait issue {issue_id}. The wait is {wait.get('kind')} "
        f"at stage {wait.get('stage')}, elapsed {wait.get('elapsed_seconds')}s, "
        f"wait_id {wait.get('wait_id')}.\n\n"
        "Do not edit source or claim files owned by another agent. Inspect the "
        "issue and dev-loop journal, complete any missing reproduction, "
        "expected behavior, acceptance, paths, or proof expectation, and append "
        "the review and evidence to the issue. If the wait recurs, add the "
        "occurrence instead of creating a duplicate issue. If a code fix is "
        "needed, report it with evidence and claim it separately.")
    command = [
        sys.executable, str(script), "add",
        "--agent", "build-wait-reviewer",
        "--issue", issue_id,
        "--when", "manual",
        "--adapter", primary,
        "--prompt", prompt,
    ]
    for adapter in fallbacks:
        command.extend(["--fallback-adapter", adapter])
    queued = subprocess.run(command, cwd=ROOT, text=True,
                            capture_output=True, check=False)
    if queued.returncode != 0:
        return {"queued": False, "reason": queued.stderr.strip() or
                queued.stdout.strip() or "wake add failed"}
    # Launch immediately if no daemon is already active. The wake queue is
    # idempotent for a claimed request, so a concurrent daemon is safe.
    launched = subprocess.run(
        [sys.executable, str(script), "run", "--once"], cwd=ROOT,
        text=True, capture_output=True, check=False, timeout=30)
    return {
        "queued": True,
        "adapter": primary,
        "launch_result": launched.stdout.strip()[-2000:],
        "launch_error": launched.stderr.strip()[-1000:],
    }


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


