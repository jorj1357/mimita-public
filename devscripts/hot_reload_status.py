#!/usr/bin/env python3
"""Aggregate current hot-reload facts into one generated JSON file.

This is read-only with respect to code and runtime. It reads existing manifests,
ledgers, build state, and audit output; it does not build, launch, or edit specs.
"""

from __future__ import annotations

import argparse
import json
import subprocess
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_OUTPUT = ROOT / "build" / "hot-reload" / "status.json"


def read_json(relative: str) -> dict:
    try:
        payload = json.loads((ROOT / relative).read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return {}
    return payload if isinstance(payload, dict) else {}


def latest_journal() -> str:
    logs = ROOT / "logs"
    if not logs.is_dir():
        return ""
    candidates = [path for path in logs.rglob("events.jsonl") if path.is_file()]
    if not candidates:
        return ""
    return str(max(candidates, key=lambda path: path.stat().st_mtime))


def hot_client_runtime_proven() -> bool:
    logs = ROOT / "logs"
    if not logs.is_dir():
        return False
    journals = sorted(logs.rglob("client*/events.jsonl"),
                      key=lambda path: path.stat().st_mtime, reverse=True)
    for journal in journals[:20]:
        try:
            with journal.open("rb") as handle:
                head = handle.read(2_000_000).decode("utf-8", errors="ignore")
                handle.seek(max(0, journal.stat().st_size - 1_000_000))
                tail = handle.read().decode("utf-8", errors="ignore")
        except OSError:
            continue
        text = head + tail
        if '"path":"dll"' in text and '"to":"Connected"' in text:
            return True
    return False


def git_value(*args: str) -> str:
    try:
        result = subprocess.run(
            ["git", *args],
            cwd=ROOT,
            text=True,
            capture_output=True,
            check=False,
        )
    except OSError:
        return ""
    return result.stdout.strip()


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Aggregate hot-reload status JSON")
    parser.add_argument("--output", default=str(DEFAULT_OUTPUT))
    parser.add_argument("--print", action="store_true", help="print JSON to stdout")
    args = parser.parse_args(argv)

    manifest = read_json("build/source-manifest.json")
    sources = manifest.get("sources", []) if isinstance(manifest.get("sources"), list) else []
    owners: dict[str, int] = {}
    for row in sources:
        owner = str(row.get("owner", "unresolved"))
        owners[owner] = owners.get(owner, 0) + 1

    current = read_json("build/game/current.json")
    ready = read_json("build/game/ready.json")
    packet_ledger = read_json("config/hot-reload/packet-ledger.json")
    state_ledger = read_json("config/hot-reload/state-ledger.json")
    audit = read_json("build/hot-reload/audit.json")
    dev_state = read_json(".dev/state.json")
    progress = read_json("build/progress.json")

    packet_counts = packet_ledger.get("counts", {})
    client_total = int(packet_counts.get("total", 0) or 0)
    client_dll_overrides = int(packet_counts.get("client_dll", 0) or 0)
    client_resident = int(packet_counts.get("client_resident", 0) or 0)
    client_unknown = int(packet_counts.get("client_unknown", 0) or 0)
    server_hot = int(packet_counts.get("server_dll", 0) or 0)
    server_resident = int(packet_counts.get("server_resident", 0) or 0)
    server_unknown = int(packet_counts.get("server_unknown", 0) or 0)
    packet_tombstones = int(packet_counts.get("tombstones", 0) or 0)
    server_total = client_total
    manifest_by_path = {
        str(row.get("path", "")).replace("\\", "/"): row
        for row in sources
    }
    client_handler_sources = ("src/network/multiplayer-tick.cpp",
                              "src/network/multiplayer-packets.cpp")
    client_handlers_game_owned = all(
        manifest_by_path.get(path, {}).get("owner") == "game"
        for path in client_handler_sources
    )
    server_pump_resident = (
        manifest_by_path.get("src/network/server.cpp", {}).get("owner") == "resident")
    server_packets_game_owned = (
        manifest_by_path.get("src/network/server-packets.cpp", {}).get("owner")
        == "game")
    server_npcs_game_owned = (
        manifest_by_path.get("src/network/server-npcs.cpp", {}).get("owner") == "game")

    domains = state_ledger.get("domains", [])
    state_domains = len(domains) if isinstance(domains, list) else 0
    try:
        adapter_source = (ROOT / "src/host/host-server-entity-adapter.cpp").read_text(
            encoding="utf-8")
    except OSError:
        adapter_source = ""
    state_domains_wired = (state_domains > 0 and
                           adapter_source.count("DomainId::") >= state_domains * 2)
    try:
        server_source = (ROOT / "src/network/server.cpp").read_text(encoding="utf-8")
    except OSError:
        server_source = ""
    server_fallback_removed = (
        "server refused to start without the hot server generation" in server_source
        and "if (!hotPlayers)" not in server_source
        and "if (!hotWorld)" not in server_source)
    audit_counts = audit.get("counts", {})
    high_risk = int(audit.get("high_risk_count", 0) or 0)
    audit_rows = len(audit.get("rows", []) or [])

    generation = current.get("generation", ready.get("generation"))
    try:
        main_source = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
    except OSError:
        main_source = ""
    hot_frame_default = '_putenv_s("MIMITA_GAME_OWN_FRAME", "1")' in main_source
    hot_frame_runtime_proven = hot_client_runtime_proven()
    safe_unload_gate = read_json("build/hot-reload/safe-unload-gate.json")
    safe_unload = bool(safe_unload_gate.get("passed")) and int(
        safe_unload_gate.get("activation_count", 0) or 0) >= 3

    artifact = current.get("dll", ready.get("dll", ""))
    client_pids = dev_state.get("client_pids") or []
    active_pid = (client_pids[0] if client_pids else
                  dev_state.get("server_pid") or None)

    gates = {
        "client_handlers_game_owned": client_handlers_game_owned,
        "client_hot_frame_runtime_proven": hot_frame_runtime_proven,
        "client_packets_complete": (
            client_total > 0 and client_resident == 0 and
            client_unknown == packet_tombstones),
        "server_packets_complete": (
            server_total > 0 and server_resident == 0 and
            server_unknown == packet_tombstones),
        "state_domains_generated": state_domains > 0,
        "state_domains_wired": state_domains_wired,
        "server_fallback_removed": server_fallback_removed,
        "safe_unload": safe_unload,
        "gameplay_live_fix_proven": False,
        "audit_clean": audit_rows > 0 and high_risk == 0,
    }

    payload = {
        "schema_version": 1,
        "generated_utc": datetime.now(timezone.utc).isoformat(timespec="seconds"),
        "validation": {
            "validated": False,
            "note": "2026-11-14 17:28 initial generated values are provisional; validate source manifest, packet/state ledgers, audit classification, build identity, and runtime fields before using them as evidence.",
        },
        "source": {
            "revision": manifest.get("revision", ""),
            "branch": git_value("rev-parse", "--abbrev-ref", "HEAD"),
            "total_sources": len(sources),
            "owners": dict(sorted(owners.items())),
        },
        "build": {
            "current": current,
            "ready": ready,
            "progress": progress,
        },
        "runtime": {
            "generation": generation,
            "artifact": artifact,
            "active_pid": active_pid,
            "journal": latest_journal(),
            "game_build": dev_state.get("game_build") or {},
        },
        "coverage": {
            "client_dll_dispatch_overrides": client_dll_overrides,
            "client_packets_total": client_total,
            "client_packets_resident": client_resident,
            "client_packets_unknown": client_unknown,
            "client_packet_tombstones": packet_tombstones,
            "client_handlers_game_owned": client_handlers_game_owned,
            "client_hot_frame_default": hot_frame_default,
            "client_hot_frame_runtime_proven": hot_frame_runtime_proven,
            "client_packets_hot": (
                client_resident == 0 and client_unknown == packet_tombstones),
            "server_packets_hot": server_hot,
            "server_packets_total": server_total,
            "server_pump_resident": server_pump_resident,
            "server_packets_game_owned": server_packets_game_owned,
            "server_npcs_game_owned": server_npcs_game_owned,
            "state_domains": state_domains,
            "state_domains_wired": state_domains_wired,
            "audit_rows": audit_rows,
            "audit_high_risk": high_risk,
            "audit_counts": audit_counts,
        },
        "cutover1": gates,
    }

    output = Path(args.output)
    if not output.is_absolute():
        output = ROOT / output
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    if args.print:
        print(json.dumps(payload, indent=2, sort_keys=True))
    else:
        print(f"[HOT STATUS] wrote {output.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
