#!/usr/bin/env python3
"""Report compiler-derived source and header dependencies for one checkout."""

from __future__ import annotations

import argparse
import json
import re
import sys
from collections import Counter, defaultdict
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MANIFEST_PATH = ROOT / "build" / "source-manifest.json"
DEFAULT_REPORT = ROOT / "build" / "reports" / "dependency-graph.json"
SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".cxx"}
HEADER_SUFFIXES = {".h", ".hh", ".hpp", ".hxx", ".inl", ".ipp"}
COLD_OWNERS = {"resident", "bootstrap", "launcher"}
REPO_PREFIXES = ("src/",)
ALL_REPO_PREFIXES = ("src/", "include/", "external/", "tools/", "tests/")


def iso_mtime(path: Path) -> str:
    return datetime.fromtimestamp(path.stat().st_mtime, tz=timezone.utc).isoformat()


def relative(path: Path) -> str:
    root = ROOT.as_posix().rstrip("/")
    text = path.as_posix()
    if text.lower().startswith(root.lower() + "/"):
        return text[len(root) + 1 :].replace("//", "/")
    return ""


def normalize_token(token: str) -> str:
    return token.replace("\\", "/").strip()


def repo_relative_token(token: str) -> str:
    token = normalize_token(token)
    root = ROOT.as_posix().rstrip("/")
    if token.lower().startswith(root.lower() + "/"):
        token = token[len(root) + 1 :]
    elif token.startswith("/") or (len(token) > 2 and token[1] == ":" and token[2] == "/"):
        return ""
    return token.lstrip("./").replace("//", "/")


def parse_depfile(path: Path, prefixes: tuple[str, ...] = REPO_PREFIXES) -> tuple[str, list[str]] | None:
    try:
        text = path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return None
    text = re.sub(r"\\\r?\n", " ", text)
    match = re.match(r"^.*?\.o\s*:\s*(.*)$", text, re.S)
    if not match:
        return None
    target = ""
    dependencies: list[str] = []
    for token in match.group(1).split():
        normalized = repo_relative_token(token)
        if not normalized or normalized.endswith(":") or normalized.endswith("pch.h.gch"):
            continue
        if not normalized.startswith(prefixes):
            continue
        dependencies.append(normalized)
    return target, dependencies


def discover_depfiles(paths: list[str], scan_all: bool = False) -> list[Path]:
    if paths:
        result = []
        for value in paths:
            path = Path(value)
            if not path.is_absolute():
                path = ROOT / path
            if path.is_file():
                result.append(path)
            elif path.is_dir():
                result.extend(path.rglob("*.d"))
        return sorted(set(result))
    if scan_all:
        return sorted((ROOT / "build").rglob("*.d"))
    result: list[Path] = []
    game_objects = ROOT / "build" / "game" / "objects"
    if game_objects.is_dir():
        generations = [path for path in game_objects.iterdir() if path.is_dir()]
        if generations:
            newest = max(generations, key=lambda path: path.stat().st_mtime)
            result.extend(newest.glob("*.d"))
    for name in ("obj-debug", "obj-release"):
        directory = ROOT / "build" / name
        if directory.is_dir():
            result.extend(directory.glob("*.d"))
    return sorted(set(result))


def load_owners() -> dict[str, str]:
    try:
        manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return {}
    return {
        str(row.get("path", "")).replace("\\", "/"): str(row.get("owner", ""))
        for row in manifest.get("sources", [])
        if isinstance(row, dict)
    }


def load_revision() -> str:
    try:
        manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
        return str(manifest.get("revision", ""))
    except (OSError, json.JSONDecodeError):
        return ""


def classify_dependency(path: str, owners: dict[str, str]) -> tuple[str, bool]:
    owner = owners.get(path)
    if owner:
        return owner, False
    if path.startswith("src/host/abi/"):
        return "cold_abi", False
    if path.startswith("external/") or path.startswith("include/"):
        return "vendor", False
    stem = path.rsplit(".", 1)[0]
    for suffix in (".cpp", ".c", ".cc", ".cxx"):
        candidate = stem + suffix
        if candidate in owners:
            return owners[candidate], False
    if path.startswith("src/"):
        return "game", True
    return "unknown", True


def build_graph(depfiles: list[Path], owners: dict[str, str], prefixes: tuple[str, ...] = REPO_PREFIXES) -> dict:
    by_source: dict[str, dict] = {}
    for depfile in depfiles:
        parsed = parse_depfile(depfile, prefixes)
        if not parsed:
            continue
        _, dependencies = parsed
        source = next(
            (dep for dep in dependencies if Path(dep).suffix.lower() in SOURCE_SUFFIXES),
            "",
        )
        if not source:
            continue
        try:
            depfile_mtime = depfile.stat().st_mtime
        except OSError:
            continue
        current = by_source.get(source)
        if current and current["depfile_mtime"] >= depfile_mtime:
            continue
        by_source[source] = {
            "source": source,
            "owner": owners.get(source, "unknown"),
            "depfile": relative(depfile),
            "depfile_mtime": depfile_mtime,
            "depfile_time_utc": iso_mtime(depfile),
            "dependencies": sorted(set(dependencies)),
        }

    reverse: dict[str, set[str]] = defaultdict(set)
    for source, entry in by_source.items():
        for dependency in entry["dependencies"]:
            reverse[dependency].add(source)

    hot_cold_edges = []
    for source, entry in by_source.items():
        if entry["owner"] != "game" or not source.startswith("src/"):
            continue
        for dependency in entry["dependencies"]:
            owner, inferred = classify_dependency(dependency, owners)
            if owner in COLD_OWNERS or owner in {"cold_abi", "vendor"}:
                hot_cold_edges.append(
                    {
                        "source": source,
                        "source_owner": entry["owner"],
                        "dependency": dependency,
                        "dependency_owner": owner,
                        "owner_inferred": inferred,
                    }
                )

    stale = []
    for source, entry in by_source.items():
        source_path = ROOT / source
        try:
            if source_path.stat().st_mtime > entry["depfile_mtime"]:
                stale.append(source)
        except OSError:
            stale.append(source)

    fan_in = [
        {"path": path, "dependents": len(dependents)}
        for path, dependents in reverse.items()
        if Path(path).suffix.lower() in HEADER_SUFFIXES
    ]
    fan_in.sort(key=lambda row: (-row["dependents"], row["path"]))

    return {
        "revision": load_revision(),
        "depfiles": [relative(path) for path in depfiles],
        "sources": by_source,
        "reverse": {path: sorted(sources) for path, sources in reverse.items()},
        "hot_to_cold_edges": sorted(
            hot_cold_edges, key=lambda row: (row["source"], row["dependency"])
        ),
        "fan_in": fan_in,
        "stale_sources": sorted(stale),
        "counts": {
            "depfiles": len(depfiles),
            "sources": len(by_source),
            "headers": sum(
                1 for path in reverse if Path(path).suffix.lower() in HEADER_SUFFIXES
            ),
            "hot_to_cold_edges": len(hot_cold_edges),
            "stale_sources": len(stale),
            "by_owner": dict(Counter(entry["owner"] for entry in by_source.values())),
        },
    }


def transitive_dependents(graph: dict, target: str) -> list[str]:
    reverse = graph["reverse"]
    seen: set[str] = set()
    pending = [target]
    while pending:
        current = pending.pop()
        for dependent in reverse.get(current, []):
            if dependent not in seen:
                seen.add(dependent)
                pending.append(dependent)
    return sorted(seen)


def transitive_dependencies(graph: dict, source: str) -> set[str]:
    entries = graph["sources"]
    seen: set[str] = set()
    pending = [source]
    while pending:
        current = pending.pop()
        entry = entries.get(current)
        if not entry:
            continue
        for dependency in entry["dependencies"]:
            if dependency not in seen:
                seen.add(dependency)
                pending.append(dependency)
    return seen


def write_report(graph: dict, path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(graph, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def print_summary(graph: dict, top: int) -> None:
    counts = graph["counts"]
    print(
        f"[DEPENDENCY GRAPH] revision={graph['revision'] or 'unknown'} "
        f"depfiles={counts['depfiles']} sources={counts['sources']} "
        f"headers={counts['headers']} stale_sources={counts['stale_sources']}"
    )
    owners = ", ".join(
        f"{owner}={count}" for owner, count in sorted(counts["by_owner"].items())
    )
    print(f"[DEPENDENCY GRAPH] owners={owners}")
    if graph["stale_sources"]:
        print("[DEPENDENCY GRAPH] stale source list:")
        for source in graph["stale_sources"][:top]:
            print(f"  {source}")
    print(f"[DEPENDENCY GRAPH] top {top} headers by fan-in:")
    for row in graph["fan_in"][:top]:
        print(f"  {row['dependents']:4d} {row['path']}")
    print(f"[DEPENDENCY GRAPH] hot-to-cold direct edges={counts['hot_to_cold_edges']}:")
    for edge in graph["hot_to_cold_edges"][:top]:
        inferred = " inferred" if edge["owner_inferred"] else ""
        print(
            f"  {edge['source']} -> {edge['dependency']} "
            f"[{edge['dependency_owner']}{inferred}]"
        )
    if counts["hot_to_cold_edges"] > top:
        print(f"  ... {counts['hot_to_cold_edges'] - top} more")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("paths", nargs="*", help="dependency .d files or directories")
    parser.add_argument("--focus", default="", help="show direct dependents of a path")
    parser.add_argument("--dependents", default="", help="show transitive dependents of a path")
    parser.add_argument("--affected", default="", help="show sources affected by a changed path")
    parser.add_argument("--top", type=int, default=20, help="number of rows to print")
    parser.add_argument("--all", action="store_true",
                        help="scan every build .d file and include external/header dependencies")
    parser.add_argument("--json", default="", help="write the report to a custom path")
    parser.add_argument("--write", action="store_true", help="write the default JSON report")
    args = parser.parse_args(argv)

    depfiles = discover_depfiles(args.paths, args.all)
    if not depfiles:
        print("[DEPENDENCY GRAPH] no .d files found; compile first", file=sys.stderr)
        return 1

    graph = build_graph(depfiles, load_owners(), ALL_REPO_PREFIXES if args.all else REPO_PREFIXES)

    if args.focus:
        dependents = graph["reverse"].get(args.focus.replace("\\", "/"), [])
        print(f"[DEPENDENCY GRAPH] direct dependents of {args.focus}: {len(dependents)}")
        for source in dependents:
            print(f"  {source}")
        return 0

    if args.dependents:
        target = args.dependents.replace("\\", "/")
        dependents = transitive_dependents(graph, target)
        print(f"[DEPENDENCY GRAPH] transitive dependents of {target}: {len(dependents)}")
        for source in dependents:
            print(f"  {source}")
        return 0

    if args.affected:
        target = args.affected.replace("\\", "/")
        sources = transitive_dependents(graph, target)
        if target in graph["sources"]:
            sources = sorted(set(sources) | {target})
        print(f"[DEPENDENCY GRAPH] affected sources for {target}: {len(sources)}")
        for source in sources:
            print(f"  {source}")
        return 0

    print_summary(graph, max(1, args.top))
    output = Path(args.json) if args.json else (DEFAULT_REPORT if args.write else None)
    if output:
        if not output.is_absolute():
            output = ROOT / output
        write_report(graph, output)
        print(f"[DEPENDENCY GRAPH] report={relative(output)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
