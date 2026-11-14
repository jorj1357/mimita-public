#!/usr/bin/env python3
"""Fill the canonical changelog template with automatic evidence.

The narrative sections stay human/agent-written. This tool captures the branch,
git state, changed files, build identity, generation, and journal path so the
next agent can see what changed and how to reverse it.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TEMPLATE = ROOT / "docs" / "changelog" / "TEMPLATE.md"
CHANGELOG_ROOT = ROOT / "docs" / "changelog"


def utc_now() -> datetime:
    return datetime.now(timezone.utc)


def run_git(*args: str) -> str:
    try:
        completed = subprocess.run(
            ["git", *args],
            cwd=ROOT,
            text=True,
            capture_output=True,
            check=False,
        )
        return completed.stdout.strip()
    except OSError:
        return ""


def read_json(relative: str) -> dict | None:
    path = ROOT / relative
    try:
        payload = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return None
    return payload if isinstance(payload, dict) else None


def latest_journal() -> str:
    logs = ROOT / "logs"
    if not logs.is_dir():
        return ""
    candidates = [path for path in logs.rglob("events.jsonl") if path.is_file()]
    if not candidates:
        return ""
    return str(max(candidates, key=lambda path: path.stat().st_mtime))


def status_rows() -> list[tuple[str, str]]:
    raw = run_git("status", "--porcelain=v1")
    rows: list[tuple[str, str]] = []
    for line in raw.splitlines():
        if len(line) < 4:
            continue
        code = line[:2].strip() or "?"
        path = line[2:].lstrip()
        if " -> " in path:
            path = path.split(" -> ", 1)[1]
        rows.append((code, path))
    return rows


def reverse_for(code: str, path: str) -> str:
    if code == "D":
        return f"git restore --source=HEAD -- {path}"
    if code == "??":
        return f"Remove-Item -LiteralPath '{path}'"
    if code in {"A", "M", "AM", "MM"}:
        return f"git checkout -- {path}"
    return f"git checkout -- {path}"


def fill_line(text: str, prefix: str, value: str) -> str:
    pattern = re.compile(rf"^({re.escape(prefix)}).*$", re.MULTILINE)
    replacement = f"{prefix} {value}"
    return pattern.sub(lambda _: replacement, text)


def fill_changed_table(text: str, rows: list[tuple[str, str]]) -> str:
    marker = "| `path/to/file` |  |  |  |  |"
    if not rows:
        return text.replace(marker, "| _none_ |  |  |  |  |")
    body = "\n".join(
        f"| `{path}` |  |  |  | {reverse_for(code, path)} |"
        for code, path in rows
    )
    return text.replace(marker, body)


def build_identity() -> dict:
    progress = read_json("build/progress.json") or {}
    dev = read_json(".dev/state.json") or {}
    current = read_json("build/game/current.json") or {}
    ready = read_json("build/game/ready.json") or {}
    return {
        "dev_loop": {
            "build_authority": dev.get("build_authority", ""),
            "build_status": dev.get("build_status", ""),
            "latest_successful_build": dev.get("latest_successful_build", ""),
            "latest_successful_build_id": dev.get("latest_successful_build_id", ""),
            "latest_successful_source_generation": dev.get("latest_successful_source_generation"),
            "latest_build_stale": dev.get("latest_build_stale"),
        },
        "game_current": {
            key: current.get(key) for key in
            ("generation", "dll", "revision", "code_hash", "status", "build_mode")
            if key in current
        },
        "game_ready": {
            key: ready.get(key) for key in
            ("generation", "dll", "revision", "status", "published_utc")
            if key in ready
        },
        "progress": {
            key: progress.get(key) for key in
            ("phase", "result", "compiled", "skipped", "elapsed_s", "updated_utc")
            if key in progress
        },
    }


def fill_evidence(text: str, evidence: dict) -> str:
    replacements = {
        "- Branch:": evidence["branch"],
        "- Base commit:": evidence["base"],
        "- Final commit:": evidence["final"],
        "- Branch/base/final:": f'{evidence["branch"]} / {evidence["base"]} / {evidence["final"]}',
        "- Git status:": "captured below",
        "- Diff stat:": "captured below",
        "- Build identity:": json.dumps(evidence["build"], sort_keys=True),
        "- Generation:": evidence["generation"],
        "- Artifact:": evidence["artifact"],
        "- Journal path:": evidence["journal"],
        "- Session ID:": evidence["session_id"],
    }
    for prefix, value in replacements.items():
        text = fill_line(text, prefix, value)
    return text


def append_evidence_block(text: str, evidence: dict, include_diff: bool) -> str:
    lines = [
        "",
        "## Captured git state",
        "",
        "```text",
        evidence["status"],
        "```",
        "",
        "## Captured diff stat",
        "",
        "```text",
        evidence["diff_stat"],
        "```",
        "",
        "## Captured changed paths",
        "",
        "```text",
        evidence["name_status"],
        "```",
        "",
        "## Captured build identity",
        "",
        "```json",
        json.dumps(evidence["build"], indent=2, sort_keys=True),
        "```",
    ]
    if include_diff:
        lines.extend(["", "## Included diff", "", "```diff", evidence["diff"], "```"])
    return text.rstrip() + "\n" + "\n".join(lines) + "\n"


def write_changelog(args: argparse.Namespace, evidence: dict) -> Path:
    template = TEMPLATE.read_text(encoding="utf-8")
    title = args.title.strip() or "session"
    body = template
    body = body.replace("# Task", f"# {title}\n\n# Task", 1)
    body = fill_line(body, "- Task ID:", args.session_id)
    body = fill_line(body, "- Session ID:", args.session_id)
    body = fill_line(body, "- Owner:", args.owner)
    body = fill_line(body, "- Focus:", args.focus)
    body = fill_line(body, "- Summary:", args.summary)
    body = fill_line(body, "- Status:", args.status)
    body = fill_line(body, "- Date, time, timezone:", utc_now().isoformat(timespec="seconds"))
    body = fill_changed_table(body, status_rows())
    body = fill_evidence(body, evidence)
    body = append_evidence_block(body, evidence, args.include_diff)

    filename = f"{utc_now().strftime('%Y%m%d_%H%M%S')}-{slugify(title)}.md"
    target = CHANGELOG_ROOT / utc_now().strftime("%Y-%m-%d") / filename
    target.parent.mkdir(parents=True, exist_ok=True)
    suffix = 1
    while target.exists():
        target = target.with_name(f"{utc_now().strftime('%Y%m%d_%H%M%S')}-{slugify(title)}-{suffix}.md")
        suffix += 1
    target.write_text(body, encoding="utf-8")
    return target


def slugify(value: str) -> str:
    slug = re.sub(r"[^a-zA-Z0-9]+", "-", value.strip().lower()).strip("-")
    return slug or "session"


def check_one(args: argparse.Namespace) -> int:
    session_id = args.session_id or os.environ.get("MIMITA_SESSION_ID", "").strip()
    if not session_id:
        print("[CHANGELOG] --check requires --session-id or MIMITA_SESSION_ID", file=sys.stderr)
        return 2
    matches = []
    for path in CHANGELOG_ROOT.rglob("*.md"):
        try:
            text = path.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        if re.search(rf"^- Session ID:\s*{re.escape(session_id)}\s*$", text, re.MULTILINE):
            matches.append(path)
    if len(matches) != 1:
        print(f"[CHANGELOG] expected exactly one changelog for session {session_id}; found {len(matches)}",
              file=sys.stderr)
        for path in matches:
            print(f"  {path.relative_to(ROOT)}", file=sys.stderr)
        return 1
    print(f"[CHANGELOG] session={session_id} path={matches[0].relative_to(ROOT)}")
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Create or verify a MiMITA changelog")
    parser.add_argument("--title", default="session")
    parser.add_argument("--summary", default="")
    parser.add_argument("--owner", default="")
    parser.add_argument("--focus", default="")
    parser.add_argument("--status", default="in-progress")
    parser.add_argument("--session-id", default="")
    parser.add_argument("--check", action="store_true", help="require exactly one changelog for the session")
    parser.add_argument("--include-diff", action="store_true", help="include the full tracked diff")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args(argv)

    if args.check:
        return check_one(args)

    if not args.session_id:
        args.session_id = utc_now().strftime("session-%Y%m%d-%H%M%S")

    branch = run_git("rev-parse", "--abbrev-ref", "HEAD") or "unknown"
    final = run_git("rev-parse", "HEAD") or "unknown"
    base = (
        run_git("merge-base", "HEAD", "origin/main")
        or run_git("merge-base", "HEAD", "origin/master")
        or final
    )
    status = run_git("status", "--short") or "(clean)"
    diff_stat = run_git("diff", "--stat") or "(no unstaged diff)"
    name_status = run_git("diff", "--name-status") or "(no unstaged diff)"

    current = read_json("build/game/current.json") or {}
    ready = read_json("build/game/ready.json") or {}
    evidence = {
        "branch": branch,
        "base": base,
        "final": final,
        "status": status,
        "diff_stat": diff_stat,
        "name_status": name_status,
        "build": build_identity(),
        "generation": str(current.get("generation", ready.get("generation", ""))),
        "artifact": str(current.get("dll", ready.get("dll", ""))),
        "journal": latest_journal(),
        "session_id": args.session_id,
        "diff": run_git("diff") if args.include_diff else "",
    }

    if args.dry_run:
        print(json.dumps({key: value for key, value in evidence.items() if key != "diff"},
                         indent=2, sort_keys=True))
        return 0

    target = write_changelog(args, evidence)
    print(f"[CHANGELOG] wrote {target.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
