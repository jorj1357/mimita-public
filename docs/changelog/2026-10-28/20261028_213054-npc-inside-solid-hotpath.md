# NPC inside-solid recovery hot path

- Recorded: 2026-10-28T21:30:54Z (2026-10-28 17:30:54 EDT)
- Branch: `20261011zt`
- Starting HEAD: `5e1223b9`
- Result: `PASS_WITH_HUMAN_REVIEW`
- Build: `3700` (`source_current=true`)
- Runtime evidence: `C:\mimita-v9\logs\2026-10-28\20261028_212353\events.jsonl` (build 3695)

## Root cause

The listen server dropped to 17–19 Hz because `simulateSharedNpcs()` called
`worldPointInsideSolid(world, n.body.pos)` for every NPC every tick. That
function linearly scans every world collision triangle. `funworld3` has 44,214
collision triangles; 9 NPCs produced about 398k ray-triangle tests per tick.

Evidence from `20261028_205629` (build 3687):

- client/listen server: `actual_hz` 17.3–18.8, `npc_avg_ms` 50–56;
- dedicated server with 0 NPCs: 59.8–60.5 Hz;
- earlier 9-NPC run on a 1,280-triangle map: `npc_avg_ms` about 1.6.

The scaling matched the full triangle scan, not networking.

## Change

`src/network/server-npcs.cpp`:

- removed the per-NPC every-tick `worldPointInsideSolid` call from the main NPC
  loop;
- added one staggered capsule-recovery candidate check per server tick;
- candidates trigger from `NpcNavigation::isStuck`, `wasStuck`, or an active
  stuck episode, with a 120-tick per-NPC periodic fallback;
- added a 120-tick per-NPC cooldown so an NPC that remains inside does not repeat
  the expensive recovery every tick;
- replaced the full `resolveSafeStandingSpawn` recovery in this hot path with a
  broadphase `NpcNavigation::groundHeightAt` ground snap plus one inside-solid
  validation. Full safe-spawn search remains at spawn/respawn.

`docs/specs/open-gameplay-issues.md`:

- recorded the future per-tick accelerated ray query (chunk-DDA/AABB-tree)
  instead of scanning every collision triangle;
- recorded that occasional multi-hundred-ms NPC navigation spikes remain
  separate from this inside-solid fix and need their own profile.

## Runtime result

9 NPCs on `funworld3` (build 3695): most one-second server windows were
49–63 Hz with `npc_avg_ms` about 1.8–9 and `npc_max_ms` usually 20–120 ms, down
from the 17–19 Hz / 50–56 ms continuous cost. One remaining window showed an
8.4 Hz / 2.0 s NPC spike with no `npc.capsule.recovered` event; that is a
separate NPC navigation cost, not the inside-solid scan.

## Validation

- Dev-loop build `3700`: success, source-current.
- Runtime server windows inspected for 9 NPCs on `funworld3`.
- No collision or NPC config values changed.
- `git diff --check`: PASS except CRLF conversion warnings.

## Human review still required

- Confirm the server tick display is stable around 60 Hz during normal play.
- Confirm NPCs still recover from deliberate inside-geometry placement.
- Profile the remaining occasional NPC navigation spike separately.
