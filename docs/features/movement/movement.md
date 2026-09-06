// 09 06 2026, 12 00
/* purpose
* track movement timing, evidence, and open decisions
* link movement specifications, tests, logs, changelogs, and regressions
* distinguish local frame responsiveness from 60 Hz authoritative replication
* this file DOES NOT replace docs/specs/movement/movement.md
* this file DOES NOT define new movement behavior by itself
* this file DOES NOT claim multiplayer acceptance without two-client evidence
*/

# Movement feature record

## End goal

Sample input every render frame, show local movement with minimum latency, run
authoritative gameplay at 60 Hz, and replicate snapshots to other clients at up
to 60 Hz using shared movement primitives.

## Current status

- Status: specification clarification required

## Links

- Specification: `docs/specs/movement/movement.md`
- Review skill: `docs/skills/spec-behavior-review-v1.md`
- Tests: `tests/features/movement/`
- Logs: `logs/features/movement/`
- Regressions: `docs/regressions/regressions-v1.md`
- Changelogs: add immutable session links here as work completes

## Acceptance

- Local input is observed on the first eligible render frame.
- Authoritative movement and collision remain fixed at 60 Hz.
- Remote clients receive movement snapshots at the configured rate.
- Frame, tick, send, receive, and display timestamps are preserved.
- Two-client acceptance remains separate from source and build evidence.
