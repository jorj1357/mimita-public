# Gold behavior: website failures are recoverable in two prompts

## Desired engineering behavior

When a website auth outage occurs, a lightweight AI model should be able to
understand and move the incident forward within roughly two focused prompts:

1. Inspect the repository diff, auth data path, service state, PostgreSQL
   readiness, and recent logs.
2. Apply or recommend the smallest safe recovery, then verify with a
   non-mutating auth probe and clearly separate runtime proof from human signup
   acceptance.

This is an architecture target, not a claim that every future incident will
always fit exactly two prompts.

## What makes the 2026-09-16 incident gold behavior

- The failure had one clear owner chain: browser auth → API → PostgreSQL.
- The unrelated Git diff was easy to rule out.
- `mimita-api online` was not treated as database health.
- Recovery used ordinary PostgreSQL/systemd operations, not an AI-dependent
  loop.
- A safe invalid-login probe proved database reachability without creating data.
- The fix added a structured 503, a user-facing fallback, and a watchdog.
- The problem was diagnosed and recovered substantially faster than expected.

## Architecture rule

Prefer explicit owners, small contracts, observable health states, reversible
operations, and non-mutating proof probes. Keep website auth, database
availability, recovery, and user messaging understandable enough that a small
model can trace the path without reconstructing hidden behavior.

## Anti-patterns

- A process-level “online” state that hides dependency failure.
- Generic 500 responses for known database outages.
- Recovery instructions that exist only in an AI conversation.
- Auth fallbacks that pretend signup/signin succeeded without database proof.
- Website edits that remove or bypass health, watchdog, or recovery evidence.
