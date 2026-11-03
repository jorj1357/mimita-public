# Performance regressions

Append-only master ledger. Each entry links to the detailed dated record that owns the full evidence.

Entries: 5

---

## Entry 1 — NPC Avatar Frame Stall

- Recorded: 2026-09-09T11:51:43Z
- Status: ATTEMPTED FIX (1)
- Summary: docs/specs/performance/performance.md
- Detail: `docs/regressions/2026-09-09/npc-avatar-frame-stall-REG.md`

## Entry 2 — Frame Timing Policy Repeats

- Recorded: 2026-09-21T01:23:53Z
- Status: UNRESOLVED
- Summary: - docs/architecture/time-and-formatting/time-and-formatting.md - docs/features/live-code-development/live-code-development.md
- Detail: `docs/regressions/2026-09-21/frame-timing-policy-repeats-REG.md`

## Entry 3 — Repeated Background Work in the Frame Loop

- Recorded: 2026-09-21T02:00:00Z
- Status: SOLUTION AS OF 2026-09-21T02:54:48Z
- Summary: docs/features/live-code-development/live-code-development.md
- Detail: `docs/regressions/2026-09-21/repeated-background-work-REG.md`

## Entry 4 — Animation and death frame choppiness

- Recorded: 2026-10-28T13:34:00Z
- Status: ATTEMPTED FIX (1) — major measured improvement; full performance acceptance remains open
- Summary: docs/specs/performance/performance.md
- Detail: `docs/regressions/2026-10-28/animation-death-frame-choppiness-REG.md`

## Entry 5 — Weapon hot reload re-scanned the animation library per melee action

- Recorded: 2026-11-01T22:55:00Z
- Status: IMPLEMENTED IN SOURCE; FREEZE REDUCED; LONG-RUN VISUAL PENDING
- Summary: docs/specs/debug-logging/debug-logging.md
- Detail: `docs/regressions/2026-11-01/weapon-reload-full-scan-REG.md`

## Entry 6 — Fixed-tick stalls from NPC containment and per-record journal flushes

- Recorded: 2026-11-03T19:09:00Z
- Status: ATTEMPTED FIX (1) — containment path removed and flush bounded; runtime review pending
- Summary: 707 fixed-tick stalls with NPC stage 112-320 ms, and every Important event flushed the journal under a cross-process mutex.
- Detail: `docs/regressions/2026-11-03/server-tick-budget-REG.md`

## Entry 7 — Scenario recorder wrote a second full per-tick copy

- Recorded: 2026-11-03T19:09:00Z
- Status: ATTEMPTED FIX (1) — canonical capture is journal-only in source; runtime review pending
- Summary: Canonical recording wrote both the executable journal and a per-tick scenario sidecar, doubling serialization and hiding capture cost inside the replay scope.
- Detail: `docs/regressions/2026-11-03/scenario-journal-only-REG.md`
