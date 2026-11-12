#!/usr/bin/env python3
# 2026-11-12
# purpose
# Pass B: per-symbol state-owner decisions for the 40 STATIC_STATE_REVIEW rows.
#
# For each row, scan the source for static variables, thread_local variables,
# file-scope globals, and extern declarations, then record a proposed lifetime
# decision: immutable, generation-scoped cache, durable resident record, external
# resource, callback/job target, or adapter dependency.
#
# This is a static map. PROPOSED / NEEDS_READ rows require a confirmation read.
# It does not compile, link, launch, or change source ownership.

from __future__ import annotations

import argparse
import csv
import json
import re
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TRIAGE_CSV = Path("docs/handoffs/cpp-audit-static-triage-20261112.csv")
INVENTORY_JSON = Path("docs/handoffs/cpp-source-inventory.json")
REPORT_MD = Path("docs/handoffs/cpp-state-owner-decisions-20261112.md")
REPORT_CSV = Path("docs/handoffs/cpp-state-owner-decisions-20261112.csv")

STATIC_FUNCTION_RE = re.compile(r"^\s*static\s+[\w:<>,*&\s]+\s+\w+\s*\(")
STATIC_VAR_RE = re.compile(r"^\s*static\s+(?!const\b|constexpr\b)([\w:<>,*&\s]+?)\s+(\w+)\s*(?:=|;|\{|\[)")
STATIC_CONST_RE = re.compile(r"^\s*static\s+(?:const|constexpr)\b.*")
THREAD_LOCAL_RE = re.compile(r"^\s*thread_local\b")
GLOBAL_RE = re.compile(r"^(?:std::[\w:]+(?:<[^;]+>)?|[\w:]+(?:\s*\*)?)\s+(g[A-Z]\w*)\s*(?:=|;|\{|\[)")
EXTERN_VAR_RE = re.compile(r"^\s*extern\s+(?!\"C\")(?!.*\()([\w:<>,*&\s]+?)\s+(\w+)\s*(?:=|;)")
IDENT_RE = re.compile(r"([A-Za-z_]\w*)\s*(?==|;|\{|\[|$)")

# Confirmation-read decisions for rows where a proposed default is not enough.
# key = (path, symbol); value = (decision, state, reason)
MANUAL = {
    ("external/recastnavigation/Detour/Source/DetourAlloc.cpp", "sAllocFunc"): (
        "RESIDENT_OWNER", "CONFIRMED", "process-wide allocator hook; caller-set"),
    ("external/recastnavigation/Detour/Source/DetourAlloc.cpp", "sFreeFunc"): (
        "RESIDENT_OWNER", "CONFIRMED", "process-wide allocator hook; caller-set"),
    ("external/recastnavigation/Detour/Source/DetourAssert.cpp", "sAssertFailFunc"): (
        "RESIDENT_OWNER", "CONFIRMED", "process-wide assert hook; caller-set"),
    ("external/recastnavigation/Recast/Source/RecastAlloc.cpp", "sRecastAllocFunc"): (
        "RESIDENT_OWNER", "CONFIRMED", "process-wide allocator hook; caller-set"),
    ("external/recastnavigation/Recast/Source/RecastAlloc.cpp", "sRecastFreeFunc"): (
        "RESIDENT_OWNER", "CONFIRMED", "process-wide allocator hook; caller-set"),
    ("external/recastnavigation/Recast/Source/RecastAssert.cpp", "sRecastAssertFailFunc"): (
        "RESIDENT_OWNER", "CONFIRMED", "process-wide assert hook; caller-set"),
    ("src/impact/boolean-mesh.cpp", "gSessions"): (
        "RESIDENT_OWNER", "CONFIRMED", "live boolean sessions keyed by stable id"),
    ("src/impact/boolean-mesh.cpp", "gSessionClock"): (
        "RESIDENT_OWNER", "CONFIRMED", "session id clock; resident with sessions"),
    ("src/impact/destructible-mesh-loader.cpp", "gCache"): (
        "GENERATION_SCOPED_RESET", "CONFIRMED", "parsed mesh cache; first access rebuilds"),
    ("src/impact/destructible-mesh-loader.cpp", "gModelCache"): (
        "GENERATION_SCOPED_RESET", "CONFIRMED", "parsed GLB cache; first access rebuilds"),
    ("src/replay/replay.cpp", "gActiveReplayRecorder"): (
        "RESIDENT_OWNER", "CONFIRMED", "active capture must survive a code swap"),
    ("src/replay/replay.cpp", "gReplayCaptureEnabled"): (
        "RESIDENT_OWNER", "CONFIRMED", "capture policy must survive a code swap"),
    ("src/replay/replay.cpp", "gReplayFactoryNotifyFn"): (
        "RESIDENT_OWNER", "CONFIRMED", "callback target; host-owned registration"),
    ("src/competitive/competitive-match.cpp", "gCompetitiveMatchActive"): (
        "RESIDENT_OWNER", "CONFIRMED", "match/session state"),
    ("src/competitive/competitive-match.cpp", "gCompetitiveOpponentMmr"): (
        "RESIDENT_OWNER", "CONFIRMED", "match/session state"),
    ("src/ui/hitmarker.cpp", "gHitmarkerTimer"): (
        "GENERATION_SCOPED_RESET", "CONFIRMED", "transient UI timer"),
    ("src/gui/ui-tooltip.cpp", "gTooltipMap"): (
        "GENERATION_SCOPED_RESET", "CONFIRMED", "UI asset cache; reset and reload"),
    ("src/gui/ui-tooltip.cpp", "gLastHovered"): (
        "GENERATION_SCOPED_RESET", "CONFIRMED", "transient hover state"),
    ("src/gui/ui-tooltip.cpp", "gHoverStart"): (
        "GENERATION_SCOPED_RESET", "CONFIRMED", "transient hover timer"),
    ("src/gui/ui-tooltip.cpp", "gFadingText"): (
        "GENERATION_SCOPED_RESET", "CONFIRMED", "transient fade state"),
    ("src/gui/ui-tooltip.cpp", "gFadeStart"): (
        "GENERATION_SCOPED_RESET", "CONFIRMED", "transient fade timer"),
    ("src/network/server-contact.cpp", "seen"): (
        "GENERATION_SCOPED_RESET", "CONFIRMED", "bounded dedupe cache"),
    ("src/network/server-contact.cpp", "order"): (
        "GENERATION_SCOPED_RESET", "CONFIRMED", "bounded dedupe cache"),
    ("src/npc/npc-nav-graph.cpp", "gMeshMinZ"): (
        "GENERATION_SCOPED_RESET", "CONFIRMED", "derived from world; recompute on activation"),
    ("src/npc/npc-nav-graph.cpp", "gMeshMaxZ"): (
        "GENERATION_SCOPED_RESET", "CONFIRMED", "derived from world; recompute on activation"),
    ("src/npc/npc-nav-graph.cpp", "graph"): (
        "RESIDENT_OWNER", "CONFIRMED", "nav graph; validates against current world"),
    ("src/camera.cpp", "logTimer"): (
        "GENERATION_SCOPED_RESET", "CONFIRMED", "transient debug throttle"),
    ("src/debug/transform-debug.cpp", "dbg"): (
        "GENERATION_SCOPED_RESET", "CONFIRMED", "debug state; reset on activation"),
    ("src/devtools/dev-commands.cpp", "sInstance"): (
        "RESIDENT_OWNER", "CONFIRMED", "command registry; resident tool service"),
    ("src/effects/zombie-tower-effects.cpp", "effects"): (
        "GENERATION_SCOPED_RESET", "CONFIRMED", "effect state; reset on activation"),
    ("src/gamemode/capability-registry.cpp", "registry"): (
        "GENERATION_SCOPED_RESET", "CONFIRMED", "capability catalog; reload from data"),
    ("src/gui/gui-coord.cpp", "sys"): (
        "GENERATION_SCOPED_RESET", "CONFIRMED", "coordinate config; reload from data"),
    ("src/gui/hud/match-timer.cpp", "timer"): (
        "RESIDENT_OWNER", "CONFIRMED", "match timer must survive a swap"),
    ("src/impact/destructible-geometry.cpp", "system"): (
        "RESIDENT_OWNER", "CONFIRMED", "destructible world geometry owner"),
    ("src/impact/destructible-geometry.cpp", "gNextSessionId"): (
        "RESIDENT_OWNER", "CONFIRMED", "session id clock"),
    ("external/recastnavigation/Detour/Source/DetourNavMeshBuilder.cpp", "MESH_NULL_IDX"): (
        "FREEZE_IN_GAME_DLL", "CONFIRMED", "sentinel constant"),
    ("src/combat/death-system-bodies.cpp", "gRenderer"): (
        "ADAPTER_INVENTORY", "CONFIRMED", "host renderer service"),
    ("src/debug/debug-visuals-render.cpp", "gTriVerts"): (
        "ADAPTER_INVENTORY", "CONFIRMED", "debug draw buffer -> CAP_DEBUG_VIS"),
    ("src/debug/debug-visuals-render.cpp", "gOverlayTriVerts"): (
        "ADAPTER_INVENTORY", "CONFIRMED", "debug draw buffer -> CAP_DEBUG_VIS"),
    ("src/debug/debug-visuals-render.cpp", "gSolidTriVerts"): (
        "ADAPTER_INVENTORY", "CONFIRMED", "debug draw buffer -> CAP_DEBUG_VIS"),
    ("src/debug/debug-visuals-render.cpp", "gCameraWorldOverlay"): (
        "GENERATION_SCOPED_RESET", "CONFIRMED", "transient overlay state"),
    ("src/debug/debug-visuals-render.cpp", "gCameraHudOverlay"): (
        "GENERATION_SCOPED_RESET", "CONFIRMED", "transient overlay state"),
    ("src/debug/entity-visibility.cpp", "gMaster"): (
        "RESIDENT_OWNER", "CONFIRMED", "debug visibility setting"),
    ("src/debug/entity-visibility.cpp", "gCategories"): (
        "RESIDENT_OWNER", "CONFIRMED", "debug visibility settings"),
    ("src/impact/boolean-mesh.cpp", "gSessionMutex"): (
        "RESIDENT_OWNER", "CONFIRMED", "guards resident sessions"),
    ("src/impact/destructible-mesh-loader.cpp", "gCacheMutex"): (
        "GENERATION_SCOPED_RESET", "CONFIRMED", "guards resettable cache"),
    ("src/map/map-loader-gltf.cpp", "gTextures"): (
        "ADAPTER_INVENTORY", "CONFIRMED", "host texture store -> CAP_RESOURCES"),
}


def read_rows() -> list[dict]:
    with open(ROOT / TRIAGE_CSV, "r", encoding="utf-8", newline="") as handle:
        return list(csv.DictReader(handle))


def symbol_from_line(line: str) -> tuple[str, str]:
    stripped = line.strip()
    if STATIC_FUNCTION_RE.match(line):
        match = re.search(r"(\w+)\s*\(", stripped)
        return ("", "STATIC_FUNCTION") if not match else (match.group(1), "STATIC_FUNCTION")
    if STATIC_CONST_RE.match(line):
        names = IDENT_RE.findall(stripped)
        return (names[-1] if names else "", "IMMUTABLE")
    if STATIC_VAR_RE.match(line):
        match = STATIC_VAR_RE.match(line)
        return (match.group(2), "STATIC_VARIABLE")
    if THREAD_LOCAL_RE.match(line):
        names = IDENT_RE.findall(stripped)
        return (names[-1] if names else "", "THREAD_LOCAL")
    if GLOBAL_RE.match(line):
        return (GLOBAL_RE.match(line).group(1), "FILE_GLOBAL")
    if EXTERN_VAR_RE.match(line):
        match = EXTERN_VAR_RE.match(line)
        return (match.group(2), "EXTERN")
    names = IDENT_RE.findall(stripped)
    return (names[-1] if names else "", "OTHER")


def proposed(path: str, symbol: str, kind: str, line: str) -> tuple[str, str, str]:
    manual = MANUAL.get((path, symbol))
    if manual:
        return manual
    if kind in ("STATIC_FUNCTION", "OTHER"):
        return ("NO_STATE", "CONFIRMED", "static helper; no persistent state")
    if kind == "IMMUTABLE":
        return ("FREEZE_IN_GAME_DLL", "CONFIRMED", "const/constexpr value")
    if kind == "THREAD_LOCAL":
        return ("GENERATION_SCOPED_RESET", "PROPOSED", "thread-local state; reset on activation")
    if kind == "STATIC_VARIABLE":
        scope = "function" if line[:1].isspace() else "file"
        if scope == "function":
            return ("GENERATION_SCOPED_RESET", "PROPOSED", "function-static cache; reset on activation")
        return ("RESIDENT_OWNER_OR_RESET", "NEEDS_READ", "file-scope static; choose resident vs reset")
    if kind == "FILE_GLOBAL":
        return ("RESIDENT_OWNER_OR_RESET", "NEEDS_READ", "file-scope global; choose resident vs reset")
    if kind == "EXTERN":
        return ("ADAPTER_INVENTORY", "NEEDS_READ", "extern dependency; map to a capability or resident owner")
    return ("NEEDS_READ", "NEEDS_READ", "manual trace")


def scan(path: str) -> list[dict]:
    results: list[dict] = []
    text = (ROOT / path).read_text(encoding="utf-8", errors="replace")
    for index, line in enumerate(text.splitlines(), start=1):
        if not (STATIC_FUNCTION_RE.match(line) or STATIC_CONST_RE.match(line)
                or STATIC_VAR_RE.match(line) or THREAD_LOCAL_RE.match(line)
                or GLOBAL_RE.match(line) or EXTERN_VAR_RE.match(line)):
            continue
        symbol, kind = symbol_from_line(line)
        decision, state, reason = proposed(path, symbol, kind, line)
        results.append({
            "path": path,
            "line": index,
            "symbol": symbol,
            "kind": kind,
            "scope": "function" if line[:1].isspace() else "file",
            "decision": decision,
            "state": state,
            "reason": reason,
            "snippet": line.strip()[:160],
        })
    return results


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Pass B state-owner decisions")
    parser.add_argument("--report", default=str(REPORT_MD))
    parser.add_argument("--csv", default=str(REPORT_CSV))
    args = parser.parse_args(argv)

    rows = [row for row in read_rows() if row["class"] == "STATIC_STATE_REVIEW"]
    symbols: list[dict] = []
    for row in sorted(rows, key=lambda item: item["path"]):
        symbols.extend(scan(row["path"]))

    csv_path = ROOT / args.csv
    with open(csv_path, "w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=["path", "line", "symbol", "kind", "scope",
                                                    "decision", "state", "reason", "snippet"])
        writer.writeheader()
        writer.writerows(symbols)

    decisions = Counter(item["decision"] for item in symbols)
    states = Counter(item["state"] for item in symbols)
    by_path: dict[str, list[dict]] = {}
    for item in symbols:
        by_path.setdefault(item["path"], []).append(item)

    lines = [
        "# State-owner decisions (Pass B, 2026-11-12)",
        "",
        "**Status:** per-symbol static map for the 40 `STATIC_STATE_REVIEW` rows.",
        "`CONFIRMED` means the confirmation read decided the lifetime; `PROPOSED` means",
        "the default is reasonable but the owner path still needs a trace; `NEEDS_READ`",
        "is a blocking unknown. This is not runtime evidence.",
        "",
        f"- Machine table: `{args.csv}`",
        f"- Rows reviewed: {len(rows)}",
        f"- Symbols recorded: {len(symbols)}",
        "",
        "## Decision counts",
        "",
        "| Decision | Symbols |",
        "|---|---:|",
    ]
    for decision, count in decisions.most_common():
        lines.append(f"| {decision} | {count} |")
    lines.extend([
        "",
        "## State counts",
        "",
        "| State | Symbols |",
        "|---|---:|",
    ])
    for state, count in states.most_common():
        lines.append(f"| {state} | {count} |")
    lines.extend([
        "",
        "## Decision meanings",
        "",
        "- `NO_STATE`: static helper; nothing to move.",
        "- `FREEZE_IN_GAME_DLL`: immutable const/constexpr; ship as module data.",
        "- `GENERATION_SCOPED_RESET`: cache/timer/transient state; reset in `initialize`.",
        "- `RESIDENT_OWNER`: durable state or process-wide hook; must live in the host.",
        "- `RESIDENT_OWNER_OR_RESET`: file-scope state that needs a decision.",
        "- `ADAPTER_INVENTORY`: extern dependency; map to a capability or resident owner.",
        "",
        "## Per-file symbols",
        "",
    ])
    for path in sorted(by_path):
        lines.append(f"### `{path}`")
        lines.append("")
        lines.append("| Line | Symbol | Kind | Decision | State | Reason |")
        lines.append("|---:|---|---|---|---|---|")
        for item in sorted(by_path[path], key=lambda entry: entry["line"]):
            lines.append(
                f"| {item['line']} | `{item['symbol']}` | {item['kind']} | "
                f"{item['decision']} | {item['state']} | {item['reason']} |")
        lines.append("")
    lines.extend([
        "## Exit criteria",
        "",
        "- Every `NEEDS_READ` symbol is traced and given a decision.",
        "- Every `RESIDENT_OWNER` symbol has a resident record or service owner.",
        "- Every `GENERATION_SCOPED_RESET` symbol has a reset site in `initialize`.",
        "- Every `ADAPTER_INVENTORY` symbol appears in Pass C.",
        "- No symbol relies on a per-file runtime test; the barrier rule covers it.",
    ])
    (ROOT / args.report).write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"[PASS B] rows={len(rows)} symbols={len(symbols)} report={args.report}")
    for decision, count in decisions.most_common():
        print(f"[PASS B] {decision}={count}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
