# Server NPC regression correlation

- Task ID: server-npc-regression-correlation
- Summary: Compare the fresh low-Hz journal with recent NPC/server changes.
- Status: leading cause identified; A/B runtime proof pending
- Date, time, timezone: 2026-10-10T20:40:37-04:00, America/New_York

## User question

The user suspected that recent NPC/server work changed NPC behavior and caused
the server to fall from a healthy rate to roughly 5 Hz.

## Fresh runtime evidence

- Journal:
  `C:\mimita-v9\logs\2026-10-11\20261011_003742\events-000001.jsonl`
- The server reaches about 59--60 Hz before the NPC roster is active.
- At 67 NPCs, one initial five-tick window reports `npc_avg_ms=844.84` and
  `npc_max_ms=2112.86`.
- After initialization, sustained NPC averages are generally about 220--350
  ms, with peaks above 900 ms. The server therefore cannot meet its 16.67 ms
  fixed-tick budget.
- Gamemode work is mostly below 2 ms after the initial transition, snapshots
  are below 1 ms, and contact-weapon timing is approximately 0.004 ms.
- Recast route events report query times around 0.06--0.10 ms. The one-time
  navmesh bake is about 940 ms at startup, not the sustained cost.
- Coordinator poll records remain `background=true`, so this is not the old
  coordinator-main-thread stall.

## Recent source correlation

Commit `f3ede0ab` (2026-10-08) added a call from the shared NPC fixed-tick
executor to `RagdollModeSystem::updateNpcAim`. That path invokes the full
AimBody solver per NPC, including multi-part integration, joint solving, world
and self collision, depenetration, limits, and transform synchronization.

This is the leading explanation for the regression because it is the major
recent per-NPC authoritative work added after the coordinator fix, and the
journal shows exactly the sustained NPC-stage cost expected from 67 repeated
body solvers. It is not yet claimed as proven because no controlled A/B build
has run.

## Other reviewed changes

- The current uncommitted NPC contact-weapon code is not the cause shown by
  this journal: its aggregate stage is about 0.004 ms.
- The Recast diagnostic expansion adds measurement work, but the journal’s
  route query times are small and the bake occurs once during startup.
- Recent Zombie Tower gamemode work causes a one-time gamemode spike, but not
  the sustained 240--350 ms NPC stage.

## Next proof and correction boundary

Run a matched A/B on the same 67-NPC scenario with only the per-NPC
`updateNpcAim` call disabled. If server NPC timing returns near baseline, keep
server-authoritative movement/collision but remove the full render-oriented
AimBody solver from the server fixed tick, or replace it with a cheaper
server-specific pose/collision path. Do not infer success from compilation;
the real journal must show restored tick timing.

## Evidence boundary

- Source evidence: `git blame` identifies `f3ede0ab` as the introduction of
  `updateNpcAim` in `src/npc/npc.cpp:2265-2275`.
- Runtime evidence: fresh user-provided server journal analyzed above.
- Build evidence: no build or source behavior change performed in this turn.
- Human acceptance: pending the matched A/B and live gameplay review.
