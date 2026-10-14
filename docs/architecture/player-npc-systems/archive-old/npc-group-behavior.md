# MiMITA NPC group behavior contract

Date: 2026-10-06
Status: Draft v1; implementation contract for the general team-focus and squad
movement layer used by mode NPCs.
Audience: MiMITA contributors, AI coding agents, reviewers, and the project owner
Related architecture: `docs/architecture/player-npc-systems/player-npc-systems.md`
Related movement/navigation:
`docs/architecture/player-npc-systems/npc-movement.md`,
`docs/architecture/player-npc-systems/npc-navigation-implementation.md`,
`docs/architecture/player-npc-systems/npc-local-avoidance.md`
Related mode spec: `docs/specs/gamemodes/gamemodes.md`,
`docs/specs/gamemodes/juggernaut.md`

## 1. Purpose

This document defines the one general owner for NPC **team focus** and **group
movement**: how a squad decides what it is attacking, where it moves as a unit,
and how individual members spread so they swarm rather than stack or wall-grind.

It exists so that a mode such as Juggernaut can say, through configuration only,
"the Fighters move together to the Juggernauts and attack", without a
Juggernaut-specific AI system.

It does not describe any specific mode. It describes reusable behavior that the
Counter-Strike, Juggernaut, NPC Waves, and future team/PvE modes compose.

## 2. Core rule

Group behavior is a **suggestion producer**, exactly like the navigator and the
combat brain.

It may produce for each actor:

- a preferred focus actor or focus area;
- a squad anchor position;
- a preferred approach direction / slot offset;
- a cohesion weight and a spacing preference;
- a request to rally, hold, spread, or advance.

It must never:

- set actor position, velocity, gravity, grounded state, jump, dash, or
  collision result;
- replace the navigator's route;
- replace the `MovementIntentArbiter`;
- add a second combat or target-selection owner;
- teleport or interpolate actors.

Final movement is always resolved by the existing `MovementIntentArbiter` into
`ActorIntent`, then executed by shared MiMITA physics.

## 3. One owner per concept

- **TeamFocus** owns "which hostile actors or area the team is currently
  prioritizing". One focus set per team.
- **SquadCoordinator** owns "where the squad is moving together and how members
  are distributed". One coordinator per team.
- **Actor slot assignment** owns "which sub-position of the group a single actor
  occupies". Owned by the coordinator, consumed by the actor brain.
- **Local avoidance** owns "a preferred-velocity suggestion so members do not
  occupy the same space". See `npc-local-avoidance.md`. It is a separate,
  optional owner; it does not own group membership.
- **Physical actor collision** remains owned by MiMITA physics. Avoidance must
  not erase legitimate contact.

TeamFocus and SquadCoordinator are components of the existing `TeamBrain`
(`src/npc/team-brain.*`) or a sibling owner created by it. They must not create a
parallel strategy brain.

## 4. TeamFocus

### 4.1 Inputs

- the team's living members (id, team, position, health);
- valid hostile actors (id, team, position, health, last-known position);
- the mode's intent (objective present or not);
- the active behavior profile of the members.

### 4.2 Focus set

A focus set is a small, ordered list of hostile actors the team should converge
on. For a two-team elimination mode the focus set is the hostile team. For a
mode with a declared priority actor type, the focus set is those actors first,
then any other hostile as fallback.

Focus selection must be deterministic for a given state (stable ordering by a
documented score, then by actor id).

Suggested focus score inputs:

- hostile team membership;
- distance to the squad anchor;
- whether that hostile is currently engaged by teammates;
- health / threat;
- visibility confidence.

### 4.3 Focus decay

Focus decays with time since the focus actor was last confirmed. A stale focus
becomes a **last-known focus area** (a position), not a tracked transform, so
NPCs do not behave as if they have line of sight they do not have.

Focus must never grant hidden omniscience: a focus actor is a movement/travel
target, not an aim target. Aiming still requires the perception owner to report a
visible target.

## 5. SquadCoordinator

### 5.1 Anchor

The squad owns one anchor point: the position the unit is trying to occupy as a
group (for example, a point on the approach to the focus, offset from the focus
by the preferred range).

The anchor advances toward the focus when the squad is in "advance" and stops
when the squad is in "hold", "engage", or "rally".

### 5.2 Slots

Each member receives a **slot**: a bounded offset from the anchor (or from the
focus) on a spread ring/arc. Slot assignment is deterministic and stable within
an engagement so members do not swap places every tick.

Slot properties:

- slot index and count;
- radial distance from anchor;
- angular position, distributed so members approach from different sides;
- assignment stickiness (an actor keeps its slot unless it is full, dead, or the
  focus moves beyond a threshold).

### 5.3 Modes

- **Rally**: members converge to the anchor before advancing.
- **Advance**: anchor moves toward the focus; members move to slots; cohesion
  keeps the unit together.
- **Engage/Swarm**: anchor holds near the focus; members take firing slots around
  the focus and fill up to the configured attacker limit.
- **Hold**: anchor holds; members hold slots.
- **Regroup**: after losses, re-rally and re-slot.

Mode transitions are event-driven and logged; the coordinator must not oscillate
between modes in a single engagement window.

### 5.4 Cohesion and spacing

Cohesion pulls members toward the anchor; spacing (including local avoidance)
pushes members apart so they do not stack. Both are suggestions with explicit
weights, resolved by the arbiter.

Cohesion must be weaker than a genuine visible-combat need: an actor under fire
or in a required traversal is not forced back to formation.

## 6. Configuration

Group behavior is configured in **behavior profiles**
(`config/behavior-profiles.json`), because it is per-actor behavior tuning that a
role already references. The mode chooses which role/preset each team uses; the
behavior profile supplies how that actor participates in a group.

Proposed fields (all optional; missing = group behavior off, preserving today's
per-actor behavior):

```
focus_enabled                 bool    // participate in TeamFocus
focus_mode                    string  // "hostile_team" (default) | "priority_then_hostile"
swarm                         bool    // take firing slots around the focus
cohesion                      float   // pull toward squad anchor, 0..1
spread_radius_meters          float   // distance between slots
approach_style                string  // "direct" | "arc" | "flank"
max_attackers_per_target      int     // cap on members committed to one focus actor
rally_distance_meters         float   // regroup threshold
focus_reacquire_seconds       float   // decay before focus becomes area-only
slot_stickiness               float   // resistance to changing slot
```

Rules:

- A field must have one owner. Focus/cohesion tuning does not belong in the
  gamemode JSON, and mode identity does not belong in the behavior profile.
- Group behavior defaults to off so existing profiles and modes are unchanged.
- Values are hot-reloadable with the rest of the behavior profile.

## 7. Interaction with navigation

The coordinator produces a **destination** or a slot point, not a path. The
navigator (`NpcNavigator` + the Recast/Detour backend) computes the route to that
point using the general external-navigation owner. Group behavior must not
compute its own route.

When the focus is a moving actor and the route goes stale, the coordinator
requests a re-slot/re-target event and the navigator replans (see
`npc-navigation-implementation.md` corridor-retention and replan rules).

## 8. Interaction with local avoidance

Local avoidance is a separate owner (`npc-local-avoidance.md`) that turns
neighbor proximity into a preferred-velocity suggestion. The SquadCoordinator
supplies slots and cohesion; avoidance supplies the spacing correction inside a
slot. Neither may create invisible collision bubbles large enough to block a
doorway or prevent reaching a target.

## 9. Interaction with combat

- A visible hostile is still owned by the combat/target owner. Group behavior
  does not select the aim target.
- Combat movement can temporarily outrank group movement through the arbiter.
- When combat movement ends, the actor resumes its slot / resumes advancing.
- A focus actor is shared knowledge for movement and travel; it is not an aim
  permission.

## 10. Diagnostics

Use the canonical `StructuredLogger` / `events.jsonl`. Bounded, change-edge
events:

- `npc.focus-changed` (team, focus set ids, reason, confidence);
- `npc.squad-mode-changed` (team, mode, anchor, reason);
- `npc.squad-slot-assigned` (actor, slot index/count, anchor, focus);
- `npc.squad-rally` (team, anchor, living count);
- `npc.group-cohesion` (optional sampled actor, anchor distance, slot distance).

Required fields: team, actor id(s), tick, positions, mode/reason, profile id.
No per-tick spam by default.

## 11. Acceptance

- A squad with `focus_enabled` and `swarm` moves toward the focus as a unit and
  arrives distributed, not stacked.
- Members take different approach angles and do not all share one line.
- No member wall-grinds while advancing to the focus.
- After losing a member, the squad re-rallies and continues.
- Focus decay prevents aiming or persistent tracking through walls.
- With group behavior disabled, behavior is identical to the per-actor baseline.
- Dedicated maps (first: `dust2cyberiav4`) show the effect in real
  `events.jsonl`.

## 12. Non-goals

- No mode-specific AI system.
- No new movement executor, no direct actor integration.
- No aim or fire authority.
- No route planning (that is the navigator).
- No physical collision ownership (that is MiMITA physics).
