"""Verify the last gate recording roundtrip produced a complete scenario file.

Run after devscripts/scenarios/scenario_record_roundtrip_v1.json.
Checks header, server rows, valid footer, and absence of a leftover sidecar.
Deletes the test artifact only after a successful check.
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
USER_DIR = ROOT / "devscripts" / "scenarios" / "user"


def fail(message: str) -> int:
    print(f"[SCENARIO ROUNDTRIP] FAIL: {message}")
    return 1


def main() -> int:
    candidates = sorted(
        USER_DIR.glob("scenario-*-gate-record-roundtrip.jsonl"),
        key=lambda p: p.stat().st_mtime,
    )
    if not candidates:
        return fail("no scenario-*-gate-record-roundtrip.jsonl was written")
    path = candidates[-1]
    print(f"[SCENARIO ROUNDTRIP] file: {path}")
    if path.stat().st_size <= 0:
        return fail("scenario file is empty")

    header = None
    footer = None
    server_ticks = 0
    client_ticks = 0
    with path.open("r", encoding="utf-8", errors="replace") as handle:
        for line in handle:
            try:
                value = json.loads(line)
            except json.JSONDecodeError:
                continue
            kind = value.get("kind", "")
            if kind == "header":
                header = value
            elif kind == "footer":
                footer = value
            elif kind == "server_tick":
                server_ticks += 1
            elif kind == "tick":
                client_ticks += 1

    if not isinstance(header, dict):
        return fail("missing header")
    if not isinstance(footer, dict):
        return fail("missing footer")
    if client_ticks <= 0:
        return fail("missing client ticks")
    if server_ticks <= 0:
        return fail("missing server ticks")
    if not footer.get("valid", False):
        return fail(f"footer is invalid: {footer.get('capture_failure', '')}")
    if int(footer.get("server_ticks", 0) or 0) <= 0:
        return fail("footer has no merged server ticks")

    sidecar = path.with_suffix(path.suffix + ".server.jsonl")
    if sidecar.exists():
        return fail(f"leftover sidecar not merged: {sidecar}")

    print(
        f"[SCENARIO ROUNDTRIP] PASS: client_ticks={client_ticks} "
        f"server_ticks={server_ticks} valid=true"
    )
    path.unlink(missing_ok=True)
    print("[SCENARIO ROUNDTRIP] removed test artifact after successful check")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
