"""Bounded launch timeline and diagnostic reports for the dev-loop."""

from __future__ import annotations

import datetime
import json
import os
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
REPORT_ROOT = ROOT / ".dev" / "launch-reports"


def utc_now() -> str:
    return datetime.datetime.now(datetime.timezone.utc).isoformat(
        timespec="milliseconds").replace("+00:00", "Z")


def safe_id(value: str) -> str:
    return re.sub(r"[^A-Za-z0-9_.-]+", "-", str(value or "")).strip("-") or "launch"


def report_dir(root: Path | None = None, launch_id: str = "") -> Path:
    base = (root or ROOT) / ".dev" / "launch-reports"
    if not launch_id:
        return base
    return base / safe_id(launch_id)


def failure_code(stage: str, result: str, reason: str = "") -> str:
    if result == "succeeded":
        return ""
    name = str(stage or "")
    text = (str(reason or "") + " " + name).lower()
    if "launch_exception" in name:
        return "launch_exception"
    if "artifact_selection" in name:
        return "no_artifact"
    if "artifact_validation" in name:
        return "artifact_invalid"
    if "launch_configuration" in name:
        return "launch_config_invalid"
    if "firewall" in name:
        return "firewall_blocked"
    if "server_process_start" in name:
        return "server_spawn_failed"
    if "server_logger" in name or ("server" in text and "logger" in text):
        return "server_logger_timeout"
    if "server_gameplay" in name:
        return "server_gameplay_timeout"
    if "room_registration" in name or "room_code" in name:
        return "room_code_timeout"
    if "client_process_start" in name:
        return "client_spawn_failed"
    if "client_logger" in name or ("client" in text and "logger" in text):
        return "client_logger_timeout"
    if "client_connection" in name:
        return "client_connect_timeout"
    if "map" in text and ("ready" in text or "load" in text):
        return "map_load_timeout"
    if "mode_start" in name:
        return "mode_start_timeout"
    if "build_identity_match" in name:
        return "identity_mismatch"
    if "hot_client_gate" in name:
        return "render_init_failed"
    if "crash" in text:
        return "crash"
    if "lock" in text:
        return "stale_lock"
    if "memory" in text or "disk" in text or "resource" in text:
        return "resource_exhausted"
    if "timeout" in text:
        return "timeout"
    return "unknown"


def _read_json(path: Path) -> dict:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
        return value if isinstance(value, dict) else {}
    except (OSError, json.JSONDecodeError):
        return {}


def start_launch(root: Path, launch_id: str, request_id: str = "",
                 build_id: str = "", generation: int = 0) -> None:
    directory = report_dir(root, launch_id)
    directory.mkdir(parents=True, exist_ok=True)
    summary = {
        "schema_version": 1,
        "launch_id": launch_id,
        "request_id": request_id,
        "build_id": build_id,
        "generation": generation,
        "started_utc": utc_now(),
        "result": "running",
        "failure_code": "",
        "failure_stage": "",
        "failure_detail": "",
    }
    (directory / "launch.json").write_text(
        json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    (directory / "stages.jsonl").touch()


def append_stage(root: Path, launch_id: str, fields: dict) -> None:
    directory = report_dir(root, launch_id)
    directory.mkdir(parents=True, exist_ok=True)
    with (directory / "stages.jsonl").open("a", encoding="utf-8",
                                           newline="\n") as handle:
        handle.write(json.dumps(fields, separators=(",", ":")) + "\n")


def _create_launch_issue(summary: dict) -> dict:
    try:
        import sys
        sys.path.insert(0, str(ROOT))
        from devscripts.agent_coord import CoordStore
        store = CoordStore(publish_docs=False)
        try:
            store.ensure_agent("launch-doctor", "launch failure diagnosis")
            title = (f"Launch failure: {summary.get('failure_code') or 'unknown'} "
                     f"at {summary.get('failure_stage') or 'unknown'}")
            existing = next((row for row in store.issue_list(limit=200)
                             if row.get("title") == title), None)
            evidence = {
                "launch_id": summary.get("launch_id"),
                "report": str(report_dir(ROOT, str(summary.get("launch_id") or ""))),
                "build_id": summary.get("build_id"),
                "generation": summary.get("generation"),
                "failure_code": summary.get("failure_code"),
                "failure_stage": summary.get("failure_stage"),
                "detail": summary.get("failure_detail"),
            }
            if existing:
                result = store.issue_occurrence(
                    "launch-doctor", existing["id"],
                    f"Failure recurred at {summary.get('failure_stage')}: "
                    f"{summary.get('failure_detail')}", evidence)
                issue_id = str(existing["id"])
                action = "appended"
            else:
                result = store.issue_add(
                    "launch-doctor", title,
                    quote=str(summary.get("failure_detail") or
                              f"Launch failed at "
                              f"{summary.get('failure_stage') or 'unknown'}"),
                    summary="Launch failure automatically reported by dev-loop",
                    kind="launch-friction", source="dev-loop",
                    via_agent="launch-doctor", priority="high", evidence=evidence,
                    reproduction=(f"Launch {summary.get('launch_id')} failed at "
                                  f"{summary.get('failure_stage')}."),
                    expected_behavior=("The requested launch reaches a live "
                                       "server/client pair without a readiness "
                                       "or mode-start failure."),
                    acceptance=("The launch report identifies the first failed "
                                "stage, and the corrected build passes the same "
                                "launch path."),
                    paths=["devscripts/dev-loop.py",
                           "devscripts/dev_loop/launch_report.py"],
                    dependencies=[],
                    risk="Launch recovery may mask a reproducible build or mode defect.",
                    proof_expectation=("A successful launch report plus the exact "
                                       "server/client events.jsonl journals."))
                issue_id = str((result or {}).get("id") or "")
                action = "created"
            return {"status": action, "issue_id": issue_id}
        finally:
            store.close()
    except Exception as exc:
        return {"status": "failed", "error": f"{type(exc).__name__}: {exc}"}


def finish_launch(root: Path, launch_id: str, stage: str, result: str,
                  reason: str = "", *, create_issue: bool = False, **details) -> dict:
    directory = report_dir(root, launch_id)
    summary = _read_json(directory / "launch.json")
    rows = stages(root, launch_id)
    failed_stage = ""
    failed_reason = ""
    if result != "succeeded":
        for row in rows:
            if row.get("result") not in (
                    "succeeded", "ready", "started", "accepted", "selected",
                    "resolved", "passed", "matched", "reused", "completed"):
                failed_stage = str(row.get("stage") or "")
                failed_reason = str(row.get("reason") or "")
                break
        if not failed_stage:
            failed_stage = stage
    failure_detail = failed_reason or reason
    code = failure_code(failed_stage or stage, result, failure_detail)
    summary.update({
        "finished_utc": utc_now(),
        "result": "succeeded" if result == "succeeded" else "failed",
        "failure_code": code,
        "failure_stage": failed_stage,
        "failure_detail": "" if result == "succeeded" else failure_detail,
        "stage": stage,
        "stage_result": result,
        "reason": reason,
    })
    for key, value in details.items():
        summary[key] = value
    if result != "succeeded" and create_issue:
        summary["issue_result"] = _create_launch_issue(summary)
    elif result == "succeeded":
        summary["issue_result"] = {"status": "not_needed"}
    else:
        summary["issue_result"] = {"status": "not_requested"}
    (directory / "launch.json").write_text(
        json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    if result != "succeeded":
        (directory / "report.md").write_text(render_markdown(summary), encoding="utf-8")
    return summary


def render_markdown(summary: dict) -> str:
    return "\n".join([
        f"# Launch report: {summary.get('launch_id', '-')}",
        "",
        f"- Result: **{summary.get('result', '-')}**",
        f"- Failure code: `{summary.get('failure_code') or '-'}`",
        f"- Failure stage: `{summary.get('failure_stage') or '-'}`",
        f"- Detail: {summary.get('failure_detail') or summary.get('reason') or '-'}",
        f"- Build ID: `{summary.get('build_id') or '-'}`",
        f"- Generation: `{summary.get('generation') or '-'}`",
        f"- Started: `{summary.get('started_utc') or '-'}`",
        f"- Finished: `{summary.get('finished_utc') or '-'}`",
        f"- Automatic issue: `{summary.get('issue_result', {}).get('status', 'not_requested')}`",
        f"- Issue ID: `{summary.get('issue_result', {}).get('issue_id') or '-'}`",
        (f"- Issue logging error: {summary.get('issue_result', {}).get('error')}"
         if summary.get('issue_result', {}).get('error') else ""),
        "",
        "## Next probes",
        "",
        "- Read `stages.jsonl` and find the first failed stage.",
        "- Check the exact server/client journals and crash directory.",
        "- Verify the artifact/build identity before retrying.",
        "- Retry once only when the failure is known-transient.",
        "- Escalate firewall, unrelated-process, or authority changes to the user.",
    ]) + "\n"


def list_reports(root: Path | None = None) -> list[dict]:
    base = report_dir(root)
    rows = []
    if not base.is_dir():
        return rows
    for directory in base.iterdir():
        summary = _read_json(directory / "launch.json")
        if summary:
            rows.append(summary)
    rows.sort(key=lambda item: str(item.get("started_utc") or ""), reverse=True)
    return rows


def load_report(root: Path | None = None, launch_id: str = "") -> dict:
    if not launch_id:
        rows = list_reports(root)
        return rows[0] if rows else {}
    directory = report_dir(root, launch_id)
    return _read_json(directory / "launch.json")


def stages(root: Path | None = None, launch_id: str = "") -> list[dict]:
    path = report_dir(root, launch_id) / "stages.jsonl"
    rows = []
    try:
        for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
            try:
                value = json.loads(line)
                if isinstance(value, dict):
                    rows.append(value)
            except json.JSONDecodeError:
                continue
    except OSError:
        pass
    return rows


def probe_report(root: Path | None = None, launch_id: str = "") -> dict:
    summary = load_report(root, launch_id)
    rows = stages(root, launch_id or str(summary.get("launch_id") or ""))
    failed = [row for row in rows if row.get("result") not in ("succeeded", "ready", "started", "accepted", "selected", "resolved", "passed", "matched", "reused", "completed", "ready")]
    return {
        "launch": summary,
        "failed_stage": failed[0] if failed else {},
        "stages": rows,
        "next_probe": (
            "inspect artifact/build identity" if summary.get("failure_code") in
            {"no_artifact", "artifact_invalid", "artifact_stale", "identity_mismatch"}
            else "inspect the failed stage journal and child process identity"
        ),
    }





