#!/usr/bin/env python3
"""Validate and print the multi-client actor/weapon/network matrix plan.

The full one-server/N-client launcher is a separate dev-loop extension. This
tool ensures the required matrix file stays valid and shows what must run.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_MATRIX = ROOT / "devscripts" / "scenarios" / "matrix" / "actor-weapon-matrix.json"


def load(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def validate(value: dict) -> list[str]:
    errors: list[str] = []
    if value.get("schema") != 1:
        errors.append("schema must be 1")
    pairings = value.get("actor_pairings")
    if not isinstance(pairings, list) or len(pairings) != 4:
        errors.append("actor_pairings must contain the four required pairings")
    for key in ("weapons", "assertions"):
        if not isinstance(value.get(key), list) or not value[key]:
            errors.append(f"{key} must be a non-empty list")
    server = value.get("server")
    if not isinstance(server, dict) or not server.get("profiles"):
        errors.append("server.profiles must be a non-empty list")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--matrix", default=str(DEFAULT_MATRIX))
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--print-plan", action="store_true")
    args = parser.parse_args()

    path = Path(args.matrix)
    if not path.is_absolute():
        path = ROOT / path
    try:
        value = load(path)
    except (OSError, json.JSONDecodeError) as error:
        print(f"[matrix] invalid: {error}")
        return 1

    errors = validate(value)
    if errors:
        for error in errors:
            print(f"[matrix] error: {error}")
        return 1
    print(f"[matrix] valid: {path.relative_to(ROOT)}")
    if args.print_plan:
        for profile in value["server"]["profiles"]:
            for pairing in value["actor_pairings"]:
                for weapon in value["weapons"]:
                    print(
                        f"  {profile}: {pairing['id']} weapon={weapon} "
                        f"attacker={pairing['attacker']} target={pairing['target']}"
                    )
    if args.check:
        print(
            "[matrix] full N-client launch is not implemented yet; "
            "this check validates the acceptance contract."
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
