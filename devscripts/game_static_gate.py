#!/usr/bin/env python3
# 2026-11-12
# purpose
# Static gate for game-owned sources: reject raw thread/GL/socket/file-handle
# patterns that bypass the resident barrier capabilities. Read-only.
#
# This is the gate that lets per-file runtime proof be zero: the barrier owns
# lifetime, and a bypass is a build defect, not an unproven row.

from __future__ import annotations

import argparse
import json
import re
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

GATE_REL = Path("config/game-static-gate.json")
MANIFEST_REL = Path("build/source-manifest.json")
REPORT_REL = Path("docs/handoffs/cpp-static-gate-20261112.md")


def load_manifest() -> dict:
    path = ROOT / MANIFEST_REL
    if path.is_file():
        return json.loads(path.read_text(encoding="utf-8"))
    from devscripts.source_manifest import build_manifest
    return build_manifest(ROOT)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Static gate for game-owned sources")
    parser.add_argument("--report", default=str(REPORT_REL))
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args(argv)

    gate = json.loads((ROOT / GATE_REL).read_text(encoding="utf-8"))
    banned = {
        name: {
            "regex": re.compile(entry["pattern"]),
            "capability": entry.get("capability", ""),
            "reason": entry.get("reason", ""),
        }
        for name, entry in gate.get("banned", {}).items()
    }
    allowed = {entry["path"] for entry in gate.get("allowed", [])}

    manifest = load_manifest()
    game_sources = [item for item in manifest.get("sources", [])
                    if item.get("owner") == "game"]

    violations: list[dict] = []
    for item in game_sources:
        rel = item["path"]
        if rel in allowed:
            continue
        text = (ROOT / rel).read_text(encoding="utf-8", errors="replace")
        for index, line in enumerate(text.splitlines(), start=1):
            for name, rule in banned.items():
                if rule["regex"].search(line):
                    violations.append({
                        "path": rel,
                        "line": index,
                        "rule": name,
                        "capability": rule["capability"],
                        "reason": rule["reason"],
                        "snippet": line.strip()[:160],
                    })

    counts = Counter(item["rule"] for item in violations)
    files = sorted({item["path"] for item in violations})

    if args.json:
        print(json.dumps({"violations": violations, "counts": dict(counts),
                          "files": files}, indent=2))
    else:
        print(f"[STATIC GATE] game_sources={len(game_sources)} "
              f"violations={len(violations)} files={len(files)}")
        for rule, count in counts.most_common():
            print(f"[STATIC GATE] {rule}={count}")

    report = [
        "# Game static gate (2026-11-12)",
        "",
        "**Status:** static scan of game-owned sources for raw lifetime patterns that",
        "must use a resident capability instead. This is not a semantic review and not",
        "runtime evidence.",
        "",
        f"- Game sources scanned: {len(game_sources)}",
        f"- Violations: {len(violations)}",
        f"- Files with violations: {len(files)}",
        "",
        "## Rule counts",
        "",
        "| Rule | Capability | Violations |",
        "|---|---|---:|",
    ]
    for name, rule in banned.items():
        report.append(f"| {name} | {rule['capability']} | {counts.get(name, 0)} |")
    report.extend([
        "",
        "## Files",
        "",
        "| File | Rule | Line | Snippet |",
        "|---|---|---:|---|",
    ])
    for item in violations:
        report.append(
            f"| `{item['path']}` | {item['rule']} | {item['line']} | `{item['snippet']}` |")
    (ROOT / args.report).write_text("\n".join(report) + "\n", encoding="utf-8")
    print(f"[STATIC GATE] report={args.report}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
