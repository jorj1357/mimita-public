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

## Follow-up evidence — 2026-11-02

The user reported that keeping a regression record helped and that the latest
website database outage was resolved in about one or two prompts, with GPT-6
Luna at Medium. This is user-reported experience, not an independently
measured prompt count or verified model-session record. The user gave the time
as 2026-11-02 09:54 without a timezone; treating that as New York local time
(EST on this date) gives approximately 2026-11-02T14:54:00Z.

Operational evidence recorded for the recovery: PostgreSQL accepted
connections, a read-only `SELECT 1` succeeded, the public homepage returned
HTTP 200, an invalid sign-in returned the expected HTTP 401, and a 7,178,142
byte custom-format database archive passed its archive listing check. These
checks show recovery and database reachability; the archive has not been
restore-tested, and no human sign-in/signup acceptance was recorded. The new
website source health endpoint and recovery/storage services are not yet
deployed. See the [recurrence record](../regressions/gameplay/website-auth-database-REG-2026-09-16.md)
and [2026-11-02 recovery changelog](../changelog/2026-11-02/20261102_142003-website-database-storage-turn-plan.md).
