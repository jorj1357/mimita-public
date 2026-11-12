#!/usr/bin/env python3
# 2026-11-12
# purpose
# Static packet catalog for the hot networking target. Parses packet ids and
# wire structs from src/network/packets.h, then maps usage to encode/decode and
# handler files. Read-only: no compile, link, launch, or ownership change.

from __future__ import annotations

import argparse
import csv
import re
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PACKETS_H = Path("src/network/packets.h")
REPORT_MD = Path("docs/handoffs/cpp-packet-catalog-20261112.md")
REPORT_CSV = Path("docs/handoffs/cpp-packet-catalog-20261112.csv")

PACKET_ENUM_RE = re.compile(r"^\s*(PACKET_[A-Z0-9_]+)\s*=\s*(\d+)\s*,?")
STRUCT_RE = re.compile(r"^\s*struct\s+(\w+Packet)\b")
CONST_RE = re.compile(r"^\s*(?:inline\s+)?constexpr\s+([\w:<>,\s]+?)\s+([A-Z][A-Z0-9_]+)\s*=")
PACKET_NAME_RE = re.compile(r"\b(PACKET_[A-Z0-9_]+)\b")
HANDLER_FILES = {
    "src/network/multiplayer-packets.cpp": "encode/decode",
    "src/network/packets.cpp": "helpers",
    "src/network/server-packets.cpp": "server send",
    "src/network/server-packet-handlers.cpp": "server recv",
    "src/network/server-packet-chat.cpp": "server chat",
}


def parse_packets() -> tuple[dict[str, int], dict[str, dict], list[dict]]:
    text = (ROOT / PACKETS_H).read_text(encoding="utf-8", errors="replace")
    lines = text.splitlines()

    packet_ids: dict[str, int] = {}
    in_enum = False
    for line in lines:
        if "enum PacketType" in line:
            in_enum = True
            continue
        if in_enum:
            if line.strip().startswith("};"):
                break
            match = PACKET_ENUM_RE.match(line)
            if match:
                packet_ids[match.group(1)] = int(match.group(2))

    structs: dict[str, dict] = {}
    current = None
    start = 0
    fields = 0
    for index, line in enumerate(lines, start=1):
        match = STRUCT_RE.match(line)
        if match and current is None:
            current = match.group(1)
            start = index
            fields = 0
            continue
        if current is not None:
            if line.strip().startswith("};"):
                structs[current] = {"line": start, "fields": fields}
                current = None
                continue
            if ";" in line and not line.strip().startswith("//"):
                fields += 1

    constants: list[dict] = []
    for index, line in enumerate(lines, start=1):
        match = CONST_RE.match(line)
        if match:
            constants.append({"line": index, "type": match.group(1).strip(),
                              "name": match.group(2), "text": line.strip()[:140]})
    return packet_ids, structs, constants


def scan_usage(packet_ids: dict[str, int]) -> dict[str, dict[str, int]]:
    usage: dict[str, dict[str, int]] = defaultdict(dict)
    for path in sorted((ROOT / "src").rglob("*.cpp")):
        text = path.read_text(encoding="utf-8", errors="replace")
        rel = path.relative_to(ROOT).as_posix()
        for name in PACKET_NAME_RE.findall(text):
            if name in packet_ids:
                usage[name][rel] = usage[name].get(rel, 0) + 1
    return usage


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Packet catalog")
    parser.add_argument("--report", default=str(REPORT_MD))
    parser.add_argument("--csv", default=str(REPORT_CSV))
    args = parser.parse_args(argv)

    packet_ids, structs, constants = parse_packets()
    usage = scan_usage(packet_ids)

    rows = []
    for name, packet_id in sorted(packet_ids.items(), key=lambda item: item[1]):
        used = usage.get(name, {})
        owners = sorted(used)
        handlers = [path for path in owners if path in HANDLER_FILES]
        # Heuristic wire-struct match: ExamplePacket <-> PACKET_EXAMPLE
        stem = name[len("PACKET_"):].title().replace("_", "")
        struct_name = f"{stem}Packet"
        struct = structs.get(struct_name)
        rows.append({
            "packet": name,
            "id": packet_id,
            "struct": struct_name if struct else "",
            "struct_line": struct["line"] if struct else "",
            "struct_fields": struct["fields"] if struct else "",
            "used_by": ",".join(owners),
            "handler_files": ",".join(handlers),
            "usage_count": sum(used.values()),
        })

    csv_path = ROOT / args.csv
    with open(csv_path, "w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)

    lines = [
        "# Packet catalog (2026-11-12)",
        "",
        "**Status:** static catalog of packet ids, wire structs, and usage sites.",
        "This is the hot-networking inventory; it is not a schema migration and not runtime evidence.",
        "",
        f"- Packet ids: {len(rows)}",
        f"- Parsed wire structs: {len(structs)}",
        f"- Protocol constants: {len(constants)}",
        f"- Machine table: `{args.csv}`",
        "",
        "## Hot-networking target",
        "",
        "Transport mechanism stays resident: sockets, ICE/UDP, framing, encryption,",
        "channel/reliability machinery, and the opaque payload envelope.",
        "Packet ids, fields, encode/decode, handlers, and error/reconnect behavior move",
        "to `game.dll`. Adding/removing a packet must not relink the EXE.",
        "",
        "## Packet table",
        "",
        "| Id | Packet | Wire struct | Fields | Used by | Handler files |",
        "|---:|---|---|---:|---|---|",
    ]
    for row in rows:
        lines.append(
            f"| {row['id']} | `{row['packet']}` | `{row['struct'] or '?'}` | "
            f"{row['struct_fields'] or '?'} | {row['used_by'] or '?'} | "
            f"{row['handler_files'] or '?'} |")
    lines.extend([
        "",
        "## Encode/decode and handler owners",
        "",
        "| File | Role |",
        "|---|---|",
    ])
    for path, role in HANDLER_FILES.items():
        lines.append(f"| `{path}` | {role} |")
    lines.extend([
        "",
        "## Protocol constants",
        "",
        "| Line | Name | Type | Value |",
        "|---:|---|---|---|",
    ])
    for entry in constants:
        lines.append(f"| {entry['line']} | `{entry['name']}` | {entry['type']} | `{entry['text']}` |")
    lines.extend([
        "",
        "## Gaps to resolve before cutover",
        "",
        "- Packets with no matching wire struct or no usage site need a confirmation read.",
        "- Packet ids must never be reused within a live session; removal leaves a tombstone until all peers upgrade.",
        "- Every packet needs an explicit error/timeout/reconnect path in the catalog.",
        "- The opaque transport envelope and schema-hash agreement must be designed before packet code moves.",
    ])
    (ROOT / args.report).write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"[PACKETS] ids={len(rows)} structs={len(structs)} constants={len(constants)} report={args.report}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
