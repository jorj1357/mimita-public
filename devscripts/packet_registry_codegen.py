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
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_PACKETS = "src/network/packets.h"
DEFAULT_HEADER = "src/host/generated/packet-registry-v1.h"
DEFAULT_LEDGER = "config/hot-reload/packet-ledger.json"
MANIFEST = "build/source-manifest.json"
REFERENCE_CACHE = ".dev/packet-reference-cache.json"
REFERENCE_CACHE_SCHEMA = 1

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


def scan_source_references(
    packets: list[dict], owners: dict[str, str],
    manifest_sources: list[dict] | None = None,
    cache_path: Path | None = None,
    metrics: dict | None = None,
    source_root: Path = ROOT,
) -> dict[str, dict]:
    started = time.perf_counter()
    by_name = {packet["name"]: packet["id"] for packet in packets}
    found: dict[str, dict] = {}
    entries = manifest_sources
    if entries is None:
        entries = [
            {"path": path.relative_to(source_root).as_posix(),
             "owner": owners.get(path.relative_to(source_root).as_posix(), "unknown")}
            for path in sorted((source_root / "src").rglob("*.cpp"))
        ]
    cache: dict = {"sources": {}}
    if cache_path is not None:
        try:
            data = json.loads(cache_path.read_text(encoding="utf-8"))
            if (isinstance(data, dict)
                    and data.get("schema") == REFERENCE_CACHE_SCHEMA
                    and isinstance(data.get("sources"), dict)):
                cache = data
        except (OSError, ValueError, TypeError):
            pass
    old_sources = cache.get("sources", {})
    new_sources: dict[str, dict] = {}
    cache_hits = files_read = packet_tokens = 0
    scan_started = time.perf_counter()
    for entry in sorted(entries, key=lambda row: str(row.get("path", ""))):
        source = str(entry.get("path", "")).replace("\\", "/")
        if not source.startswith("src/") or not source.endswith(".cpp"):
            continue
        if source.startswith("src/host/generated/") or source.endswith("packet-registry-v1.h"):
            continue
        owner = str(entry.get("owner", owners.get(source, "unknown")))
        if owner == "unknown":
            continue
        path = source_root / source
        try:
            stat = path.stat()
        except OSError:
            continue
        signature = [stat.st_size, stat.st_mtime_ns, stat.st_ctime_ns]
        cached = old_sources.get(source, {})
        cached_signature = cached.get("signature") if isinstance(cached, dict) else None
        cached_tokens = cached.get("tokens") if isinstance(cached, dict) else None
        valid_tokens = (
            isinstance(cached_tokens, list)
            and all(isinstance(token, list) and len(token) == 2
                    and isinstance(token[0], str)
                    and isinstance(token[1], int)
                    for token in cached_tokens)
        )
        if cached_signature == signature and valid_tokens:
            tokens = cached_tokens
            cache_hits += 1
        else:
            files_read += 1
            tokens = []
            try:
                for index, line in enumerate(path.read_text(
                        encoding="utf-8", errors="ignore").splitlines(), 1):
                    tokens.extend([token, index]
                                  for token in PACKET_REFERENCE_RE.findall(line))
            except OSError:
                continue
        new_sources[source] = {"signature": signature, "tokens": tokens}
        packet_tokens += len(tokens)
        for token, line_number in tokens:
            # Preserve old substring matching for extended PACKET_* names.
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
                    "line": line_number,
                    "owner": owner,
                    "path": "dll" if owner == "game" else "resident",
                }
    scan_seconds = time.perf_counter() - scan_started
    cache_write_started = time.perf_counter()
    cache_written = False
    if cache_path is not None:
        try:
            cache_path.parent.mkdir(parents=True, exist_ok=True)
            cache_path.write_text(json.dumps({
                "schema": REFERENCE_CACHE_SCHEMA,
                "sources": new_sources,
            }, separators=(",", ":")) + "\n", encoding="utf-8")
            cache_written = True
        except OSError:
            # This is derived acceleration state; a failed write must not fail
            # packet generation or the game build.
            pass
    if metrics is not None:
        metrics.update({
            "source_files": len(new_sources), "cache_hits": cache_hits,
            "cache_misses": files_read, "files_read": files_read,
            "packet_tokens": packet_tokens,
            "scan_ms": round(scan_seconds * 1000, 3),
            "cache_write_ms": round((time.perf_counter() - cache_write_started) * 1000, 3),
            "cache_written": cache_written,
            "total_ms": round((time.perf_counter() - started) * 1000, 3),
        })
    return found


def generate(packets: list[dict], protocol_version: int, manifest: dict,
             reference_scan_metrics: dict | None = None) -> tuple[str, str]:
    owners = owner_map(manifest)
    scans = {
        direction: scan_dispatch(ROOT / relative, direction, owners)
        for direction, relative in DISPATCH.items()
    }
    references = scan_source_references(
        packets, owners, manifest.get("sources", []),
        ROOT / REFERENCE_CACHE, reference_scan_metrics)
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
    main_started = time.perf_counter()
    parser = argparse.ArgumentParser(description="Generate the packet hotness registry")
    parser.add_argument("--packets", default=DEFAULT_PACKETS)
    parser.add_argument("--header", default=DEFAULT_HEADER)
    parser.add_argument("--ledger", default=DEFAULT_LEDGER)
    parser.add_argument("--schema", default="", help="deprecated; ignored")
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args(argv)

    input_started = time.perf_counter()
    try:
        packets = parse_packets(resolve(args.packets))
        manifest = read_json(ROOT / MANIFEST)
    except (OSError, json.JSONDecodeError) as error:
        fail(f"cannot read input: {error}")
        return 2
    input_seconds = time.perf_counter() - input_started
    if not packets:
        fail("no PACKET_* entries found")
        return 2
    protocol_version = parse_protocol_version(
        ROOT / "src/network/schema/packet-header.h")
    reference_scan_metrics: dict = {}
    generate_started = time.perf_counter()
    header, ledger_text = generate(
        packets, protocol_version, manifest, reference_scan_metrics)
    generate_seconds = time.perf_counter() - generate_started
    header_path = resolve(args.header)
    ledger_path = resolve(args.ledger)

    if args.check:
        check_started = time.perf_counter()
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
        print(f"[PACKET CODEGEN] timing {json.dumps({
            'input_ms': round(input_seconds * 1000, 3),
            'generate_ms': round(generate_seconds * 1000, 3),
            'reference_scan': reference_scan_metrics,
            'check_ms': round((time.perf_counter() - check_started) * 1000, 3),
            'total_ms': round((time.perf_counter() - main_started) * 1000, 3),
        }, sort_keys=True)}")
        print(f"[PACKET CODEGEN] ok packets={len(packets)}")
        return 0

    wrote_header = write_if_changed(header_path, header)
    wrote_ledger = write_if_changed(ledger_path, ledger_text)
    changed = "wrote" if (wrote_header or wrote_ledger) else "unchanged"
    print(f"[PACKET CODEGEN] timing {json.dumps({
        'input_ms': round(input_seconds * 1000, 3),
        'generate_ms': round(generate_seconds * 1000, 3),
        'reference_scan': reference_scan_metrics,
        'write_ms': round((time.perf_counter() - generate_started - generate_seconds) * 1000, 3),
        'total_ms': round((time.perf_counter() - main_started) * 1000, 3),
    }, sort_keys=True)}")
    print(f"[PACKET CODEGEN] {changed} packets={len(packets)} "
          f"header={args.header} ledger={args.ledger}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
