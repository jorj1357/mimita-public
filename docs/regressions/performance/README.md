# Performance regressions

Domain regression index. Each linked record owns its chronology and evidence.

Entries: 11
---

## Evidence-backed recurring problems and fixes

| Recurring pattern | Evidence-backed response |
|---|---|
| NPC, contact, or per-actor work overruns fixed-tick budget. | Record phase timings, precompute map data before ticks, and optimize the measured owner. |
| Synchronous journal or recording I/O stalls a tick or heartbeat. | Buffer diagnostics and move merges/writes off latency-sensitive paths; inspect the exact run. |

---

## Entry 1 — NPC Avatar Frame Stall

- Recorded: 2026-09-09T11:51:43Z
- Status: ATTEMPTED FIX (1)
- Summary: docs/specs/performance/performance.md
- Detail: [npc-avatar-frame-REG-2026-09-09.md](npc-avatar-frame-REG-2026-09-09.md)

## Entry 2 — Frame Timing Policy Repeats

- Recorded: 2026-09-21T01:23:53Z
- Status: UNRESOLVED
- Summary: - docs/architecture/time-and-formatting/time-and-formatting.md - docs/features/live-code-development/live-code-development.md
- Detail: [frame-timing-policy-REG-2026-09-21.md](frame-timing-policy-REG-2026-09-21.md)

## Entry 3 — Repeated Background Work in the Frame Loop

- Recorded: 2026-09-21T02:00:00Z
- Status: SOLUTION AS OF 2026-09-21T02:54:48Z
- Summary: docs/features/live-code-development/live-code-development.md
- Detail: [repeated-background-work-REG-2026-09-21.md](repeated-background-work-REG-2026-09-21.md)

## Entry 4 — Animation and death frame choppiness

- Recorded: 2026-10-28T13:34:00Z
- Status: ATTEMPTED FIX (1) — major measured improvement; full performance acceptance remains open
- Summary: docs/specs/performance/performance.md
- Detail: [animation-death-frame-REG-2026-10-28.md](animation-death-frame-REG-2026-10-28.md)

## Entry 5 — Weapon hot reload re-scanned the animation library per melee action

- Recorded: 2026-11-01T22:55:00Z
- Status: IMPLEMENTED IN SOURCE; FREEZE REDUCED; LONG-RUN VISUAL PENDING
- Summary: docs/specs/debug-logging/debug-logging.md
- Detail: [weapon-reload-full-REG-2026-11-01.md](weapon-reload-full-REG-2026-11-01.md)

## Entry 6 — Fixed-tick stalls from NPC containment and per-record journal flushes

- Recorded: 2026-11-03T19:09:00Z
- Status: ATTEMPTED FIX (1) — containment path removed and flush bounded; runtime review pending
- Summary: 707 fixed-tick stalls with NPC stage 112-320 ms, and every Important event flushed the journal under a cross-process mutex.
- Detail: [server-tick-budget-REG-2026-11-03.md](server-tick-budget-REG-2026-11-03.md)

## Entry 8 — Fullscreen shadow cadence flicker

- Recorded: 2026-11-04T00:13:49Z
- Status: ATTEMPTED FIX (2) — source cadence restored; mode 6 visual review pending
- Summary: Fullscreen mode 6 world flicker recurred after shadow-map updates were switched back to alternating frames for performance.
- Detail: [fullscreen-shadow-cadence-REG-2026-11-04.md](fullscreen-shadow-cadence-REG-2026-11-04.md)

## Entry 9 — Streamed world frame stalls

- Recorded: 2026-11-06 (exact time unknown)
- Status: UNRESOLVED — radius-10 repeat run pending
- Summary: Earlier two-cell-radius run exceeded the 4.17 ms frame budget;
  measured stall windows included menu, swap, and audio work.
- Detail: [streamed-world-frame-stalls-REG-2026-11-06.md](streamed-world-frame-stalls-REG-2026-11-06.md)

## Entry 10 — FCS journal rotation stalls and NPC tick overload

- Recorded: 2026-11-08T02:22:00Z
- Status: PARTIAL FIX — synchronous journal copy removed; NPC tick overload and
  reconnect remain unresolved
- Summary: A 100 MB JSONL segment copy held the server journal writer for
  870.635 ms. Separate FCS runs show 19 NPC updates taking about 50–100 ms per
  server tick, with repeated client reconnect failures.
- Detail: [fcs-journal-rotation-and-npc-tick-REG-2026-11-08.md](fcs-journal-rotation-and-npc-tick-REG-2026-11-08.md)

## Entry 11 — Build time and cold-build frequency

- Recorded: 2026-11-08T22:45:00Z
- Status: UNRESOLVED — 20-minute lane rebuild observed; target 2–3 minutes
- Summary: Wide shared headers, divergent lane scheduler roots, low memory, and
  multiple concurrent lanes turned a debug rebuild into a tens-of-minutes
  full rebuild. Build time is now recorded in uild/build-metrics.jsonl.
- Detail: [build-time-REG-2026-11-08.md](build-time-REG-2026-11-08.md)
