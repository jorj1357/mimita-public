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
import os
import sys
import time
from collections import Counter, defaultdict
from pathlib import Path
from typing import Callable

ROOT = Path(__file__).resolve().parent.parent
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

OWNERS_REL = Path("config/source-owners.json")
CATALOG_REL = Path("config/modules/modules.json")
MANIFEST_REL = Path("build/source-manifest.json")
INVENTORY_REL = Path("docs/handoffs/cpp-source-inventory.json")
EXCLUDE_DIRS = {
    ".git", ".dev", "build", "dist", "node_modules", ".venv", "__pycache__",
    ".codex", ".vscode", "assets", "Characters", "content",
    "coordinator-server", "deploy", "docs", "installer", "logs", "manifests",
    "replays", "shaders", "website",
}
DEEP_EXCLUDE_DIRS = {
    ".git", "build", "dist", "node_modules", ".venv", "__pycache__",
    ".codex", ".vscode", "logs",
}
HEADER_EXCLUDE_DIRS = {
    ".git", ".dev", "build", "dist", "node_modules", ".venv", "__pycache__",
}
HEADER_SUFFIXES = (".h", ".hpp", ".hh", ".hxx", ".inl", ".ipp")
HEADER_ROOTS = ("src", "include")
ALLOWED_OWNERS = {"game", "resident", "bootstrap", "launcher", "tool", "test", "vendor"}
_INVENTORY_CACHE: dict[tuple[Path, str], tuple[tuple[str, ...], dict[str, int]]] = {}


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def stamped_sha256(path: Path, attempts: int = 3) -> tuple[str, int, int]:
    """Hash a file and return the stat observed around that read.

    A build may reuse the stamp only for the attempt whose snapshot produced
    it. If the file changes while it is being hashed, retry so a torn read is
    never paired with a post-change stamp.
    """
    for _ in range(max(1, attempts)):
        before = path.stat()
        digest = sha256_file(path)
        after = path.stat()
        if (before.st_size, before.st_mtime_ns) == (after.st_size, after.st_mtime_ns):
            return digest, after.st_size, after.st_mtime_ns
    return sha256_file(path), after.st_size, after.st_mtime_ns


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
    root = Path(root).resolve()
    path = Path(path)
    # Paths produced by os.walk/rglob are already lexically rooted here. Avoid
    # resolving each ordinary file through Windows final-path APIs; preserve
    # the canonical containment check for symlinks and external paths.
    if path.is_relative_to(root) and not path.is_symlink():
        return path.relative_to(root).as_posix()
    return path.resolve().relative_to(root).as_posix()


def _directory_stamp_changes(root: Path, stamps: dict[str, int]) -> list[str]:
    changed: list[str] = []
    for relative_dir, expected in stamps.items():
        try:
            current = (root / relative_dir).stat().st_mtime_ns
        except OSError:
            changed.append(relative_dir)
            continue
        if current != expected:
            changed.append(relative_dir)
    return changed


def _scan_cpp_inventory(root: Path, start: Path | None = None
                        ) -> tuple[list[str], dict[str, int]]:
    result: list[str] = []
    stamps: dict[str, int] = {}
    for directory, dirs, files in os.walk(start or root):
        base = Path(directory)
        if base == root:
            dirs[:] = [name for name in dirs if name not in EXCLUDE_DIRS]
        else:
            dirs[:] = [name for name in dirs if name not in DEEP_EXCLUDE_DIRS]
        try:
            stamps[base.relative_to(root).as_posix()] = base.stat().st_mtime_ns
        except OSError:
            pass
        for name in files:
            if name.endswith(".cpp"):
                result.append(relative(root, base / name))
    return sorted(result), stamps


def _scan_header_inventory(root: Path, start: Path | None = None
                            ) -> tuple[list[str], dict[str, int]]:
    result: list[str] = []
    if start is None:
        try:
            stamps: dict[str, int] = {".": root.stat().st_mtime_ns}
        except OSError:
            stamps = {}
        roots = [root / directory for directory in HEADER_ROOTS]
    else:
        stamps = {}
        roots = [start]
    for base in roots:
        if not base.is_dir():
            continue
        for current, dirs, files in os.walk(base):
            current_path = Path(current)
            dirs[:] = [name for name in dirs if name not in HEADER_EXCLUDE_DIRS]
            try:
                stamps[current_path.relative_to(root).as_posix()] = current_path.stat().st_mtime_ns
            except OSError:
                pass
            for name in files:
                path = current_path / name
                if path.suffix.lower() in HEADER_SUFFIXES:
                    result.append(relative(root, path))
    return sorted(set(result)), stamps


def _incremental_inventory(root: Path, kind: str, cached_paths: tuple[str, ...],
                           cached_stamps: dict[str, int],
                           changed_directories: list[str]
                           ) -> tuple[list[str], dict[str, int], int] | None:
    """Refresh changed directory subtrees without walking the whole source tree."""
    if "." in changed_directories:
        return None
    allowed_roots = set(HEADER_ROOTS) if kind == "headers" else None
    starts: list[tuple[str, Path]] = []
    for value in sorted(changed_directories,
                        key=lambda item: (item.count("/"), item)):
        relative_dir = Path(value)
        if allowed_roots is not None and not any(
                relative_dir.parts[:1] == (allowed,) for allowed in allowed_roots):
            continue
        candidate = root / relative_dir
        if not candidate.is_dir():
            candidate = candidate.parent
        if not candidate.is_dir():
            return None
        try:
            relative_candidate = candidate.relative_to(root).as_posix()
        except ValueError:
            return None
        if relative_candidate == ".":
            return None
        if allowed_roots is not None and not any(
                Path(relative_candidate).parts[:1] == (allowed,)
                for allowed in allowed_roots):
            continue
        if any(relative_candidate == parent or
               relative_candidate.startswith(parent + "/")
               for parent, _ in starts):
            continue
        starts.append((relative_candidate, candidate))

    # A large or unrelated invalidation is cheaper and simpler to rebuild in
    # one pass. A small set of changed source directories is the common case.
    if not starts or len(starts) > 32:
        return None

    paths = set(cached_paths)
    stamps = dict(cached_stamps)
    rescanned_directories = 0
    for prefix, start in starts:
        paths = {path for path in paths
                 if path != prefix and not path.startswith(prefix + "/")}
        stamps = {path: stamp for path, stamp in stamps.items()
                  if path != prefix and not path.startswith(prefix + "/")}
        scanner = _scan_cpp_inventory if kind == "cpp" else _scan_header_inventory
        subtree_paths, subtree_stamps = scanner(root, start)
        paths.update(subtree_paths)
        stamps.update(subtree_stamps)
        rescanned_directories += len(subtree_stamps)
    return sorted(paths), stamps, rescanned_directories


def _cached_inventory(root: Path, kind: str) -> tuple[list[str], dict]:
    key = (root, kind)
    cached = _INVENTORY_CACHE.get(key)
    changed_directories = (_directory_stamp_changes(root, cached[1])
                           if cached is not None else [])
    if cached is not None and not changed_directories:
        return list(cached[0]), {"cache_hit": True}
    scanner = _scan_cpp_inventory if kind == "cpp" else _scan_header_inventory
    incremental = (_incremental_inventory(root, kind, cached[0], cached[1],
                                           changed_directories)
                   if cached is not None and changed_directories else None)
    if incremental is None:
        result, stamps = scanner(root)
        rescanned_directories = len(stamps)
        incremental_used = False
    else:
        result, stamps, rescanned_directories = incremental
        incremental_used = True
    # Validate the directory tree after enumerating it. If a file or directory
    # was added/removed during the scan, return the scan result but do not trust
    # it as a reusable inventory on the next build.
    stable = not _directory_stamp_changes(root, stamps)
    if stable:
        _INVENTORY_CACHE[key] = (tuple(result), stamps)
    else:
        _INVENTORY_CACHE.pop(key, None)
    return result, {
        "cache_hit": False,
        "invalidated_reason": ("directory_stamp_changed" if changed_directories
                               else "cold_or_unstable_scan"),
        "changed_directory_count": len(changed_directories),
        "changed_directories": changed_directories[:32],
        "changed_directories_truncated": len(changed_directories) > 32,
        "rescanned_directory_count": rescanned_directories,
        "incremental": incremental_used,
        "inventory_stable": stable,
    }


def enumerate_repository_cpp(root: Path) -> list[str]:
    return _cached_inventory(Path(root).resolve(), "cpp")[0]


def enumerate_repository_headers(root: Path) -> list[str]:
    return _cached_inventory(Path(root).resolve(), "headers")[0]


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


_RESOLVER_CACHE: dict[Path, tuple[tuple, OwnerResolver]] = {}


def _owner_resolver_signature(root: Path, cpp_paths: list[str]) -> tuple:
    """Fingerprint every input that can change source ownership resolution."""
    owner_path = root / OWNERS_REL
    catalog_path = root / CATALOG_REL
    owner_hash = sha256_file(owner_path) if owner_path.is_file() else ""
    catalog_hash = sha256_file(catalog_path) if catalog_path.is_file() else ""
    try:
        owner_config = read_json(owner_path)
    except (OSError, json.JSONDecodeError):
        owner_config = {}

    override_state = tuple(
        (str(entry.get("path", "")),
         (root / str(entry.get("path", ""))).is_file())
        for entry in owner_config.get("overrides", []) or []
        if isinstance(entry, dict)
    )
    link_input_paths: list[str] = []
    for entries in (owner_config.get("link_inputs", {}) or {}).values():
        for entry in entries or []:
            if not isinstance(entry, dict):
                continue
            link_input_paths.extend(
                relative(root, path)
                for path in expand_patterns(root, [entry.get("path", "")])
            )
    return (owner_hash, catalog_hash, tuple(cpp_paths), override_state,
            tuple(sorted(link_input_paths)))


def _cached_owner_resolver(root: Path, cpp_paths: list[str]) -> tuple[OwnerResolver, bool]:
    signature = _owner_resolver_signature(root, cpp_paths)
    cached = _RESOLVER_CACHE.get(root)
    if cached is not None and cached[0] == signature:
        return cached[1], True
    resolver = OwnerResolver(root)
    _RESOLVER_CACHE[root] = (signature, resolver)
    return resolver, False


def build_manifest(root: Path, resolver: OwnerResolver | None = None,
                   hash_cache: dict | None = None,
                   progress_callback: Callable[[dict], None] | None = None) -> dict:
    def report(name: str, state: str, started: float, **fields) -> None:
        if progress_callback is None:
            return
        event = {
            "subphase": name,
            "state": state,
            "elapsed_ms": round((time.perf_counter() - started) * 1000.0, 3),
            **fields,
        }
        try:
            progress_callback(event)
        except Exception:
            # Diagnostics must never make a valid source snapshot fail.
            pass

    root = Path(root).resolve()
    stats = {"cache_hits": 0, "cache_misses": 0, "files_hashed": 0,
             "bytes_hashed": 0, "stat_ms": 0.0, "hash_ms": 0.0,
             "owner_resolve_ms": 0.0}
    used_cache: dict[str, dict] = {}

    def digest_for(rel: str) -> tuple[str, int, int]:
        path = root / rel
        stat_started = time.perf_counter()
        stat = path.stat()
        stats["stat_ms"] += (time.perf_counter() - stat_started) * 1000.0
        if hash_cache is not None:
            cached = hash_cache.get(rel)
            if isinstance(cached, dict) and cached.get("sha256"):
                try:
                    cached_size = int(cached.get("size", -1))
                    cached_mtime = int(cached.get("mtime_ns", -1))
                except (TypeError, ValueError):
                    cached_size = cached_mtime = -1
                if (cached_size == stat.st_size
                        and cached_mtime == stat.st_mtime_ns):
                    stats["cache_hits"] += 1
                    used_cache[rel] = cached
                    return str(cached["sha256"]), stat.st_size, stat.st_mtime_ns
        stats["cache_misses"] += 1
        hash_started = time.perf_counter()
        digest, size, mtime_ns = stamped_sha256(path)
        stats["hash_ms"] += (time.perf_counter() - hash_started) * 1000.0
        stats["files_hashed"] += 1
        stats["bytes_hashed"] += size
        if hash_cache is not None:
            used_cache[rel] = {"sha256": digest, "size": size,
                               "mtime_ns": mtime_ns}
        return digest, size, mtime_ns

    phase_started = time.perf_counter()
    report("enumerate_cpp", "started", phase_started)
    cpp_paths, cpp_inventory = _cached_inventory(root, "cpp")
    report("enumerate_cpp", "completed", phase_started,
           files_total=len(cpp_paths), **cpp_inventory)
    if resolver is None:
        phase_started = time.perf_counter()
        report("owner_resolver", "started", phase_started)
        resolver, cache_hit = _cached_owner_resolver(root, cpp_paths)
        report("owner_resolver", "completed", phase_started,
               source_rule_count=len(resolver.rule_assignments),
               module_owner_count=len(resolver.module_owners),
               cache_hit=cache_hit)

    sources: list[dict] = []
    phase_started = time.perf_counter()
    report("source_ownership_and_digest", "started", phase_started,
           files_total=len(cpp_paths))
    for index, rel in enumerate(cpp_paths, 1):
        owner_started = time.perf_counter()
        owner, reason = resolver.resolve(rel)
        stats["owner_resolve_ms"] += (time.perf_counter() - owner_started) * 1000.0
        digest, size, mtime_ns = digest_for(rel)
        sources.append({
            "path": rel,
            "sha256": digest,
            "size": size,
            "mtime_ns": mtime_ns,
            "owner": owner,
            "owner_reason": reason,
            "active_in_manifest": owner not in ("unresolved", "vendor"),
        })
        if index % 64 == 0 or index == len(cpp_paths):
            report("source_ownership_and_digest", "progress", phase_started,
                   files_completed=index, files_total=len(cpp_paths), **stats)
    report("source_ownership_and_digest", "completed", phase_started,
           files_completed=len(cpp_paths), files_total=len(cpp_paths), **stats)

    lines = [f"{item['path']}:{item['sha256']}" for item in sources]
    headers: list[dict] = []
    phase_started = time.perf_counter()
    report("enumerate_headers", "started", phase_started)
    header_paths, header_inventory = _cached_inventory(root, "headers")
    report("enumerate_headers", "completed", phase_started,
           files_total=len(header_paths), **header_inventory)

    phase_started = time.perf_counter()
    report("header_digest", "started", phase_started,
           files_total=len(header_paths))
    for index, rel in enumerate(header_paths, 1):
        digest, size, mtime_ns = digest_for(rel)
        headers.append({"path": rel, "sha256": digest,
                        "size": size, "mtime_ns": mtime_ns})
        if index % 64 == 0 or index == len(header_paths):
            report("header_digest", "progress", phase_started,
                   files_completed=index, files_total=len(header_paths), **stats)
    report("header_digest", "completed", phase_started,
           files_completed=len(header_paths), files_total=len(header_paths), **stats)
    if hash_cache is not None:
        hash_cache.clear()
        hash_cache.update(used_cache)
    lines.extend(f"header:{item['path']}:{item['sha256']}" for item in headers)
    phase_started = time.perf_counter()
    report("config_hash_and_revision", "started", phase_started)
    config_hashes = resolver.config_hashes()
    lines.extend(f"config:{name}:{value}" for name, value in config_hashes.items())
    revision = hashlib.sha256("\n".join(lines).encode("utf-8")).hexdigest()
    report("config_hash_and_revision", "completed", phase_started,
           config_count=len(config_hashes), source_count=len(sources),
           header_count=len(headers), **stats)

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
