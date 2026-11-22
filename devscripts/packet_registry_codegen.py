#!/usr/bin/env python3
# 2026-11-16
# Generate the packet hotness ledger from actual inbound dispatch code.
#
# Source of truth:
#   src/network/packets.h              packet ids
#   src/network/multiplayer-tick.cpp   client inbound dispatch
#   src/network/server-packets.cpp     server inbound dispatch
#   build/source-manifest.json         source owner
#
# The old hand-maintained packet-schema.json defaults/overrides are gone. A
# packet direction with no discovered inbound branch is UNKNOWN, never resident.
#
#   python devscripts/packet_registry_codegen.py
#   python devscripts/packet_registry_codegen.py --check

from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_PACKETS = "src/network/packets.h"
DEFAULT_HEADER = "src/host/generated/packet-registry-v1.h"
DEFAULT_LEDGER = "config/hot-reload/packet-ledger.json"
MANIFEST = "build/source-manifest.json"

ENUM_RE = re.compile(r"^\s*(PACKET_[A-Z0-9_]+)\s*=\s*(\d+)\s*,\s*$")
PROTOCOL_RE = re.compile(r"PROTOCOL_VERSION\s*=\s*(\d+)")
INBOUND_RE = re.compile(r"\b(?:header|h)->type\s*(?:==|!=)\s*(PACKET_[A-Z0-9_]+)")
PACKET_LOCAL_RE = re.compile(r"\bpacketType\s*(?:==|!=)\s*(PACKET_[A-Z0-9_]+)")
CASE_RE = re.compile(r"^\s*case\s+(PACKET_[A-Z0-9_]+)\s*:")
CALL_RE = re.compile(r"\b([A-Za-z_]\w*)\s*\(")
PACKET_REFERENCE_RE = re.compile(r"(PACKET_[A-Z0-9_]+)")
CALL_SKIP = {
    "if", "else", "for", "while", "switch", "return", "sizeof", "reinterpret_cast",
    "static_cast", "const_cast", "std", "memcpy", "memset", "snprintf", "printf",
}

DISPATCH = {
    "client": "src/network/multiplayer-tick.cpp",
    "server": "src/network/server-packets.cpp",
}


def fail(message: str) -> None:
    print(f"[PACKET CODEGEN] ERROR {message}", file=sys.stderr)


def read_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def parse_packets(path: Path) -> list[dict]:
    entries: list[dict] = []
    in_enum = False
    for line in path.read_text(encoding="utf-8").splitlines():
        if "enum PacketType" in line:
            in_enum = True
            continue
        if in_enum and line.strip().startswith("};"):
            break
        if not in_enum:
            continue
        match = ENUM_RE.match(line)
        if match:
            entries.append({"name": match.group(1), "id": int(match.group(2))})
    return entries


def parse_protocol_version(path: Path) -> int:
    try:
        match = PROTOCOL_RE.search(path.read_text(encoding="utf-8"))
        return int(match.group(1)) if match else 0
    except OSError:
        return 0


def owner_map(manifest: dict) -> dict[str, str]:
    return {
        str(row.get("path", "")).replace("\\", "/"): str(row.get("owner", "unknown"))
        for row in manifest.get("sources", [])
    }


def handler_after(lines: list[str], index: int) -> str:
    for candidate in lines[index + 1:index + 10]:
        text = candidate.strip()
        if text.startswith("{"):
            text = text[1:].strip()
        match = CALL_RE.search(text)
        if not match:
            continue
        name = match.group(1)
        if name not in CALL_SKIP:
            return name
    return ""


def scan_dispatch(path: Path, direction: str, owners: dict[str, str]) -> dict[str, dict]:
    found: dict[str, dict] = {}
    if not path.is_file():
        return found
    lines = path.read_text(encoding="utf-8").splitlines()
    source = path.relative_to(ROOT).as_posix()
    owner = owners.get(source, "unknown")
    path_kind = "dll" if owner == "game" else ("resident" if owner == "resident" else "unknown")
    for index, line in enumerate(lines):
        names: list[str] = []
        for pattern in (INBOUND_RE, PACKET_LOCAL_RE, CASE_RE):
            names.extend(pattern.findall(line))
        if not names:
            continue
        handler = handler_after(lines, index)
        for name in names:
            previous = found.get(name)
            if previous is None:
                found[name] = {
                    "path": path_kind,
                    "source": source,
                    "line": index + 1,
                    "owner": owner,
                    "handler": handler,
                }
    return found


def scan_source_references(packets: list[dict], owners: dict[str, str]) -> dict[str, dict]:
    by_name = {packet["name"]: packet["id"] for packet in packets}
    found: dict[str, dict] = {}
    for path in sorted((ROOT / "src").rglob("*.cpp")):
        source = path.relative_to(ROOT).as_posix()
        if source.startswith("src/host/generated/") or source.endswith("packet-registry-v1.h"):
            continue
        owner = owners.get(source, "unknown")
        if owner == "unknown":
            continue
        try:
            lines = path.read_text(encoding="utf-8", errors="ignore").splitlines()
        except OSError:
            continue
        for index, line in enumerate(lines):
            # The old loop tested every packet name against every source line.
            # Tokenize only packet identifiers present in the line instead.
            for match in PACKET_REFERENCE_RE.finditer(line):
                token = match.group(1)
                # Preserve the previous substring semantics for identifiers
                # that extend a packet name, while checking all names only for
                # the comparatively rare PACKET_* tokens rather than every line.
                names = ([token] if token in by_name else
                         [name for name in by_name if name in token])
                for name in names:
                    previous = found.get(name)
                    if previous is not None and previous["owner"] == "game":
                        continue
                    if previous is not None and owner != "game":
                        continue
                    found[name] = {
                        "source": source,
                        "line": index + 1,
                        "owner": owner,
                        "path": "dll" if owner == "game" else "resident",
                    }
    return found


def generate(packets: list[dict], protocol_version: int, manifest: dict) -> tuple[str, str]:
    owners = owner_map(manifest)
    scans = {
        direction: scan_dispatch(ROOT / relative, direction, owners)
        for direction, relative in DISPATCH.items()
    }
    references = scan_source_references(packets, owners)
    rows: list[dict] = []
    for packet in sorted(packets, key=lambda item: item["id"]):
        row = {"id": packet["id"], "name": packet["name"]}
        reference = references.get(packet["name"])
        for direction in ("server", "client"):
            hit = scans[direction].get(packet["name"])
            if hit is not None:
                row[direction] = hit["path"]
                row[f"{direction}_source"] = hit["source"]
                row[f"{direction}_line"] = hit["line"]
                row[f"{direction}_owner"] = hit["owner"]
                row[f"{direction}_handler"] = hit["handler"]
                row[f"{direction}_inbound"] = True
            elif reference is not None:
                row[direction] = reference["path"]
                row[f"{direction}_source"] = reference["source"]
                row[f"{direction}_line"] = reference["line"]
                row[f"{direction}_owner"] = reference["owner"]
                row[f"{direction}_handler"] = ""
                row[f"{direction}_inbound"] = False
            else:
                row[direction] = "unknown"
                row[f"{direction}_source"] = ""
                row[f"{direction}_line"] = 0
                row[f"{direction}_owner"] = "unknown"
                row[f"{direction}_handler"] = ""
                row[f"{direction}_inbound"] = False
        if row["client_inbound"] or row["server_inbound"]:
            row["classification"] = "inbound"
        elif row["client"] != "unknown" or row["server"] != "unknown":
            row["classification"] = "referenced"
        else:
            row["classification"] = "tombstone"
        rows.append(row)

    schema_sha = hashlib.sha256(
        json.dumps({"protocol_version": protocol_version, "packets": rows},
                   sort_keys=True, separators=(",", ":")).encode("utf-8")
    ).hexdigest()
    lines = [
        "// Generated by devscripts/packet_registry_codegen.py.",
        "// Source: packets.h + actual inbound dispatch branches.",
        "// Do not hand edit. Unknown means no inbound handler was discovered.",
        "",
        "#pragma once",
        "",
        "#include <cstddef>",
        "#include <cstdint>",
        "",
        "namespace mimita::host::packet_registry {",
        "",
        f"inline constexpr std::uint16_t kProtocolVersion = {protocol_version};",
        "",
        "enum class Path : std::uint8_t { Resident = 0, Dll = 1, Unknown = 2 };",
        "",
        "struct Entry {",
        "    std::uint16_t id;",
        "    const char* name;",
        "    Path server;",
        "    Path client;",
        "};",
        "",
        "inline constexpr Entry kEntries[] = {",
    ]
    path_names = {"resident": "Resident", "dll": "Dll", "unknown": "Unknown"}
    for row in rows:
        lines.append(
            f'    {{{row["id"]}, "{row["name"]}", '
            f'Path::{path_names[row["server"]]}, Path::{path_names[row["client"]]}}},')
    lines.extend([
        "};",
        f"inline constexpr std::size_t kEntryCount = {len(rows)};",
        "",
        "inline const Entry* find(std::uint16_t id) {",
        "    for (const Entry& entry : kEntries) {",
        "        if (entry.id == id) return &entry;",
        "    }",
        "    return nullptr;",
        "}",
        "",
        "inline Path serverPath(std::uint16_t id) {",
        "    const Entry* entry = find(id);",
        "    return entry ? entry->server : Path::Unknown;",
        "}",
        "",
        "inline Path clientPath(std::uint16_t id) {",
        "    const Entry* entry = find(id);",
        "    return entry ? entry->client : Path::Unknown;",
        "}",
        "",
        "}  // namespace mimita::host::packet_registry",
        "",
    ])
    header = "\n".join(lines)

    ledger = {
        "schema_version": 1,
        "protocol_version": protocol_version,
        "generator": "devscripts/packet_registry_codegen.py",
        "generator_version": 2,
        "source_sha256": schema_sha,
        "header": DEFAULT_HEADER,
        "counts": {
            "total": len(rows),
            "server_dll": sum(1 for row in rows if row["server"] == "dll"),
            "server_resident": sum(1 for row in rows if row["server"] == "resident"),
            "server_unknown": sum(1 for row in rows if row["server"] == "unknown"),
            "client_dll": sum(1 for row in rows if row["client"] == "dll"),
            "client_resident": sum(1 for row in rows if row["client"] == "resident"),
            "client_unknown": sum(1 for row in rows if row["client"] == "unknown"),
            "client_inbound_dll": sum(1 for row in rows if row["client_inbound"] and row["client"] == "dll"),
            "client_inbound_unknown": sum(1 for row in rows if not row["client_inbound"]),
            "server_inbound_dll": sum(1 for row in rows if row["server_inbound"] and row["server"] == "dll"),
            "server_inbound_unknown": sum(1 for row in rows if not row["server_inbound"]),
            "tombstones": sum(1 for row in rows if row["classification"] == "tombstone"),
        },
        "packets": rows,
    }
    ledger_text = json.dumps(ledger, indent=2, sort_keys=True) + "\n"
    return header, ledger_text


def resolve(path: str) -> Path:
    value = Path(path)
    return value if value.is_absolute() else ROOT / value


def write_if_changed(path: Path, text: str) -> bool:
    try:
        if path.read_text(encoding="utf-8") == text:
            return False
    except OSError:
        pass
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")
    return True


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Generate the packet hotness registry")
    parser.add_argument("--packets", default=DEFAULT_PACKETS)
    parser.add_argument("--header", default=DEFAULT_HEADER)
    parser.add_argument("--ledger", default=DEFAULT_LEDGER)
    parser.add_argument("--schema", default="", help="deprecated; ignored")
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args(argv)

    try:
        packets = parse_packets(resolve(args.packets))
        manifest = read_json(ROOT / MANIFEST)
    except (OSError, json.JSONDecodeError) as error:
        fail(f"cannot read input: {error}")
        return 2
    if not packets:
        fail("no PACKET_* entries found")
        return 2
    protocol_version = parse_protocol_version(
        ROOT / "src/network/schema/packet-header.h")
    header, ledger_text = generate(packets, protocol_version, manifest)
    header_path = resolve(args.header)
    ledger_path = resolve(args.ledger)

    if args.check:
        stale: list[str] = []
        for path, expected in ((header_path, header), (ledger_path, ledger_text)):
            try:
                current = path.read_text(encoding="utf-8")
            except OSError:
                stale.append(f"missing {path.relative_to(ROOT)}")
                continue
            if current != expected:
                stale.append(f"stale {path.relative_to(ROOT)}")
        if stale:
            for item in stale:
                fail(item)
            fail("run: python devscripts/packet_registry_codegen.py")
            return 1
        print(f"[PACKET CODEGEN] ok packets={len(packets)}")
        return 0

    wrote_header = write_if_changed(header_path, header)
    wrote_ledger = write_if_changed(ledger_path, ledger_text)
    changed = "wrote" if (wrote_header or wrote_ledger) else "unchanged"
    print(f"[PACKET CODEGEN] {changed} packets={len(packets)} "
          f"header={args.header} ledger={args.ledger}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
