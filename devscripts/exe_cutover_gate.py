#!/usr/bin/env python3
"""Check whether any game-owned sources are still linked into the EXE.

The EXE cutover is complete only when the stable EXE contains host/cold-core
sources and no source owned by the replaceable game generation.
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))


def load_link_inputs(explicit: str = ""):
    if explicit:
        path = ROOT / explicit
    else:
        candidates = sorted(
            (ROOT / "build").glob("link-inputs-*.json"),
            key=lambda item: item.stat().st_mtime,
            reverse=True,
        )
        path = candidates[0] if candidates else None
    if path is None or not path.is_file():
        return None, "missing build/link-inputs-<mode>.json"
    try:
        payload = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        return None, f"invalid {path}: {error}"
    entries = payload.get("entries")
    if not isinstance(entries, list) or not entries:
        return None, f"{path} has no linker entries"
    return payload, ""


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Check EXE/game link cutover")
    parser.add_argument("--json", dest="json_path",
                        default="build/hot-reload/exe-cutover.json")
    parser.add_argument("--link-inputs", default="",
                        help="actual build/link-inputs-<mode>.json file")
    parser.add_argument("--report-only", action="store_true",
                        help="write the report without failing on linked game sources")
    parser.add_argument("--max-list", type=int, default=40)
    args = parser.parse_args(argv)

    manifest = json.loads((ROOT / "build/source-manifest.json").read_text(
        encoding="utf-8"))
    rows = manifest.get("sources", [])
    link_inputs, link_error = load_link_inputs(args.link_inputs)
    if link_inputs is None:
        print(f"[EXE CUTOVER] FAIL {link_error}", file=sys.stderr)
        return 1
    exe_sources = {
        str(entry.get("source", "")).replace("\\", "/")
        for entry in link_inputs.get("entries", [])
        if str(entry.get("source", "")).replace("\\", "/").startswith("src/")
    }
    linked_game = sorted(
        str(row.get("path", "")).replace("\\", "/")
        for row in rows
        if row.get("owner") == "game"
        and str(row.get("path", "")).replace("\\", "/") in exe_sources)
    owners: dict[str, int] = {}
    for row in rows:
        path = str(row.get("path", "")).replace("\\", "/")
        if path in linked_game:
            owner = str(row.get("owner", "unknown"))
            owners[owner] = owners.get(owner, 0) + 1

    payload = {
        "schema_version": 1,
        "revision": manifest.get("revision", ""),
        "link_inputs": str(args.link_inputs or "latest"),
        "exe_sources": len(exe_sources),
        "linked_game_sources": len(linked_game),
        "linked_game_owners": owners,
        "linked_game_paths": linked_game,
        "clean": not linked_game,
    }
    output = ROOT / args.json_path
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n",
                      encoding="utf-8")

    print(f"[EXE CUTOVER] exe_sources={len(exe_sources)} "
          f"linked_game_sources={len(linked_game)}")
    for path in linked_game[:max(0, args.max_list)]:
        print(f"[EXE CUTOVER] LINKED {path}")
    if len(linked_game) > args.max_list:
        print(f"[EXE CUTOVER] ... {len(linked_game) - args.max_list} more")
    print(f"[EXE CUTOVER] {'PASS' if not linked_game else 'FAIL'} "
          f"report={output.relative_to(ROOT)}")
    return 0 if args.report_only or not linked_game else 1


if __name__ == "__main__":
    raise SystemExit(main())
