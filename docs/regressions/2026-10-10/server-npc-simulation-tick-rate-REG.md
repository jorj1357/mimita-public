# Server tick rate collapses under live NPC simulation

Time created: 2026-10-10T20:36:39-04:00
Time last updated: 2026-10-10T20:40:37-04:00
Status: UNRESOLVED

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

This is a strong code-and-timing correlation, not yet a completed A/B proof:

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

The next safe proof is a matched runtime A/B using the same map and NPC count:
disable only the per-NPC `updateNpcAim` call, keep authoritative movement and
collision enabled, and compare `performance.server-tick-window`. If the NPC
stage returns near the prior baseline, the AimBody integration must be moved
out of the authoritative server tick or given a cheaper server-specific path.

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
