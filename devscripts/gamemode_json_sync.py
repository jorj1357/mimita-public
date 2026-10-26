#!/usr/bin/env python3
"""Canonicalize config/gamemodes/*.json against the masterlist.

The masterlist defines the supported settings, defaults, and field order.
Existing per-mode values are preserved. Missing settings are filled with the
masterlist default; nullable settings are filled with null ("inherit shared
settings"). `--check` never writes. `--write` performs the explicit migration.

Usage:
  python devscripts/gamemode_json_sync.py --check
  python devscripts/gamemode_json_sync.py --write
  python devscripts/gamemode_json_sync.py --check --write  # write only if clean
"""

from __future__ import annotations

import argparse
import copy
import json
import sys
from pathlib import Path

from jsonc import load

ROOT = Path(__file__).resolve().parents[1]
MASTER = ROOT / "config/gamemodes/masterlist/gamemode-masterlist.json"
MODES = ROOT / "config/gamemodes"
HEADER = (
    "// Mode values. Schema, defaults, ranges, and field order:\n"
    "// config/gamemodes/masterlist/gamemode-masterlist.json\n"
    "// null means inherit the shared config/playervisuals.json value.\n"
)
OPTIONAL_ENABLED = ("payload", "npc_targeting", "npc_travel", "avatar", "npc_avatar")


def default_for(schema: dict):
    if "default" in schema:
        return copy.deepcopy(schema["default"])
    if schema.get("nullable"):
        return None
    kind = schema.get("type")
    if kind == "object":
        fields = schema.get("fields")
        if isinstance(fields, dict):
            return {key: default_for(field) for key, field in fields.items()}
        return {}
    if kind == "array":
        return []
    return None


def canonical_value(value, schema: dict, path: str, errors: list[str]):
    if value is None:
        if schema.get("nullable"):
            return None
        return default_for(schema)
    kind = schema.get("type")
    if kind == "object":
        fields = schema.get("fields")
        if isinstance(fields, dict):
            source = value if isinstance(value, dict) else {}
            for key in source:
                if key not in fields:
                    errors.append(f"{path}.{key}: unknown setting")
            return {
                key: canonical_value(source[key], field, f"{path}.{key}", errors)
                if key in source else default_for(field)
                for key, field in fields.items()
            }
        additional = schema.get("additional")
        if isinstance(additional, dict):
            source = value if isinstance(value, dict) else {}
            return {
                key: canonical_value(source[key], additional, f"{path}.{key}", errors)
                for key in sorted(source)
            }
        return {}
    if kind == "array":
        source = value if isinstance(value, list) else []
        item = schema.get("item")
        if isinstance(item, dict):
            return [
                canonical_value(entry, item, f"{path}[{index}]", errors)
                for index, entry in enumerate(source)
            ]
        return source
    return value


def canonical_mode(root: dict, settings: dict, errors: list[str]):
    # A block that already exists is active unless it explicitly says otherwise.
    prepared = copy.deepcopy(root)
    for block in OPTIONAL_ENABLED:
        value = prepared.get(block)
        if isinstance(value, dict) and "enabled" not in value:
            value["enabled"] = True
    camera = prepared.get("camera")
    if isinstance(camera, dict) and isinstance(camera.get("aim_fov"), dict):
        camera["aim_fov"].setdefault("enabled", True)
    for key in prepared:
        if key not in settings:
            errors.append(f"{key}: unknown top-level setting")
    out = {}
    for key, schema in settings.items():
        if key in prepared:
            out[key] = canonical_value(prepared[key], schema, key, errors)
        else:
            out[key] = default_for(schema)
    return out


def ordered_mismatches(actual, canonical, path: str, errors: list[str]):
    if isinstance(canonical, dict):
        if not isinstance(actual, dict):
            errors.append(f"{path}: expected an object")
            return
        actual_keys = [key for key in actual if key in canonical]
        canonical_keys = list(canonical.keys())
        if actual_keys != canonical_keys:
            errors.append(f"{path}: field order does not match the masterlist")
        for key in canonical:
            if key not in actual:
                errors.append(f"{path}.{key}: missing setting")
            else:
                ordered_mismatches(actual[key], canonical[key], f"{path}.{key}", errors)
    elif isinstance(canonical, list):
        if not isinstance(actual, list):
            errors.append(f"{path}: expected an array")
            return
        for index, (a, c) in enumerate(zip(actual, canonical)):
            ordered_mismatches(a, c, f"{path}[{index}]", errors)


def validate_values(actual, canonical, path: str, errors: list[str]):
    if canonical is None:
        return
    if isinstance(canonical, dict):
        if not isinstance(actual, dict):
            return
        for key, value in canonical.items():
            if key in actual:
                validate_values(actual[key], value, f"{path}.{key}", errors)
    elif isinstance(canonical, list):
        if not isinstance(actual, list):
            return
        for index, (a, c) in enumerate(zip(actual, canonical)):
            validate_values(a, c, f"{path}[{index}]", errors)
    elif actual != canonical:
        errors.append(f"{path}: value {actual!r} != expected {canonical!r}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true", help="report without writing")
    parser.add_argument("--write", action="store_true", help="write canonical files")
    parser.add_argument("--force", action="store_true",
                        help="write even when unknown keys or required settings are missing")
    args = parser.parse_args()
    if not args.check and not args.write:
        args.check = True

    master = load(MASTER)
    settings = master.get("settings")
    if not isinstance(settings, dict):
        print(f"{MASTER}: missing settings object")
        return 1

    failures = 0
    for path in sorted(MODES.glob("*.json")):
        source = path.relative_to(ROOT).as_posix()
        root = load(path)
        if not isinstance(root, dict):
            print(f"{source}: root must be an object")
            failures += 1
            continue
        errors: list[str] = []
        canonical = canonical_mode(root, settings, errors)
        summary = []
        if "id" not in canonical or canonical.get("id") in (None, ""):
            errors.append("id: required setting is missing")
        if "name" not in canonical or canonical.get("name") in (None, ""):
            errors.append("name: required setting is missing")
        if args.check and not errors:
            ordered = []
            ordered_mismatches(root, canonical, source, ordered)
            # Missing/order findings are the canonical-check result.
            errors.extend(ordered)
        if errors:
            failures += 1
            print(f"{path.name}: {len(errors)} issue(s)")
            for error in errors:
                print(f"  {error}")
        if args.write:
            if errors and not args.force:
                print(f"  SKIP write: fix the issues or use --force")
                continue
            text = HEADER + json.dumps(canonical, indent=2, ensure_ascii=False) + "\n"
            path.write_text(text, encoding="utf-8", newline="\n")
            print(f"{path.name}: wrote canonical file")
        else:
            summary.append(path.name)

    if args.check and failures:
        print(f"[GAMEMODE SYNC] FAIL files={failures}")
        return 1
    print("[GAMEMODE SYNC] PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
