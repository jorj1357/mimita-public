"""Migrate and validate canonical per-weapon configuration files.

Run ``py devscripts/weapon_json_sync.py --migrate`` once to split the legacy
aggregate. Run ``--check`` after editing weapon files or the masterlist.
"""

import argparse
import json
from pathlib import Path

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
        data = json.loads(path.read_text(encoding="utf-8"))
        if list(data) != fields:
            errors.append(f"{path}: missing, unknown, or out-of-order fields")
        if data.get("id") != path.parent.name:
            errors.append(f"{path}: id does not match folder")
        for field in fields:
            value = data.get(field)
            rule = rules[field]
            if value is None and not rule["nullable"]:
                errors.append(f"{path}: {field} cannot be null")
            elif value is not None and value_type(value) not in rule["types"]:
                errors.append(f"{path}: {field} has wrong type")
    aggregate = json.loads(AGGREGATE.read_text(encoding="utf-8"))
    remaining = [key for key, value in aggregate.items()
                 if isinstance(value, dict) and value.get("id") == key]
    if remaining:
        errors.append("Aggregate still contains weapon definitions: " + ", ".join(remaining))
    print(f"masterlist=1 definitions={count} errors={len(errors)}")
    for error in errors:
        print(error)
    return not errors


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--migrate", action="store_true")
    mode.add_argument("--check", action="store_true")
    args = parser.parse_args()
    if args.migrate:
        migrate()
    if not check():
        raise SystemExit(1)
