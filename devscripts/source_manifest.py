#!/usr/bin/env python3
# 2026-11-12
# purpose
# Canonical source-owner and dependency manifest for the whole-game hot-reload
# cutover. Resolves exactly one owner per repository .cpp, captures one coherent
# source revision, reports added/removed/renamed sources and header dependents,
# and rejects unknown or duplicate ownership.
#
# This tool does NOT compile, link, or request a build. It only writes a manifest
# snapshot that the root dev-loop build authority may consume later.
#
# Owner categories:
#   game       ordinary game implementation; the set that becomes one game.dll
#   resident   stable host/loader/device/transport/logging/worker mechanism
#   bootstrap  process entry and orchestration
#   module:<n> existing subsystem ModuleV1 DLL source
#   launcher   standalone launcher process
#   tool       standalone development tool
#   test       test/self-test translation unit
#   vendor     unused or binary-only vendored source

from __future__ import annotations

import argparse
import hashlib
import json
import sys
from collections import Counter, defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

OWNERS_REL = Path("config/source-owners.json")
CATALOG_REL = Path("config/modules/modules.json")
MANIFEST_REL = Path("build/source-manifest.json")
INVENTORY_REL = Path("docs/handoffs/cpp-source-inventory.json")
EXCLUDE_DIRS = {".git", ".dev", "build", "dist", "node_modules", ".venv", "__pycache__"}
HEADER_SUFFIXES = (".h", ".hpp", ".hh", ".hxx", ".inl", ".ipp")
HEADER_ROOTS = ("src", "include")
ALLOWED_OWNERS = {"game", "resident", "bootstrap", "launcher", "tool", "test", "vendor"}


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def read_json(path: Path) -> dict:
    with open(path, "r", encoding="utf-8") as handle:
        return json.load(handle)


def expand_patterns(root: Path, patterns) -> list[Path]:
    files: list[Path] = []
    for pattern in patterns or []:
        if not isinstance(pattern, str) or not pattern:
            continue
        for path in sorted(root.glob(pattern)):
            if path.is_file():
                files.append(path.resolve())
    return list(dict.fromkeys(files))


def relative(root: Path, path: Path) -> str:
    return path.resolve().relative_to(root.resolve()).as_posix()


def enumerate_repository_cpp(root: Path) -> list[str]:
    result: list[str] = []
    for path in root.rglob("*.cpp"):
        parts = set(path.relative_to(root).parts)
        if parts & EXCLUDE_DIRS:
            continue
        result.append(relative(root, path))
    return sorted(result)


def enumerate_repository_headers(root: Path) -> list[str]:
    result: list[str] = []
    for directory in HEADER_ROOTS:
        base = root / directory
        if not base.is_dir():
            continue
        for path in base.rglob("*"):
            if not path.is_file() or path.suffix.lower() not in HEADER_SUFFIXES:
                continue
            parts = set(path.relative_to(root).parts)
            if parts & EXCLUDE_DIRS:
                continue
            result.append(relative(root, path))
    return sorted(set(result))


class OwnerResolver:
    """Resolve one owner per source with module > override > ordered-rule precedence."""

    def __init__(self, root: Path):
        self.root = Path(root).resolve()
        self.config = read_json(self.root / OWNERS_REL)
        self.errors: list[str] = []
        self.warnings: list[str] = []

        self.module_owners: dict[str, str] = {}
        try:
            from devscripts.module_ownership import module_source_owners, read_catalog
            catalog = read_catalog(self.root)
            owners, overlaps = module_source_owners(self.root, catalog)
            self.module_owners = owners
            self.errors.extend(f"module source overlap: {item}" for item in overlaps)
        except Exception as error:  # noqa: BLE001 - surface as a manifest error
            self.errors.append(f"module catalog unavailable: {error}")

        self.overrides: dict[str, dict] = {}
        for entry in self.config.get("overrides", []) or []:
            path = entry.get("path")
            owner = entry.get("owner")
            if not isinstance(path, str) or not isinstance(owner, str):
                self.errors.append(f"malformed override: {entry!r}")
                continue
            if path in self.overrides:
                self.errors.append(f"duplicate override: {path}")
                continue
            if not self._owner_allowed(owner):
                self.errors.append(f"override {path}: invalid owner {owner!r}")
            if not (self.root / path).is_file():
                self.errors.append(f"override path does not exist: {path}")
            previous = self.module_owners.get(path)
            if previous and previous != owner and owner != f"module:{previous}":
                self.errors.append(
                    f"override conflicts with module ownership: {path} is module:{previous}, override says {owner}")
            self.overrides[path] = entry

        self.rule_assignments: dict[str, tuple[str, str]] = {}
        for rule in self.config.get("rules", []) or []:
            owner = rule.get("owner")
            reason = rule.get("reason", "")
            if not self._owner_allowed(owner):
                self.errors.append(f"rule: invalid owner {owner!r}")
                continue
            for path in expand_patterns(self.root, rule.get("include", [])):
                rel = relative(self.root, path)
                if rel in self.module_owners or rel in self.overrides:
                    continue
                if rel in self.rule_assignments:
                    continue
                self.rule_assignments[rel] = (owner, reason)

        self.link_inputs = self._resolve_link_inputs()

    @staticmethod
    def _owner_allowed(owner: str) -> bool:
        return isinstance(owner, str) and (owner in ALLOWED_OWNERS or owner.startswith("module:"))

    def _resolve_link_inputs(self) -> dict:
        result: dict[str, list[dict]] = {}
        for group, entries in (self.config.get("link_inputs", {}) or {}).items():
            resolved: list[dict] = []
            for entry in entries or []:
                pattern = entry.get("path", "")
                paths = expand_patterns(self.root, [pattern])
                if not paths:
                    self.errors.append(f"link input matched nothing: {pattern}")
                for path in paths:
                    resolved.append({
                        "path": relative(self.root, path),
                        "owner": entry.get("owner", ""),
                        "reason": entry.get("reason", ""),
                    })
            result[group] = resolved
        return result

    def resolve(self, rel: str) -> tuple[str, str]:
        module = self.module_owners.get(rel)
        if module:
            return f"module:{module}", "module catalog"
        override = self.overrides.get(rel)
        if override:
            return override["owner"], override.get("reason", "explicit override")
        rule = self.rule_assignments.get(rel)
        if rule:
            return rule[0], rule[1]
        return "unresolved", "no rule or override matched"

    def config_hashes(self) -> dict[str, str]:
        files = [
            "build.py",
            "build_game_dll.py",
            "CMakeLists.txt",
            "config/modules/modules.json",
            "config/source-owners.json",
        ]
        result: dict[str, str] = {}
        for name in files:
            path = self.root / name
            result[name] = sha256_file(path) if path.is_file() else ""
        return result


def build_manifest(root: Path, resolver: OwnerResolver | None = None) -> dict:
    root = Path(root).resolve()
    resolver = resolver or OwnerResolver(root)
    sources: list[dict] = []
    for rel in enumerate_repository_cpp(root):
        owner, reason = resolver.resolve(rel)
        path = root / rel
        sources.append({
            "path": rel,
            "sha256": sha256_file(path),
            "owner": owner,
            "owner_reason": reason,
            "active_in_manifest": owner not in ("unresolved", "vendor"),
        })

    lines = [f"{item['path']}:{item['sha256']}" for item in sources]
    headers: list[dict] = []
    for rel in enumerate_repository_headers(root):
        headers.append({"path": rel, "sha256": sha256_file(root / rel)})
    lines.extend(f"header:{item['path']}:{item['sha256']}" for item in headers)
    config_hashes = resolver.config_hashes()
    lines.extend(f"config:{name}:{value}" for name, value in config_hashes.items())
    revision = hashlib.sha256("\n".join(lines).encode("utf-8")).hexdigest()

    counts = Counter(item["owner"] for item in sources)
    return {
        "schema_version": 1,
        "revision": revision,
        "config_hashes": config_hashes,
        "counts": {
            "total": len(sources),
            "headers": len(headers),
            "by_owner": dict(sorted(counts.items())),
        },
        "link_inputs": resolver.link_inputs,
        "sources": sources,
        "headers": headers,
        "resolver_errors": resolver.errors,
        "resolver_warnings": resolver.warnings,
    }


def validate(manifest: dict) -> tuple[list[str], list[str]]:
    errors: list[str] = list(manifest.get("resolver_errors", []))
    warnings: list[str] = list(manifest.get("resolver_warnings", []))
    for item in manifest["sources"]:
        if item["owner"] == "unresolved":
            errors.append(f"unknown ownership: {item['path']}")
    for group, entries in manifest.get("link_inputs", {}).items():
        for entry in entries:
            if entry["owner"] and not (
                    entry["owner"] in ALLOWED_OWNERS or entry["owner"].startswith("module:")):
                errors.append(f"{group}: invalid link-input owner for {entry['path']}: {entry['owner']}")
    seen: dict[str, str] = {}
    for item in manifest["sources"]:
        previous = seen.get(item["path"])
        if previous is not None:
            errors.append(f"duplicate source row: {item['path']}")
        seen[item["path"]] = item["owner"]
    return errors, warnings


def diff_manifests(old: dict, new: dict) -> dict:
    old_by_path = {item["path"]: item for item in old.get("sources", [])}
    new_by_path = {item["path"]: item for item in new.get("sources", [])}
    added = sorted(set(new_by_path) - set(old_by_path))
    removed = sorted(set(old_by_path) - set(new_by_path))
    changed = sorted(
        path for path in set(old_by_path) & set(new_by_path)
        if old_by_path[path]["sha256"] != new_by_path[path]["sha256"])

    renamed: list[dict] = []
    removed_by_hash: dict[str, list[str]] = defaultdict(list)
    for path in removed:
        removed_by_hash[old_by_path[path]["sha256"]].append(path)
    still_added: list[str] = []
    for path in added:
        digest = new_by_path[path]["sha256"]
        candidates = removed_by_hash.get(digest)
        if candidates:
            source = candidates.pop(0)
            renamed.append({"from": source, "to": path, "sha256": digest})
            removed.remove(source)
        else:
            still_added.append(path)
    added = still_added

    old_headers = {item["path"]: item for item in old.get("headers", [])}
    new_headers = {item["path"]: item for item in new.get("headers", [])}
    added_headers = sorted(set(new_headers) - set(old_headers))
    removed_headers = sorted(set(old_headers) - set(new_headers))
    changed_headers = sorted(
        path for path in set(old_headers) & set(new_headers)
        if old_headers[path]["sha256"] != new_headers[path]["sha256"])

    return {
        "old_revision": old.get("revision"),
        "new_revision": new.get("revision"),
        "added": added,
        "removed": removed,
        "renamed": renamed,
        "changed": changed,
        "added_headers": added_headers,
        "removed_headers": removed_headers,
        "changed_headers": changed_headers,
        "changed_owners": [
            {"path": path, "from": old_by_path[path]["owner"], "to": new_by_path[path]["owner"]}
            for path in sorted(set(old_by_path) & set(new_by_path))
            if old_by_path[path]["owner"] != new_by_path[path]["owner"]],
    }


def header_dependents(root: Path, header: str) -> list[dict]:
    inventory_path = root / INVENTORY_REL
    if not inventory_path.is_file():
        return []
    inventory = read_json(inventory_path)
    result: list[dict] = []
    for item in inventory.get("sources", []):
        deps = item.get("include_dependencies", {})
        direct = {entry.get("resolved_path") for entry in deps.get("direct", [])}
        transitive = set(deps.get("transitive_repository_headers_by_current_source_scan", []) or [])
        if header in direct or header in transitive:
            result.append({
                "source": item.get("path"),
                "direct": header in direct,
                "transitive": header in transitive,
            })
    return sorted(result, key=lambda entry: (not entry["direct"], entry["source"] or ""))


def format_manifest_summary(manifest: dict) -> str:
    counts = manifest["counts"]["by_owner"]
    lines = [
        f"[SOURCE MANIFEST] revision={manifest['revision']} total={manifest['counts']['total']}",
        "[SOURCE MANIFEST] owners=" + ", ".join(f"{name}={count}" for name, count in counts.items()),
    ]
    for entry in manifest.get("link_inputs", {}).get("archives", []):
        lines.append(f"[SOURCE MANIFEST] archive {entry['path']} owner={entry['owner']} ({entry['reason']})")
    return "\n".join(lines)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Canonical MiMITA source-owner manifest")
    parser.add_argument("--root", default=str(ROOT))
    parser.add_argument("--write", action="store_true", help="write build/source-manifest.json")
    parser.add_argument("--check", action="store_true", help="validate ownership; non-zero on error")
    parser.add_argument("--diff", nargs="+", metavar="MANIFEST",
                        help="diff an old manifest against a new one (or the current tree)")
    parser.add_argument("--dependents", metavar="HEADER",
                        help="list sources that include or transitively include a header")
    parser.add_argument("--json", action="store_true", help="emit machine-readable JSON")
    parser.add_argument("--out", default="", help="override manifest output path")
    args = parser.parse_args(argv)

    root = Path(args.root).resolve()

    if args.dependents:
        entries = header_dependents(root, args.dependents)
        if args.json:
            print(json.dumps({"header": args.dependents, "dependents": entries}, indent=2))
        else:
            print(f"[SOURCE MANIFEST] dependents of {args.dependents}: {len(entries)}")
            for entry in entries:
                kind = "direct" if entry["direct"] else "transitive"
                print(f"  {kind:10} {entry['source']}")
        return 0

    if args.diff:
        old_path = Path(args.diff[0])
        if not old_path.is_file():
            print(f"[SOURCE MANIFEST] old manifest not found: {old_path}", file=sys.stderr)
            return 1
        old = read_json(old_path)
        if len(args.diff) > 1:
            new = read_json(Path(args.diff[1]))
        else:
            new = build_manifest(root)
        report = diff_manifests(old, new)
        if args.json:
            print(json.dumps(report, indent=2))
        else:
            print(f"[SOURCE MANIFEST] diff {report['old_revision']} -> {report['new_revision']}")
            for key in ("added", "removed", "changed"):
                print(f"  {key}: {len(report[key])}")
                for path in report[key]:
                    print(f"    {path}")
            for entry in report["renamed"]:
                print(f"  renamed: {entry['from']} -> {entry['to']}")
            for entry in report["changed_owners"]:
                print(f"  owner: {entry['path']} {entry['from']} -> {entry['to']}")
            for key in ("added_headers", "removed_headers", "changed_headers"):
                print(f"  {key}: {len(report[key])}")
                for path in report[key]:
                    print(f"    {path}")
            for header in report["changed_headers"]:
                dependents = header_dependents(root, header)
                if dependents:
                    print(f"  rebuild dependents of {header}: {len(dependents)}")
                    for entry in dependents[:20]:
                        print(f"    {'direct' if entry['direct'] else 'transitive':10} {entry['source']}")
        return 0

    manifest = build_manifest(root)
    errors, warnings = validate(manifest)

    if args.write:
        output = Path(args.out).resolve() if args.out else root / MANIFEST_REL
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        if not args.json:
            print(f"[SOURCE MANIFEST] wrote {output}")

    if args.json:
        print(json.dumps({"manifest": manifest, "errors": errors, "warnings": warnings}, indent=2))
    else:
        print(format_manifest_summary(manifest))
        for warning in warnings:
            print(f"[SOURCE MANIFEST] WARN {warning}")
        for error in errors:
            print(f"[SOURCE MANIFEST] ERROR {error}")

    if args.check or args.write:
        return 0 if not errors else 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
