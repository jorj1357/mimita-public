#!/usr/bin/env python3
# 2026-11-14
# purpose
# Generate the resident state domain ABI header and its hotness ledger from
# config/hot-reload/state-schema.json.
#
# The generated header is the only source of domain structs, field masks, and
# clear/initialize helpers. Do not hand edit it; edit the schema and rerun:
#
#   python devscripts/resident_state_codegen.py
#   python devscripts/resident_state_codegen.py --check
#
# --check exits 1 when the checked-in header or ledger is stale.

from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_SCHEMA = "config/hot-reload/state-schema.json"
DEFAULT_HEADER = "src/host/generated/resident-state-v1.h"
DEFAULT_LEDGER = "config/hot-reload/state-ledger.json"

C_TYPES = {
    "u8": "std::uint8_t",
    "u16": "std::uint16_t",
    "u32": "std::uint32_t",
    "u64": "std::uint64_t",
    "i8": "std::int8_t",
    "i16": "std::int16_t",
    "i32": "std::int32_t",
    "i64": "std::int64_t",
    "f32": "float",
    "f64": "double",
    "bool": "bool",
    "vec3": "float",
    "string": "char",
}
RESERVED_FIELDS = {"structSize", "version", "reserved", "handle", "fieldMask"}
NAME_RE = re.compile(r"^[a-z][a-z0-9_]*$")


def fail(message: str) -> None:
    print(f"[STATE CODEGEN] ERROR {message}", file=sys.stderr)


def pascal(name: str) -> str:
    return "".join(part.capitalize() for part in name.split("_"))


def default_literal(field: dict) -> str:
    kind = field["type"]
    if kind == "bool":
        return "true" if field.get("default") else "false"
    if kind == "string":
        return "{}"
    if kind == "vec3":
        return "{0.0f, 0.0f, 0.0f}"
    if kind == "f32":
        return f"{float(field.get('default', 0))}f"
    if kind == "f64":
        return f"{float(field.get('default', 0))}"
    value = field.get("default", 0)
    if value is None:
        value = 0
    if int(value) < 0:
        return str(int(value))
    suffix = "ull" if kind == "u64" else "u"
    return f"{int(value)}{suffix}"


def validate(schema: dict) -> list[str]:
    errors: list[str] = []
    if schema.get("schema_version") != 1:
        errors.append(f"unsupported schema_version: {schema.get('schema_version')}")
    if not isinstance(schema.get("abi_version"), int) or schema["abi_version"] < 1:
        errors.append("abi_version must be a positive integer")
    if schema.get("handle_type", "u32") != "u32":
        errors.append("handle_type must be u32")
    domains = schema.get("domains")
    if not isinstance(domains, list) or not domains:
        errors.append("domains must be a non-empty list")
        return errors
    seen_domains: set[str] = set()
    for domain in domains:
        name = domain.get("name", "")
        if not isinstance(name, str) or not NAME_RE.match(name):
            errors.append(f"invalid domain name: {name!r}")
            continue
        if name in seen_domains:
            errors.append(f"duplicate domain: {name}")
        seen_domains.add(name)
        if not isinstance(domain.get("version"), int) or domain["version"] < 1:
            errors.append(f"{name}: version must be a positive integer")
        for key in ("owner", "source"):
            if not isinstance(domain.get(key), str) or not domain[key]:
                errors.append(f"{name}: {key} must be a non-empty string")
        for key in ("writers", "readers"):
            value = domain.get(key)
            if not isinstance(value, list) or not all(isinstance(v, str) and v for v in value):
                errors.append(f"{name}: {key} must be a list of non-empty strings")
        fields = domain.get("fields")
        if not isinstance(fields, list) or not fields:
            errors.append(f"{name}: fields must be a non-empty list")
            continue
        if len(fields) > 64:
            errors.append(f"{name}: at most 64 fields are supported")
        seen_fields: set[str] = set()
        for field in fields:
            field_name = field.get("name", "")
            if not isinstance(field_name, str) or not NAME_RE.match(field_name):
                errors.append(f"{name}: invalid field name: {field_name!r}")
                continue
            if field_name in RESERVED_FIELDS:
                errors.append(f"{name}: {field_name} is a reserved record member")
            if field_name in seen_fields:
                errors.append(f"{name}: duplicate field: {field_name}")
            seen_fields.add(field_name)
            kind = field.get("type")
            if kind not in C_TYPES:
                errors.append(f"{name}.{field_name}: unsupported type: {kind!r}")
                continue
            if kind == "string":
                capacity = field.get("capacity")
                if not isinstance(capacity, int) or not 1 <= capacity <= 1024:
                    errors.append(f"{name}.{field_name}: string capacity must be 1..1024")
    return errors


def generate(schema: dict, header_name: str = DEFAULT_HEADER) -> tuple[str, str]:
    schema_sha = hashlib.sha256(
        json.dumps(schema, sort_keys=True, separators=(",", ":")).encode("utf-8")
    ).hexdigest()
    lines: list[str] = [
        "// Generated by devscripts/resident_state_codegen.py.",
        "// Source: config/hot-reload/state-schema.json",
        "// Do not hand edit. Edit the schema and regenerate.",
        "",
        "#pragma once",
        "",
        "#include <cstddef>",
        "#include <cstdint>",
        "#include <cstring>",
        "",
        "namespace mimita::host::state {",
        "",
        f"inline constexpr std::uint32_t kSchemaVersion = {schema['schema_version']};",
        f"inline constexpr std::uint16_t kAbiVersion = {schema['abi_version']};",
        "using Handle = std::uint32_t;",
        "",
    ]
    ledger_domains: list[dict] = []
    for domain in schema["domains"]:
        name = domain["name"]
        fields = domain["fields"]
        lines.append(f"namespace {name} {{")
        lines.append("")
        lines.append(f"inline constexpr std::uint16_t kVersion = {domain['version']};")
        lines.append(f"inline constexpr std::size_t kFieldCount = {len(fields)};")
        lines.append("")
        lines.append("enum Field : std::uint64_t {")
        for index, field in enumerate(fields):
            lines.append(f"    kField{pascal(field['name'])} = 1ull << {index},")
        if len(fields) == 64:
            lines.append("    kFieldAll = ~0ull,")
        else:
            lines.append(f"    kFieldAll = (1ull << {len(fields)}) - 1ull,")
        lines.append("};")
        lines.append("")
        lines.append("struct Record {")
        lines.append("    std::uint32_t structSize = sizeof(Record);")
        lines.append("    std::uint16_t version = kVersion;")
        lines.append("    std::uint16_t reserved = 0;")
        lines.append("    Handle handle = 0;")
        lines.append("    std::uint64_t fieldMask = 0;")
        for field in fields:
            ctype = C_TYPES[field["type"]]
            if field["type"] == "string":
                line = f"    {ctype} {field['name']}[{field['capacity']}] = {{}};"
            elif field["type"] == "vec3":
                line = f"    {ctype} {field['name']}[3] = {default_literal(field)};"
            else:
                line = f"    {ctype} {field['name']} = {default_literal(field)};"
            lines.append(line)
        lines.append("};")
        lines.append("")
        lines.append("inline constexpr std::size_t kStructSize = sizeof(Record);")
        lines.append("")
        lines.append("inline void initialize(Record& record, Handle handle) {")
        lines.append("    record = Record{};")
        lines.append("    record.handle = handle;")
        lines.append("}")
        lines.append("")
        lines.append("inline void clearFields(Record& record, std::uint64_t mask) {")
        for field in fields:
            field_name = field["name"]
            enum_name = f"kField{pascal(field_name)}"
            if field["type"] == "string":
                lines.append(f"    if (mask & {enum_name}) {{")
                lines.append(f"        std::memset(record.{field_name}, 0, sizeof(record.{field_name}));")
                lines.append("    }")
            elif field["type"] == "vec3":
                lines.append(f"    if (mask & {enum_name}) {{")
                lines.append(f"        record.{field_name}[0] = 0.0f;")
                lines.append(f"        record.{field_name}[1] = 0.0f;")
                lines.append(f"        record.{field_name}[2] = 0.0f;")
                lines.append("    }")
            else:
                lines.append(f"    if (mask & {enum_name}) {{")
                lines.append(f"        record.{field_name} = {default_literal(field)};")
                lines.append("    }")
        lines.append("}")
        lines.append("")
        lines.append(f"}}  // namespace {name}")
        lines.append("")
        ledger_domains.append({
            "name": name,
            "version": domain["version"],
            "owner": domain["owner"],
            "source": domain["source"],
            "writers": domain["writers"],
            "readers": domain["readers"],
            "field_count": len(fields),
            "struct_size": "sizeof(Record)",
            "fields": [
                {
                    "name": field["name"],
                    "type": field["type"],
                    "c_type": ("float[3]" if field["type"] == "vec3"
                               else f"char[{field['capacity']}]" if field["type"] == "string"
                               else C_TYPES[field["type"]]),
                    "capacity": field.get("capacity"),
                    "mask": f"1ull << {index}",
                }
                for index, field in enumerate(fields)
            ],
        })
    lines.append("}  // namespace mimita::host::state")
    lines.append("")
    header = "\n".join(lines)

    ledger = {
        "schema_version": schema["schema_version"],
        "abi_version": schema["abi_version"],
        "generator": "devscripts/resident_state_codegen.py",
        "generator_version": 1,
        "schema_sha256": schema_sha,
        "header": header_name,
        "domains": ledger_domains,
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
    path.write_text(text, encoding="utf-8")
    return True


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Generate resident state domain ABI")
    parser.add_argument("--schema", default=DEFAULT_SCHEMA)
    parser.add_argument("--header", default=DEFAULT_HEADER)
    parser.add_argument("--ledger", default=DEFAULT_LEDGER)
    parser.add_argument("--check", action="store_true",
                        help="fail when generated outputs are stale")
    args = parser.parse_args(argv)

    schema_path = resolve(args.schema)
    try:
        schema = json.loads(schema_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        fail(f"cannot read schema {schema_path}: {error}")
        return 2
    errors = validate(schema)
    if errors:
        for error in errors:
            fail(error)
        return 2

    header, ledger_text = generate(schema, args.header)
    header_path = resolve(args.header)
    ledger_path = resolve(args.ledger)
    header_path.parent.mkdir(parents=True, exist_ok=True)
    ledger_path.parent.mkdir(parents=True, exist_ok=True)

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
            fail("run: python devscripts/resident_state_codegen.py")
            return 1
        print(f"[STATE CODEGEN] ok domains={len(schema['domains'])} header={args.header}")
        return 0

    wrote_header = write_if_changed(header_path, header)
    wrote_ledger = write_if_changed(ledger_path, ledger_text)
    total_fields = sum(len(domain["fields"]) for domain in schema["domains"])
    changed = "wrote" if (wrote_header or wrote_ledger) else "unchanged"
    print(f"[STATE CODEGEN] {changed} domains={len(schema['domains'])} "
          f"fields={total_fields} header={args.header} ledger={args.ledger}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
