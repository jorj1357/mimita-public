#!/usr/bin/env python3
"""Inventory numeric min/max bounds in authored config schemas.

The target state is no numeric min/max anywhere. Until then this script records
what still has a range, grouped by the first three path segments, so a migration
can remove one small group at a time.

Usage:
    python devscripts/config-limit-inventory.py
    python devscripts/config-limit-inventory.py --json
"""

from __future__ import annotations

import argparse
import json
import re
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_SCHEMA = ROOT / "config/gamemodes/masterlist/gamemode-masterlist.json"


def load_json_with_comments(path: Path) -> object:
    text = path.read_text(encoding="utf-8")
    text = re.sub(r"(?m)//.*$", "", text)
    text = re.sub(r",\s*([}\]])", r"\1", text)
    return json.loads(text)


def walk(node: object, path: str, out: list[tuple[str, str, object, object]]) -> None:
    if isinstance(node, dict):
        if "min" in node or "max" in node:
            out.append((path, str(node.get("type", "")), node.get("min"), node.get("max")))
        for key, value in node.items():
            walk(value, f"{path}.{key}" if path else key, out)
    elif isinstance(node, list):
        for index, value in enumerate(node):
            walk(value, f"{path}[{index}]", out)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--schema", default=str(DEFAULT_SCHEMA))
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()

    path = Path(args.schema)
    fields: list[tuple[str, str, object, object]] = []
    walk(load_json_with_comments(path), "", fields)

    groups = Counter(".".join(item[0].split(".")[:3]) for item in fields)
    min_keys = sum(1 for item in fields if item[2] is not None)
    max_keys = sum(1 for item in fields if item[3] is not None)

    if args.json:
        print(json.dumps({
            "schema": str(path),
            "fields_with_range": len(fields),
            "min_keys": min_keys,
            "max_keys": max_keys,
            "groups": groups.most_common(),
            "fields": [
                {"path": p, "type": t, "min": lo, "max": hi}
                for p, t, lo, hi in fields
            ],
        }, indent=2, sort_keys=True))
        return 0

    print(f"# Config numeric ranges — {path.relative_to(ROOT)}")
    print()
    print(f"- fields with a range: {len(fields)}")
    print(f"- min keys: {min_keys}")
    print(f"- max keys: {max_keys}")
    print()
    print("| Group | Fields with min/max |")
    print("|---|---:|")
    for group, count in groups.most_common():
        print(f"| `{group}` | {count} |")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
