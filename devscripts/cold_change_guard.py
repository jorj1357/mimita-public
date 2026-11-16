#!/usr/bin/env python3
"""Classify changed files as hot, cold, module, tooling, or unknown."""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MANIFEST_PATH = ROOT / "build" / "source-manifest.json"

COLD_OWNERS = {"resident", "bootstrap", "launcher"}
IGNORE_PREFIXES = ("tests/", "external/", "docs/", "logs/")
TOOLING_PATHS = {
    "build.py",
    "config/source-owners.json",
    "config/modules/modules.json",
    "devscripts/game_build_worker.py",
    "devscripts/module_ownership.py",
    "devscripts/build_target_gate.py",
    "devscripts/exe_cutover_gate.py",
    "devscripts/exe_cold_link_trial.py",
    "devscripts/hot_client_gate.py",
    "devscripts/cap_render_gate.py",
    "devscripts/hot_reload_status.py",
    "devscripts/cold_change_guard.py",
}


def git_lines(args: list[str]) -> list[str]:
    result = subprocess.run(
        ["git", *args], cwd=ROOT, text=True, capture_output=True, check=False
    )
    if result.returncode != 0:
        return []
    return [line.strip() for line in result.stdout.splitlines() if line.strip()]


def changed_paths() -> list[str]:
    paths = []
    paths.extend(git_lines(["diff", "--name-only", "--cached"]))
    paths.extend(git_lines(["diff", "--name-only"]))
    paths.extend(git_lines(["ls-files", "--others", "--exclude-standard"]))
    return sorted(dict.fromkeys(path.replace("\\", "/") for path in paths))


def load_manifest() -> dict:
    try:
        return json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise RuntimeError(f"cannot read {MANIFEST_PATH}: {error}") from error


def classify(path: str, owners: dict[str, str]) -> tuple[str, str]:
    if path in TOOLING_PATHS or path.startswith("devscripts/"):
        return "TOOLING", "development tooling"
    if path.startswith(IGNORE_PREFIXES):
        return "IGNORE", "documentation, log, test, or vendored path"
    if path.startswith("config/"):
        return "CONFIG", "configuration/data; hotness depends on its owner"
    if path.startswith("src/host/abi/"):
        return "COLD_ABI", "host/game ABI header"
    if path.endswith((".h", ".hpp", ".hh", ".hxx", ".inl", ".ipp")):
        if path.startswith("src/"):
            return "HEADER_REVIEW", "header under src; check hot/cold dependents"
        return "NON_PRODUCT", "external/header-only path"
    if path.endswith(".cpp") and path.startswith("src/"):
        owner = owners.get(path)
        if not owner:
            return "UNKNOWN", "no owner in source manifest"
        if owner == "game":
            return "HOT", "game-owned source"
        if owner in COLD_OWNERS:
            return "COLD", f"{owner}-owned source"
        if owner.startswith("module:"):
            return "MODULE_DLL", f"{owner} module source"
        if owner in {"test", "vendor"}:
            return "NON_PRODUCT", f"{owner} source"
        if owner == "tool":
            return "NON_PRODUCT", "standalone tool source"
        return "UNKNOWN", f"unclassified owner: {owner}"
    return "IGNORE", "not a C++ source or tracked build input"


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("paths", nargs="*", help="explicit paths to check")
    parser.add_argument("--report-only", action="store_true",
                        help="always exit 0 and only report findings")
    parser.add_argument("--json", default="",
                        help="write the full report to this path")
    args = parser.parse_args(argv)

    try:
        manifest = load_manifest()
    except RuntimeError as error:
        print(f"[COLD GUARD] ERROR {error}", file=sys.stderr)
        return 2

    owners = {
        str(row.get("path", "")).replace("\\", "/"): str(row.get("owner", ""))
        for row in manifest.get("sources", [])
        if isinstance(row, dict)
    }
    paths = [path.replace("\\", "/") for path in args.paths] or changed_paths()
    findings = []
    for path in paths:
        classification, reason = classify(path, owners)
        if classification == "IGNORE":
            continue
        findings.append({
            "path": path,
            "classification": classification,
            "reason": reason,
        })

    counts: dict[str, int] = {}
    for finding in findings:
        key = finding["classification"]
        counts[key] = counts.get(key, 0) + 1

    for finding in findings:
        print(
            f"[COLD GUARD] {finding['classification']:<13} "
            f"{finding['path']} ({finding['reason']})"
        )
    if not findings:
        print("[COLD GUARD] no relevant changed files")
    print(
        "[COLD GUARD] summary "
        + " ".join(f"{key}={value}" for key, value in sorted(counts.items()))
    )

    blocking = {"COLD", "COLD_ABI", "HEADER_REVIEW", "UNKNOWN"}
    failed = any(finding["classification"] in blocking for finding in findings)
    if args.json:
        output = Path(args.json)
        if not output.is_absolute():
            output = ROOT / output
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(
            json.dumps(
                {
                    "manifest": MANIFEST_PATH.relative_to(ROOT).as_posix(),
                    "findings": findings,
                    "counts": counts,
                    "blocking": sorted(blocking),
                    "failed": failed,
                },
                indent=2,
                sort_keys=True,
            )
            + "\n",
            encoding="utf-8",
        )
        print(f"[COLD GUARD] report={output.relative_to(ROOT).as_posix()}")
    if failed and not args.report_only:
        print("[COLD GUARD] FAIL cold or unknown change requires classification",
              file=sys.stderr)
        return 1
    print("[COLD GUARD] PASS hot/config/tooling-only or report-only")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
