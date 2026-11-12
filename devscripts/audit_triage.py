#!/usr/bin/env python3
# 2026-11-12
# purpose
# Static triage for the whole-game hot-reload audit. Classifies every repository
# .cpp into:
#   NON_GAME                  explicit vendor/tool/test/launcher/module exception
#   RESIDENT_HOST             stable host/bootstrap/loader/device/transport code
#   RUNTIME_PROOF_REQUIRED    threads/jobs/callbacks/resources/network state
#   STATIC_STATE_REVIEW       mutable static/extern state, no async/resource hit
#   STATIC_CLOSABLE_CANDIDATE no lexical hit; still requires a confirmation read
#
# This is a map, not a semantic pass. Absence of lexical hits is not proof that a
# file is safe to unload. Known blockers from the handoff batches are hard-coded.
#
# Does not compile, link, or launch anything.

from __future__ import annotations

import argparse
import csv
import json
import re
from collections import Counter, defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
INVENTORY_REL = Path("docs/handoffs/cpp-source-inventory.json")
REPORT_REL = Path("docs/handoffs/cpp-audit-static-triage-20261112.md")
CSV_REL = Path("docs/handoffs/cpp-audit-static-triage-20261112.csv")
SERVICE_MAP_REL = Path("docs/handoffs/resident-service-dependencies-20261112.json")
MANIFEST_REL = Path("build/source-manifest.json")
FAST_PROGRESS_REL = Path("build/audit-triage-progress.json")

# Source-backed blockers from docs/handoffs/cpp-lifetime-*.md and the lifetime
# review. These cannot be closed by a lexical scan.
KNOWN_RUNTIME_BLOCKERS = {
    "src/analytics/analytics-uploader.cpp",
    "src/audio/music-manager.cpp",
    "src/avatar/avatar-atlas.cpp",
    "src/combat/weapon-model-cache.cpp",
    "src/debug/log-manager.cpp",
    "src/duel/duel-queue.cpp",
    "src/engine/engine-tick-replay.cpp",
    "src/entities/player-loader.cpp",
    "src/network/server-authored-rebuild.cpp",
    "src/network/server-browser.cpp",
    "src/persistence/persistence-queue.cpp",
    "src/replay/replay-export.cpp",
    "src/replay/replay-export-json.cpp",
    "src/utils/json-file-watch.cpp",
}

# Rows closed by a confirmation read (see docs/handoffs/cpp-audit-batch-pure-npc-20261112.md).
CONFIRMED_STATIC = {
    "src/npc/npc-targeting.cpp":
        "pure policy; no state/callback/thread/resource; callers are game-owned",
    "src/npc/npc-movement-context.cpp":
        "pure enum/string helpers; no state/callback/thread/resource",
    "src/npc/npc-navigation-settings.cpp":
        "pure JSON schema parse/clamp; no state/callback/thread/resource",
    "src/npc/npc-movement-policy.cpp":
        "pure JSON policy schema/query helpers; no state/callback/thread/resource",
    "src/npc/npc-nav-request.cpp":
        "pure request-to-goal and segment classification; no state",
    "src/npc/npc-surface.cpp":
        "pure surface/capability naming and normal classification; no state",
    "src/physics/mesh-mass-properties.cpp":
        "pure mesh volume/moment integrator; no state/callback/thread/resource",
    "src/combat/attack-timeline.cpp":
        "pure per-instance attack phase timeline; no global/async/resource",
    "src/combat/spyknife-damage.cpp":
        "pure damage evaluation over caller-owned weapon/impact values",
    "src/game/host-log-bridge-module.cpp":
        "CAP_LOG bridge; generation-scoped capability pointers; no persistent state/resource",
    "src/game/host-compat-bridge-module.cpp":
        "pure nowMs and CAP_LOG crash-breadcrumb bridge; no persistent state/resource",
    "src/game/transport-bridge-module.cpp":
        "CAP_TRANSPORT bridge; generation-scoped capability pointer; no socket/session ownership",
    "src/game/module-host-bridge-module.cpp":
        "partial CAP_MODULES bridge; generation-scoped capability pointer; no module state ownership",
    "src/game/audio-bridge-module.cpp":
        "CAP_AUDIO bridge; generation-scoped capability pointer; no device/voice ownership",
    "src/game/effects-bridge-module.cpp":
        "CAP_EFFECTS bridge; generation-scoped capability pointer; host owns the effect pool",
    "src/game/entity-bridge-module.cpp":
        "CAP_ENTITY bridge; bounded snapshots and stable handles only; no state ownership",
    "src/game/lifetime-bridge-module.cpp":
        "CAP_JOBS/CAP_CALLBACKS/CAP_RESOURCES bridge; generation leases only",
    "src/game/render-bridge-module.cpp":
        "CAP_RENDER bridge; bounded command stream only; no device ownership",
    "src/game/probe/npc-target-probe-module.cpp":
        "validation-only probe module; no persistent state/resource",
    "src/gui/hud/chat-history.cpp":
        "per-instance ring buffer methods; no global/async/resource; transient UI state",
    "src/gui/hud/ui-tick-clock.cpp":
        "per-instance fixed-step UI clock; no global/async/resource; transient UI state",
    "src/entities/aim-fov.cpp":
        "pure FOV blend/easing/application over caller values; no state",
    "src/entities/animation-rotation.cpp":
        "pure rotation encoding and SLERP math; no state",
    "src/gamemode/action-graph.cpp":
        "pure manifest-to-action scheduling queries; no global/async/resource",
    "src/gamemode/disaster-runtime.cpp":
        "pure deterministic disaster selection/rules; no global/async/resource",
    "src/gamemode/payload-runtime.cpp":
        "pure deterministic payload runtime over caller-owned runtime data; no global/async/resource",
    "src/persistence/persistence-rewards.cpp":
        "immutable const reward defaults; no mutable state/async/resource",
    "src/persistence/persistence-level.cpp":
        "pure XP-to-level math; no state/async/resource",
    "src/actor/actor-intent.cpp":
        "pure ActorIntent->InputState adapter over caller values; no state",
    "src/network/projectile-terminal-dedupe.cpp":
        "per-instance bounded tombstone set; no global/async/resource",
    "src/analytics/analytics-events.cpp":
        "immutable constexpr event table; no mutable state/async/resource",
    "src/network/chat-rate-limiter.cpp":
        "per-instance token-bucket state machine; no global/async/resource",
    "src/network/connection-health.cpp":
        "pure connection-health state machine over caller timestamps; no state",
    "src/devtools/command-search.cpp":
        "pure fuzzy-match helper over caller strings; no state",
    "src/map/map-loader-gltf-builder.cpp":
        "empty builder translation unit; no code, state, async, or resource",
    "src/map/map-loader-mesh.cpp":
        "pure triangle-normal generation over caller vertex data",
    "src/network/badconn/badconn-reorder.cpp":
        "pure per-packet reorder roll over caller RNG/state",
    "src/network/badconn/badconn-loss.cpp":
        "pure per-packet loss/burst roll over caller RNG/state",
    "src/network/badconn/badconn-blackout.cpp":
        "pure blackout state machine over caller state",
    "src/network/badconn/badconn-latency.cpp":
        "bounded latency queue helpers over caller simulator state",
    "src/map/map-catalog.cpp":
        "pure filesystem catalog scan; no persistent handle or service",
    "src/tiny_obj_loader_declare.cpp":
        "header-library implementation TU; no host service or global target",
    "src/tinygltf_declare.cpp":
        "header-library implementation TU; no host service or global target",
    "src/utils/stb_image_impl.cpp":
        "header-library implementation TU; no host service or global target",
    "src/combat/area-effect.cpp":
        "pure area-effect value/geometry helpers; no global/async/resource",
    "src/fister/sim/FisterSim.cpp":
        "pure fister simulation math over caller-owned state; no global/async/resource",
    "src/fister/sim/Pose.cpp":
        "pure pose math over caller-owned state; no global/async/resource",
    "src/game/objective-state.cpp":
        "pure objective-state helpers over caller values; no global/async/resource",
    "src/network/avatar-transfer.cpp":
        "pure avatar transfer encoding helpers over caller values",
    "src/network/net_mode.cpp":
        "pure launch-mode parsing; no global/async/resource",
    "src/physics/movement/actor-triangle-spike.cpp":
        "pure triangle-spike diagnostics math over caller values",
    "src/video/frame-pacer.cpp":
        "per-instance frame pacing math; no global/async/resource",
    "src/vip/vip-name-render.cpp":
        "pure VIP name/visual value helpers; no global/async/resource",
    "src/replay/replay-factory-search.cpp":
        "transient filesystem replay scan; handles are per-call, no persistent resource",
    "src/replay/replay-io.cpp":
        "transient replay file read/write helpers; handles are per-call",
    "external/recastnavigation/Detour/Source/DetourCommon.cpp":
        "vendored Detour algorithm; static helpers only, no host service/thread/resource",
    "external/recastnavigation/Detour/Source/DetourNavMesh.cpp":
        "vendored Detour algorithm; static helpers only, no host service/thread/resource",
    "external/recastnavigation/Detour/Source/DetourNavMeshQuery.cpp":
        "vendored Detour algorithm; static helpers only, no host service/thread/resource",
    "external/recastnavigation/Detour/Source/DetourNode.cpp":
        "vendored Detour algorithm; static helpers only, no host service/thread/resource",
    "external/recastnavigation/DetourTileCache/Source/DetourTileCacheBuilder.cpp":
        "vendored Detour tile-cache algorithm; static helpers only, no host service/thread/resource",
    "external/recastnavigation/Recast/Source/Recast.cpp":
        "vendored Recast algorithm; static helpers only, no host service/thread/resource",
    "external/recastnavigation/Recast/Source/RecastArea.cpp":
        "vendored Recast algorithm; static helpers only, no host service/thread/resource",
    "external/recastnavigation/Recast/Source/RecastFilter.cpp":
        "vendored Recast algorithm; static helpers only, no host service/thread/resource",
    "external/recastnavigation/Recast/Source/RecastLayers.cpp":
        "vendored Recast algorithm; static helpers only, no host service/thread/resource",
    "external/recastnavigation/Recast/Source/RecastMeshDetail.cpp":
        "vendored Recast algorithm; static helpers only, no host service/thread/resource",
    "external/recastnavigation/Recast/Source/RecastRegion.cpp":
        "vendored Recast algorithm; static helpers only, no host service/thread/resource",
}

# Confirmation-read rows that need a resident adapter before they can be game.dll
# members. The blocker is a named service or a host-owned pointer, not unload risk
# by itself.
KNOWN_ADAPTERS = {
    "src/npc/npc-states.cpp": "NpcDifficultyConfig::instance config owner",
    "src/npc/npc-traversal.cpp": "ModuleHost policy calls + MovementJsonConfig::instance",
    "src/npc/npc-state-machine.cpp": "ModuleHost hold-pursuit calls",
    "src/npc/npc-perception.cpp": "ModuleHost perception calls",
    "src/npc/npc-utility-bridge.cpp": "ModuleHost behavior calls",
    "src/npc/npc-world-query.cpp": "thread_local const World* host pointer",
    "src/npc/team-brain.cpp": "StructuredLogger host logging calls",
    "src/physics/movement/physics-down-dash.cpp": "mutates Player host state + movement config helper",
    "src/physics/movement/physics-freeze.cpp": "mutates Player host state + movement config helper",
    "src/physics/movement/physics-friction.cpp": "mutates Player host state + Debug logging + physics config",
    "src/physics/movement/physics-gravity.cpp": "mutates Player host state + Debug logging + physics config",
    "src/physics/movement/physics-ground-return.cpp": "mutates Player host state + Debug logging + physics config",
    "src/physics/movement/physics-jump.cpp": "mutates Player host state + SizeScalingConfig + Debug logging",
    "src/physics/movement/physics-walk.cpp": "mutates Player host state + SizeScalingConfig + Debug logging",
    "src/physics/movement/movement-conversion.cpp": "Player<->MovementState conversion over host state",
    "src/physics/ray-utils.cpp": "const World& reference; needs a plain-span or resident-caller adapter",
    "src/physics/movement/physics-collision-glb-safety.cpp": "mutates Player + World query + Debug logging",
    "src/combat/weapon-godball.cpp": "host Player/Camera/NpcSystem references",
    "src/combat/shot-profiler.cpp": "global ShotProfiler* gShotProfiler pointer",
    "src/engine/engine-frame.cpp": "delegates to the resident Renderer/device",
    "src/map/map-loader-gltf-verts.cpp": "Debug::logAuto GLB logging service",
    "src/perf/perf-allocator.cpp": "global operator new/delete and allocation counters must stay resident",
    "src/devtools/terminal-input.cpp": "implements the resident Terminal service input path",
    "src/devtools/terminal-render.cpp": "implements the resident Terminal service render path",
    "src/map/map-loader-gltf-accessors.cpp": "Debug::logAuto GLB logging service",
    "src/network/remote-entity-lifecycle.cpp": "takes Player& host state",
    "src/replay/replay-recorder-clips.cpp": "Debug::warn replay logging service",
}

GL_RE = re.compile(r"\bgl[A-Z][A-Za-z0-9_]*\s*\(|\bGLuint\b|\bGLFW[a-z]+\b")
AUDIO_RE = re.compile(r"\bma_(engine|sound|device)\w*\b|miniaudio")
NET_RE = re.compile(r"\bSOCKET\b|\bsocket\s*\(|\bWSA[A-Za-z]+\b|\bjuice_\w+\b|\bUdpTransport\b|WinHTTP")
FILE_RE = re.compile(r"\bstd::(?:o|i)?fstream\b|\bstd::filesystem\b|\bfopen\s*\(|\bCreateFileW?\b|\bWriteFile\b")
OS_RE = re.compile(r"\bCreateThread\b|\b_beginthread\b|\bLoadLibraryW?\b|\bShellExecute\w*\b|\bstd::system\b")
THREAD_LOCAL_RE = re.compile(r"\bthread_local\b")
# File-scope mutable definitions such as `std::mutex gSessionMutex;`,
# `std::unordered_map<...> gCache;`, `uint64_t gClock = 0;`, or
# `ShotProfiler* gShotProfiler = nullptr;`. These are persistent process state
# that the inventory's `static` scan does not catch.
GLOBAL_DEF_RE = re.compile(
    r"^(?:std::[\w:]+(?:<[^;]+>)?|[\w:]+(?:\s*\*)?)\s+g[A-Z]\w*\s*(?:=|;|\{)",
    re.MULTILINE)
STATIC_FUNCTION_RE = re.compile(r"^\s*static\s+[\w:<>,*&\s]+\s+\w+\s*\(")
STATIC_VAR_RE = re.compile(r"^\s*(?:static|extern)\s+[^;{()]*\b\w+\s*(?:=|;|\[)")
CALLBACK_RE = re.compile(r"\bstd::function\b|\bcallback\b", re.IGNORECASE)
THREAD_RE = re.compile(r"\bstd::thread\b|\bstd::async\b|\bCreateThread\b|\b_beginthread\b|\bstd::jthread\b")

# Named resident services/globals a game source may call. Used to build the
# adapter dependency map, not only a yes/no adapter flag.
SERVICE_PATTERNS = {
    "StructuredLogger": re.compile(r"\bStructuredLogger::instance\s*\("),
    "ModuleHost": re.compile(r"\bModuleHost::instance\s*\("),
    "HotReloadSystem": re.compile(r"\bHotReloadSystem::instance\s*\("),
    "LogManager": re.compile(r"\bLogManager::instance\s*\("),
    "AnalyticsManager": re.compile(r"\bAnalyticsManager::instance\s*\("),
    "LocalProfileSystem": re.compile(r"\bLocalProfileSystem::instance\s*\("),
    "Perf": re.compile(r"\bPerf::"),
    "serverGamemodeState": re.compile(r"\bserverGamemodeState\s*\("),
    "GamemodeRegistry": re.compile(r"\bGamemodeRegistry::instance\s*\("),
    "WeaponRegistry": re.compile(r"\bWeaponRegistry::instance\s*\("),
    "SizeScalingConfig": re.compile(r"\bSizeScalingConfig::instance\s*\("),
    "MovementJsonConfig": re.compile(r"\bMovementJsonConfig::instance\s*\("),
    "NpcDifficultyConfig": re.compile(r"\bNpcDifficultyConfig::instance\s*\("),
    "BehaviorProfileRegistry": re.compile(r"\bBehaviorProfileRegistry::instance\s*\("),
    "ActorCollisionConfig": re.compile(r"\bActorCollisionConfig::instance\s*\("),
    "DebugLog": re.compile(r"\bDebug::log(?:Throttled)?\s*\("),
    "Terminal": re.compile(r"\bTerminal::instance\s*\("),
    "AuthSystem": re.compile(r"\bAuthSystem::instance\s*\("),
    "TerminalState": re.compile(r"#include\s+[<\"]terminal/terminal-state\.h[>\"]"),
    "MultiplayerContextGlobal": re.compile(r"\bgpMpContext\b"),
    "GuiLayoutManager": re.compile(r"\bGuiLayoutManager::instance\s*\("),
    "MapConfigRegistry": re.compile(r"\bMapConfigRegistry::instance\s*\("),
    "PersistenceQueue": re.compile(r"\bPersistenceQueue::instance\s*\("),
    "CharacterRegistry": re.compile(r"\bCharacterRegistry::instance\s*\("),
    "PhysicalEntitySystem": re.compile(r"\bPhysicalEntitySystem::instance\s*\("),
    "ConstraintSystem": re.compile(r"\bConstraintSystem::instance\s*\("),
    "DebugVis": re.compile(r"\bDebugVis::"),
    "VehicleSystem": re.compile(r"\bVehicleSystem::instance\s*\("),
    "VehicleConfigRegistry": re.compile(r"\bVehicleConfigRegistry::instance\s*\("),
}


def resident_services(text: str) -> list[str]:
    return sorted(name for name, pattern in SERVICE_PATTERNS.items()
                  if pattern.search(text))


def read_text(path: Path) -> str:
    try:
        return path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return ""


def is_const_static(snippet: str) -> bool:
    return "const" in snippet


def resource_hits(text: str) -> list[str]:
    hits: list[str] = []
    for name, pattern in (("gl", GL_RE), ("audio", AUDIO_RE), ("net", NET_RE),
                          ("file", FILE_RE), ("os", OS_RE)):
        if pattern.search(text):
            hits.append(name)
    return hits


def fast_signals(text: str) -> dict:
    statics: list[dict] = []
    callbacks: list[dict] = []
    threads: list[dict] = []
    for index, line in enumerate(text.splitlines(), start=1):
        snippet = line.strip()
        if STATIC_VAR_RE.match(line) and not STATIC_FUNCTION_RE.match(line):
            statics.append({"line": index, "snippet": snippet[:160]})
        if CALLBACK_RE.search(line):
            callbacks.append({"line": index, "snippet": snippet[:160]})
        if THREAD_RE.search(line):
            threads.append({"line": index, "snippet": snippet[:160]})
    return {
        "contains_main_function": bool(re.search(r"^\s*(?:int|auto)\s+main\s*\(", text, re.MULTILINE)),
        "mentions_thread_or_async": bool(THREAD_RE.search(text)),
        "mentions_function_pointer_or_callback": bool(CALLBACK_RE.search(text)),
        "mentions_packets_or_network_context": any(token in text for token in
            ("network/packets.h", "MultiplayerContext", "PACKET_")),
        "mentions_global_state_keywords": len(re.findall(r"\b(?:extern|static)\b", text)),
        "perf_scope_count": text.count("MIMITA_PERF_SCOPE("),
        "lifetime_review_candidates": {
            "static_or_extern_candidates": statics,
            "callback_candidates": callbacks,
            "thread_or_job_candidates": threads,
        },
        "note": "fast manifest scan; skips depfiles/transitive headers",
    }


def items_from_manifest(manifest: dict) -> list[dict]:
    items: list[dict] = []
    sources = manifest.get("sources", [])
    total = len(sources)
    progress_path = ROOT / FAST_PROGRESS_REL
    progress_path.parent.mkdir(parents=True, exist_ok=True)
    for index, entry in enumerate(sources, start=1):
        path = entry.get("path", "")
        if not path.endswith(".cpp"):
            continue
        text = read_text(ROOT / path)
        owner = entry.get("owner", "unresolved")
        if owner.startswith("module:"):
            current_owner, target = "module", "module"
        elif owner in {"resident", "bootstrap"}:
            current_owner, target = "exe", "resident"
        else:
            current_owner, target = "exe", "game_dll_candidate"
        items.append({
            "path": path,
            "source_kind": "game_source_candidate" if owner == "game" else owner,
            "active_in_canonical_game_build": owner not in {"vendor", "test", "tool", "launcher"},
            "directly_compiled_into_mimita_exe": owner not in {"vendor", "test", "tool", "launcher"},
            "currently_owned_by_subsystem_module": owner.startswith("module:"),
            "in_linked_library_source_manifest": False,
            "size_bytes": (ROOT / path).stat().st_size if (ROOT / path).is_file() else 0,
            "line_count": len(text.splitlines()),
            "sha256": entry.get("sha256", ""),
            "current_build": {"owner": current_owner, "target": target,
                              "module": owner.split(":", 1)[1] if owner.startswith("module:") else None,
                              "in_exe_source_list": owner not in {"vendor", "test", "tool", "launcher"},
                              "shared_with_host": False, "ownership_overlap": [],
                              "additional_link_targets": [],
                              "membership_evidence": "source manifest fast scan"},
            "planned_target": target,
            "include_dependencies": {"direct": [],
                                     "transitive_repository_headers_by_current_source_scan": [],
                                     "compiler_observed_headers_best_snapshot": [],
                                     "compiler_dependency_snapshots": [],
                                     "source_scan_unresolved": []},
            "owner_category": owner,
            "owner_reason": "source manifest",
            "reload_review_signals": fast_signals(text),
        })
        if index % 100 == 0 or index == total:
            progress_path.write_text(json.dumps({"mode": "manifest-fast", "done": index,
                                                 "total": total, "path": path}, indent=2) + "\n",
                                     encoding="utf-8")
            print(f"[AUDIT TRIAGE] fast scan {index}/{total}", flush=True)
    return items


def classify(item: dict, text: str) -> tuple[str, list[str]]:
    owner = item.get("owner_category") or "unresolved"
    path = item.get("path", "")
    reasons: list[str] = []

    if owner.startswith("module:"):
        return "NON_GAME", ["existing subsystem module"]
    if owner in {"vendor", "tool", "test", "launcher"}:
        return "NON_GAME", [f"owner={owner}"]
    if owner in {"resident", "bootstrap"}:
        return "RESIDENT_HOST", [f"owner={owner}"]
    if owner == "unresolved":
        return "UNRESOLVED", ["unknown owner"]
    if item.get("path") in CONFIRMED_STATIC:
        return "STATIC_CLOSED", [CONFIRMED_STATIC[item["path"]]]

    signals = item.get("reload_review_signals", {})
    candidates = signals.get("lifetime_review_candidates", {})
    static_candidates = candidates.get("static_or_extern_candidates", [])
    mutable_static = [entry for entry in static_candidates
                      if not is_const_static(entry.get("snippet", ""))]
    callbacks = candidates.get("callback_candidates", [])
    threads = candidates.get("thread_or_job_candidates", [])

    if item.get("path") in KNOWN_RUNTIME_BLOCKERS:
        reasons.append("known handoff blocker")
    if threads:
        reasons.append(f"thread/job hits={len(threads)}")
    if callbacks:
        reasons.append(f"callback hits={len(callbacks)}")
    resources = resource_hits(text)
    if resources:
        reasons.append("resource hits=" + ",".join(resources))
    services = resident_services(text)
    if signals.get("mentions_packets_or_network_context"):
        reasons.append("network context")
    if signals.get("contains_main_function"):
        reasons.append("main function")

    if reasons:
        return "RUNTIME_PROOF_REQUIRED", reasons
    if item.get("path") in KNOWN_ADAPTERS:
        detail = KNOWN_ADAPTERS[item["path"]]
        if services:
            detail += " (services: " + ", ".join(services) + ")"
        return "RESIDENT_ADAPTER_REQUIRED", [detail]
    if THREAD_LOCAL_RE.search(text):
        detail = "thread_local host state"
        if services:
            detail += " (services: " + ", ".join(services) + ")"
        return "RESIDENT_ADAPTER_REQUIRED", [detail]
    if services:
        return "RESIDENT_ADAPTER_REQUIRED", ["resident services: " + ", ".join(services)]
    global_defs = GLOBAL_DEF_RE.findall(text)
    if global_defs:
        return "STATIC_STATE_REVIEW", [f"file-scope globals={len(global_defs)}"]
    if mutable_static:
        return "STATIC_STATE_REVIEW", [f"mutable static candidates={len(mutable_static)}"]
    if signals.get("mentions_global_state_keywords", 0) and not mutable_static:
        return "STATIC_STATE_REVIEW", [
            f"global/extern keyword mentions={signals.get('mentions_global_state_keywords')}"]
    return "STATIC_CLOSABLE_CANDIDATE", ["no lexical lifetime hit; confirmation read required"]


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Static audit triage")
    parser.add_argument("--inventory", default=str(INVENTORY_REL))
    parser.add_argument("--report", default=str(REPORT_REL))
    parser.add_argument("--csv", default=str(CSV_REL))
    parser.add_argument("--service-map", default=str(SERVICE_MAP_REL))
    parser.add_argument("--manifest", default=str(MANIFEST_REL))
    parser.add_argument("--from-manifest", action="store_true",
                        help="fast mode: use the source manifest; skip depfiles/transitive headers")
    parser.add_argument("--full", action="store_true", help="force the full inventory input")
    args = parser.parse_args(argv)

    use_fast = not args.full and (
        args.from_manifest or (ROOT / args.manifest).is_file())
    if use_fast:
        manifest_path = ROOT / args.manifest
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        sources = items_from_manifest(manifest)
        source_label = "manifest-fast"
        print(f"[AUDIT TRIAGE] mode=manifest-fast rows={len(sources)} "
              f"watch={FAST_PROGRESS_REL.as_posix()}", flush=True)
    else:
        inventory_path = ROOT / args.inventory
        inventory = json.loads(inventory_path.read_text(encoding="utf-8"))
        sources = inventory.get("sources", [])
        source_label = "full-inventory"
        print(f"[AUDIT TRIAGE] mode=full-inventory rows={len(sources)} "
              f"watch={args.inventory}", flush=True)

    rows: list[dict] = []
    service_map: dict[str, list[str]] = defaultdict(list)
    path_services: dict[str, list[str]] = {}
    for item in sources:
        path = item.get("path", "")
        text = read_text(ROOT / path) if path.endswith(".cpp") else ""
        services = resident_services(text)
        if services:
            path_services[path] = services
            for service in services:
                service_map[service].append(path)
        category, reasons = classify(item, text)
        rows.append({
            "path": path,
            "owner": item.get("owner_category", "unresolved"),
            "class": category,
            "reasons": "; ".join(reasons),
            "line_count": item.get("line_count", 0),
            "sha256": item.get("sha256", ""),
        })

    counts = Counter(row["class"] for row in rows)
    by_dir: dict[str, Counter] = defaultdict(Counter)
    for row in rows:
        if row["class"] == "STATIC_CLOSABLE_CANDIDATE":
            parts = row["path"].split("/")
            by_dir[parts[1] if len(parts) > 1 else parts[0]][row["path"]] += 1

    csv_path = ROOT / args.csv
    csv_path.parent.mkdir(parents=True, exist_ok=True)
    with open(csv_path, "w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=["path", "owner", "class", "reasons",
                                                    "line_count", "sha256"])
        writer.writeheader()
        writer.writerows(rows)

    service_map_path = ROOT / args.service_map
    service_map_path.parent.mkdir(parents=True, exist_ok=True)
    service_map_path.write_text(json.dumps({
        "services": {name: sorted(paths) for name, paths in sorted(service_map.items())},
        "paths": {path: services for path, services in sorted(path_services.items())},
    }, indent=2, sort_keys=True) + "\n", encoding="utf-8")

    report_path = ROOT / args.report
    lines: list[str] = []
    lines.append("# Static audit triage (2026-11-12)")
    lines.append("")
    lines.append("**Status:** heuristic triage, not semantic closure. Absence of a lexical hit is a")
    lines.append("lead, not proof that a source is safe to unload. Every `STATIC_CLOSABLE_CANDIDATE`")
    lines.append("still needs a confirmation read of its declarations, call graph, and resources.")
    lines.append("")
    lines.append(f"- Source: `{source_label}` (`{args.manifest if use_fast else args.inventory}`)")
    lines.append(f"- Machine table: `{args.csv}`")
    lines.append(f"- Rows: {len(rows)}")
    lines.append("")
    lines.append("## Classification counts")
    lines.append("")
    lines.append("| Class | Rows | Meaning |")
    lines.append("|---|---:|---|")
    meanings = {
        "STATIC_CLOSED": "confirmation read closed; see batch record",
        "NON_GAME": "vendor/tool/test/launcher/module; explicit exception",
        "RESIDENT_HOST": "host/bootstrap/loader/device/transport; not game.dll",
        "RUNTIME_PROOF_REQUIRED": "threads/jobs/callbacks/resources/network state",
        "RESIDENT_ADAPTER_REQUIRED": "calls a resident service or holds thread_local host state",
        "STATIC_STATE_REVIEW": "mutable static/extern state; read and assign owner",
        "STATIC_CLOSABLE_CANDIDATE": "no lexical hit; confirmation read still required",
        "UNRESOLVED": "unknown owner; blocking",
    }
    for name in ("STATIC_CLOSED", "STATIC_CLOSABLE_CANDIDATE", "STATIC_STATE_REVIEW",
                 "RESIDENT_ADAPTER_REQUIRED", "RUNTIME_PROOF_REQUIRED", "RESIDENT_HOST",
                 "NON_GAME", "UNRESOLVED"):
        lines.append(f"| {name} | {counts.get(name, 0)} | {meanings[name]} |")
    lines.append("")
    lines.append("## Confirmed static closures")
    lines.append("")
    for path, note in sorted(CONFIRMED_STATIC.items()):
        lines.append(f"- `{path}` - {note}")
    lines.append("")
    lines.append("## STATIC_CLOSABLE_CANDIDATE rows")
    lines.append("")
    lines.append("These are the rows most likely to close with one focused read: no thread, job,")
    lines.append("callback, GL/audio/network/file/OS, main, or mutable-static hit.")
    lines.append("")
    for directory in sorted(by_dir, key=lambda name: (-sum(by_dir[name].values()), name)):
        paths = sorted(by_dir[directory])
        lines.append(f"### {directory} ({len(paths)})")
        lines.append("")
        for path in paths:
            lines.append(f"- `{path}`")
        lines.append("")
    lines.append("## STATIC_STATE_REVIEW rows")
    lines.append("")
    for row in rows:
        if row["class"] == "STATIC_STATE_REVIEW":
            lines.append(f"- `{row['path']}` - {row['reasons']}")
    lines.append("")
    lines.append("## RESIDENT_ADAPTER_REQUIRED rows")
    lines.append("")
    for row in rows:
        if row["class"] == "RESIDENT_ADAPTER_REQUIRED":
            lines.append(f"- `{row['path']}` - {row['reasons']}")
    lines.append("")
    lines.append("## RUNTIME_PROOF_REQUIRED rows")
    lines.append("")
    for row in rows:
        if row["class"] == "RUNTIME_PROOF_REQUIRED":
            lines.append(f"- `{row['path']}` - {row['reasons']}")
    lines.append("")
    lines.append("## Resident service dependency map")
    lines.append("")
    lines.append("Services a game source calls directly. Each one needs a host-service")
    lines.append("adapter (or the source stays resident with a thin bridge) before the")
    lines.append("all-files cutover. Machine table: `" + args.service_map + "`.")
    lines.append("")
    for service in sorted(service_map, key=lambda name: (-len(service_map[name]), name)):
        paths = sorted(set(service_map[service]))
        lines.append(f"### {service} ({len(paths)})")
        lines.append("")
        for path in paths:
            lines.append(f"- `{path}`")
        lines.append("")
    lines.append("## Limits")
    lines.append("")
    lines.append("- A regex hit is a lead. It can be a false positive (a comment or a dead branch).")
    lines.append("- A missing hit can be a false negative (macros, indirect calls, generated code).")
    lines.append("- Static triage cannot prove stop/join, resource release, or no re-entry after unload.")
    lines.append("- Compiler depfile freshness is still unknown; the inventory does not invoke a compiler.")
    report_path.write_text("\n".join(lines) + "\n", encoding="utf-8")

    print(f"[AUDIT TRIAGE] rows={len(rows)} source={source_label}", flush=True)
    for name in ("STATIC_CLOSED", "STATIC_CLOSABLE_CANDIDATE", "STATIC_STATE_REVIEW",
                 "RESIDENT_ADAPTER_REQUIRED", "RUNTIME_PROOF_REQUIRED", "RESIDENT_HOST",
                 "NON_GAME", "UNRESOLVED"):
        print(f"[AUDIT TRIAGE] {name}={counts.get(name, 0)}")
    print(f"[AUDIT TRIAGE] report={args.report}", flush=True)
    print(f"[AUDIT TRIAGE] csv={args.csv}", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
