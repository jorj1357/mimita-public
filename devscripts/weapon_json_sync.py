"""Migrate and validate canonical per-weapon configuration files.

Run ``py devscripts/weapon_json_sync.py --migrate`` once to split the legacy
aggregate. Run ``--check`` after editing weapon files or the masterlist.
"""

import argparse
import json
import re
from pathlib import Path

from jsonc import load as load_jsonc

ROOT = Path(__file__).resolve().parents[1]
AGGREGATE = ROOT / "config" / "weapons.json"
WEAPONS = ROOT / "config" / "weapons"
MASTER = WEAPONS / "masterlist" / "weapon-masterlist.json"


def value_type(value):
    if isinstance(value, bool):
        return "boolean"
    if isinstance(value, (int, float)):
        return "number"
    if isinstance(value, str):
        return "string"
    if isinstance(value, list):
        return "array"
    if isinstance(value, dict):
        return "object"
    return "null"


def write_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def migrate():
    if MASTER.exists():
        raise ValueError("Masterlist already exists; migration only runs once")
    aggregate = json.loads(AGGREGATE.read_text(encoding="utf-8"))
    definitions = {key: value for key, value in aggregate.items()
                   if isinstance(value, dict) and value.get("id") == key}
    if not definitions:
        raise ValueError("No weapon definitions found in the aggregate")
    fields = ["id", "display_name", "slot"]
    fields += sorted(set().union(*(set(value) for value in definitions.values())) - set(fields))
    types = {field: sorted({value_type(value[field]) for value in definitions.values()
                            if field in value}) for field in fields}
    master = {"version": 1, "description": "Canonical top-level fields for every tool definition."
              " Null means this definition does not use the field.",
              "fields": fields,
              "rules": {field: {"types": types[field], "nullable": field not in
                                 ("id", "display_name", "slot")}
                        for field in fields}}
    for key in definitions:
        if (WEAPONS / key / "config.json").exists():
            raise ValueError("Per-weapon file already exists: " + key)
    write_json(MASTER, master)
    for key, value in definitions.items():
        write_json(WEAPONS / key / "config.json",
                   {field: value.get(field) for field in fields})
    comments = {key: value for key, value in aggregate.items() if key not in definitions}
    comments["migration"] = "Weapon definitions are in config/weapons/<id>/config.json."
    write_json(AGGREGATE, comments)
    print(f"Migrated {len(definitions)} definitions; aggregate now contains no active weapons")


def check():
    master = json.loads(MASTER.read_text(encoding="utf-8"))
    fields = master["fields"]
    rules = master["rules"]
    errors = []
    count = 0
    for path in sorted(WEAPONS.glob("*/config.json")):
        count += 1
        data = load_jsonc(path)
        if set(data) != set(fields):
            missing = sorted(set(fields) - set(data))
            unknown = sorted(set(data) - set(fields))
            errors.append(f"{path}: missing={missing} unknown={unknown}")
        if data.get("id") != path.parent.name:
            errors.append(f"{path}: id does not match folder")
        expected_order = [field for field in fields if field in data]
        actual_order = [field for field in data if field in fields]
        if actual_order != expected_order:
            first = next((i for i, (a, b) in enumerate(zip(actual_order, expected_order))
                          if a != b), 0)
            errors.append(
                f"{path}: fields are not in masterlist order "
                f"(first mismatch at position {first}: "
                f"expected {expected_order[first]!r}, found {actual_order[first]!r})")
        for field in fields:
            value = data.get(field)
            rule = rules[field]
            if value is None and not rule["nullable"]:
                errors.append(f"{path}: {field} cannot be null")
            elif value is not None and value_type(value) not in rule["types"]:
                errors.append(f"{path}: {field} has wrong type")
    aggregate = load_jsonc(AGGREGATE)
    remaining = [key for key, value in aggregate.items()
                 if isinstance(value, dict) and value.get("id") == key]
    if remaining:
        errors.append("Aggregate still contains weapon definitions: " + ", ".join(remaining))
    print(f"masterlist=1 definitions={count} errors={len(errors)}")
    for error in errors:
        print(error)
    return not errors


def sync():
    """Add every newly declared masterlist field without removing JSONC comments."""
    master = load_jsonc(MASTER)
    fields = master["fields"]
    rules = master["rules"]
    synced = 0
    for path in sorted(WEAPONS.glob("*/config.json")):
        text = path.read_text(encoding="utf-8")
        data = load_jsonc(path)
        missing = [field for field in fields if field not in data]
        if not missing:
            continue
        additions = []
        for field in missing:
            rule = rules[field]
            if rule.get("nullable", False):
                value = None
            elif "default" in rule:
                value = rule["default"]
            elif field == "id":
                value = path.parent.name
            else:
                raise ValueError(f"{field} is required and needs a masterlist default before syncing {path}")
            additions.append("  " + json.dumps(field) + ": " + json.dumps(value, ensure_ascii=False))
        opening = text.find("{")
        if opening < 0:
            raise ValueError(f"{path}: expected a top-level object")
        comma = "," if data else ""
        inserted = "\n" + (",\n".join(additions)) + comma + text[opening + 1:]
        path.write_text(text[:opening + 1] + inserted, encoding="utf-8")
        synced += 1
    print(f"Synced missing masterlist fields in {synced} weapon files")


def split_top_level_members(text):
    opening = text.find("{")
    closing = text.rfind("}")
    if opening < 0 or closing <= opening:
        raise ValueError("expected a top-level object")
    body = text[opening + 1:closing]
    members = []
    buf = []
    depth = 0
    in_string = False
    escaped = False
    line_comment = False
    block_comment = False
    i = 0
    while i < len(body):
        ch = body[i]
        nxt = body[i + 1] if i + 1 < len(body) else ""
        if line_comment:
            buf.append(ch)
            if ch == "\n":
                line_comment = False
        elif block_comment:
            buf.append(ch)
            if ch == "*" and nxt == "/":
                buf.append(nxt)
                i += 1
                block_comment = False
        elif in_string:
            buf.append(ch)
            if escaped:
                escaped = False
            elif ch == "\\":
                escaped = True
            elif ch == '"':
                in_string = False
        elif ch == '"':
            in_string = True
            buf.append(ch)
        elif ch == "/" and nxt == "/":
            line_comment = True
            buf.append(ch)
        elif ch == "/" and nxt == "*":
            block_comment = True
            buf.append(ch)
            buf.append(nxt)
            i += 1
        elif ch in "{[":
            depth += 1
            buf.append(ch)
        elif ch in "}]":
            depth -= 1
            buf.append(ch)
        elif ch == "," and depth == 0:
            member = "".join(buf).strip()
            if member:
                members.append(member)
            buf = []
        else:
            buf.append(ch)
        i += 1
    member = "".join(buf).strip()
    if member:
        members.append(member)
    return members


def reorder():
    """Put every top-level field in exact masterlist order, preserving comments
    and existing values. Adds missing fields first through sync()."""
    sync()
    master = load_jsonc(MASTER)
    fields = master["fields"]
    reordered = 0
    for path in sorted(WEAPONS.glob("*/config.json")):
        text = path.read_text(encoding="utf-8")
        members = split_top_level_members(text)
        by_key = {}
        unknown = []
        for member in members:
            match = re.match(
                r'^\s*(?://[^\n]*\n|/\*.*?\*/\s*)*\s*"([^"]+)"\s*:',
                member, re.DOTALL)
            if match:
                by_key[match.group(1)] = member
            else:
                unknown.append(member)
        ordered = [by_key[field] for field in fields if field in by_key]
        ordered.extend(unknown)
        new_text = "{\n" + ",\n".join(ordered) + "\n}\n"
        if new_text != text:
            path.write_text(new_text, encoding="utf-8")
            reordered += 1
    print(f"Reordered {reordered} weapon files")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--migrate", action="store_true")
    mode.add_argument("--check", action="store_true")
    mode.add_argument("--sync", action="store_true")
    mode.add_argument("--reorder", action="store_true")
    args = parser.parse_args()
    if args.migrate:
        migrate()
    elif args.sync:
        sync()
    elif args.reorder:
        reorder()
    if not check():
        raise SystemExit(1)
