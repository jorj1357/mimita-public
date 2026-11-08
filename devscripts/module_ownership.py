#!/usr/bin/env python3
# 2026-11-08
# purpose
# Single source of truth for which source files belong to a reloadable module
# and which are compiled into the stable EXE host.
# Consumed by build.py, dev-loop.py, and module-build-worker.py so the catalog
# cannot drift from the actual EXE source inventory.
# Does NOT build anything and does NOT load modules.

from __future__ import annotations

import fnmatch
import json
from pathlib import Path

CATALOG_REL = Path("config/modules/modules.json")
ENTRY_SUFFIX = "-module.cpp"


def read_catalog(root: Path) -> dict:
    path = Path(root) / CATALOG_REL
    with open(path, "r", encoding="utf-8") as handle:
        catalog = json.load(handle)
    if not isinstance(catalog, dict):
        raise ValueError(f"{path}: catalog root must be an object")
    modules = catalog.get("modules")
    if not isinstance(modules, list):
        raise ValueError(f"{path}: 'modules' must be an array")
    return catalog


def module_entries(catalog: dict) -> list[dict]:
    return [module for module in catalog.get("modules", []) if isinstance(module, dict)]


def normalize(root: Path, path: Path) -> str:
    root = Path(root).resolve()
    return Path(path).resolve().relative_to(root).as_posix()


def expand_pattern(root: Path, pattern: str) -> list[Path]:
    root = Path(root).resolve()
    text = pattern.replace("\\", "/")
    if any(character in text for character in "*?["):
        matches = sorted(root.glob(text))
        return [path.resolve() for path in matches if path.is_file()]
    candidate = (root / text).resolve()
    return [candidate] if candidate.is_file() else []


def expand_patterns(root: Path, patterns) -> list[Path]:
    files: list[Path] = []
    for pattern in patterns or []:
        if isinstance(pattern, str) and pattern:
            files.extend(expand_pattern(root, pattern))
    return list(dict.fromkeys(files))


def module_sources(root: Path, module: dict) -> list[Path]:
    return expand_patterns(root, module.get("sources", []))


def module_headers(root: Path, module: dict) -> list[Path]:
    return expand_patterns(root, module.get("headers", []))


def module_source_owners(root: Path, catalog: dict) -> tuple[dict[str, str], list[str]]:
    owners: dict[str, str] = {}
    overlaps: list[str] = []
    for module in module_entries(catalog):
        name = module.get("name", "")
        for path in module_sources(root, module):
            relative = normalize(root, path)
            previous = owners.get(relative)
            if previous and previous != name:
                overlaps.append(f"{relative}: {previous} and {name}")
            owners[relative] = name
    return owners, overlaps


def shared_with_host(root: Path, catalog: dict) -> set[str]:
    result: set[str] = set()
    for module in module_entries(catalog):
        for path in expand_patterns(root, module.get("shared_with_host", [])):
            result.add(normalize(root, path))
    return result


def module_source_paths(root: Path, catalog: dict, *, watch_excluded: bool = False) -> set[Path]:
    paths: set[Path] = set()
    for module in module_entries(catalog):
        for path in module_sources(root, module):
            paths.add(path.resolve())
    if watch_excluded:
        keep: set[Path] = set()
        shared = {normalize(root, path) for path in paths}
        shared -= shared_with_host(root, catalog)
        for path in paths:
            if normalize(root, path) in shared:
                keep.add(path)
        return keep
    return paths


def exe_source_relpaths(root: Path, catalog: dict | None = None) -> list[str]:
    """Every src/**/*.cpp compiled into the EXE.

    Module sources are excluded. Sources listed in a module's
    ``shared_with_host`` migration list stay in the EXE until their host bridge
    replaces them; the audit reports those as pending extractions.
    """
    root = Path(root).resolve()
    catalog = catalog if catalog is not None else read_catalog(root)
    module_owned, _ = module_source_owners(root, catalog)
    shared = shared_with_host(root, catalog)
    result: list[str] = []
    src = root / "src"
    if not src.is_dir():
        return result
    for path in sorted(src.rglob("*.cpp")):
        relative = normalize(root, path)
        if path.name.endswith(ENTRY_SUFFIX):
            continue
        if relative in module_owned and relative not in shared:
            continue
        result.append(relative)
    return result


def audit(root: Path, catalog: dict | None = None) -> dict:
    root = Path(root).resolve()
    catalog = catalog if catalog is not None else read_catalog(root)
    errors: list[str] = []
    warnings: list[str] = []
    owners, overlaps = module_source_owners(root, catalog)
    errors.extend(f"source overlap: {item}" for item in overlaps)

    event_owners: dict[int, str] = {}
    capability_owners: dict[int, list[str]] = {}
    inventory: list[dict] = []
    for module in module_entries(catalog):
        name = module.get("name", "")
        if not name:
            errors.append("catalog entry without a name")
            continue
        sources = module_sources(root, module)
        headers = module_headers(root, module)
        declared = [pattern for pattern in module.get("sources", []) if isinstance(pattern, str)]
        if not sources:
            errors.append(f"{name}: no source files matched {declared}")
        missing = [pattern for pattern in declared
                   if not expand_pattern(root, pattern)]
        errors.extend(f"{name}: source pattern matched nothing: {pattern}"
                      for pattern in missing)
        entry_sources = [path for path in sources
                         if path.name == f"{name}{ENTRY_SUFFIX}"]
        if len(entry_sources) != 1:
            errors.append(f"{name}: expected exactly one {name}{ENTRY_SUFFIX} entry, "
                          f"found {len(entry_sources)}")
        for event in module.get("events", []):
            if not isinstance(event, int):
                continue
            previous = event_owners.get(event)
            if previous and previous != name:
                errors.append(f"event {event} owned by {previous} and {name}")
            event_owners[event] = name
        for capability in module.get("capabilities", []):
            if isinstance(capability, int):
                capability_owners.setdefault(capability, []).append(name)
        inventory.append({
            "name": name,
            "sources": len(sources),
            "headers": len(headers),
            "events": module.get("events", []),
            "capabilities": module.get("capabilities", []),
            "activation": module.get("activation", ""),
        })

    shared = shared_with_host(root, catalog)
    for relative in sorted(shared):
        if relative not in owners:
            warnings.append(f"shared_with_host entry is not a module source: {relative}")
        else:
            warnings.append(f"pending extraction, still compiled into EXE: {relative}")

    exe_sources = exe_source_relpaths(root, catalog)
    exe_set = set(exe_sources)
    for relative, module_name in sorted(owners.items()):
        if relative in exe_set:
            errors.append(f"{relative} is owned by {module_name} and compiled into the EXE")
        if relative.endswith(ENTRY_SUFFIX) and relative not in exe_set:
            continue

    return {
        "ok": not errors,
        "errors": errors,
        "warnings": warnings,
        "inventory": inventory,
        "module_sources": len(owners),
        "exe_sources": len(exe_sources),
        "shared_with_host": sorted(shared),
    }


def format_report(result: dict) -> str:
    lines: list[str] = []
    for item in result.get("inventory", []):
        lines.append(
            f"[MODULE AUDIT] {item['name']}: sources={item['sources']} "
            f"headers={item['headers']} events={item['events']} "
            f"capabilities={item['capabilities']} activation={item['activation']}")
    for warning in result.get("warnings", []):
        lines.append(f"[MODULE AUDIT] WARN {warning}")
    for error in result.get("errors", []):
        lines.append(f"[MODULE AUDIT] ERROR {error}")
    lines.append(
        f"[MODULE AUDIT] {'PASS' if result['ok'] else 'FAIL'} "
        f"module_sources={result['module_sources']} exe_sources={result['exe_sources']}")
    return "\n".join(lines)


if __name__ == "__main__":
    import sys

    root = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else Path.cwd().resolve()
    report = audit(root)
    print(format_report(report))
    raise SystemExit(0 if report["ok"] else 1)
