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
import ctypes
import hashlib
import json
import os
import sys
import threading
import time
from collections import Counter, defaultdict, deque
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
_INVENTORY_WATCHERS: dict[Path, "_DirectoryChangeWatcher"] = {}
_INVENTORY_WATCHER_CURSORS: dict[tuple[Path, str], int] = {}
_HASH_WATCHER_CURSORS: dict[Path, int] = {}
_HASH_WATCHER_CURSOR_LOCK = threading.Lock()
INVENTORY_CACHE_SCHEMA = 1
OWNER_RESOLVER_CACHE_SCHEMA = 1


class _DirectoryChangeWatcher:
    """Best-effort Windows name-change journal for warm source inventories.

    The persistent inventory remains authoritative after a daemon restart: its
    directory stamps are checked once before this watcher is trusted. If the
    notification stream overflows or stops, callers fall back to a full stamp
    check and tree scan.
    """

    _BUFFER_BYTES = 64 * 1024
    _EVENT_LIMIT = 16384
    _FILE_NAME = 0x00000001
    _DIR_NAME = 0x00000002
    _SIZE = 0x00000008
    _LAST_WRITE = 0x00000010
    _ACTIONS = {"added": 1, "removed": 2, "renamed_old": 4, "renamed_new": 5}

    def __init__(self, root: Path):
        if os.name != "nt":
            raise OSError("directory notifications require Windows")
        from ctypes import wintypes

        self.root = root
        self._lock = threading.Lock()
        self._ready = threading.Event()
        self._events: deque[tuple[int, int, str]] = deque(maxlen=self._EVENT_LIMIT)
        self._sequence = 0
        self._overflow_sequence = 0
        self._error = ""
        kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
        self._kernel32 = kernel32
        kernel32.CreateFileW.restype = wintypes.HANDLE
        kernel32.CreateFileW.argtypes = (
            wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD,
            wintypes.LPVOID, wintypes.DWORD, wintypes.DWORD, wintypes.HANDLE)
        kernel32.ReadDirectoryChangesW.restype = wintypes.BOOL
        kernel32.ReadDirectoryChangesW.argtypes = (
            wintypes.HANDLE, wintypes.LPVOID, wintypes.DWORD,
            wintypes.BOOL, wintypes.DWORD, ctypes.POINTER(wintypes.DWORD),
            wintypes.LPVOID, wintypes.LPVOID)
        handle = kernel32.CreateFileW(
            str(root), 0x0001, 0x00000001 | 0x00000002 | 0x00000004,
            None, 3, 0x02000000, None)
        if handle == wintypes.HANDLE(-1).value:
            raise ctypes.WinError(ctypes.get_last_error())
        self._handle = handle
        self._thread = threading.Thread(
            target=self._run, name="mimita-source-inventory-watch", daemon=True)
        self._thread.start()
        if not self._ready.wait(2.0):
            kernel32.CloseHandle(handle)
            raise OSError("source inventory watcher did not start")

    @property
    def sequence(self) -> int:
        with self._lock:
            return self._sequence

    @property
    def error(self) -> str:
        with self._lock:
            return self._error

    def events_since(self, sequence: int
                     ) -> tuple[int, list[tuple[int, int, str]]] | None:
        with self._lock:
            if self._error:
                return None
            if self._overflow_sequence > sequence:
                return None
            if self._events and sequence < self._events[0][0] - 1:
                return None
            snapshot_sequence = self._sequence
            return (snapshot_sequence,
                    [item for item in self._events if item[0] > sequence])

    def _append(self, action: int, relative_name: str) -> None:
        relative_name = relative_name.replace("\\", "/").strip("/")
        if not relative_name or relative_name == "." or ".." in Path(relative_name).parts:
            return
        parts = Path(relative_name).parts
        # Ignore paths the source inventories never include. Counting their
        # notifications would make unrelated cache/build writes invalidate a
        # source-tree snapshot and force a needless retry.
        if (not parts or parts[0] in EXCLUDE_DIRS
                or any(part in DEEP_EXCLUDE_DIRS for part in parts[1:])):
            return
        with self._lock:
            next_sequence = self._sequence + 1
            if len(self._events) == self._EVENT_LIMIT:
                self._overflow_sequence = next_sequence
            self._sequence = next_sequence
            self._events.append((next_sequence, action, relative_name))

    def _run(self) -> None:
        from ctypes import wintypes

        buffer = ctypes.create_string_buffer(self._BUFFER_BYTES)
        self._ready.set()
        while True:
            bytes_returned = wintypes.DWORD()
            ok = self._kernel32.ReadDirectoryChangesW(
                self._handle, buffer, len(buffer), True,
                self._FILE_NAME | self._DIR_NAME | self._SIZE | self._LAST_WRITE,
                ctypes.byref(bytes_returned), None, None)
            if not ok:
                error = ctypes.get_last_error()
                with self._lock:
                    self._error = f"ReadDirectoryChangesW failed: {error}"
                return
            if not bytes_returned.value:
                with self._lock:
                    self._sequence += 1
                    self._overflow_sequence = self._sequence
                continue
            offset = 0
            total = int(bytes_returned.value)
            base_address = ctypes.addressof(buffer)
            while offset + 12 <= total:
                next_offset = wintypes.DWORD.from_buffer(buffer, offset).value
                action = wintypes.DWORD.from_buffer(buffer, offset + 4).value
                name_bytes = wintypes.DWORD.from_buffer(buffer, offset + 8).value
                if name_bytes % 2 or offset + 12 + name_bytes > total:
                    with self._lock:
                        self._sequence += 1
                        self._overflow_sequence = self._sequence
                    break
                name = ctypes.wstring_at(base_address + offset + 12,
                                          name_bytes // 2)
                self._append(int(action), name)
                if not next_offset:
                    break
                offset += int(next_offset)


def _directory_watcher(root: Path) -> _DirectoryChangeWatcher | None:
    if os.name != "nt":
        return None
    watcher = _INVENTORY_WATCHERS.get(root)
    if watcher is not None:
        return watcher if not watcher.error else None
    try:
        watcher = _DirectoryChangeWatcher(root)
    except (OSError, AttributeError):
        return None
    _INVENTORY_WATCHERS[root] = watcher
    return watcher


def _watcher_changed_directories(root: Path, kind: str,
                                 events: list[tuple[int, int, str]],
                                 cached_stamps: dict[str, int]) -> list[str]:
    changed: set[str] = set()
    suffixes = (".cpp",) if kind == "cpp" else HEADER_SUFFIXES
    excluded_deep = DEEP_EXCLUDE_DIRS if kind == "cpp" else HEADER_EXCLUDE_DIRS
    for _sequence, _action, relative_name in events:
        if _action == 3:  # modified content; it does not change the path inventory
            continue
        parts = Path(relative_name).parts
        if not parts or parts[0] in EXCLUDE_DIRS:
            continue
        if any(part in excluded_deep for part in parts[1:]):
            continue
        if kind == "headers" and parts[0] not in HEADER_ROOTS:
            continue
        path = Path(relative_name)
        absolute = root / path
        if (relative_name in cached_stamps or absolute.is_dir()):
            changed.add(path.as_posix())
        elif path.suffix.lower() in suffixes:
            parent = path.parent.as_posix()
            if parent != ".":
                changed.add(parent)
    return sorted(changed)


def _watcher_changed_source_files(
        events: list[tuple[int, int, str]]) -> set[str]:
    """Return source/header paths touched since a digest cursor."""
    suffixes = {".cpp", *HEADER_SUFFIXES}
    changed: set[str] = set()
    for _sequence, _action, relative_name in events:
        if Path(relative_name).suffix.lower() in suffixes:
            changed.add(relative_name)
    return changed


def _digest_watcher_snapshot(root: Path) -> tuple[
        _DirectoryChangeWatcher | None, int | None, set[str], bool]:
    watcher = _directory_watcher(root)
    if watcher is None:
        return None, None, set(), True
    cursor = _HASH_WATCHER_CURSORS.get(root)
    if cursor is None:
        # No cursor means this process has no complete event history. Validate
        # every cached stamp once, then establish a cursor after a stable
        # snapshot. Files whose metadata changed are rehashed by digest_for.
        return watcher, watcher.sequence, set(), True
    events = watcher.events_since(cursor)
    if events is None:
        # Overflow or watcher error: rebuild the trusted stamp baseline.
        return watcher, watcher.sequence, set(), True
    sequence, changes = events
    return watcher, sequence, _watcher_changed_source_files(changes), False


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


def _cached_inventory(root: Path, kind: str,
                      _retry: int = 0) -> tuple[list[str], dict]:
    key = (root, kind)
    cache_path = root / ".dev" / f"source-inventory-{kind}-cache.json"
    try:
        cache_path.parent.mkdir(parents=True, exist_ok=True)
    except OSError:
        pass
    cached = _INVENTORY_CACHE.get(key)
    memory_hit = cached is not None
    persistent_hit = False
    if cached is None:
        try:
            payload = read_json(cache_path)
            if (isinstance(payload, dict)
                    and payload.get("schema") == INVENTORY_CACHE_SCHEMA
                    and payload.get("root") == str(root)
                    and payload.get("kind") == kind
                    and isinstance(payload.get("paths"), list)
                    and isinstance(payload.get("stamps"), dict)):
                cached = (tuple(str(path) for path in payload["paths"]),
                          {str(path): int(stamp)
                           for path, stamp in payload["stamps"].items()})
                persistent_hit = True
        except (OSError, ValueError, TypeError):
            cached = None
    watcher = _directory_watcher(root)
    watcher_used = watcher is not None
    watcher_cursor = _INVENTORY_WATCHER_CURSORS.get(key)
    watcher_sequence = watcher.sequence if watcher is not None else -1
    changed_directories: list[str] | None = []
    cache_origin = ("memory" if memory_hit else
                    "persistent" if persistent_hit else "cold")
    cache_diagnostics = {
        "cache_origin": cache_origin,
        "memory_cache_hit": memory_hit,
        "persistent_cache_hit": persistent_hit,
        "process_id": os.getpid(),
        "module_name": __name__,
        "module_instance_id": id(sys.modules.get(__name__)),
        "inventory_cache_instance_id": id(_INVENTORY_CACHE),
        "inventory_cache_key": str(root),
        "watcher_id": id(watcher) if watcher is not None else None,
        "watcher_cursor": watcher_cursor,
    }
    if cached is not None:
        if watcher is None:
            changed_directories = _directory_stamp_changes(root, cached[1])
        elif watcher_cursor is None:
            # A persistent cache cannot trust notifications emitted before the
            # daemon started watching. Validate its stamps once, then use the
            # live stream for later requests.
            baseline = watcher.sequence
            changed_directories = _directory_stamp_changes(root, cached[1])
            event_snapshot = watcher.events_since(baseline)
            if event_snapshot is None:
                changed_directories = None
            else:
                watcher_sequence, events = event_snapshot
                changed_directories.extend(_watcher_changed_directories(
                    root, kind, events, cached[1]))
        else:
            event_snapshot = watcher.events_since(watcher_cursor)
            if event_snapshot is None:
                changed_directories = None
            else:
                watcher_sequence, events = event_snapshot
                changed_directories = _watcher_changed_directories(
                    root, kind, events, cached[1])
    elif watcher is not None:
        watcher_sequence = watcher.sequence

    if cached is not None and changed_directories == []:
        if watcher is None or watcher.sequence == watcher_sequence:
            _INVENTORY_CACHE[key] = cached
            if watcher is not None:
                _INVENTORY_WATCHER_CURSORS[key] = watcher_sequence
            return list(cached[0]), {
                "cache_hit": True, "watcher_cache": watcher_used,
                "directory_count": len(cached[1]),
                **cache_diagnostics,
                "watcher_sequence": watcher_sequence if watcher is not None else None,
            }
        if _retry < 2:
            return _cached_inventory(root, kind, _retry + 1)
        changed_directories = None
    scanner = _scan_cpp_inventory if kind == "cpp" else _scan_header_inventory
    incremental = (_incremental_inventory(root, kind, cached[0], cached[1],
                                           changed_directories)
                   if cached is not None and changed_directories else None)
    if incremental is None:
        if watcher is not None and watcher_cursor is not None:
            watcher_sequence = watcher.sequence
        result, stamps = scanner(root)
        rescanned_directories = len(stamps)
        incremental_used = False
    else:
        result, stamps, rescanned_directories = incremental
        incremental_used = True
    # Validate the directory tree after enumerating it. If a file or directory
    # was added/removed during the scan, return the scan result but do not trust
    # it as a reusable inventory on the next build.
    if watcher is None:
        stable = not _directory_stamp_changes(root, stamps)
    else:
        stable = watcher.sequence == watcher_sequence
        if not stable and _retry < 2:
            # Cache this coherent-enough enumeration at the sequence it saw;
            # the next pass applies every later event before returning it.
            _INVENTORY_CACHE[key] = (tuple(result), stamps)
            _INVENTORY_WATCHER_CURSORS[key] = watcher_sequence
            return _cached_inventory(root, kind, _retry + 1)
    if stable:
        _INVENTORY_CACHE[key] = (tuple(result), stamps)
        if watcher is not None:
            _INVENTORY_WATCHER_CURSORS[key] = watcher_sequence
        try:
            cache_path.parent.mkdir(parents=True, exist_ok=True)
            temporary = cache_path.with_name(
                f"{cache_path.name}.{os.getpid()}.tmp")
            temporary.write_text(json.dumps({
                "schema": INVENTORY_CACHE_SCHEMA, "root": str(root),
                "kind": kind, "paths": result, "stamps": stamps,
            }, separators=(",", ":")), encoding="utf-8")
            os.replace(temporary, cache_path)
        except OSError:
            # Inventory cache is derived data; a later call can rescan.
            pass
    else:
        _INVENTORY_CACHE.pop(key, None)
    reported_changed_directories = changed_directories or []
    return result, {
        "cache_hit": False, "persistent_cache_hit": persistent_hit,
        "invalidated_reason": (
            "watcher_history_unavailable" if changed_directories is None else
            "directory_stamp_changed" if changed_directories else
            "cold_or_unstable_scan"),
        "changed_directory_count": len(reported_changed_directories),
        "changed_directories": reported_changed_directories[:32],
        "changed_directories_truncated": (
            len(reported_changed_directories) > 32),
        "rescanned_directory_count": rescanned_directories,
        "incremental": incremental_used,
        "inventory_stable": stable,
        "watcher_cache": watcher_used,
        "directory_count": len(stamps),
        "watcher_sequence": watcher_sequence if watcher is not None else None,
        **cache_diagnostics,
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


def _resolver_from_cache(root: Path, owner_config: dict, payload: dict
                         ) -> OwnerResolver | None:
    module_owners = payload.get("module_owners")
    overrides = payload.get("overrides")
    rule_assignments = payload.get("rule_assignments")
    link_inputs = payload.get("link_inputs")
    errors = payload.get("errors")
    warnings = payload.get("warnings")
    if not all(isinstance(value, dict) for value in
               (module_owners, overrides, rule_assignments, link_inputs)):
        return None
    if any(not isinstance(path, str) or not isinstance(value, dict)
           for path, value in overrides.items()):
        return None
    if any(not isinstance(group, str) or not isinstance(entries, list)
           or any(not isinstance(entry, dict)
                  or not isinstance(entry.get("path"), str)
                  or not isinstance(entry.get("owner"), str)
                  or not isinstance(entry.get("reason"), str)
                  for entry in entries)
           for group, entries in link_inputs.items()):
        return None
    if not isinstance(errors, list) or not isinstance(warnings, list):
        return None
    parsed_rules: dict[str, tuple[str, str]] = {}
    for path, value in rule_assignments.items():
        if (not isinstance(path, str) or not isinstance(value, list)
                or len(value) != 2 or not all(isinstance(item, str) for item in value)):
            return None
        parsed_rules[path] = (value[0], value[1])
    if any(not isinstance(path, str) or not isinstance(owner, str)
           for path, owner in module_owners.items()):
        return None
    resolver = OwnerResolver.__new__(OwnerResolver)
    resolver.root = root
    resolver.config = owner_config
    resolver.errors = [str(item) for item in errors]
    resolver.warnings = [str(item) for item in warnings]
    resolver.module_owners = module_owners
    resolver.overrides = overrides
    resolver.rule_assignments = parsed_rules
    resolver.link_inputs = link_inputs
    return resolver


def _resolver_cache_payload(root: Path, signature: tuple,
                            resolver: OwnerResolver) -> dict:
    return {
        "schema": OWNER_RESOLVER_CACHE_SCHEMA,
        "root": str(root),
        "signature": hashlib.sha256(json.dumps(
            signature, separators=(",", ":")).encode("utf-8")).hexdigest(),
        "module_owners": resolver.module_owners,
        "overrides": resolver.overrides,
        "rule_assignments": {path: list(value)
                             for path, value in resolver.rule_assignments.items()},
        "link_inputs": resolver.link_inputs,
        "errors": resolver.errors,
        "warnings": resolver.warnings,
    }


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
    signature_hash = hashlib.sha256(json.dumps(
        signature, separators=(",", ":")).encode("utf-8")).hexdigest()
    cache_path = root / ".dev" / "source-owner-resolver-cache.json"
    try:
        payload = read_json(cache_path)
        if (isinstance(payload, dict)
                and payload.get("schema") == OWNER_RESOLVER_CACHE_SCHEMA
                and payload.get("root") == str(root)
                and payload.get("signature") == signature_hash):
            try:
                owner_config = read_json(root / OWNERS_REL)
            except (OSError, json.JSONDecodeError):
                owner_config = {}
            resolver = _resolver_from_cache(root, owner_config, payload)
            if resolver is not None:
                _RESOLVER_CACHE[root] = (signature, resolver)
                return resolver, True
    except (OSError, ValueError, TypeError):
        pass
    resolver = OwnerResolver(root)
    _RESOLVER_CACHE[root] = (signature, resolver)
    try:
        cache_path.parent.mkdir(parents=True, exist_ok=True)
        temporary = cache_path.with_name(f"{cache_path.name}.{os.getpid()}.tmp")
        temporary.write_text(json.dumps(
            _resolver_cache_payload(root, signature, resolver),
            separators=(",", ":")), encoding="utf-8")
        os.replace(temporary, cache_path)
    except OSError:
        # Ownership remains authoritative in config/catalog; this cache only
        # avoids expanding unchanged glob rules on the next attempt.
        pass
    return resolver, False


def build_manifest(root: Path, resolver: OwnerResolver | None = None,
                   hash_cache: dict | None = None,
                   progress_callback: Callable[[dict], None] | None = None,
                   _inventory_retry: int = 0) -> dict:
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
    stats = {"cache_hits": 0, "event_cache_hits": 0,
             "baseline_stat_cache_hits": 0, "event_forced_rehashes": 0,
             "cache_misses": 0, "files_hashed": 0,
             "bytes_hashed": 0, "stat_ms": 0.0, "hash_ms": 0.0,
             "owner_resolve_ms": 0.0}
    used_cache: dict[str, dict] = {}

    # The watcher lives in the root dev-loop process. Its event sequence lets
    # warm attempts reuse unchanged digests without issuing thousands of file
    # stats. A fresh process, watcher overflow, or unavailable watcher has no
    # trustworthy event history, so it validates cached size/time stamps; any
    # mismatch is hashed before the baseline cursor is trusted.
    digest_watcher, digest_sequence, changed_source_files, validate_all_stamps = (
        _digest_watcher_snapshot(root))

    def digest_for(rel: str) -> tuple[str, int, int]:
        path = root / rel
        changed_event = rel in changed_source_files
        if hash_cache is not None:
            cached = hash_cache.get(rel)
            if isinstance(cached, dict) and cached.get("sha256"):
                try:
                    cached_size = int(cached.get("size", -1))
                    cached_mtime = int(cached.get("mtime_ns", -1))
                except (TypeError, ValueError):
                    cached_size = cached_mtime = -1
                if (not validate_all_stamps and not changed_event):
                    stats["event_cache_hits"] += 1
                    used_cache[rel] = cached
                    return (str(cached["sha256"]), cached_size, cached_mtime)
        stat_started = time.perf_counter()
        stat = path.stat()
        stats["stat_ms"] += (time.perf_counter() - stat_started) * 1000.0
        if (hash_cache is not None and not changed_event
                and isinstance(cached, dict) and cached.get("sha256")
                and cached_size == stat.st_size
                and cached_mtime == stat.st_mtime_ns):
            if validate_all_stamps:
                stats["baseline_stat_cache_hits"] += 1
            else:
                stats["cache_hits"] += 1
            used_cache[rel] = cached
            return str(cached["sha256"]), stat.st_size, stat.st_mtime_ns
        if changed_event:
            stats["event_forced_rehashes"] += 1
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
    lines.extend(f"header:{item['path']}:{item['sha256']}" for item in headers)
    phase_started = time.perf_counter()
    report("config_hash_and_revision", "started", phase_started)
    config_hashes = resolver.config_hashes()
    lines.extend(f"config:{name}:{value}" for name, value in config_hashes.items())
    revision = hashlib.sha256("\n".join(lines).encode("utf-8")).hexdigest()
    report("config_hash_and_revision", "completed", phase_started,
           config_count=len(config_hashes), source_count=len(sources),
           header_count=len(headers), **stats)

    # A new/removed/renamed translation unit can arrive while the manifest is
    # being assembled. Re-read the event-backed inventories before returning;
    # if their path sets moved, rebuild the manifest from the new stable set.
    final_cpp_paths = _cached_inventory(root, "cpp")[0]
    final_header_paths = _cached_inventory(root, "headers")[0]
    event_race = False
    if digest_watcher is not None and digest_sequence is not None:
        final_events = digest_watcher.events_since(digest_sequence)
        event_race = (final_events is None
                      or final_events[0] != digest_sequence)
    if (final_cpp_paths != cpp_paths or final_header_paths != header_paths
            or event_race):
        if _inventory_retry >= 2:
            raise RuntimeError(
                "source tree changed repeatedly while building the manifest")
        return build_manifest(root, None, hash_cache, progress_callback,
                              _inventory_retry + 1)

    if hash_cache is not None:
        hash_cache.clear()
        hash_cache.update(used_cache)
    if digest_watcher is not None and digest_sequence is not None:
        with _HASH_WATCHER_CURSOR_LOCK:
            _HASH_WATCHER_CURSORS[root] = max(
                digest_sequence, _HASH_WATCHER_CURSORS.get(root, -1))

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
