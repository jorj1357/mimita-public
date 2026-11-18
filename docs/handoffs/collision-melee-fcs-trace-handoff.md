# Collision, melee, and FCS NPC trace handoff

**Created:** 2026-11-18 (UTC)
**Status:** Source and live-journal trace complete; diagnostic implementation not started.
**Audience:** Next MiMITA coding session working on collision evidence, physical melee, or Fun Counter Strike NPC behavior.

## Purpose

Carry forward the owner-level trace and a small implementation plan so the next
edit can connect existing evidence instead of creating a parallel collision,
damage, or NPC system.

## Does not

- Change collision, weapon, damage, NPC, or objective behavior.
- Claim that a visual snag, unclear hit, or CT spawn cluster was reproduced or
  fixed.
- Authorize a build or replace the shared dev-loop as build authority.

## User-reported goals

1. Understand why the player catches on corners and edges.
2. For a hit or melee contact, see what touched whom, where it happened, and
   the direction and result of the response.
3. In Fun Counter Strike, Ts should travel to a bomb site and plant; CTs should
   stop them, then defuse after a plant. The user has observed CTs bunching near
   spawn.

## Source trace

### World and weapon collision

- The shared world solver is owned by
  `src/physics/movement/physics-collision-core.cpp`, function
  `solveActorWorldCorrection`. Its rate-limited `collision.actor-world` event
  reports actor, generation, contact/manifold counts, maximum penetration,
  aggregate correction, grounded/blocked state, and a small set of source
  triangles. It does not identify a particular contact point or its normal.
- The actor-triangle response path is in
  `src/physics/movement/actor-triangle-solver.cpp`. For each actual triangle
  contact it has `c.point`, depenetration and response normals, surface normal,
  triangle/entity IDs, actor part, penetration, time of impact, velocity before
  response, and velocity after response. The detailed
  `collision.contact.before_response` / `after_response` events are emitted
  only when Collision Trace logging is enabled. This is the strongest existing
  owner-level evidence for corner snags.
- `src/physics/movement/physics-collision-dispatch.cpp` emits
  `collision.bounce.applied` with actor/part, triangle, normal, impact,
  penetration, and velocity/impulse before and after. It does not include the
  world-space contact point. The current `MovementContact` type in
  `src/physics/movement/movement-types.h` already has point, normal, source and
  target entity IDs, surface/material IDs, tick, and contact identity; reuse
  those fields where the specific solver actually has them.
- `DebugVis::recordContact(point, normal, penetration, triangleIndex, label)`
  already stores a point and normal (`src/debug/debug-visuals.cpp`). The current
  player debug draw in `src/physics/movement/physics-collision-glb-main.cpp`
  is explicitly marked legacy and rebuilds temporary contact/sample vectors.
  Do not expand that per-tick allocation path. Prefer visualizing the actual
  solver result or a bounded, reusable contact record.

### Melee and authoritative damage

- `src/combat/melee-client.cpp` builds a physical melee claim with target ID,
  target generation, `contact_id`, contact tick, hit point, direction, blade
  base/tip, impact speed, force, and directness. It logs these in
  `melee.client.contact_claim` and batches the claim.
- `src/network/server-packet-handlers.cpp` validates the claim and creates a
  `ServerContactRequest` with attacker/target, contact ID, weapon, hit point,
  normal/knockback, and tick. `melee.contact.claim.accepted` includes attacker,
  target, contact ID, weapon/action, force, damage, and before/after health,
  but omits the validated hit point and direction.
- The shared authority in `src/network/server-contact.cpp` validates the hit
  and calls `applyActorDamage` in `src/network/server-damage.cpp`. The
  authoritative damage log includes attacker/target, damage and health change;
  it does not consistently include the melee contact/correlation ID or hit
  point. That is the first journal-join gap: client claim → server acceptance →
  damage result cannot always be followed by one stable contact ID in
  `events.jsonl`.
- Do not add another melee damage route. Extend existing events and preserve
  `ServerContactRequest` / shared damage ownership.

### Fun Counter Strike NPC objectives and navigation

- The host creates objective/team context in
  `src/network/server-gamemode.cpp`, including the chosen objective position,
  formation travel slot, objective state, carrier/defuser roles, site
  eligibility, and `canPlant` / `canDefuse`. `npc.objective-context` reports
  objective position, actor site, distance, role, and eligibility reason, but
  not the full travel slot or utility score.
- `src/npc/npc-utility.cpp` scores `MoveToObjective`, `PlantObjective`,
  `DefendSite`, `DefuseObjective`, and other goals. Before planting, the T-side
  `HuntArea` bias is zeroed. `PlantObjective` becomes eligible only when
  `canPlant` is true. Goal switching retains minimum-duration and score-margin
  hysteresis.
- `config/behavior-profiles.json` defines `fun_cs_rush` as elite with
  `objective_rush:true`, six formation slots, a 5 m formation radius, and
  `swarm:false`. `objective_rush` currently affects dash/route behavior; it is
  not a hard override of utility goal selection.
- `src/npc/npc.cpp` emits `npc.utility-decision`, `npc.goal-changed`, nav plan
  success/failure, and movement-progress failure records. The streams share
  actor/tick identity, but they do not all report the same representation of
  the target: utility decisions give the named utility goal and position;
  navigation failure gives a destination and integer goal kind; the selected
  formation slot and goal scores are not in the decision record.

## Live journal evidence inspected

Run: `20261118_020227`, FCS on `dust2cyberiav4`; server journal:
`logs/2026-11-18/20261118_020227/server-01/events.jsonl`.

- Across the inspected journal: 566 `collision.actor-world`, 1,106
  `collision.bounce.applied`, 272 `npc.nav-plan-failed`, 210
  `npc.movement-progress-failed`, 731 `npc.objective-context`, and 782
  `npc.utility-decision` events. There were 10 melee batch receives, but no
  accepted melee claim in the counted event set. This run therefore offers
  useful NPC navigation evidence, not a current human melee reproduction.
- Latest per-actor utility decisions showed team 1 actors choosing
  `MoveToObjective`; team 0 actors mostly chose `DefendSite`. Team 0 positions
  were visibly concentrated in two map areas in that sample, but the journal
  does not label the authored spawn regions, so it does not prove they were
  stuck at spawn.
- The carrier `100017` had a nav-plan-failed record with destination near
  `[-914.8, -91.3, 2351.3]`; many T-side plan failures were `blocked` or
  `initial`. This makes navigation/path execution a concrete place to inspect
  after the correct `MoveToObjective` decision, rather than evidence that the
  goal policy itself selected the wrong task.
- A separate sample from the same run previously showed carrier `100017`
  outside a bomb site and `can_plant:false` with reason `outside_bomb_site`.
  This explains ineligibility at that sample, not the entire route failure.
- The old 5–7 Hz sample came from a different run. This run later reported
  roughly 44–61 Hz with 19 NPCs in sampled windows. Do not attribute the old
  rate to DLL reload based on the available evidence.

## Proposed implementation, in order

1. **Join melee events.** Add the server-validated hit point, direction, and
   contact tick/ID to `melee.contact.claim.accepted`; include the same contact
   correlation ID and hit point in the shared `server.damage.applied` journal
   record for contact damage. Keep packet and damage behavior unchanged.
2. **Expose one world-contact trace.** Extend the existing collision owner
   record to include the actual contact point, response/surface normals,
   actor/part, triangle/entity/material where available, penetration, intended
   movement, and velocity before/after. Mark contact kind explicitly as world,
   weapon-world, or actor contact. Keep detailed records behind the existing
   Collision Trace/category gate and bounded to an active scenario or
   rate-limited sample.
3. **Draw a readable marker.** Feed the actual point plus a normal arrow and,
   where meaningful, the response/impulse delta to the existing debug
   visualization path. Keep marker lifetime and storage bounded. Avoid new
   broadphase queries, allocations in hot collision loops, and extra 60 Hz
   logging by default.
4. **Join FCS intent to movement.** At decision/plan transitions, log actor/team,
   objective phase and role, chosen utility goal and relevant score, objective
   position, formation slot/target position, nav result/reason, and measured
   progress/stuck reason under actor ID and fixed tick. Do not replace the
   utility brain or make `objective_rush` a second objective state machine.
5. **Validate on the reported behavior.** Use a human-recorded corner snag and
   a live FCS objective round, then correlate only the exact run's journal.
   Check T carrier reaches a valid site and plants, CTs rotate/intercept, and a
   CT begins defusing only after the bomb is planted. If a recorded repro is
   not available, capture one before solver or NPC-policy changes.

## Constraints and evidence still needed

- Collision, damage, and physics remain on the fixed 60 Hz tick.
- Preserve cached broadphase and avoid per-query allocation in hot loops.
- Use existing `StructuredLogger`; do not create a second collision log file.
- Do not claim a code fix from a source trace or successful DLL publication.
- Required acceptance is the exact live executable/run journal plus in-game
  observation of the requested marker and objective behavior.

## Routed references

- `docs/architecture/collision/collision.md`
- `docs/specs/movement/movement.md`
- `docs/specs/weapons/melee/melee-weapons.md`
- `docs/specs/networking/contact-authority.md`
- `docs/specs/gamemodes/funcounterstrike.md`
- `docs/specs/npc-behavior/npc-behavior.md`
- `docs/architecture/player-npc-systems/npc-movement.md`
- `docs/specs/debug-logging/debug-logging.md`
- `docs/specs/dev-loop/scenario-input-recording.md`
- `docs/workflows/runtime-scenario-validation.md`
- `docs/skills/spec-behavior-review-v1.md`
- `docs/skills/documentation-checker-v1.md`
