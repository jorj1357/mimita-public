#!/usr/bin/env python3
"""Report the current EXE cold-core target and reject game-owned EXE inputs."""
from __future__ import annotations
import argparse
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

ALLOWED_COLD_OWNERS = {"resident", "bootstrap", "launcher", "tool"}
LEGACY_MARKERS = ()
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
    parser = argparse.ArgumentParser(description="Validate the EXE cold-core target")
    parser.add_argument("--json", dest="json_path",
                        default="build/hot-reload/build-target-gate.json")
    parser.add_argument("--require-cold", action="store_true",
                        help="compatibility alias for strict cold validation")
    parser.add_argument("--report-only", action="store_true",
                        help="write the report but do not fail")
    parser.add_argument("--max-list", type=int, default=40)
    parser.add_argument("--link-inputs", default="",
                        help="actual build/link-inputs-<mode>.json file")
    parser.add_argument("--cold-trial",
                        default="build/hot-reload/cold-link-trial.json",
                        help="cold-link trial report used by the final gate")
    parser.add_argument("--require-cold-zero", action="store_true",
                        help="require the cold trial undefined symbol count to be zero")
    args = parser.parse_args(argv)

    manifest = json.loads((ROOT / "build/source-manifest.json").read_text(
        encoding="utf-8"))
    rows = manifest.get("sources", [])
    by_path = {str(row.get("path", "")).replace("\\", "/"): row for row in rows}
    link_inputs, link_error = load_link_inputs(args.link_inputs)
    if link_inputs is None:
        print(f"[BUILD TARGET] FAIL {link_error}", file=sys.stderr)
        return 1
    exe_sources = {
        str(entry.get("source", "")).replace("\\", "/")
        for entry in link_inputs.get("entries", [])
        if str(entry.get("source", "")).replace("\\", "/").startswith("src/")
    }
    game = sorted(path for path in exe_sources
                  if by_path.get(path, {}).get("owner") == "game")
    cold = sorted(path for path in exe_sources
                  if by_path.get(path, {}).get("owner") in ALLOWED_COLD_OWNERS)
    tests = sorted(path for path in exe_sources
                   if by_path.get(path, {}).get("owner") == "test")
    legacy = sorted(path for path in exe_sources if path in LEGACY_MARKERS)
    cold_trial_path = ROOT / args.cold_trial
    cold_undefined = None
    cold_trial_error = ""
    if cold_trial_path.is_file():
        try:
            cold_trial = json.loads(cold_trial_path.read_text(encoding="utf-8"))
            cold_undefined = int(cold_trial.get("undefined_symbol_count", -1))
            if cold_undefined < 0:
                cold_trial_error = "cold trial has no undefined_symbol_count"
                cold_undefined = None
        except (OSError, json.JSONDecodeError, TypeError, ValueError) as error:
            cold_trial_error = f"invalid {cold_trial_path}: {error}"
    elif args.require_cold_zero:
        cold_trial_error = f"missing {cold_trial_path}"
    cold_zero = cold_undefined == 0
    payload = {
        "schema_version": 1,
        "revision": manifest.get("revision", ""),
        "link_inputs": str(args.link_inputs or "latest"),
        "exe_sources": len(exe_sources),
        "cold_core_sources": len(cold),
        "test_sources": len(tests),
        "game_sources_in_exe": len(game),
        "legacy_bridges": legacy,
        "cold_undefined": cold_undefined,
        "cold_trial": str(cold_trial_path.relative_to(ROOT)).replace("\\", "/"),
        "cold_trial_error": cold_trial_error,
        "clean": not game and not tests and (
            cold_zero if args.require_cold_zero else True),
        "game_paths": game,
    }
    output = ROOT / args.json_path
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n",
                      encoding="utf-8")
    print(f"[BUILD TARGET] exe={len(exe_sources)} cold={len(cold)} "
          f"tests={len(tests)} game_in_exe={len(game)} "
          f"cold_undefined={cold_undefined}")
    if cold_trial_error:
        print(f"[BUILD TARGET] COLD_TRIAL {cold_trial_error}", file=sys.stderr)
    for path in game[:max(0, args.max_list)]:
        print(f"[BUILD TARGET] GAME_IN_EXE {path}")
    if len(game) > args.max_list:
        print(f"[BUILD TARGET] ... {len(game) - args.max_list} more")
    for path in legacy:
        print(f"[BUILD TARGET] LEGACY_BRIDGE {path}")
    print(f"[BUILD TARGET] {'PASS' if payload['clean'] else 'FAIL'} "
          f"report={output.relative_to(ROOT)}")
    if args.report_only:
        return 0
    return 0 if payload["clean"] else 1

if __name__ == "__main__":
    raise SystemExit(main())
