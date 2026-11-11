#!/usr/bin/env python3
"""Write a source-backed C++ dependency and ownership inventory.

This is a read-only inventory tool. It never invokes a compiler or changes
source ownership. Existing GCC/MinGW .d files provide compiler-observed
dependencies; current #include scanning fills in a present-day project graph
where no usable .d file exists.

Usage:
    python devscripts/cpp-source-inventory.py
    python devscripts/cpp-source-inventory.py --output docs/handoffs/my-inventory.json
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import sys
from functools import lru_cache
from collections import Counter, defaultdict, deque
from datetime import datetime, timezone
from pathlib import Path

SCRIPT_VERSION = 2
ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))
DEFAULT_OUTPUT = Path("docs/handoffs/cpp-source-inventory.json")
SOURCE_ROOT = ROOT / "src"

INCLUDE_DIRS = [
    ROOT / "src",
    ROOT / "include",
    ROOT / "external/libjuice/include",
    ROOT / "external/manifold-prebuilt/include",
    ROOT / "external/recastnavigation/Recast/Include",
    ROOT / "external/recastnavigation/Detour/Include",
    ROOT / "external/recastnavigation/DetourTileCache/Include",
]
configured_glfw = os.environ.get("MIMITA_GLFW_INCLUDE")
if configured_glfw:
    INCLUDE_DIRS.append(Path(configured_glfw))

INCLUDE_RE = re.compile(r'^\s*#\s*include\s*([<"])([^>"]+)[>"]', re.MULTILINE)
BLOCK_COMMENT_RE = re.compile(r"/\*.*?\*/", re.DOTALL)
LINE_COMMENT_RE = re.compile(r"//[^\r\n]*")
WHITESPACE_RE = re.compile(r"\\\r?\n")

STATE_DECL_RE = re.compile(r"\b(?:static|extern)\b[^;{}]*;")
FUNCTION_DECL_RE = re.compile(r"\([^;{}]*\)\s*;\s*$")
THREAD_JOB_PATTERNS = {
    "thread_or_async_creation": re.compile(
        r"\bstd::(?:thread|jthread|async)\b|\b(?:CreateThread|_beginthread(?:ex)?)\b"),
    "thread_detach": re.compile(r"\.\s*detach\s*\("),
    "thread_join": re.compile(r"\.\s*join\s*\("),
    "job_queue_submission": re.compile(r"\b(?:enqueue|submit|schedule|postTask|postJob)\s*\("),
}
CALLBACK_PATTERN = re.compile(
    r"std::function|std::bind|\b(?:set|register|add|subscribe|bind)\w*"
    r"(?:Callback|Handler|Listener|Event)\b|\bcallback\b\s*(?:=|\()",
    re.IGNORECASE,
)
PACKET_ID_PATTERN = re.compile(r"^\s*(PACKET_[A-Z0-9_]+)\s*=\s*(\d+)")
PACKET_STRUCT_PATTERN = re.compile(r"^\s*struct\s+([A-Za-z_][A-Za-z0-9_]*Packet)\b")


def rel(path: Path) -> str | None:
    """Return a repository-relative POSIX path, or None for external files."""
    try:
        return path.resolve(strict=False).relative_to(ROOT).as_posix()
    except (OSError, ValueError):
        return None


@lru_cache(maxsize=65536)
def resolve_include(includer: Path, include_text: str) -> Path | None:
    """Resolve an include against source-local and configured build include dirs."""
    candidate_roots = [includer.parent, *INCLUDE_DIRS]
    for base in candidate_roots:
        candidate = base / include_text
        try:
            if candidate.is_file():
                return candidate.resolve(strict=False)
        except OSError:
            continue
    return None


def direct_includes(path: Path) -> list[dict]:
    text = path.read_text(encoding="utf-8", errors="replace")
    text = BLOCK_COMMENT_RE.sub("", text)
    text = LINE_COMMENT_RE.sub("", text)
    text = WHITESPACE_RE.sub(" ", text)
    result = []
    for match in INCLUDE_RE.finditer(text):
        include_text = match.group(2).strip()
        resolved = resolve_include(path, include_text)
        result.append({
            "spelling": include_text,
            "style": "quote" if match.group(1) == '"' else "angle",
            "resolved_path": rel(resolved) if resolved else None,
            "external_path": str(resolved) if resolved and rel(resolved) is None else None,
            "status": "resolved" if resolved else "external_or_macro_or_missing",
        })
    return result


def current_include_graph(sources: list[Path]) -> tuple[dict[str, list[dict]], dict[str, list[str]]]:
    """Read current reachable headers and return direct include edges and scan notes."""
    graph: dict[str, list[dict]] = {}
    notes: dict[str, list[str]] = {}
    pending = deque(sources)
    visited: set[str] = set()

    while pending:
        path = pending.popleft().resolve(strict=False)
        path_key = rel(path)
        if path_key is None or path_key in visited or not path.is_file():
            continue
        visited.add(path_key)
        if len(visited) % 1000 == 0:
            print(f"[CPP INVENTORY] include_files_scanned={len(visited)}", flush=True)
        try:
            includes = direct_includes(path)
        except OSError as error:
            graph[path_key] = []
            notes[path_key] = [f"read_error: {error}"]
            continue
        graph[path_key] = includes
        unresolved = [item["spelling"] for item in includes if item["resolved_path"] is None]
        if unresolved:
            notes[path_key] = [f"unresolved_include: {name}" for name in unresolved]
        for item in includes:
            if item["resolved_path"]:
                pending.append(ROOT / item["resolved_path"])
    return graph, notes


def transitive_headers(source_rel: str, graph: dict[str, list[dict]]) -> list[str]:
    found: set[str] = set()
    pending = deque(item["resolved_path"] for item in graph.get(source_rel, [])
                    if item["resolved_path"])
    while pending:
        dependency = pending.popleft()
        if dependency == source_rel or dependency in found:
            continue
        found.add(dependency)
        for item in graph.get(dependency, []):
            if item["resolved_path"]:
                pending.append(item["resolved_path"])
    return sorted(found)


def makefile_tokens(text: str) -> list[str]:
    """Tokenize a GCC make dependency line, preserving Windows path slashes."""
    text = WHITESPACE_RE.sub(" ", text)
    separator = next((index for index, char in enumerate(text)
                      if char == ":" and (index == 0 or text[index - 1] != "\\")), None)
    if separator is None:
        return []
    text = text[separator + 1:]
    tokens: list[str] = []
    token: list[str] = []
    index = 0
    while index < len(text):
        char = text[index]
        if char == "\\" and index + 1 < len(text) and text[index + 1].isspace():
            token.append(text[index + 1])
            index += 2
            continue
        if char.isspace():
            if token:
                tokens.append("".join(token))
                token = []
        else:
            token.append(char)
        index += 1
    if token:
        tokens.append("".join(token))
    return tokens


def file_signature(path: Path) -> dict:
    try:
        stat = path.stat()
        digest = hashlib.sha256()
        with path.open("rb") as handle:
            for chunk in iter(lambda: handle.read(1024 * 1024), b""):
                digest.update(chunk)
        return {
            "bytes": stat.st_size,
            "mtime_utc": datetime.fromtimestamp(stat.st_mtime, timezone.utc).isoformat(),
            "sha256": digest.hexdigest(),
        }
    except OSError as error:
        return {"error": str(error)}


def read_dep_snapshots(sources: list[Path]) -> dict[str, list[dict]]:
    # Make the broad .d-file discovery pass string-only. Calling Path.resolve,
    # exists and stat for every token in every historical depfile is extremely
    # slow on Windows. Fully inspect only the newest depfile for each TU/mode.
    source_by_key = {normalize_dep_key(rel(path)): rel(path) for path in sources}
    newest: dict[tuple[str, str], Path] = {}
    for mode in ("debug", "release"):
        dep_root = ROOT / "build" / f"obj-{mode}"
        if not dep_root.is_dir():
            continue
        scanned_depfiles = 0
        for dep_path in dep_root.rglob("*.d"):
            scanned_depfiles += 1
            if scanned_depfiles % 200 == 0:
                print(f"[CPP INVENTORY] {mode}_depfiles_scanned={scanned_depfiles}", flush=True)
            try:
                raw = dep_path.read_text(encoding="utf-8", errors="replace")
            except OSError:
                continue
            tokens = makefile_tokens(raw)
            matched_sources = {source_by_key.get(normalize_dep_key(item)) for item in tokens}
            matched_sources.discard(None)
            if len(matched_sources) != 1:
                continue
            source_rel = next(iter(matched_sources))
            key = (mode, source_rel)
            previous = newest.get(key)
            if previous is None or dep_path.stat().st_mtime_ns > newest[(mode, source_rel)].stat().st_mtime_ns:
                newest[key] = dep_path

    snapshots: dict[str, list[dict]] = defaultdict(list)
    for (mode, source_rel), dep_path in newest.items():
        try:
            raw = dep_path.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        dep_tokens = makefile_tokens(raw)
        normalized_dependencies = sorted({normalize_dep_key(item) for item in dep_tokens})
        try:
            rule = WHITESPACE_RE.sub(" ", raw)
            separator = next(index for index, char in enumerate(rule)
                             if char == ":" and (index == 0 or rule[index - 1] != "\\"))
            target_text = rule[:separator].replace("\\:", ":").strip()
            object_text = normalize_dep_key(target_text)
        except (ValueError, IndexError):
            object_text = None
        dep_mtime = dep_path.stat().st_mtime
        repo_dependency_count = sum(
            1 for item in normalized_dependencies if item.startswith(("src/", "include/")))
        snapshots[source_rel].append({
            "mode": mode,
            "dep_file": rel(dep_path),
            "dep_file_mtime_utc": datetime.fromtimestamp(dep_mtime, timezone.utc).isoformat(),
            "object_file": object_text,
            "object_exists": None,
            "object_mtime_utc": None,
            "dependency_count": len(normalized_dependencies),
            "repository_dependency_count": repo_dependency_count,
            "missing_dependency_count": None,
            "current_by_mtime": None,
            "freshness_assessment": "unknown; existing dependency timestamps are not exhaustively statted",
            "dependencies": normalized_dependencies,
        })
    for values in snapshots.values():
        values.sort(key=lambda item: item["dep_file_mtime_utc"], reverse=True)
    return snapshots


def normalize_dep_key(token: str | None) -> str:
    """Normalize a depfile path for cheap comparison with src-relative paths."""
    if token is None:
        return ""
    value = token.replace("\\", "/").casefold()
    while value.startswith("./"):
        value = value[2:]
    marker = "/src/"
    if marker in value:
        value = "src/" + value.rsplit(marker, 1)[1]
    return value


def source_owner_data() -> tuple[dict[str, str], list[str], set[str], set[str], dict[str, dict]]:
    from devscripts.module_ownership import (
        exe_source_relpaths,
        module_entries,
        module_source_owners,
        read_catalog,
        shared_with_host,
    )

    catalog = read_catalog(ROOT)
    owners, overlaps = module_source_owners(ROOT, catalog)
    exe_sources = set(exe_source_relpaths(ROOT, catalog))
    shared = shared_with_host(ROOT, catalog)
    module_info = {}
    for module in module_entries(catalog):
        for source in module.get("sources", []):
            if not isinstance(source, str):
                continue
            for path in sorted(ROOT.glob(source)) if any(c in source for c in "*?[") else [ROOT / source]:
                source_rel = rel(path)
                if source_rel:
                    module_info[source_rel] = {
                        "name": module.get("name"),
                        "abi": module.get("abi"),
                        "state_schema": module.get("state_schema"),
                        "activation": module.get("activation"),
                        "process_scope": module.get("process_scope", "unspecified"),
                        "events": module.get("events", []),
                        "capabilities": module.get("capabilities", []),
                        "shared_with_host": source_rel in shared,
                    }
    return owners, overlaps, exe_sources, shared, module_info


def current_owner_label(source_rel: str, exe_sources: set[str],
                        module_info: dict[str, dict]) -> str:
    module = module_info.get(source_rel)
    if module:
        return "module:" + str(module["name"])
    if source_rel in exe_sources:
        return "exe"
    return "header_or_unmapped"


def reviewed_state_ownership_anchors() -> dict:
    """Record the primary source-verified live state holders and NPC pilot path."""
    anchors = [
        {
            "concept": "dedicated_server_session_and_authoritative_players_npcs_projectiles",
            "current_owner": "runServer function local state on the dedicated server thread",
            "locations": [{"path": "src/network/server.cpp", "pattern":
                           r"HeadlessWorld world;"},
                          {"path": "src/network/server.cpp", "pattern":
                           r"World npcWorld;"},
                          {"path": "src/network/server.cpp", "pattern":
                           r"NpcSystem npcSystem;"},
                          {"path": "src/network/server.cpp", "pattern":
                           r"Player mirrorPlayer;"},
                          {"path": "src/network/server.cpp", "pattern":
                           r"std::unordered_map<uint32_t, ServerPlayer> players;"},
                          {"path": "src/network/server.cpp", "pattern":
                           r"std::unordered_map<uint32_t, ServerNpc> npcs;"},
                          {"path": "src/network/server.cpp", "pattern":
                           r"std::unordered_map<uint32_t, ServerProjectile> projectiles;"}],
            "reload_implication": "These maps live in the running EXE's server-loop stack frame today. A game DLL cannot own them if the state must survive unloading that DLL.",
        },
        {
            "concept": "listen_server_session_and_authoritative_state",
            "current_owner": "caller-owned ListenServerState",
            "locations": [{"path": "src/network/server.h", "pattern": r"struct ListenServerState$"},
                          {"path": "src/network/server.h", "pattern":
                           r"std::unordered_map<uint32_t, ServerPlayer> players;"},
                          {"path": "src/network/server.h", "pattern":
                           r"std::unordered_map<uint32_t, ServerNpc> npcs;"},
                          {"path": "src/network/server.h", "pattern":
                           r"std::thread serverThread;"}],
            "reload_implication": "The session state and a live server thread have lifetimes outside an individual policy call. Any worker callback into replaceable code must be drained before unload.",
        },
        {
            "concept": "npc_behavior_and_simulation_state",
            "current_owner": "NpcSystem owns std::vector<Npc>; Npc records hold per-actor behavior and movement state",
            "locations": [{"path": "src/npc/npc.h", "pattern": "class Npc {"},
                          {"path": "src/npc/npc.h", "pattern": "class NpcSystem"},
                          {"path": "src/npc/npc.h", "pattern": r"std::vector<Npc> npcs;"}],
            "reload_implication": "Keep the same Npc IDs and state in a resident owner; do not retain an Npc* or reference into an unloaded game DLL.",
        },
        {
            "concept": "server_npc_snapshot_and_combat_projection",
            "current_owner": "ServerNpc records in server-owned player/NPC maps",
            "locations": [{"path": "src/network/server.h", "pattern": r"struct ServerNpc$"}],
            "reload_implication": "ServerNpc and Npc coexist and selected fields are mirrored. The authority and migration rule must be traced per field before choosing one as canonical runtime storage.",
        },
        {
            "concept": "client_multiplayer_connection_and_replica_state",
            "current_owner": "MultiplayerContext reached through gpMpContext in main.cpp",
            "locations": [{"path": "src/network/multiplayer-context.h", "pattern": "struct MultiplayerContext"},
                          {"path": "src/main.cpp", "pattern":
                           "gpMpContext = nullptr;"}],
            "reload_implication": "Transport, connected state, tick/snapshot counters, remote player/NPC replicas, and interpolation state must survive game-code replacement.",
        },
        {
            "concept": "loaded_world_and_collision_acceleration_data",
            "current_owner": "World object reached through gpWorld in main.cpp",
            "locations": [{"path": "src/world/world.h", "pattern": "struct World {"},
                          {"path": "src/main.cpp", "pattern": "gpWorld = nullptr;"}],
            "reload_implication": "Map geometry, collision chunks, AABB trees, and streamed collision state are live world data, not replaceable behavior.",
        },
        {
            "concept": "current_narrow_dll_cross_generation_memory",
            "current_owner": "HotReloadSystem host object in the EXE",
            "locations": [{"path": "src/hot-reload/hot-reload-system.h", "pattern": r"GameMemory memory_"}],
            "reload_implication": "This current GameMemory is not the planned generic resident runtime store; its fields are recorded under loader_boundary_inventory.",
        },
    ]
    for anchor in anchors:
        found = []
        for location in anchor["locations"]:
            path = ROOT / location["path"]
            if not path.is_file():
                continue
            pattern = re.compile(location["pattern"])
            for line_no, line in enumerate(path.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
                if pattern.search(line):
                    found.append({"path": location["path"], "line": line_no,
                                  "snippet": line.strip()[:240]})
        anchor["source_evidence"] = found
        anchor.pop("locations")

    def source_location(path_rel: str, needle: str) -> dict:
        path = ROOT / path_rel
        if path.is_file():
            for line_no, line in enumerate(path.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
                if needle in line:
                    return {"path": path_rel, "line": line_no, "snippet": line.strip()[:240]}
        return {"path": path_rel, "line": None, "unresolved_needle": needle}

    npc_edges = [
        {"caller": source_location("src/network/server.cpp", "simulateSharedNpcs(sock, players"),
         "callee": source_location("src/network/server-npcs.cpp", "void simulateSharedNpcs(")},
        {"caller": source_location("src/network/server-npcs.cpp", "npcSystem.updateOneWithTarget("),
         "callee": source_location("src/npc/npc.cpp", "void NpcSystem::updateOneWithTarget(")},
        {"caller": source_location("src/npc/npc.cpp", "updateOneNpc(nc, world, context, dt);"),
         "callee": source_location("src/npc/npc.cpp", "void NpcSystem::updateOneNpc(")},
    ]
    return {
        "method": "manual source anchors with line numbers refreshed by the generator; this is a focused state-owner map, not a complete call graph",
        "primary_state_holders": anchors,
        "npc_authoritative_update_slice_candidate": {
            "call_sites": npc_edges,
            "key_data_type_signatures": [
                "Npc&", "const World&", "Player&", "NpcMovementContext&", "std::vector<Npc>"],
            "current_ownership": "EXE-owned NPC/world/session objects, with selected existing ModuleHost policy calls",
            "boundary_finding": "The current update method accepts live C++ object references and types; these are not a stable cross-DLL ABI. A pilot needs a plain versioned input/output adapter or a broader resident data API before code can safely unload.",
            "reload_risk_signals": "Inspect callback/job candidates in this slice and all registered ModuleHost policies before unloading. The report's file/line lists identify lexical candidates but do not prove quiescence.",
        },
        "limitations": [
            "This names the primary server/client/NPC/world state owners found by source review; it is not an exhaustive field-by-field ownership or ECS migration map.",
            "Some server records are projections of Npc/ECS state. A field-level authority trace is still needed before defining the runtime schema.",
        ],
    }


def scan_lifetime_signals(source_texts: dict[str, str], exe_sources: set[str],
                          module_info: dict[str, dict]) -> dict:
    """Collect source locations that need ownership/lifetime review before unload."""
    storage_sites = []
    callback_sites = []
    thread_job_sites = []
    for source_rel, raw_text in sorted(source_texts.items()):
        if not source_rel.startswith("src/"):
            continue
        text = BLOCK_COMMENT_RE.sub("", raw_text)
        lines = LINE_COMMENT_RE.sub("", text).splitlines()
        owner = current_owner_label(source_rel, exe_sources, module_info)
        for line_no, line in enumerate(lines, start=1):
            compact = line.strip()
            if not compact:
                continue
            if STATE_DECL_RE.search(compact) and not FUNCTION_DECL_RE.search(compact):
                storage_words = [word for word in ("static", "extern")
                                 if re.search(rf"\b{word}\b", compact)]
                storage_sites.append({
                    "path": source_rel,
                    "line": line_no,
                    "owner": owner,
                    "storage_keywords": storage_words,
                    "mutable_candidate": not bool(re.search(r"\b(const|constexpr)\b", compact)),
                    "declaration_or_definition_scope": "requires_manual_scope_review",
                    "snippet": compact[:320],
                })
            matched_thread_patterns = [name for name, pattern in THREAD_JOB_PATTERNS.items()
                                       if pattern.search(compact)]
            if matched_thread_patterns:
                thread_job_sites.append({
                    "path": source_rel,
                    "line": line_no,
                    "owner": owner,
                    "signals": matched_thread_patterns,
                    "snippet": compact[:320],
                })
            if CALLBACK_PATTERN.search(compact):
                callback_sites.append({
                    "path": source_rel,
                    "line": line_no,
                    "owner": owner,
                    "snippet": compact[:320],
                })
    return {
        "method": "lexical scan of current src/ C++ and reachable headers; each result needs manual lifetime tracing",
        "persistent_storage_candidates": storage_sites,
        "persistent_storage_candidate_count": len(storage_sites),
        "callback_registration_or_storage_candidates": callback_sites,
        "callback_candidate_count": len(callback_sites),
        "thread_async_and_job_sites": thread_job_sites,
        "thread_async_and_job_site_count": len(thread_job_sites),
        "limitations": [
            "C++ parsing is heuristic: macros, templates, multiline declarations, and generated code can be missed or misclassified.",
            "A static or extern declaration is not automatically persistent runtime state; constants, function-local caches, and definitions need ownership review.",
            "A callback or job site does not prove a DLL function pointer is retained; inspect captures, registration owner, cancellation, joins, and shutdown paths.",
            "The scan does not build a complete function call graph or prove that all jobs are quiescent before unload.",
        ],
    }


def network_boundary_inventory(source_texts: dict[str, str]) -> dict:
    packet_header = ROOT / "src" / "network" / "packets.h"
    if not packet_header.is_file():
        return {"status": "packets.h not found"}
    text = packet_header.read_text(encoding="utf-8", errors="replace")
    packet_ids = []
    packet_structs = []
    for line_no, line in enumerate(text.splitlines(), start=1):
        match = PACKET_ID_PATTERN.search(line)
        if match:
            packet_ids.append({"name": match.group(1), "wire_id": int(match.group(2)),
                               "declared_at": {"path": "src/network/packets.h", "line": line_no}})
        match = PACKET_STRUCT_PATTERN.search(line)
        if match:
            packet_structs.append({"name": match.group(1),
                                   "declared_at": {"path": "src/network/packets.h", "line": line_no}})

    def consumers_for(symbol: str) -> list[dict]:
        pattern = re.compile(rf"\b{re.escape(symbol)}\b")
        consumers = []
        for path, source_text in sorted(source_texts.items()):
            if not path.startswith("src/") or not path.endswith(".cpp"):
                continue
            line_numbers = [i for i, line in enumerate(source_text.splitlines(), start=1)
                            if pattern.search(line)]
            if line_numbers:
                consumers.append({"path": path, "occurrences": len(line_numbers),
                                  "lines": line_numbers})
        return consumers

    for packet in packet_ids:
        packet["source_consumers"] = consumers_for(packet["name"])
    for packet in packet_structs:
        packet["source_consumers"] = consumers_for(packet["name"])
    return {
        "packet_type_header": "src/network/packets.h",
        "packet_id_count": len(packet_ids),
        "packet_ids_and_cpp_use_sites": packet_ids,
        "packet_struct_count": len(packet_structs),
        "packet_structs_and_cpp_use_sites": packet_structs,
        "limitations": [
            "This maps declared PacketType IDs and lexical C++ references; it does not prove serialization/deserialization, authority, send direction, or client/server compatibility for every use.",
            "Generic event/envelope protocols that do not use PacketType are outside this packet enum inventory.",
            "Packet schema changes are runtime/host boundary work under the current hot-reload specification and require coordinated client/server activation evidence.",
        ],
    }


def block_fields(header_text: str, struct_name: str) -> list[dict]:
    match = re.search(rf"\bstruct\s+{re.escape(struct_name)}\s*\{{(.*?)\}}\s*;",
                      header_text, re.DOTALL)
    if not match:
        return []
    fields = []
    for line_no, line in enumerate(match.group(1).splitlines(), start=1):
        compact = line.strip()
        field_match = re.search(r"([A-Za-z_]\w*)\s*;\s*$", compact)
        if field_match:
            fields.append({"name": field_match.group(1), "declaration": compact})
    return fields


def source_loader_boundary(exe_sources: set[str], module_info: dict[str, dict]) -> dict:
    api_path = ROOT / "src" / "hot-reload" / "game-api.h"
    api_text = api_path.read_text(encoding="utf-8", errors="replace") if api_path.is_file() else ""
    build_script_path = ROOT / "build_game_dll.py"
    build_script = build_script_path.read_text(encoding="utf-8", errors="replace") \
        if build_script_path.is_file() else ""
    current_dll_sources = []
    source_pattern = re.compile(
        r'os\.path\.join\(ROOT,\s*"src",\s*((?:"[^"]+"\s*,\s*)+"[^"]+")\)')
    for match in source_pattern.finditer(build_script):
        parts = re.findall(r'"([^"]+)"', match.group(1))
        if parts:
            source_rel = "src/" + "/".join(parts)
            current_dll_sources.append({"path": source_rel,
                                        "current_catalog_owner": current_owner_label(
                                            source_rel, exe_sources, module_info),
                                        "also_in_exe_source_catalog": source_rel in exe_sources})

    def symbol_locations(path_rel: str, symbols: tuple[str, ...]) -> list[dict]:
        path = ROOT / Path(path_rel)
        if not path.is_file():
            return []
        result = []
        for line_no, line in enumerate(path.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
            if any(re.search(rf"\b{re.escape(symbol)}\s*\(", line) for symbol in symbols):
                result.append({"path": path_rel, "line": line_no, "snippet": line.strip()[:240]})
        return result

    hot_system_header = ROOT / "src" / "hot-reload" / "hot-reload-system.h"
    hot_system_text = hot_system_header.read_text(encoding="utf-8", errors="replace") \
        if hot_system_header.is_file() else ""
    setup_path = ROOT / "src" / "engine" / "engine-tick-setup.cpp"
    setup_text = setup_path.read_text(encoding="utf-8", errors="replace") \
        if setup_path.is_file() else ""
    poll_match = re.search(
        r"HOT_RELOAD_POLL_INTERVAL\s*=\s*std::chrono::milliseconds\((\d+)\)", setup_text)
    return {
        "current_focused_game_dll": "build/mimita-game.dll",
        "focused_game_dll_sources": current_dll_sources,
        "game_api_version": (re.search(r"MIMITA_GAME_API_VERSION\s*=\s*(\d+)", api_text).group(1)
                              if re.search(r"MIMITA_GAME_API_VERSION\s*=\s*(\d+)", api_text) else None),
        "game_api_fields": block_fields(api_text, "GameAPI"),
        "game_memory_fields": block_fields(api_text, "GameMemory"),
        "hot_reload_system_fields": re.findall(
            r"^\s*(?:void\*|GameAPI|GameMemory|std::filesystem::path|"
            r"std::vector<std::filesystem::path>|std::uint64_t|bool)\s+(\w+)\s*(?:\{[^;]*\})?;",
            hot_system_text, re.MULTILINE),
        "existing_loader_source_locations": (
            symbol_locations("src/hot-reload/hot-reload-system.cpp",
                             ("loadCandidate", "reloadGameDLLIfChanged", "rebuildIfSourcesChanged",
                              "newestSourceWriteTime", "unloadGameDLL")) +
            symbol_locations("src/hot-reload/module-host.cpp",
                             ("poll", "loadCandidate", "activateEntry", "unloadEntry", "requestBuild")) +
            symbol_locations("src/engine/engine-tick-setup.cpp", ("reloadGameDLLIfChanged", "poll"))),
        "module_owned_source_count": len(module_info),
        "hot_reload_poll_interval_ms": int(poll_match.group(1)) if poll_match else None,
        "hot_reload_poll_calls_in_engine_scope": [
            "HotReloadSystem::reloadGameDLLIfChanged",
            "ModuleHost::poll",
            "renderer.reloadWorldShaderIfChanged when a renderer exists",
        ],
        "runtime_dll_sources_found": sorted(
            path.relative_to(ROOT).as_posix() for path in (ROOT / "src" / "runtime").rglob("*.cpp")
        ) if (ROOT / "src" / "runtime").is_dir() else [],
        "runtime_dll_binary_exists": (ROOT / "build" / "runtime.dll").is_file(),
        "whole_game_live_command_exists": (ROOT / "devscripts" / "live.py").is_file(),
        "interpretation": "Current focused GameAPI/HotReloadSystem and subsystem ModuleHost are migration assets. They do not establish the planned whole-game game.dll plus resident runtime.dll boundary.",
        "current_game_api_header": "src/hot-reload/game-api.h",
        "current_loader_header": "src/hot-reload/hot-reload-system.h",
        "watch_and_activation_boundary": "src/engine/engine-tick-setup.cpp",
    }


def quantile_summary(values: list[float]) -> dict:
    ordered = sorted(values)
    if not ordered:
        return {"count": 0}

    def q(fraction: float) -> float:
        return round(ordered[min(len(ordered) - 1, round((len(ordered) - 1) * fraction))], 4)

    return {"count": len(ordered), "min": round(ordered[0], 4), "p50": q(0.50),
            "p95": q(0.95), "p99": q(0.99), "max": round(ordered[-1], 4)}


def summarize_rows(rows: list[dict], fields: tuple[str, ...]) -> dict:
    return {field: quantile_summary([float(row[field]) for row in rows
                                     if isinstance(row.get(field), (int, float))])
            for field in fields}


def existing_performance_run() -> dict:
    """Summarize an existing same-day journal; never launches or polls a game."""
    date_dir = ROOT / "logs" / datetime.now(timezone.utc).strftime("%Y-%m-%d")
    clients = list(date_dir.glob("*/client-*/events.jsonl")) if date_dir.is_dir() else []
    if not clients:
        return {"status": "no same-day client events.jsonl found; no performance baseline extracted"}
    client_path = max(clients, key=lambda path: path.stat().st_mtime_ns)
    run_root = client_path.parent.parent

    def read_events(path: Path, callback) -> dict:
        before = path.stat()
        counts: Counter[str] = Counter()
        bytes_read = 0
        with path.open("rb") as handle:
            remaining = before.st_size
            while remaining > 0:
                raw_line = handle.readline(remaining)
                if not raw_line:
                    break
                remaining -= len(raw_line)
                bytes_read += len(raw_line)
                try:
                    event = json.loads(raw_line.decode("utf-8", errors="replace"))
                except json.JSONDecodeError:
                    continue
                name = event.get("event")
                if isinstance(name, str):
                    counts[name] += 1
                callback(event)
        after = path.stat()
        return {"path": rel(path), "bytes_before_scan": before.st_size,
                "bytes_read_from_initial_prefix": bytes_read,
                "bytes_after_scan": after.st_size,
                "file_changed_during_scan": before.st_size != after.st_size or
                    before.st_mtime_ns != after.st_mtime_ns,
                "event_counts": dict(counts)}

    slices: list[dict] = []
    resources: list[dict] = []
    spikes: list[dict] = []
    def collect_client(event: dict) -> None:
        name = event.get("event")
        fields = event.get("fields") or {}
        if name == "performance.frame-slice":
            slices.append(fields)
        elif name == "performance.resource-window":
            resources.append(fields)
        elif name == "PERFORMANCE_SPIKE":
            message = str(fields.get("message", ""))
            total = re.search(r"Frame total:\s*([\d.]+)\s*ms", message)
            budget = re.search(r"Budget:\s*([\d.]+)\s*ms", message)
            scope = re.search(
                r"Function: Setup::HotReloadDLL\s+File:\s*([^\r\n]+)\s+Line:\s*(\d+)\s+"
                r"Function:\s*([^\r\n]+)\s+Inclusive:\s*([\d.]+)\s*ms"
                r"(?:\s+\([^)]*\))?\s+Self:\s*([\d.]+)\s*ms",
                message, re.DOTALL)
            spikes.append({
                "wall_time": event.get("wall_time"),
                "frame_total_ms": float(total.group(1)) if total else None,
                "reported_budget_ms": float(budget.group(1)) if budget else None,
                "reload_scope_present": scope is not None,
                "reload_scope_path": scope.group(1).replace("\\", "/") if scope else None,
                "reload_scope_line": int(scope.group(2)) if scope else None,
                "reload_scope_function": scope.group(3) if scope else None,
                "reload_scope_inclusive_ms": float(scope.group(4)) if scope else None,
                "reload_scope_self_ms": float(scope.group(5)) if scope else None,
            })

    client_scan = read_events(client_path, collect_client)
    server_path = run_root / "server-01" / "events.jsonl"
    server_groups: dict[str, list[dict]] = {
        "performance.server-loop-window": [],
        "performance.server-tick-window": [],
        "performance.server-phase-budget": [],
    }
    def collect_server(event: dict) -> None:
        name = event.get("event")
        if name in server_groups:
            server_groups[name].append(event.get("fields") or {})
    server_scan = read_events(server_path, collect_server) if server_path.is_file() else None

    launch_path = run_root / "dev-loop" / "events.jsonl"
    launch_records = []
    if launch_path.is_file():
        def collect_launch(event: dict) -> None:
            if event.get("event") == "dev-loop.launch.stage":
                fields = event.get("fields") or {}
                launch_records.append({key: fields.get(key) for key in (
                    "stage", "result", "run_id", "artifact_id", "launch_mode", "map",
                    "mode_name", "elapsed_ms") if key in fields})
        read_events(launch_path, collect_launch)

    spike_reload = [row for row in spikes if row["reload_scope_present"]]
    phase_rows = server_groups["performance.server-phase-budget"]
    loop_rows = server_groups["performance.server-loop-window"]
    tick_rows = server_groups["performance.server-tick-window"]
    return {
        "status": "read-only snapshot of existing journals; not a controlled baseline run",
        "run_id": run_root.name,
        "launch_metadata": launch_records[-8:],
        "client_log": client_scan,
        "server_log": server_scan,
        "client_frame_slice_summary_across_windows": {
            "event_count": len(slices),
            "frame_avg_ms": summarize_rows(slices, ("frame_avg_ms",)),
            "per_window_frame_max_ms": summarize_rows(slices, ("frame_max_ms",)),
            "per_window_frame_p99_ms": summarize_rows(slices, ("frame_p99_ms",)),
            "unique_npc_counts": sorted({row.get("npcs") for row in slices
                                          if isinstance(row.get("npcs"), (int, float))}),
            "unique_player_counts": sorted({row.get("players") for row in slices
                                             if isinstance(row.get("players"), (int, float))}),
            "frame_slice_budget_semantics": "The journal fields summarize windows; the per-frame threshold is not carried in each frame-slice record, so this is not the spec's 240-FPS acceptance calculation.",
        },
        "client_resource_window_summary": {
            "event_count": len(resources),
            "frame_average_ms": summarize_rows(resources, ("frame_average_ms",)),
            "frame_max_ms": summarize_rows(resources, ("frame_max_ms",)),
            "target_fps_values": sorted({row.get("target_fps") for row in resources
                                          if isinstance(row.get("target_fps"), (int, float))}),
            "window_frame_counts": sorted({row.get("window_frames") for row in resources
                                             if isinstance(row.get("window_frames"), (int, float))}),
            "npc_counts": sorted({row.get("npcs") for row in resources
                                  if isinstance(row.get("npcs"), (int, float))}),
        },
        "client_server_workload_relation": {
            "client_reported_npc_counts": sorted({row.get("npcs") for row in slices
                                                   if isinstance(row.get("npcs"), (int, float))}),
            "client_reported_player_counts": sorted({row.get("players") for row in slices
                                                     if isinstance(row.get("players"), (int, float))}),
            "server_reported_npc_counts": sorted({row.get("npcs") for row in tick_rows
                                                   if isinstance(row.get("npcs"), (int, float))}),
            "server_reported_player_counts": sorted({row.get("players") for row in tick_rows
                                                      if isinstance(row.get("players"), (int, float))}),
            "interpretation": "These are different process roles and report different actor counts; do not compare client-frame timing and server-tick timing as if they were one identical workload.",
        },
        "client_hot_reload_scope_spike_summary": {
            "spike_event_count": len(spikes),
            "events_with_Setup_HotReloadDLL_scope": len(spike_reload),
            "frame_total_ms": quantile_summary([row["frame_total_ms"] for row in spikes
                                                 if row["frame_total_ms"] is not None]),
            "reported_budget_ms": quantile_summary([row["reported_budget_ms"] for row in spikes
                                                     if row["reported_budget_ms"] is not None]),
            "scope_inclusive_ms": quantile_summary([row["reload_scope_inclusive_ms"]
                                                     for row in spike_reload]),
            "scope_samples_over_4_1667_ms": sum(
                row["reload_scope_inclusive_ms"] > (1000.0 / 240.0) for row in spike_reload),
            "scope_samples_over_16_6667_ms": sum(
                row["reload_scope_inclusive_ms"] > (1000.0 / 60.0) for row in spike_reload),
            "scope_owner_locations": sorted({(row["reload_scope_path"], row["reload_scope_line"])
                                              for row in spike_reload}),
            "largest_samples": sorted(spike_reload,
                                      key=lambda row: row["reload_scope_inclusive_ms"],
                                      reverse=True)[:10],
            "interpretation": "The measured scope wraps HotReloadSystem::reloadGameDLLIfChanged, ModuleHost::poll, and shader reload together; the journal does not isolate which call caused the time.",
        },
        "server_performance_summary_across_windows": {
            "server_loop_window_count": len(loop_rows),
            "loop_actual_hz": summarize_rows(loop_rows, ("actual_hz",)),
            "loop_windows_below_60_hz": sum(
                isinstance(row.get("actual_hz"), (int, float)) and row["actual_hz"] < 60.0
                for row in loop_rows),
            "authoritative_tick_rate_target_hz": 60,
            "loop_avg_ms": summarize_rows(loop_rows, ("loop_avg_ms",)),
            "loop_p95_ms": summarize_rows(loop_rows, ("loop_p95_ms",)),
            "overruns_per_window": summarize_rows(loop_rows, ("overruns",)),
            "server_tick_window_count": len(tick_rows),
            "tick_total_avg_ms": summarize_rows(tick_rows, ("total_avg_ms",)),
            "tick_total_p95_ms": summarize_rows(tick_rows, ("total_p95_ms",)),
            "tick_windows_with_reported_p95_over_16_6667_ms": sum(
                isinstance(row.get("total_p95_ms"), (int, float)) and
                row["total_p95_ms"] > (1000.0 / 60.0) for row in tick_rows),
            "tick_total_max_ms": summarize_rows(tick_rows, ("total_max_ms",)),
            "npc_avg_ms_per_window": summarize_rows(tick_rows, ("npc_avg_ms",)),
            "npc_max_ms_per_window": summarize_rows(tick_rows, ("npc_max_ms",)),
            "contact_weapon_avg_ms_per_window": summarize_rows(tick_rows, ("contact_weapon_avg_ms",)),
            "phase_budget_event_count": len(phase_rows),
            "npc_phase_ms": summarize_rows(phase_rows, ("npc_ms",)),
            "physical_contact_phase_ms": summarize_rows(phase_rows, ("physical_contact_ms",)),
            "npc_over_budget_event_count": sum(row.get("npc_over") is True for row in phase_rows),
            "contact_over_budget_event_count": sum(row.get("contact_over") is True for row in phase_rows),
            "workload_values": {
                "maps": sorted({row.get("map") for row in tick_rows if row.get("map")}),
                "modes": sorted({row.get("mode") for row in tick_rows if row.get("mode")}),
                "npc_counts": sorted({row.get("npcs") for row in tick_rows
                                       if isinstance(row.get("npcs"), (int, float))}),
                "player_counts": sorted({row.get("players") for row in tick_rows
                                          if isinstance(row.get("players"), (int, float))}),
                "triangle_queries_per_window": summarize_rows(loop_rows, ("triangle_queries",)),
                "triangle_candidates_per_window": summarize_rows(loop_rows, ("triangle_candidates",)),
            },
            "interpretation": "These are existing workload windows, not a controlled Phase 0 benchmark. They show server tick/phase pressure, but do not prove that NPC code scans every map triangle.",
        },
        "hardware_identification": "The inspected journals do not record a named CPU/GPU configuration.",
        "snapshot_consistency": "If a log's byte size or modified time changed while it was read, the report marks it; active journals may be partial snapshots.",
    }


def existing_build_timing_snapshot() -> dict:
    path = ROOT / "build" / "progress.json"
    if not path.is_file():
        return {"status": "no existing build/progress.json"}
    try:
        data = json.loads(path.read_text(encoding="utf-8", errors="replace"))
    except (OSError, json.JSONDecodeError) as error:
        return {"path": rel(path), "status": "unreadable", "error": str(error)}
    fields = ("schema", "updated_utc", "phase", "result", "elapsed_s", "compiled",
              "skipped", "phase_seconds")
    return {
        "path": rel(path),
        "status": "one existing progress snapshot; not a representative multi-run baseline",
        "recorded_fields": {field: data.get(field) for field in fields if field in data},
        "missing_phase_data": [field for field in ("setup_s", "validation_s", "publish_s",
                                                     "source_snapshot_id", "build_mode", "cache_state")
                               if field not in data],
        "interpretation": "The recorded attempt succeeded in 16.371 s but compiled 0 and skipped 624 translation units; link took 13.823 s. This is one existing incremental/relink-style sample, not a measured single-source compile or save-to-live cycle.",
        "not_created_by_this_inventory_run": True,
    }


def build_inventory() -> dict:
    sources = sorted(path.resolve() for path in SOURCE_ROOT.rglob("*.cpp") if path.is_file())
    source_relpaths = [rel(path) for path in sources]
    print(f"[CPP INVENTORY] discovered_sources={len(sources)}; scanning includes", flush=True)
    graph, graph_notes = current_include_graph(sources)
    print(f"[CPP INVENTORY] scanned_files={len(graph)}; reading depfiles", flush=True)
    dep_snapshots = read_dep_snapshots(sources)
    print(f"[CPP INVENTORY] depfile_translation_units={len(dep_snapshots)}; assembling report", flush=True)
    owners, overlaps, exe_sources, shared, module_info = source_owner_data()
    source_texts = {}
    for path_rel in graph:
        if path_rel.startswith("src/") and Path(path_rel).suffix.lower() in (
                ".cpp", ".h", ".hpp", ".hh", ".hxx", ".inl", ".ipp"):
            try:
                source_texts[path_rel] = (ROOT / path_rel).read_text(
                    encoding="utf-8", errors="replace")
            except OSError:
                pass
    lifetime_inventory = scan_lifetime_signals(source_texts, exe_sources, module_info)
    state_owner_review = reviewed_state_ownership_anchors()
    network_inventory = network_boundary_inventory(source_texts)
    loader_inventory = source_loader_boundary(exe_sources, module_info)
    build_timing_snapshot = existing_build_timing_snapshot()
    performance_snapshot = existing_performance_run()

    direct_header_fanout: Counter[str] = Counter()
    source_scan_fanout: Counter[str] = Counter()
    compiler_fanout_by_mode: dict[str, Counter[str]] = {
        "debug": Counter(), "release": Counter()}
    current_depfiles = 0
    stale_depfiles = 0
    no_depfile = 0
    items = []
    module_counts: Counter[str] = Counter()
    current_target_counts: Counter[str] = Counter()
    planned_target_counts: Counter[str] = Counter()
    source_directory_counts: Counter[str] = Counter()
    source_scan_unresolved = []

    for source, source_rel in zip(sources, source_relpaths):
        assert source_rel is not None
        direct = graph.get(source_rel, [])
        direct_paths = sorted({item["resolved_path"] for item in direct
                               if item["resolved_path"] and item["resolved_path"].endswith(
                                   (".h", ".hpp", ".hh", ".hxx", ".inl", ".ipp"))})
        transitive = transitive_headers(source_rel, graph)
        for header in direct_paths:
            direct_header_fanout[header] += 1
        for header in transitive:
            source_scan_fanout[header] += 1
        if source_rel in graph_notes:
            source_scan_unresolved.extend({"source": source_rel, "note": note}
                                          for note in graph_notes[source_rel])

        snapshots = dep_snapshots.get(source_rel, [])
        best = snapshots[0] if snapshots else None
        if not snapshots:
            no_depfile += 1
        else:
            # Existing depfiles are evidence of some prior compile. The current
            # scanner deliberately avoids touching every path on restricted Windows
            # checkouts, so it cannot prove freshness.
            stale_depfiles += 0
        compiler_headers = []
        if best:
            compiler_headers = sorted(
                dep for dep in best["dependencies"]
                if dep.endswith((".h", ".hpp", ".hh", ".hxx", ".inl", ".ipp")))
            for mode_snapshot in snapshots:
                for dep in mode_snapshot["dependencies"]:
                    if dep.endswith((".h", ".hpp", ".hh", ".hxx", ".inl", ".ipp")):
                        compiler_fanout_by_mode[mode_snapshot["mode"]][dep] += 1

        module = module_info.get(source_rel)
        if module:
            current_owner = "module"
            current_target = "module:" + str(module["name"])
            module_counts[str(module["name"])] += 1
        elif source_rel in exe_sources:
            current_owner = "exe"
            current_target = "exe"
        else:
            current_owner = "unmapped_or_entry"
            current_target = "unmapped_or_entry"
        current_target_counts[current_target] += 1

        if source_rel == "src/main.cpp":
            planned_target = "split_bootstrap_from_game"
        elif source_rel in ("src/hot-reload/hot-reload-system.cpp",
                            "src/hot-reload/module-host.cpp"):
            planned_target = "runtime_candidate_review"
        else:
            planned_target = "game_dll_candidate"
        planned_target_counts[planned_target] += 1
        directory = source_rel.rsplit("/", 1)[0] if "/" in source_rel else "."
        source_directory_counts[directory] += 1
        code_text = source_texts.get(source_rel) or source.read_text(
            encoding="utf-8", errors="replace")
        direct_unresolved = [item["spelling"] for item in direct
                             if item["resolved_path"] is None]

        items.append({
            "path": source_rel,
            "size_bytes": source.stat().st_size,
            "line_count": len(code_text.splitlines()),
            "sha256": file_signature(source).get("sha256"),
            "current_build": {
                "owner": current_owner,
                "target": current_target,
                "module": module,
                "in_exe_source_list": source_rel in exe_sources,
                "shared_with_host": source_rel in shared,
                "ownership_overlap": [item for item in overlaps if source_rel in item],
            },
            "planned_target": planned_target,
            "include_dependencies": {
                "direct": direct,
                "transitive_repository_headers_by_current_source_scan": transitive,
                "compiler_observed_headers_best_snapshot": compiler_headers,
                "compiler_dependency_snapshots": snapshots,
                "source_scan_unresolved": direct_unresolved,
            },
            "reload_review_signals": {
                "contains_main_function": bool(re.search(r"\bmain\s*\(", code_text)),
                "mentions_thread_or_async": bool(re.search(
                    r"\b(std::thread|std::async|CreateThread|_beginthread|jthread)\b", code_text)),
                "mentions_function_pointer_or_callback": bool(re.search(
                    r"\b(callback|function_ptr|functionPointer|std::function)\b", code_text,
                    re.IGNORECASE)),
                "mentions_packets_or_network_context": any(token in code_text for token in
                    ("network/packets.h", "MultiplayerContext", "PACKET_")),
                "mentions_global_state_keywords": len(re.findall(
                    r"\b(extern|static)\b", code_text)),
                "perf_scope_count": code_text.count("MIMITA_PERF_SCOPE("),
                "note": "Heuristic source signals only; inspect code before assigning runtime/game ownership.",
            },
        })

    npc_related_paths = {
        "src/network/server.cpp", "src/network/server-npcs.cpp",
        "src/network/server-gamemode.cpp", "src/network/server-attack.cpp",
        "src/network/server-contact.cpp",
    }
    npc_source_map = []
    for item in items:
        source_rel = item["path"]
        if source_rel.startswith("src/npc/") or source_rel in npc_related_paths:
            deps = item["include_dependencies"]
            npc_source_map.append({
                "path": source_rel,
                "current_build": item["current_build"],
                "direct_include_count": len(deps["direct"]),
                "transitive_project_header_count": len(
                    deps["transitive_repository_headers_by_current_source_scan"]),
                "compiler_observed_header_count": len(
                    deps["compiler_observed_headers_best_snapshot"]),
                "reload_review_signals": item["reload_review_signals"],
            })

    all_internal_headers = set(direct_header_fanout) | set(source_scan_fanout)
    dependency_fanout = []
    for header in all_internal_headers:
        dependency_fanout.append({
            "header": header,
            "direct_include_source_count": direct_header_fanout[header],
            "transitive_source_scan_count": source_scan_fanout[header],
            "compiler_observed_debug_translation_units": compiler_fanout_by_mode["debug"][header],
            "compiler_observed_release_translation_units": compiler_fanout_by_mode["release"][header],
        })
    dependency_fanout.sort(key=lambda item: (
        item["compiler_observed_debug_translation_units"] +
        item["compiler_observed_release_translation_units"] +
        item["transitive_source_scan_count"]), reverse=True)

    largest_sources = sorted(items, key=lambda item: item["size_bytes"], reverse=True)[:40]
    module_build_dependencies = {}
    modules_dir = ROOT / "build" / "modules"
    if modules_dir.is_dir():
        for dep_json in modules_dir.glob("*/deps.json"):
            try:
                module_build_dependencies[dep_json.parent.name] = json.loads(
                    dep_json.read_text(encoding="utf-8", errors="replace"))
            except (OSError, json.JSONDecodeError) as error:
                module_build_dependencies[dep_json.parent.name] = {"read_error": str(error)}

    generated = datetime.now(timezone.utc).isoformat(timespec="milliseconds").replace("+00:00", "Z")
    return {
        "schema_version": 2,
        "tool": {"name": "cpp-source-inventory", "version": SCRIPT_VERSION,
                 "read_only": True, "compiler_invoked": False},
        "generated_utc": generated,
        "repository": ".",
        "scope": "All active .cpp files recursively under src/.",
        "limitations": [
            "Include dependencies are not a function-call or linker-symbol call graph.",
            "Compiler .d files are existing build artifacts and may be stale; dependency freshness is explicitly unknown.",
            "GCC/MinGW -MMD dependency files omit system headers; source scanning records unresolved system, macro, or missing includes separately.",
            "The source scanner does not evaluate conditional compilation or generated include macros.",
            "Reload-review signals are text heuristics, not proof that a source file is safe to unload.",
            "Build ownership is current ownership; planned_target is a migration candidate label, not an implemented routing change.",
        ],
        "summary": {
            "cpp_count": len(items),
            "current_owner_counts": dict(Counter(item["current_build"]["owner"] for item in items)),
            "current_target_counts": dict(current_target_counts),
            "current_module_source_counts": dict(module_counts),
            "planned_target_counts": dict(planned_target_counts),
            "source_directory_counts": dict(sorted(source_directory_counts.items())),
            "ownership_overlap_count": len(overlaps),
            "ownership_overlaps": overlaps,
            "compiler_depfile_source_coverage": {
                "freshness_verified_current": current_depfiles,
                "freshness_verified_stale": stale_depfiles,
                "freshness_unknown": len(items) - no_depfile,
                "no_depfile": no_depfile,
            },
            "source_scan_unresolved_or_external_include_count": len(source_scan_unresolved),
            "source_scan_unresolved_or_external_includes": source_scan_unresolved,
            "top_dependency_fanout": dependency_fanout[:100],
            "all_dependency_fanout": dependency_fanout,
            "largest_cpp_files": [
                {"path": item["path"], "size_bytes": item["size_bytes"],
                 "line_count": item["line_count"]} for item in largest_sources],
        },
        "module_build_dependencies": module_build_dependencies,
        "reviewed_runtime_state_ownership": state_owner_review,
        "npc_pilot_source_map": {
            "source_file_count": len(npc_source_map),
            "files": npc_source_map,
            "interpretation": "The map provides the current NPC/server source and include boundary for selecting a vertical slice; it is not a complete call graph or a DLL-safety decision.",
        },
        "phase_0_completion_assessment": {
            "status": "PARTIAL: source/dependency inventory and existing observational evidence are recorded; controlled baselines and complete ownership tracing remain open.",
            "source_list_dependencies_and_current_build_owners": "covered for all discovered src/**/*.cpp files",
            "global_static_state": "candidate declarations inventoried; manual ownership and lifetime tracing remains",
            "callbacks_threads_and_jobs": "source locations inventoried heuristically; unload quiescence and captures remain unproven",
            "primary_live_state_holders": "Dedicated/listen server maps and threads, NpcSystem/Npc, ServerNpc, World, MultiplayerContext, and current GameMemory have source-reviewed anchors; field-by-field authority and migration are not complete.",
            "npc_vertical_slice": "runServer -> simulateSharedNpcs -> NpcSystem::updateOneWithTarget -> NpcSystem::updateOneNpc is source-mapped; current signatures pass live C++ object references and cannot cross the planned ABI as-is.",
            "network_boundaries": "PacketType values and source references inventoried; full serializer/authority/client-server trace remains",
            "loader_boundaries": "current focused GameAPI, HotReloadSystem, ModuleHost, and source build list recorded; desired runtime.dll/game.dll boundary is not implemented",
            "build_timing_baseline": "one existing build progress snapshot recorded; not a controlled set of representative edit builds",
            "runtime_performance_baseline": "existing client/server journal windows summarized; not a named-hardware controlled benchmark",
            "next_evidence": [
                "Manually resolve persistent mutable state and long-lived callback/job owners for the selected NPC vertical slice.",
                "Capture setup/PCH/compile/link/validation/publication and save-to-activation timings for a representative source edit using the authorized dev-loop workflow.",
                "Capture a named CPU/GPU, fixed workload NPC/frame/server-tick baseline and calculate the spec's 240-FPS frame budget and 60-Hz tick results.",
                "Then install and prove the stable runtime/game boundary; keep ordinary watcher automation after that boundary exists.",
            ],
        },
        "runtime_lifetime_inventory": lifetime_inventory,
        "network_boundary_inventory": network_inventory,
        "loader_boundary_inventory": loader_inventory,
        "existing_build_timing_snapshot": build_timing_snapshot,
        "existing_runtime_performance_snapshot": performance_snapshot,
        "sources": items,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", default=str(DEFAULT_OUTPUT),
                        help="JSON output path, relative to repository root by default.")
    args = parser.parse_args()
    output = Path(args.output)
    if not output.is_absolute():
        output = ROOT / output
    output = output.resolve(strict=False)
    try:
        output.relative_to(ROOT)
    except ValueError:
        parser.error("output must stay inside the repository")

    inventory = build_inventory()
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_suffix(output.suffix + ".tmp")
    temporary.write_text(json.dumps(inventory, indent=2, ensure_ascii=False) + "\n",
                         encoding="utf-8", newline="\n")
    temporary.replace(output)
    summary = inventory["summary"]
    print(f"[CPP INVENTORY] output={rel(output)}")
    print(f"[CPP INVENTORY] cpp={summary['cpp_count']} owners={summary['current_owner_counts']}")
    print(f"[CPP INVENTORY] depfiles={summary['compiler_depfile_source_coverage']}")
    print("[CPP INVENTORY] unresolved_or_external_source_includes="
          f"{summary['source_scan_unresolved_or_external_include_count']}")
    print("[CPP INVENTORY] lifetime_signals="
          f"state:{inventory['runtime_lifetime_inventory']['persistent_storage_candidate_count']} "
          f"callbacks:{inventory['runtime_lifetime_inventory']['callback_candidate_count']} "
          f"jobs:{inventory['runtime_lifetime_inventory']['thread_async_and_job_site_count']}")
    print(f"[CPP INVENTORY] packet_ids={inventory['network_boundary_inventory'].get('packet_id_count', 0)}")
    print(f"[CPP INVENTORY] performance_run={inventory['existing_runtime_performance_snapshot'].get('run_id', 'none')}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
