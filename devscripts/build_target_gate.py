#!/usr/bin/env python3
"""Report the current EXE cold-core target and reject game-owned EXE inputs."""
from __future__ import annotations
import argparse
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from devscripts.module_ownership import exe_source_relpaths  # noqa: E402

ALLOWED_COLD_OWNERS = {"resident", "bootstrap", "launcher", "tool"}
LEGACY_MARKERS = (
    "src/host/game-gl-legacy.cpp",
    "src/host/game-file-legacy.cpp",
    "src/host/game-job-legacy.cpp",
    "src/host/game-socket-legacy.cpp",
    "src/host/game-state-legacy.cpp",
    "src/render/render-command-legacy.cpp",
)

def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Validate the EXE cold-core target")
    parser.add_argument("--json", dest="json_path",
                        default="build/hot-reload/build-target-gate.json")
    parser.add_argument("--require-cold", action="store_true",
                        help="fail if any game-owned source is in the EXE")
    parser.add_argument("--max-list", type=int, default=40)
    args = parser.parse_args(argv)

    manifest = json.loads((ROOT / "build/source-manifest.json").read_text(
        encoding="utf-8"))
    rows = manifest.get("sources", [])
    by_path = {str(row.get("path", "")).replace("\\", "/"): row for row in rows}
    exe_sources = set(exe_source_relpaths(ROOT))
    game = sorted(path for path in exe_sources
                  if by_path.get(path, {}).get("owner") == "game")
    cold = sorted(path for path in exe_sources
                  if by_path.get(path, {}).get("owner") in ALLOWED_COLD_OWNERS)
    tests = sorted(path for path in exe_sources
                   if by_path.get(path, {}).get("owner") == "test")
    legacy = sorted(path for path in exe_sources if path in LEGACY_MARKERS)
    payload = {
        "schema_version": 1,
        "revision": manifest.get("revision", ""),
        "exe_sources": len(exe_sources),
        "cold_core_sources": len(cold),
        "test_sources": len(tests),
        "game_sources_in_exe": len(game),
        "legacy_bridges": legacy,
        "clean": not game,
        "game_paths": game,
    }
    output = ROOT / args.json_path
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n",
                      encoding="utf-8")
    print(f"[BUILD TARGET] exe={len(exe_sources)} cold={len(cold)} "
          f"tests={len(tests)} game_in_exe={len(game)}")
    for path in game[:max(0, args.max_list)]:
        print(f"[BUILD TARGET] GAME_IN_EXE {path}")
    if len(game) > args.max_list:
        print(f"[BUILD TARGET] ... {len(game) - args.max_list} more")
    for path in legacy:
        print(f"[BUILD TARGET] LEGACY_BRIDGE {path}")
    print(f"[BUILD TARGET] {'PASS' if not game else 'FAIL'} "
          f"report={output.relative_to(ROOT)}")
    return 0 if not game or not args.require_cold else 1

if __name__ == "__main__":
    raise SystemExit(main())
