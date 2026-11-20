#!/usr/bin/env python3
"""Read-only tech-debt audit for duplicate paths, hardcoding, and waste."""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE_ROOTS = [ROOT / "src", ROOT / "devscripts"]
TEXT_SUFFIXES = {".cpp", ".cc", ".cxx", ".h", ".hpp", ".hh", ".c", ".py",
                 ".json", ".md", ".ps1", ".cmd"}


def _files():
    for root in SOURCE_ROOTS:
        if not root.is_dir():
            continue
        for path in root.rglob("*"):
            if path.is_file() and path.suffix.lower() in TEXT_SUFFIXES:
                try:
                    if path.stat().st_size > 4_000_000:
                        continue
                except OSError:
                    continue
                yield path


def _read(path: Path) -> str:
    try:
        return path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return ""


def _relative(path: Path) -> str:
    try:
        return path.relative_to(ROOT).as_posix()
    except ValueError:
        return str(path)


def scan(limit: int = 200) -> dict:
    special_patterns = [
        ("spyknife_hitclaim", re.compile(r"PACKET_SPYKNIFE_HIT_CLAIM|handleSpyKnifeHitClaim")),
        ("godball_hitclaim", re.compile(r"PACKET_GODBALL_HIT_CLAIM|handleGodballHitClaim")),
        ("npc_damage_request", re.compile(r"PACKET_NPC_DAMAGE_REQUEST|handleNpcDamageRequest")),
        ("generic_melee_batch", re.compile(r"handleGenericMeleeContactBatch")),
    ]
    direct_health = re.compile(r"\b(?:player|npc|target|victim|attacker)\.health\s*[+\-]?=")
    weapon_branch = re.compile(
        r"\b(?:swordsword|mace|chainsaw|spyknife|godball|deathball)\b")
    findings = []
    files = list(_files())
    for path in files:
        rel = _relative(path)
        text = _read(path)
        for line_no, line in enumerate(text.splitlines(), 1):
            for kind, pattern in special_patterns:
                if pattern.search(line):
                    findings.append({
                        "kind": "special_contact_path",
                        "path": rel,
                        "line": line_no,
                        "evidence": line.strip()[:240],
                        "why": "A weapon-specific contact path/packet is a second owner beside the shared contact contract.",
                        "suggested_fix": "Convert this path into one ContactClaim kind/purpose/damage-kind route; keep a legacy adapter only during reviewed migration.",
                    })
            if direct_health.search(line) and rel != "src/network/server-damage.cpp":
                findings.append({
                    "kind": "direct_health_write",
                    "path": rel,
                    "line": line_no,
                    "evidence": line.strip()[:240],
                    "why": "Health must have one owner; a direct write creates a second damage path.",
                    "suggested_fix": "Route through applyActorDamage or document an explicit lifecycle/test exemption.",
                })
            if rel.endswith((".cpp", ".h")) and weapon_branch.search(line) and (
                    "contact" in rel or "damage" in rel or rel.endswith("server-damage.cpp")):
                findings.append({
                    "kind": "weapon_name_branch",
                    "path": rel,
                    "line": line_no,
                    "evidence": line.strip()[:240],
                    "why": "Shared contact/damage code should not branch on a weapon name; policy belongs in config/data.",
                    "suggested_fix": "Move the policy to the weapon/contact profile and dispatch on generic flags/kind.",
                })
            if "TODO" in line or "FIXME" in line:
                findings.append({
                    "kind": "stale_todo",
                    "path": rel,
                    "line": line_no,
                    "evidence": line.strip()[:240],
                    "why": "A TODO without an issue/owner can hide unfinished or obsolete work.",
                    "suggested_fix": "Link it to an issue with owner/acceptance, or remove it if obsolete.",
                })
    for path in files:
        rel = _relative(path)
        try:
            size = path.stat().st_size
        except OSError:
            continue
        if size > 250_000 and not rel.startswith(("src/host/generated/", "devscripts/blender-ai/")):
            findings.append({
                "kind": "large_file",
                "path": rel,
                "line": 0,
                "evidence": f"{size} bytes",
                "why": "Large hand-maintained files increase review, build, and context cost.",
                "suggested_fix": "Split only with measured ownership/fan-out and build impact; otherwise link the file to a review issue.",
            })
    by_kind = {}
    for item in findings:
        by_kind.setdefault(item["kind"], []).append(item)
    for items in by_kind.values():
        items.sort(key=lambda item: (item["path"], item["line"]))
    return {
        "root": str(ROOT),
        "files_scanned": len(files),
        "finding_count": len(findings),
        "by_kind": {kind: len(items) for kind, items in sorted(by_kind.items())},
        "findings": findings[:limit],
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--json", action="store_true")
    parser.add_argument("--limit", type=int, default=200)
    args = parser.parse_args()
    result = scan(limit=max(1, args.limit))
    if args.json:
        print(json.dumps(result, indent=2, sort_keys=True))
    else:
        print(f"files_scanned={result['files_scanned']} findings={result['finding_count']}")
        for kind, count in result["by_kind"].items():
            print(f"{kind}: {count}")
        for item in result["findings"][:40]:
            print(f"[{item['kind']}] {item['path']}:{item['line']} {item['evidence']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
