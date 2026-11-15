#!/usr/bin/env python3
"""Link the current EXE object set without game-owned objects.

This is a diagnostic link, not a replacement EXE. It uses the last normal
link response file, drops every game-owned object, links a separate trial
binary, and records undefined symbols for the first cold-core cutover slice.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from build_toolchain import compiler as resolve_compiler, runtime_path  # noqa: E402
from devscripts.build_authority import require_authority  # noqa: E402


def object_path(source: str, mode: str) -> Path:
    relative = source.replace("\\", "/")
    if relative.startswith("src/"):
        relative = str(Path(relative[4:]).with_suffix(""))
    relative = relative.replace("\\", "_").replace("/", "_")
    return ROOT / "build" / f"obj-{mode}" / f"{relative}.o"


def quoted_args(path: Path) -> list[str]:
    result: list[str] = []
    for line in path.read_text(encoding="utf-8").splitlines():
        text = line.strip()
        if not text:
            continue
        if text.startswith('"') and text.endswith('"'):
            text = text[1:-1].replace('\\"', '"')
        result.append(text)
    return result


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Cold-only EXE link trial")
    parser.add_argument("--link", action="store_true", help="run the trial link")
    parser.add_argument("--mode", choices=("debug", "release"),
                        help="object mode; inferred from link.rsp when omitted")
    parser.add_argument("--json", dest="json_path",
                        default="build/hot-reload/cold-link-trial.json")
    parser.add_argument("--max-symbols", type=int, default=500)
    args = parser.parse_args(argv)

    response = ROOT / "build" / "link.rsp"
    if not response.is_file():
        print("[COLD LINK] build/link.rsp missing; run a normal build first",
              file=sys.stderr)
        return 2

    link_args = quoted_args(response)
    mode = args.mode
    if mode is None:
        mode = "release" if any("obj-release" in arg for arg in link_args) else "debug"

    manifest = json.loads((ROOT / "build/source-manifest.json").read_text(
        encoding="utf-8"))
    game_sources = [str(row.get("path", "")).replace("\\", "/")
                    for row in manifest.get("sources", [])
                    if row.get("owner") == "game"]
    game_objects = {object_path(source, mode).resolve() for source in game_sources}
    game_objects_str = {str(path).replace("\\", "/").lower()
                        for path in game_objects}

    trial_dir = ROOT / "build" / "cold-link-trial"
    trial_dir.mkdir(parents=True, exist_ok=True)
    trial_exe = ROOT / "build" / "out" / "mimita-cold-trial.exe"
    trial_rsp = trial_dir / "link.rsp"
    filtered: list[str] = []
    removed: list[str] = []
    skip_output = False
    for argument in link_args:
        if skip_output:
            skip_output = False
            continue
        if argument == "-o":
            skip_output = True
            continue
        candidate = Path(argument)
        normalized = str(candidate).replace("\\", "/").lower()
        if candidate.is_absolute() and normalized.endswith(".o"):
            if normalized in game_objects_str:
                removed.append(argument)
                continue
        filtered.append(argument)
    filtered.extend(["-o", str(trial_exe)])
    trial_rsp.write_text(
        "\n".join(f'"{argument.replace(chr(34), chr(92) + chr(34))}"'
                  for argument in filtered) + "\n",
        encoding="utf-8")

    object_sources = {}
    source_owners = {}
    for row in manifest.get("sources", []):
        source = str(row.get("path", "")).replace("\\", "/")
        if not source:
            continue
        object_sources[object_path(source, mode).name] = source
        source_owners[source] = str(row.get("owner", "unknown"))

    undefined: list[str] = []
    referrers: dict[str, dict[str, set[str]]] = {}
    referrer_owners: dict[str, set[str]] = {}
    unknown_referrers: set[str] = set()
    link_return = 0
    link_output = ""
    if args.link:
        require_authority(ROOT, "exe", "exe_cold_link_trial")
        compiler = resolve_compiler()
        ccache = __import__("shutil").which("ccache")
        command = ([ccache, compiler] if ccache else [compiler])
        command.append("@" + str(trial_rsp))
        env = os.environ.copy()
        env["PATH"] = runtime_path(compiler) + os.pathsep + env.get("PATH", "")
        completed = subprocess.run(
            command,
            cwd=ROOT, text=True, stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, check=False, env=env)
        link_return = completed.returncode
        link_output = completed.stdout or ""
        current_object = ""
        object_pattern = re.compile(r"[\\/]([^\\/:]+\.o): in function")
        for line in link_output.splitlines():
            object_match = object_pattern.search(line)
            if object_match:
                current_object = object_match.group(1)
            if "undefined reference to" not in line:
                continue
            symbol = line.split("undefined reference to", 1)[1].strip()
            symbol = symbol.lstrip("`'").rstrip("`'").strip()
            if not symbol:
                continue
            if symbol not in undefined:
                undefined.append(symbol)
            source = object_sources.get(current_object, "")
            owner = source_owners.get(source, "unknown")
            if not source:
                unknown_referrers.add(current_object)
                owner = "unknown"
            referrer_owners.setdefault(symbol, set()).add(owner)
            referrers.setdefault(symbol, {}).setdefault(owner, set()).add(
                current_object or "unknown")

    owner_counts: dict[str, int] = {}
    combination_counts: dict[str, int] = {}
    for owners in referrer_owners.values():
        for owner in owners:
            owner_counts[owner] = owner_counts.get(owner, 0) + 1
        key = "+".join(sorted(owners))
        combination_counts[key] = combination_counts.get(key, 0) + 1
    undefined_referrers = [
        {
            "symbol": symbol,
            "owners": sorted(referrer_owners.get(symbol, set())),
            "objects": sorted(
                named
                for objects in referrers.get(symbol, {}).values()
                for named in objects
            ),
        }
        for symbol in undefined[:max(0, args.max_symbols)]
    ]
    payload = {
        "schema_version": 1,
        "revision": manifest.get("revision", ""),
        "mode": mode,
        "linked": bool(args.link and link_return == 0),
        "link_return": link_return,
        "trial_executable": str(trial_exe),
        "trial_response": str(trial_rsp),
        "game_sources": len(game_sources),
        "removed_objects": len(removed),
        "undefined_symbol_count": len(undefined),
        "undefined_symbols": undefined[:max(0, args.max_symbols)],
        "undefined_by_referrer_owner": dict(sorted(owner_counts.items())),
        "undefined_by_referrer_combination": dict(
            sorted(combination_counts.items())),
        "unknown_referrers": sorted(unknown_referrers),
        "undefined_referrers": undefined_referrers,
        "link_output": link_output,
    }
    output = ROOT / args.json_path
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n",
                      encoding="utf-8")
    print(f"[COLD LINK] mode={mode} game_sources={len(game_sources)} "
          f"removed_objects={len(removed)} undefined={len(undefined)}")
    for owner, count in sorted(owner_counts.items()):
        print(f"[COLD LINK] referrer={owner} unique_symbols={count}")
    for symbol in undefined[:max(0, args.max_symbols)]:
        print(f"[COLD LINK] UNDEFINED {symbol}")
    if args.link and link_return != 0:
        print(f"[COLD LINK] expected trial failure rc={link_return} "
              f"report={output.relative_to(ROOT)}")
    else:
        print(f"[COLD LINK] {'LINKED' if link_return == 0 else 'REPORT'} "
              f"report={output.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())











