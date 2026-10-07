# MiMITA NPC local avoidance contract

Date: 2026-10-06
Status: Draft v1; implementation contract for optional local-avoidance providers
used by NPC group behavior.
Audience: MiMITA contributors, AI coding agents, reviewers, and the project owner
Related architecture:
`docs/architecture/player-npc-systems/npc-navigation-implementation.md`,
`docs/architecture/player-npc-systems/npc-group-behavior.md`
Library inventory: `external/npc-navigation-sources.md`
Movement owner: `docs/architecture/player-npc-systems/npc-movement.md`

## 1. Purpose

Local avoidance keeps nearby actors from occupying the same space while they
follow routes and group slots. This document defines its ownership, inputs,
outputs, and limits so that RVO2 and DetourCrowd can be used without becoming a
second movement or physics owner.

## 2. Core rule

Local avoidance produces a **preferred-velocity suggestion only**. It never sets
final position, final velocity, grounded state, jump, dash, or collision result.

```
route / group slot
  -> desired velocity
  -> optional avoidance suggestion (RVO2 | DetourCrowd | none)
  -> combat / traversal constraints
  -> MovementIntentArbiter
  -> ActorIntent
  -> MiMITA shared physics and actor collision
```

Avoidance is not global navigation and is not physical collision.

## 3. Providers

Both external providers are permitted, and both are optional:

- **DetourCrowd** — native to the vendored Recast/Detour tree
  (`external/recastnavigation/DetourCrowd`), receives the Detour corridor and
  returns a preferred velocity.
- **RVO2 / ORCA** — vendored at `external/rvo2`, receives neighbor positions,
  velocities, and a preferred velocity and returns an ORCA-corrected velocity.

A MiMITA-owned adapter hides both behind one interface. The active provider is a
general configuration value (`none`, `rvo2`, `detourcrowd`), never inferred from
gamemode.

MiMITA features are also first-class participants:

- physical actor-vs-actor collision remains authoritative and is never disabled
  by avoidance;
- MiMITA movement constraints (slope, step, dash/freeze/air rules) still apply to
  the resolved intent;
- avoidance must respect the same fixed 60 Hz simulation boundary.

## 4. Interface

Conceptual contract (spelling may change; semantics may not):

```
AvoidanceInput:
  actor id, team, radius
  current position, current velocity
  desired velocity
  neighbor list (id, team, position, velocity, radius)
  bounds (soft)

AvoidanceSuggestion:
  preferred velocity
  provider name
  valid flag (false = no suggestion; caller keeps desired velocity)
```

An invalid or unavailable provider must degrade to the caller's desired
velocity, never to zero and never to a stall.

## 5. Physics and collision coexistence

- Avoidance may reduce unnecessary contact; it must not erase legitimate contact.
- Avoidance must not create invisible actor bubbles large enough that actors
  cannot approach targets, doorways, or combat positions.
- The maximum avoidance influence and effective radius are configuration-owned
  and bounded relative to the actor radius.
- Physical collision resolution in `src/physics/movement/*` remains the final
  authority on overlap.

## 6. Determinism and budgets

- Neighbor gathering uses stable actor-id ordering.
- Avoidance runs at the fixed simulation rate, not per render frame.
- Work is bounded per actor: a maximum neighbor count and a maximum processing
  cost per tick.
- If the per-tick budget is exceeded, remaining actors fall back to their desired
  velocity for that tick and a bounded diagnostic is emitted.
- The external library is not assumed deterministic; MiMITA verifies the
  behavior it depends on.

## 7. Dynamic worlds

- DetourTileCache owns bounded dynamic navmesh tile updates.
- Avoidance uses the current published navmesh/corridor version.
- Avoidance must reject or ignore suggestions based on a stale corridor after a
  navmesh version change (the navigator replans instead).

## 8. Diagnostics

Canonical `StructuredLogger` / `events.jsonl`. Bounded events:

- `npc.avoidance-provider-selected` (provider, reason);
- `npc.avoidance-budget-exceeded` (tick, actor count, budget);
- `npc.avoidance-degraded` (reason, actor, fallback);
- optional sampled `npc.avoidance` (actor, desired vs suggested velocity).

## 9. Acceptance

Measured against a no-avoidance baseline, on the first map `dust2cyberiav4`:

- doorway throughput is not worse than baseline;
- actor collision frequency does not increase;
- no permanent deadlock or livelock is introduced;
- net progress and group arrival are not worse than baseline;
- CPU cost is within the configured tick budget;
- the provider is retained only if it measurably improves behavior.

## 10. Non-goals

- Avoidance does not plan routes.
- Avoidance does not own actor integration.
- Avoidance does not replace physical collision.
- Avoidance does not add a second movement executor.
