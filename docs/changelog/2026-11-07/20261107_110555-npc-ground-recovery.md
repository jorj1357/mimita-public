# NPC ground recovery diagnosis and guard

- Recorded: 2026-11-07 11:05:55 EST
- Branch: `20261011zt`
- HEAD before work: `8e871c39`
- Commits created: none
- Result: `PASS_WITH_HUMAN_REVIEW` for the downward-pushout recovery fix; the initial capsule floor-contact miss remains under investigation.

## User request

Diagnose why NPCs sometimes fall through or enter the ground, explain prior
collision/recovery work, make a fix, and prove it through the canonical
`events.jsonl` journal.

## Pre-existing work and repository state

`git symbolic-ref --short HEAD` and `git rev-parse --short HEAD` reported branch
`20261011zt` and HEAD `8e871c39`. `git status --short --branch` and
`git diff --check` could not read the index (`bad signature 0x00000000; index
file corrupt`). The index was not repaired or changed. This prevented a reliable
inventory of other staged/unstaged edits and a Git diff review; the edits in
this record are limited to the files listed below.

## Investigation and first divergence

The existing regression history showed several earlier changes:

- 2026-11-01 bounded the NPC ground-height query from 100 m to 4 m, removed
  downward-through-solid safe-point recovery, and reran containment after
  actor-pair collision. That work had prior partial runtime evidence.
- 2026-11-03 removed fabricated player-template body colliders from headless
  NPCs after they caused phantom contacts, high tick costs, and repeated
  containment resets. The documented headless path is a capsule plus bounded
  ground clamp.
- 2026-11-03/04 fixed NPC collision-world geometry and solid-ID filtering after
  recorded NPCs fell through an empty or incomplete world. The current run's
  world summary did not show that failure.

The active live Fun Counter Strike run was on `dust2cyberiav4`. Its server
journal, `logs/2026-11-07/20261107_154026_01/server-01/events.jsonl`, recorded
`npc.collision-world.summary` with 5,463 headless triangles, 5,463 NPC-world
triangles, and aligned solid IDs. It also recorded 60 `npc.root.pushout`, 38
`npc.root.inside`, and 10 `npc.below-map.respawned` events.

The first confirmed wrong step for actor 100004 was in
`simulateSharedNpcs`' periodic/stuck recovery (`src/network/server-npcs.cpp`):

- Tick 4,296: `npc.root.pushout` moved actor 100004 from
  `(-867.2983, 150.5471, 2343.2297)` down by 1.9380 m to z `2341.2917`.
- Tick 4,442: the same actor was at z `2133.2598`, below map bound
  `2295.3118`; `npc.below-map.respawned` finally returned it to spawn.

`pushActorRootOutOfSolid` selected the nearest enclosing face. The server then
treated “outside the solid” as proof of a safe recovery, even when that face was
the floor slab's bottom. The NPC was released underneath the walkable surface.
This was the first proven cause; it does not explain every fall event.

`actor-collision.fallback` records also showed that headless NPCs have zero body
parts and use the legacy capsule backend while the actor-triangle backend is
requested. The world was present. The old journal did not record the per-sweep
candidate and hit counts needed to tell whether that capsule path missed the
floor on other actors.

## Changes

1. `src/network/server-npcs.cpp`, `simulateSharedNpcs` recovery block:
   - Old: accept a nearest-face pushout whenever the new root is outside the
     solid, including a negative-Z correction.
   - New: if `pushDelta.z < -0.02f`, restore the pre-correction root, emit
     `npc.root.pushout.rejected`, and try the existing validated standing-point
     recovery. Accepted non-downward pushouts retain the existing behavior.
   - Old: continue simulating an NPC with no valid standing point until three
     failed containment checks. New: immediately call `respawnServerNpc` when
     the bounded safe-point search has no valid result, and emit
     `npc.root.respawned` with `reason=no_validated_safe_point` and from/to.
   - Old: require 60 ticks below the map bound before respawning. New: the
     below-map guard respawns on the next fixed tick.

2. `src/physics/movement/physics-collision-glb-main.cpp`, after the legacy
   capsule solve:
   - Added rate-limited `npc.collision.fall-sample`, once per NPC every 30
     ticks while the NPC is descending without a world-floor contact. It records
     start position, requested movement, final position, velocity, capsule feet,
     candidate count, sweep iterations/hits, recovery contacts, and penetration.
   - This makes the headless fallback's actual collision result observable; it
     does not change collision response.

3. Appended Occurrence 3 to
   `docs/regressions/collisions/npc-closed-block-REG-2026-11-01.md` with the
   failure chain, fix, evidence, and remaining limits.

## Documents and focused review

- `docs/specs/npc-behavior/npc-behavior.md`, especially final-pose recovery
  rules and the headless NPC boundary.
- `docs/architecture/collision/collision.md`, actor/world collision ownership,
  headless collision geometry, and the rule against recovering downward
  through a solid.
- `docs/workflows/runtime-scenario-validation.md` and
  `docs/specs/dev-loop/dev-loop.md` for live executable and journal evidence.
- `docs/specs/debug-logging/debug-logging.md` for structured, bounded events.
- `docs/regressions/README.md`,
  `docs/regressions/collisions/npc-closed-block-REG-2026-11-01.md`,
  `docs/regressions/collisions/npc-template-body-REG-2026-11-03.md`, and
  `docs/regressions/collisions/tdmtest3-npc-world-REG-2026-11-03.md`.
- Prior implementation evidence read:
  `docs/changelog/2026-11-01/20261101_021333-npc-recovery-and-player-final-pose.md`
  and `docs/changelog/2026-11-03/20261103_195500-npc-body-collision-and-tick-budget.md`.
- Focused reviews: `docs/skills/spec-behavior-review-v1.md` and
  `docs/skills/logging-checker-v1.md`. The spec review found a disagreement:
  recovery accepted a downward nearest-face exit despite the spec's requirement
  that recovery must not pass through a solid to a lower floor. The logging
  review found the prior fallback event identified the backend but not its
  sweep result; `npc.collision.fall-sample` fills that evidence gap.

## Validation and evidence

### Source and build

- The canonical dev-loop built the recovery changes successfully as
  `2026-11-07T15:59:33.671985Z` with build ID
  `fbb6c9494de679e6ec71dd25f8c8563006da3d9b1ee77c32e16a43183bc0b085`.
- The canonical dev-loop then built the fall-sample diagnostic successfully as
  `2026-11-07T16:03:14.815682Z` with build ID
  `b336717d0c367a74498a75fa817565143e5b244691ec21bf12a8e942ee111469`.
  Status reported `published_current`, `latest_source=5`, and `stale=False`.
- No tests or synthetic scenarios were added or run.

### Runtime

Natural mode-47 run on the first recovery build used server journal
`logs/2026-11-07/20261107_155949/server-01/events.jsonl` and build ID
`fbb6c9494de679e6ec71dd25f8c8563006da3d9b1ee77c32e16a43183bc0b085`.
It recorded:

- 2 `npc.root.pushout.rejected` events, including a `-1.8301 m` correction;
- 8 `npc.root.respawned` events after no safe point was found;
- 67 `npc.trace` summaries;
- 0 `npc.below-map.respawned` events and 0 trace samples below the map bound.

This proves the recovery path rejected downward exits and prevented the observed
long below-map falls in that run. It also shows the initial solid/capsule problem
still occurs: some actors need immediate respawn because local safe-point
recovery fails. The 73 `actor-collision.fallback` records identify the capsule
backend used, but do not by themselves prove why its floor contact was missed.

The final build containing `npc.collision.fall-sample` was not launched. During
the work the dev-loop selection changed from mode 47 to mode 51 (Bloodtest),
whose status showed `npcs=0`; the active run was left untouched. Therefore the
new event has build evidence only and still needs a same-mode live runtime check.
The mode-47 run had no scenario exit status because it was an unscripted live
run; its exact server journal is listed above.

### Human review

No direct visual or gameplay review was performed. The journal verifies
recovery and below-map bounds, not whether respawning is acceptable or whether
NPC movement feels correct. Human review remains required.

## Final state

The downward nearest-face recovery bug is source-fixed and runtime-proven for
the recorded mode-47 run. The latest source builds successfully. The initial
capsule collision miss is not yet conclusively diagnosed; relaunch the latest
artifact in mode 47 and inspect `npc.collision.fall-sample` at the first loss of
world contact, then correct the capsule sweep owner if the candidate/hit fields
confirm a miss. The current mode-51 run was preserved.
