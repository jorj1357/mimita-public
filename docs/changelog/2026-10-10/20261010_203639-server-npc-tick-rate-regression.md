# Server NPC tick-rate regression investigation

- Task ID: server-npc-tick-rate-regression
- Summary: Investigate the fresh 5.8 Hz server-performance report and record
  the confirmed regression owner.
- Status: investigation complete; runtime correction pending
- Date, time, timezone: 2026-10-10T20:36:39-04:00, America/New_York

## User report

The user reported a server-performance line near 5.8 Hz and asked whether it
was the previous coordinator-main-thread stall, plus an append-only regression
tracker for the recurrence.

## Evidence reviewed

- Fresh journal:
  `C:\mimita-v9\logs\2026-10-11\20261011_003350\events-000001.jsonl`
- At startup, one NPC produced approximately 59--60 Hz and about 3 ms of NPC
  stage time.
- Once the live population reached 66--67 NPCs, fixed-tick NPC time rose to
  roughly 90--243 ms average, with an observed maximum near 1,630 ms.
- Outer-loop actual Hz consequently fell to approximately 4.04--6.92, with
  a later window at 4.84 Hz and a loop average over 1,028 ms.
- The same journal contains `network.coordinator-ice-poll` events with
  `background=true` and durations around 46--200 ms. This separates those
  waits from the authoritative loop for this occurrence.

## Conclusion

The direct current cause is authoritative NPC simulation overload, not the
previous synchronous coordinator poll. The existing `performance.server-
tick-window` and `performance.server-loop-window` diagnostics are sufficient
to prove the broad owner, but a follow-up must split the NPC scope before a
safe runtime fix is selected.

## Changes made

- Added the append-only regression record:
  `docs/regressions/2026-10-10/server-npc-simulation-tick-rate-REG.md`
- No gameplay, networking, timing, or logging code was changed in this
  investigation.

## Evidence boundary

- Source evidence: confirmed `simulateOneServerTick` ->
  `simulateSharedNpcs` ownership and existing bounded timing events.
- Build evidence: no build requested or performed; no source code changed.
- Runtime evidence: fresh user-provided journal inspected above.
- Human acceptance: not performed. A future fix requires a newly built
  executable and a matched live scenario.
