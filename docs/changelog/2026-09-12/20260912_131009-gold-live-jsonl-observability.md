# Gold live JSONL observability direction

- EST timestamp: 2026-09-12 13:10:09 EDT (UTC 2026-09-12T17:10:09Z)
- Branch: `8292026stash`
- Result: `PASS_WITH_HUMAN_REVIEW`

## Documentation change

Added `docs/gold/2026-09-12-live-jsonl-ai-observability.md` as a gold
reference for the live JSONL journal as MiMITA's machine-readable explanation
of source edits, hot generations, server/client behavior, exact UTC and
monotonic timing, event-chain divergence, and AI-assisted diagnosis.

Updated `docs/architecture/live-development/live-development.md` to classify
`cold_restart_pending` as a severe live-development violation and link the
gold reference.

## Evidence basis

The document preserves the observed direction from live records: authoritative
server hot damage events, source/build/activation events, UTC millisecond
timestamps, and the requirement to attach generation and code hash to runtime
outcomes. No runtime code was changed.

## Human review

Review the gold reference as the intended long-term observability contract.
Runtime work remains required to attach process/session/generation/hash fields
consistently and to replace repeated cold reminders with actionable boundary
diagnostics.

## Pre-existing edits

All unrelated working-tree changes were preserved.
