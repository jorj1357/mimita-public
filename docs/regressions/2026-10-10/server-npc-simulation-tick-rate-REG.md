# Server tick rate collapses under live NPC simulation

Time created: 2026-10-10T20:36:39-04:00
Time last updated: 2026-10-10T20:49:30-04:00
Status: ATTEMPTED FIX (1)

## Goal behavior

The authoritative gameplay loop must sustain its fixed 60 Hz contract. Each
fixed tick has approximately 16.67 ms of work available. NPC simulation may
not consume several hundred milliseconds per tick and leave the server
running at roughly 5 Hz.

## Observed occurrence

- Source journal:
  `C:\mimita-v9\logs\2026-10-11\20261011_003350\events-000001.jsonl`
- The run begins near 59--60 Hz with one NPC and an NPC stage of about 3 ms.
- Around tick 151, the live NPC count rises from 1 to 67. The same journal
  then reports `NpcSimulation` averages of approximately 90--243 ms and
  maxima up to approximately 1,630 ms.
- During that interval, outer-loop actual Hz falls to approximately 4.04--6.92;
  one later window reports 4.84 Hz with a 1,028.9 ms loop average.
- Snapshot, projectile, contact, and gamemode stages remain small in the
  reported fixed-tick windows. The measured dominant stage is NPC simulation.

## Expected specification

- `docs/specs/performance/performance.md`: gameplay collision, damage, and
  physics remain fixed at 60 Hz and performance work is evidence-led.
- `docs/specs/networking/networking.md`: the authoritative server owns the
  fixed-tick simulation and state sent to clients.
- `docs/specs/debug-logging/debug-logging.md`: diagnosis uses bounded
  `StructuredLogger` events in the canonical JSONL journal.

## Wrong code / owner

The authoritative tick owner is `simulateOneServerTick` in
`src/network/server.cpp`. It calls `simulateSharedNpcs` once per fixed tick
(`src/network/server.cpp:809-816`). The broad expensive owner is
`Server::NpcSimulation` in `src/network/server-npcs.cpp:977-989`.

That owner currently combines per-NPC target selection, movement/navigation,
ground correction, weapon/combat processing, and related state updates in one
timed scope. The fresh journal proves the aggregate cost, but it does not yet
prove which inner phase is responsible.

## Confirmed cause

Confirmed at the aggregate stage level: the server is CPU-bound in the live
NPC simulation stage after the NPC population reaches about 66. This is not a
deliberate 5 Hz setting and is not explained by snapshot serialization.

Not confirmed yet: the exact inner cost among navigation/path planning,
perception/target selection, movement/collision, decisions/utility, and firing.

## Coordinator comparison

This is distinct from the earlier coordinator regression. In this journal,
`network.coordinator-ice-poll` records are marked `background=true` and take
approximately 46--200 ms, proving that the recurring coordinator wait is no
longer directly occupying the authoritative fixed-tick loop. The earlier
coordinator fix remains relevant history, but it is not the direct owner of
this occurrence.

## Recent-change correlation

The strongest recent source change is commit `f3ede0ab` from 2026-10-08,
after the coordinator fix. It added this per-NPC path to
`src/npc/npc.cpp:2265-2275`:

```text
RagdollModeSystem::instance().updateNpcAim(...)
```

That path enters `RagdollModeSystem::stepAimBody`, which performs multi-part
AimBody integration, joint solving, world collision, self-collision,
depenetration, rotation limits, and model-transform synchronization for each
NPC. The server journal also shows AimBody hybrid activity for this run.

The new journal makes the failed-path part of the cause concrete:

- It contains approximately 82,958 `actor-hybrid.pose-input` events from the
  server, not the client.
- Every sampled record reports `parts: 0` while `aimbody_mode` is `hybrid`.
- `updateNpcAim` retries whenever `body.parts.empty()` is true, so a failed
  empty-body construction is retried and logged for the same NPC on every
  update. This explains why NPCs look normal while the server still performs
  the path.

The performance impact is now source-and-journal confirmed at the failed-path
level. The exact percentage attributable to the failed body setup versus the
remaining normal NPC simulation still needs a post-fix runtime journal.

- In the fresh run, ordinary Recast route queries report approximately
  `0.06--0.10 ms`, so the regular route query is not large enough to explain
  the approximately `240--350 ms` sustained NPC stage.
- The first 67-NPC window has a one-time NPC maximum of approximately
  `2,113 ms`, consistent with initialization work; subsequent windows remain
  approximately `220--450 ms` while the same 67 NPCs continue updating.
- The uncommitted NPC contact-weapon loop is measurable in the journal at only
  about `0.004 ms` and is not a plausible explanation for the sustained stall.
- The recent Recast diagnostics include a one-time startup bake of about
  `940 ms`, but the navmesh is version 1 afterward and the continuing route
  queries are short.

## Attempted fix (1)

Removed the server-side `updateNpcAim` call and its dead-NPC cleanup call from
`src/npc/npc.cpp`. This preserves the normal authoritative NPC movement and
collision path, which is the behavior the current client is actually showing,
and prevents the server from repeatedly attempting a render-oriented AimBody
that has no parts. The player/client AimBody path remains unchanged.

The required proof is a new real 67-NPC run whose journal has no repeated
`actor-hybrid.pose-input` server events and whose fixed-tick NPC stage returns
near the pre-regression baseline.

## Follow-up occurrence: 2026-10-11_004504

- The journal is `C:\mimita-v9\logs\2026-10-11\20261011_004504\events-000001.jsonl`.
- The server starts near 59--60 Hz with zero NPCs, then reaches 67 NPCs and
  falls into alternating stalls: `0.77--48.22 Hz` in the sampled windows,
  including a `6,462 ms` outer-loop maximum.
- The fixed-tick evidence identifies NPC simulation as the recurring owner:
  after activation, NPC averages range from approximately `13.6--126.9 ms`
  in the sampled windows, with maxima up to `713.7 ms`. Gamemode work is
  generally below `1.3 ms` after the transition.
- The client reports snapshot inter-arrival gaps of `1,001--3,922 ms`, while
  `snapshots_missed=0` and `tick_gap=1`. This is a server-stall/backlog
  pattern: delayed snapshots are later observed in bursts, not evidence that
  the client randomly dropped twenty packets.
- This run used `.dev\builds\1964\mimita.exe`, built at `19:32:15`, before the
  attempted source fix was built at `20:48:24`. It therefore cannot prove or
  disprove the fix.

## Diagnostics added

`src/network/server.cpp::reportServerPerf` now emits one bounded
`network.server-io-window` event per performance window for the dedicated
server. It records packet-in/out totals and deltas plus receive attempts,
would-blocks, errors, malformed packets, protocol mismatches, unknown packets,
joins, reconnects, and input packets. This lets the next matched run separate
authoritative simulation stalls from transport receive/send backlog without
per-packet journal spam.

## Required correction

1. Split the `Server::NpcSimulation` timing into navigation/path planning,
   perception/visibility and target selection, movement/collision,
   decisions/utility, and firing/combat.
2. Re-run a matched real scenario with one-NPC and approximately 66-NPC
   windows, correlating stage timing with `performance.server-loop-window`
   and `network.coordinator-ice-poll`.
3. Apply the smallest safe fix to the proven inner owner. Expensive NPC work
   may be budgeted or amortized, but authoritative movement, collision,
   damage, and fixed-tick ordering must remain server-owned and deterministic.
4. Rebuild with the canonical process, run the actual scenario, inspect the
   new journal, and keep build, runtime, and human acceptance as separate
   evidence claims.

## Related history

- `docs/changelog/2026-10-08/20261008_142032-server-tick-npc-log-investigation.md`
  added the bounded fixed-tick stage diagnostics used by this occurrence.
- `docs/changelog/2026-10-08/20261008_150000-human-ai-runtime-investigation-loop.md`
  records the coordinator move and explicitly leaves NPC-stage spikes open.
- `C:\Users\guita\.codex\memories\MEMORY.md` records the same prior
  conclusion: coordinator polling was moved off-loop, while rare NPC-stage
  spikes remained to be split and measured.
