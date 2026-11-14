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
    client_hot = int(packet_counts.get("client_dll", 0) or 0)
    server_hot = int(packet_counts.get("server_dll", 0) or 0)
    server_total = client_total

    domains = state_ledger.get("domains", [])
    state_domains = len(domains) if isinstance(domains, list) else 0
    audit_counts = audit.get("counts", {})
    high_risk = int(audit.get("high_risk_count", 0) or 0)
    audit_rows = len(audit.get("rows", []) or [])

    generation = current.get("generation", ready.get("generation"))
    artifact = current.get("dll", ready.get("dll", ""))
    active_pid = dev_state.get("client_pid") or dev_state.get("server_pid") or None

    gates = {
        "client_packets_complete": client_total > 0 and client_hot == client_total,
        "server_packets_complete": server_total > 0 and server_hot == server_total,
        "state_domains_generated": state_domains > 0,
        "state_domains_wired": False,
        "server_fallback_removed": False,
        "safe_unload": False,
        "gameplay_live_fix_proven": False,
        "audit_clean": audit_rows > 0 and high_risk == 0,
    }

    payload = {
        "schema_version": 1,
        "generated_utc": datetime.now(timezone.utc).isoformat(timespec="seconds"),
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
        },
        "coverage": {
            "client_packets_hot": client_hot,
            "client_packets_total": client_total,
            "server_packets_hot": server_hot,
            "server_packets_total": server_total,
            "state_domains": state_domains,
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
