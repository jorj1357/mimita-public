#!/usr/bin/env python3
# 2026-11-12
# purpose
# Produce the two audit exit-pass documents from the existing triage and
# resident-service map:
#   Pass A - runtime quiesce plan for RUNTIME_PROOF_REQUIRED rows
#   Pass C - capability map for RESIDENT_ADAPTER_REQUIRED rows
#
# Read-only: it does not compile, link, launch, or change source ownership.

from __future__ import annotations

import argparse
import csv
import json
import re
from collections import Counter, defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TRIAGE_CSV = Path("docs/handoffs/cpp-audit-static-triage-20261112.csv")
SERVICE_JSON = Path("docs/handoffs/resident-service-dependencies-20261112.json")
PASS_A_MD = Path("docs/handoffs/cpp-runtime-quiesce-plan-20261112.md")
PASS_A_CSV = Path("docs/handoffs/cpp-runtime-quiesce-plan-20261112.csv")
PASS_C_MD = Path("docs/handoffs/cpp-capability-map-20261112.md")
PASS_C_CSV = Path("docs/handoffs/cpp-capability-map-20261112.csv")

BARRIER_RULES = {
    "async-worker": "CAP_JOBS: submit only; cancel/drain/join before unload",
    "callback": "CAP_CALLBACKS: generation token; unregister before unload",
    "gpu-resource": "CAP_RESOURCES: release with the context current",
    "network-resource": "CAP_TRANSPORT: close/drain; session owner quiesce",
    "network-context": "CAP_TRANSPORT/CAP_SESSION: bounded payloads; drain before unload",
    "audio-resource": "resident audio owner; stop voice/engine before unload",
    "file-io": "static check: transient per-call handles; no retained resource",
    "os-resource": "CAP_RESOURCES or resident owner; close handle",
    "known-blocker": "explicit owner quiesce plan; may stay resident",
    "process-entry": "resident/bootstrap exception",
    "other": "manual trace",
}

SERVICE_CAPABILITY = {
    "StructuredLogger": ("CAP_LOG", "event"),
    "DebugLog": ("CAP_LOG", "event"),
    "Terminal": ("CAP_TERMINAL", "emit/state"),
    "TerminalState": ("CAP_TERMINAL", "state"),
    "GuiLayoutManager": ("CAP_GUI", "element/text"),
    "DebugVis": ("CAP_DEBUG_VIS", "draw"),
    "Perf": ("CAP_PERF", "scope/counter"),
    "WeaponRegistry": ("CAP_REGISTRY", "weapon_get"),
    "GamemodeRegistry": ("CAP_REGISTRY", "gamemode_get"),
    "MapConfigRegistry": ("CAP_REGISTRY", "map_get"),
    "VehicleConfigRegistry": ("CAP_REGISTRY", "vehicle_get"),
    "CharacterRegistry": ("CAP_REGISTRY", "character_get"),
    "BehaviorProfileRegistry": ("CAP_REGISTRY", "behavior_profile_get"),
    "NpcDifficultyConfig": ("CAP_CONFIG", "get"),
    "MovementJsonConfig": ("CAP_CONFIG", "get"),
    "SizeScalingConfig": ("CAP_CONFIG", "get"),
    "ActorCollisionConfig": ("CAP_CONFIG", "get"),
    "PersistenceQueue": ("CAP_PERSISTENCE", "submit/flush"),
    "ModuleHost": ("CAP_MODULES", "policy_call"),
    "HotReloadSystem": ("CAP_MODULES", "legacy_gameapi"),
    "PhysicalEntitySystem": ("CAP_ENTITY", "read/write"),
    "ConstraintSystem": ("CAP_PHYSICS", "constraint_query"),
    "VehicleSystem": ("CAP_PHYSICS", "vehicle_query"),
    "AuthSystem": ("CAP_AUTH", "identity"),
    "gpMpContext": ("CAP_TRANSPORT", "session"),
    "MultiplayerContextGlobal": ("CAP_TRANSPORT", "session"),
    "serverGamemodeState": ("CAP_GAMEMODE", "state"),
}

PATH_CAPABILITY = {
    "src/audio/audio-acoustics.cpp": ("CAP_WORLD", "ray_query"),
    "src/combat/projectile-simulation.cpp": ("CAP_PHYSICS", "projectile_step"),
    "src/combat/shot-profiler.cpp": ("CAP_PERF", "counter"),
    "src/combat/weapon-godball.cpp": ("CAP_ENTITY", "read/write"),
    "src/devtools/terminal-input.cpp": ("CAP_TERMINAL", "input/state"),
    "src/devtools/terminal-render.cpp": ("CAP_TERMINAL", "render_queue"),
    "src/engine/engine-frame.cpp": ("CAP_RENDER", "frame_begin/end"),
    "src/map/map-loader-gltf-accessors.cpp": ("CAP_LOG", "event"),
    "src/map/map-loader-gltf-verts.cpp": ("CAP_LOG", "event"),
    "src/network/remote-entity-lifecycle.cpp": ("CAP_ENTITY", "read/write"),
    "src/npc/npc-world-query.cpp": ("CAP_WORLD", "ray_query"),
    "src/perf/perf-allocator.cpp": ("CAP_RESOURCES", "allocator"),
    "src/physics/movement/physics-down-dash.cpp": ("CAP_ENTITY", "read/write"),
    "src/physics/movement/physics-freeze.cpp": ("CAP_ENTITY", "read/write"),
    "src/physics/movement/physics-scratch.cpp": ("CAP_RESOURCES", "scratch"),
    "src/physics/physical-body.cpp": ("CAP_ENTITY", "read/write"),
    "src/physics/ray-utils.cpp": ("CAP_WORLD", "ray_query"),
    "src/replay/replay-recorder-clips.cpp": ("CAP_LOG", "event"),
}

RESOURCE_LABELS = {
    "file": "file-io",
    "gl": "gpu-resource",
    "net": "network-resource",
    "audio": "audio-resource",
    "os": "os-resource",
}


def read_rows() -> list[dict]:
    with open(ROOT / TRIAGE_CSV, "r", encoding="utf-8", newline="") as handle:
        return list(csv.DictReader(handle))


def resource_categories(reasons: str) -> list[str]:
    found = re.search(r"resource hits=([^;]+)", reasons)
    if not found:
        return []
    result = []
    for name in found.group(1).split(","):
        label = RESOURCE_LABELS.get(name.strip())
        if label:
            result.append(label)
    return result


def pass_a_categories(row: dict) -> list[str]:
    reasons = row["reasons"]
    cats: list[str] = []
    if "thread/job" in reasons:
        cats.append("async-worker")
    if "callback" in reasons:
        cats.append("callback")
    cats.extend(resource_categories(reasons))
    if "network context" in reasons:
        cats.append("network-context")
    if "known handoff blocker" in reasons:
        cats.append("known-blocker")
    if "main function" in reasons:
        cats.append("process-entry")
    return cats or ["other"]


def pass_a_rule(cats: list[str]) -> str:
    rules = []
    for cat in cats:
        rule = BARRIER_RULES.get(cat, BARRIER_RULES["other"])
        if rule not in rules:
            rules.append(rule)
    return "; ".join(rules)


def write_pass_a(rows: list[dict]) -> None:
    selected = [row for row in rows if row["class"] == "RUNTIME_PROOF_REQUIRED"]
    out_rows = []
    counts = Counter()
    for row in selected:
        cats = pass_a_categories(row)
        for cat in cats:
            counts[cat] += 1
        transient = cats == ["file-io"]
        out_rows.append({
            "path": row["path"],
            "owner": row["owner"],
            "line_count": row["line_count"],
            "categories": ",".join(cats),
            "barrier_rule": pass_a_rule(cats),
            "review_state": "TRANSIENT_CANDIDATE" if transient else "OWNED_PERSISTENT",
            "signals": row["reasons"],
        })

    csv_path = ROOT / PASS_A_CSV
    with open(csv_path, "w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(out_rows[0].keys()))
        writer.writeheader()
        writer.writerows(out_rows)

    lines = [
        "# Runtime quiesce plan (Pass A, 2026-11-12)",
        "",
        "**Status:** static classification of the `RUNTIME_PROOF_REQUIRED` rows. This is a",
        "plan, not runtime evidence. `TRANSIENT_CANDIDATE` rows still need a confirmation",
        "read; `OWNED_PERSISTENT` rows need their owner/stop/join/release path traced.",
        "",
        f"- Machine table: `{PASS_A_CSV.as_posix()}`",
        f"- Rows: {len(out_rows)}",
        "",
        "## Method",
        "",
        "1. Read the recorded signals for each row (thread/job, callback, resource, network, known blocker).",
        "2. Split compound signals into categories.",
        "3. Attach the barrier rule the file must use.",
        "4. Mark pure file-IO rows as `TRANSIENT_CANDIDATE` for a per-call handle check.",
        "5. Everything else remains `OWNED_PERSISTENT` until its owner and stop/join/release path are traced.",
        "",
        "## Category counts",
        "",
        "| Category | Rows |",
        "|---|---:|",
    ]
    for cat, count in counts.most_common():
        lines.append(f"| {cat} | {count} |")
    lines.extend([
        "",
        "## Barrier rules",
        "",
        "| Category | Required rule |",
        "|---|---|",
    ])
    for cat, rule in BARRIER_RULES.items():
        lines.append(f"| {cat} | {rule} |")
    lines.extend([
        "",
        "## Full row table",
        "",
        "| Row | Lines | Categories | Review state | Barrier rule |",
        "|---|---:|---|---|---|",
    ])
    for row in out_rows:
        lines.append(
            f"| `{row['path']}` | {row['line_count']} | {row['categories']} | "
            f"{row['review_state']} | {row['barrier_rule']} |")
    lines.extend([
        "",
        "## Exit criteria for Pass A",
        "",
        "- Every row has a traced owner or is marked `TRANSIENT_CANDIDATE` after a read.",
        "- Every `OWNED_PERSISTENT` row cites its stop/join/release path and the barrier rule.",
        "- No row relies on a per-file runtime test; the barrier rule is the mechanism.",
        "- The remaining `RUNTIME_PROOF_REQUIRED` set is only rows with a live owner that the barrier cannot yet cover.",
        "",
        "## What this pass does not prove",
        "",
        "- It does not prove quiescence. That is the barrier campaign.",
        "- It does not move any resource into a capability; it only names the rule.",
    ])
    (ROOT / PASS_A_MD).write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"[PASS A] rows={len(out_rows)} report={PASS_A_MD}")


def write_pass_c(rows: list[dict]) -> None:
    services_payload = json.loads((ROOT / SERVICE_JSON).read_text(encoding="utf-8"))
    path_services = services_payload.get("paths", {})
    selected = [row for row in rows if row["class"] == "RESIDENT_ADAPTER_REQUIRED"]

    out_rows = []
    capability_rows: dict[str, list[str]] = defaultdict(list)
    unmapped: list[str] = []
    for row in selected:
        services = path_services.get(row["path"], [])
        capabilities: list[str] = []
        ops: list[str] = []
        for service in services:
            mapping = SERVICE_CAPABILITY.get(service)
            if not mapping:
                unmapped.append(f"{row['path']}:{service}")
                continue
            capability, op = mapping
            if capability not in capabilities:
                capabilities.append(capability)
            if op not in ops:
                ops.append(op)
        if not capabilities:
            fallback = PATH_CAPABILITY.get(row["path"])
            if fallback:
                capabilities = [fallback[0]]
                ops = [fallback[1]]
            else:
                capabilities = ["UNMAPPED"]
                ops = ["manual"]
        for capability in capabilities:
            capability_rows[capability].append(row["path"])
        out_rows.append({
            "path": row["path"],
            "owner": row["owner"],
            "line_count": row["line_count"],
            "services": ",".join(services),
            "capabilities": ",".join(capabilities),
            "ops": ",".join(ops),
        })

    csv_path = ROOT / PASS_C_CSV
    with open(csv_path, "w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(out_rows[0].keys()))
        writer.writeheader()
        writer.writerows(out_rows)

    lines = [
        "# Capability map (Pass C, 2026-11-12)",
        "",
        "**Status:** static mapping from resident-service calls to proposed host capabilities.",
        "This is the seam inventory for `HostCapabilitiesV1`, not an implementation.",
        "",
        f"- Machine table: `{PASS_C_CSV.as_posix()}`",
        f"- Adapter rows: {len(out_rows)}",
        "",
        "## Method",
        "",
        "1. Take every `RESIDENT_ADAPTER_REQUIRED` row.",
        "2. Read its named services from `resident-service-dependencies-20261112.json`.",
        "3. Map each service to a capability ID and operation.",
        "4. A row with multiple services lists every capability it needs.",
        "5. `UNMAPPED` means a new capability or a resident bridge is required.",
        "",
        "## Capability retirement counts",
        "",
        "| Capability | Rows | Seed operations |",
        "|---|---:|---|",
    ]
    ops_by_capability = {
        "CAP_LOG": "event/text (bounded JSON)",
        "CAP_TERMINAL": "emit, state get/set",
        "CAP_GUI": "element/text read/write",
        "CAP_DEBUG_VIS": "line/box/label queue",
        "CAP_PERF": "scope begin/end, counter",
        "CAP_REGISTRY": "weapon/gamemode/map/vehicle/character/profile get",
        "CAP_CONFIG": "get config snapshot by id/revision",
        "CAP_PERSISTENCE": "submit/flush",
        "CAP_MODULES": "existing subsystem policy call (bridge)",
        "CAP_ENTITY": "read/write by stable handle",
        "CAP_PHYSICS": "constraint/vehicle query by handle",
        "CAP_AUTH": "identity/status read",
        "CAP_TRANSPORT": "opaque send/recv and session read",
        "CAP_GAMEMODE": "match state read",
        "CAP_WORLD": "broadphase candidates + read-only geometry spans",
        "CAP_RESOURCES": "allocate/release per generation",
        "CAP_RENDER": "generic draw command stream",
        "UNMAPPED": "manual",
    }
    for capability in sorted(capability_rows, key=lambda name: (-len(capability_rows[name]), name)):
        lines.append(
            f"| {capability} | {len(capability_rows[capability])} | "
            f"{ops_by_capability.get(capability, 'manual')} |")
    lines.extend([
        "",
        "## Seed HostCapabilitiesV1",
        "",
        "Each capability is a versioned plain-data struct with `version` and `structSize`.",
        "",
        "| Capability | Non-negotiable operations |",
        "|---|---|",
        "| `CAP_LOG` | bounded UTF-8 JSON event + text; category/level/tick/event name |",
        "| `CAP_TERMINAL` | emit text, read/write terminal state (no GUI ownership) |",
        "| `CAP_GUI` | read layout element/text, write text/visibility by stable id |",
        "| `CAP_DEBUG_VIS` | submit bounded line/box/label records to the host draw queue |",
        "| `CAP_PERF` | begin/end scope, counters; host aggregates |",
        "| `CAP_REGISTRY` | lookups by stable id returning plain bounded records |",
        "| `CAP_CONFIG` | revisioned config snapshot by id |",
        "| `CAP_PERSISTENCE` | bounded submit/flush request/result |",
        "| `CAP_ENTITY` | read/write bounded entity snapshots by handle |",
        "| `CAP_PHYSICS` | constraint/vehicle queries by handle, plain results |",
        "| `CAP_TRANSPORT` | send/recv opaque `{channel,typeId,bytes}`; session state read |",
        "| `CAP_RENDER` | generic draw command stream + shader/resource handles |",
        "| `CAP_WORLD` | broadphase candidates + read-only triangle/entity spans |",
        "| `CAP_JOBS` | submit/cancel/drain/join, generation-stamped |",
        "| `CAP_CALLBACKS` | register/unregister with generation token |",
        "| `CAP_RESOURCES` | allocate/release GPU/audio/net resources per generation |",
        "",
        "`CAP_RENDER`, `CAP_WORLD`, `CAP_JOBS`, `CAP_CALLBACKS`, and `CAP_RESOURCES`",
        "have no adapter rows yet because their current call sites are classified as",
        "runtime-proof or state rows; they are still mandatory for the hot targets.",
        "",
        "## Full row table",
        "",
        "| Row | Lines | Services | Capabilities |",
        "|---|---:|---|---|",
    ])
    for row in out_rows:
        lines.append(
            f"| `{row['path']}` | {row['line_count']} | {row['services']} | {row['capabilities']} |")
    lines.extend([
        "",
        "## Exit criteria for Pass C",
        "",
        "- Every adapter row maps to at least one capability or is explicitly `UNMAPPED`.",
        "- Every capability has a bounded plain-data operation list.",
        "- No adapter row requires passing STL, exceptions, RTTI objects, or old-DLL pointers.",
        "- The host capability table is additive and versioned; new behavior uses existing ops where practical.",
    ])
    if unmapped:
        lines.extend(["", "## Unmapped services", ""])
        lines.extend(f"- `{item}`" for item in sorted(set(unmapped)))
    (ROOT / PASS_C_MD).write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"[PASS C] rows={len(out_rows)} report={PASS_C_MD}")
    if unmapped:
        print(f"[PASS C] unmapped={len(set(unmapped))}")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Generate Pass A and Pass C audit documents")
    parser.add_argument("--pass-a", action="store_true", help="write Pass A only")
    parser.add_argument("--pass-c", action="store_true", help="write Pass C only")
    args = parser.parse_args(argv)
    rows = read_rows()
    run_a = args.pass_a or not (args.pass_a or args.pass_c)
    run_c = args.pass_c or not (args.pass_a or args.pass_c)
    if run_a:
        write_pass_a(rows)
    if run_c:
        write_pass_c(rows)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
