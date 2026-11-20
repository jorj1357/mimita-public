#!/usr/bin/env python3
"""Correlate melee contact IDs across client and server journals."""

from __future__ import annotations

import argparse
import json
import sys
from collections import Counter
from pathlib import Path


def _iter_jsonl(path: Path):
    with path.open("r", encoding="utf-8", errors="replace") as handle:
        for line in handle:
            line = line.strip()
            if not line:
                continue
            try:
                row = json.loads(line)
            except json.JSONDecodeError:
                continue
            if isinstance(row, dict):
                yield row


def _journal_paths(run_dir: Path) -> list[Path]:
    paths = []
    server = run_dir / "server-01" / "events.jsonl"
    if server.is_file():
        paths.append(server)
    paths.extend(sorted(run_dir.glob("client-*/events.jsonl")))
    return paths


def _field(row: dict, name: str, default=None):
    fields = row.get("fields")
    if isinstance(fields, dict) and name in fields:
        return fields[name]
    return default


def _contact_key(value) -> str:
    try:
        return str(int(value))
    except (TypeError, ValueError):
        return str(value)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("run_dir", type=Path)
    parser.add_argument("--json", action="store_true")
    parser.add_argument("--strict", action="store_true")
    args = parser.parse_args()

    run_dir = args.run_dir.resolve()
    paths = _journal_paths(run_dir)
    if not paths:
        raise SystemExit(f"no melee journals under {run_dir}")

    claims: dict[str, dict] = {}
    sent: set[str] = set()
    decisions: dict[str, dict] = {}
    batches = 0
    conflicts: list[dict] = []

    for path in paths:
        for row in _iter_jsonl(path):
            event = str(row.get("event") or "")
            if event == "melee.client.contact_claim":
                contact_id = _contact_key(_field(row, "contact_id", ""))
                if contact_id:
                    claims[contact_id] = {
                        "contact_id": contact_id,
                        "target": _field(row, "target"),
                        "target_is_npc": _field(row, "target_is_npc"),
                        "target_generation": _field(row, "target_generation"),
                        "contact_tick": _field(row, "contact_tick"),
                        "damage_predicted": _field(row, "damage_predicted"),
                        "source": path.name,
                    }
            elif event == "melee.client.contact_claim.batch":
                batches += 1
                first = _contact_key(_field(row, "first_contact_id", ""))
                last = _contact_key(_field(row, "last_contact_id", ""))
                try:
                    for value in range(int(first), int(last) + 1):
                        sent.add(str(value))
                except (TypeError, ValueError):
                    continue
            elif event in {"melee.contact.claim.accepted", "melee.contact.claim.rejected"}:
                contact_id = _contact_key(_field(row, "contact_id", ""))
                if not contact_id:
                    continue
                decision = decisions.get(contact_id)
                if decision and decision.get("event") != event:
                    conflicts.append({
                        "contact_id": contact_id,
                        "first": decision.get("event"),
                        "second": event,
                    })
                decisions[contact_id] = {
                    "event": event,
                    "stage": _field(row, "stage"),
                    "reason": _field(row, "reason"),
                    "damage": _field(row, "damage"),
                    "target": _field(row, "target"),
                    "target_generation": _field(row, "target_generation"),
                    "contact_tick": _field(row, "contact_tick"),
                }

    rejected = Counter()
    accepted = 0
    accepted_match = 0
    accepted_mismatch = 0
    accepted_without_prediction = 0
    unsent: list[str] = []
    unanswered: list[str] = []
    orphaned: list[str] = []
    first_divergences: list[dict] = []

    for contact_id, claim in claims.items():
        if contact_id not in sent:
            unsent.append(contact_id)
            first_divergences.append({
                "contact_id": contact_id,
                "stage": "claim_not_batched",
                "target": claim.get("target"),
            })
        decision = decisions.get(contact_id)
        if not decision:
            unanswered.append(contact_id)
            first_divergences.append({
                "contact_id": contact_id,
                "stage": "server_no_decision",
                "target": claim.get("target"),
            })
            continue
        if decision["event"].endswith(".rejected"):
            rejected[str(decision.get("stage") or "unknown")] += 1
            first_divergences.append({
                "contact_id": contact_id,
                "stage": decision.get("stage"),
                "reason": decision.get("reason"),
                "target": claim.get("target"),
            })
            continue
        accepted += 1
        predicted = claim.get("damage_predicted")
        actual = decision.get("damage")
        if predicted is None:
            accepted_without_prediction += 1
            continue
        if actual is not None and int(predicted) == int(actual):
            accepted_match += 1
        else:
            accepted_mismatch += 1
            first_divergences.append({
                "contact_id": contact_id,
                "stage": "damage_mismatch",
                "predicted_damage": predicted,
                "actual_damage": actual,
                "target": claim.get("target"),
            })

    for contact_id, decision in decisions.items():
        if contact_id not in claims:
            orphaned.append(contact_id)

    result = {
        "run_dir": str(run_dir),
        "journals": [str(path) for path in paths],
        "batches": batches,
        "client_claims": len(claims),
        "claims_sent": len(sent),
        "claims_not_sent": len(unsent),
        "server_decisions": len(decisions),
        "accepted": accepted,
        "accepted_damage_match": accepted_match,
        "accepted_damage_mismatch": accepted_mismatch,
        "accepted_without_prediction": accepted_without_prediction,
        "rejected_by_stage": dict(sorted(rejected.items())),
        "server_without_client_claim": len(orphaned),
        "duplicate_conflicting_decisions": len(conflicts),
        "claim_unanswered": len(unanswered),
        "first_divergences": first_divergences[:50],
        "conflicts": conflicts[:20],
        "orphaned_contacts": orphaned[:50],
        "unsent_contacts": unsent[:50],
        "unanswered_contacts": unanswered[:50],
    }

    if args.json:
        print(json.dumps(result, indent=2))
    else:
        print(f"run: {run_dir}")
        print(f"batches={batches} client_claims={len(claims)} sent={len(sent)}")
        print(f"server accepted={accepted} rejected={sum(rejected.values())}")
        print(f"accepted damage match={accepted_match} mismatch={accepted_mismatch}")
        if rejected:
            print("rejected stages: " + ", ".join(
                f"{stage}={count}" for stage, count in sorted(rejected.items())))
        if first_divergences:
            print("first divergences:")
            for item in first_divergences[:10]:
                print("  " + json.dumps(item, separators=(",", ":")))

    if args.strict and (
        unsent or unanswered or accepted_mismatch or conflicts or
        any(stage in {"unknown", "None"} for stage in rejected)
    ):
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
