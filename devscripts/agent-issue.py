#!/usr/bin/env python3
"""Validated issue intake front-end for MiMITA agents."""

from __future__ import annotations

import argparse
import json
import os
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from devscripts.agent_coord import CoordError, CoordStore  # noqa: E402


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--agent", default=os.environ.get("MIMITA_AGENT_ID", ""))
    parser.add_argument("--root", default="")
    parser.add_argument("--id", default="")
    parser.add_argument("--title", required=True)
    parser.add_argument("--quote", required=True)
    parser.add_argument("--summary", required=True)
    parser.add_argument("--kind", default="bug")
    parser.add_argument("--source", default="user")
    parser.add_argument("--via", default="")
    parser.add_argument("--severity", default="normal")
    parser.add_argument("--priority", default="normal")
    parser.add_argument("--owner", default="")
    parser.add_argument("--reproduction", required=True)
    parser.add_argument("--expected-behavior", required=True)
    parser.add_argument("--acceptance", required=True)
    parser.add_argument("--paths", nargs="+", required=True)
    parser.add_argument("--dependencies", nargs="*", default=[])
    parser.add_argument("--risk", required=True)
    parser.add_argument("--proof-expectation", required=True)
    parser.add_argument("--evidence", required=True,
                        help="JSON object with exact evidence")
    parser.add_argument("--context", default="")
    args = parser.parse_args()
    if not args.agent:
        print("[AGENT ISSUE] --agent or MIMITA_AGENT_ID is required",
              file=sys.stderr)
        return 2
    try:
        evidence = json.loads(args.evidence)
    except json.JSONDecodeError as error:
        print(f"[AGENT ISSUE] --evidence must be JSON: {error}", file=sys.stderr)
        return 2
    if not isinstance(evidence, dict):
        print("[AGENT ISSUE] --evidence must be a JSON object", file=sys.stderr)
        return 2
    store = CoordStore(Path(args.root) if args.root else None)
    try:
        result = store.issue_add(
            args.agent, args.title, quote=args.quote, summary=args.summary,
            kind=args.kind, source=args.source, via_agent=args.via,
            severity=args.severity, priority=args.priority, owner=args.owner,
            evidence=evidence, issue_id=args.id,
            reproduction=args.reproduction,
            expected_behavior=args.expected_behavior,
            acceptance=args.acceptance, paths=args.paths,
            dependencies=args.dependencies, risk=args.risk,
            proof_expectation=args.proof_expectation,
            context_id=args.context)
    except CoordError as error:
        print(f"[AGENT ISSUE] {error}", file=sys.stderr)
        return error.code
    finally:
        store.close()
    print(json.dumps(result, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())


