# Universal actor collision evidence investigation

Date: 2026-10-08
Status: INVESTIGATION_COMPLETE / CODE_UNCHANGED / IMPLEMENTATION_AND_RUNTIME_EVIDENCE_INCOMPLETE

## Question

Assess whether the current repository has enough evidence and diagnostics to
prove or define authoritative collision between every actor, including body
triangle contact, standing on another actor, support-velocity inheritance, and
balanced/cancellable opposing movement.

## Findings

- The active local triangle solver is an opt-in player path. It handles the
  local player's body/weapon meshes against static world triangles and selected
  physical entities. NPCs remain on the legacy collision path.
- The existing physical-entity support path proves the contact vocabulary for a
  moving crate-like entity, including support identity, surface velocity, carry,
  and velocity inheritance. It is not a general actor-to-actor path.
- `resolveCapsuleVsCapsule` exists and is used by replay/simulation for player
  versus NPC separation, but it is root-capsule geometry, not authoritative
  body-triangle contact, and it is not the dedicated-server authority path.
- Dedicated-server `resolvePlayerCollision` is currently an intentional no-op
  because active players use validated client-transform authority. The server
  therefore does not currently authoritatively depenetrate or impulse players
  against one another.
- Server body geometry currently has a separate headless representation used for
  hit validation; the repository's collision inventory records server body
  parts as AABBs and explicitly lists NPC/server triangle wiring as unfinished.
- Existing collision trace events can prove local actor/world contact stages,
  but no current event chain proves actor A versus actor B: candidate pair,
  shape pair, authoritative ordering, contact normal/penetration, support
  identity, impulse split, post-solve velocities/positions, and client
  reconciliation.

## Evidence conclusion

The repository has enough source evidence to define the missing work and choose
owners, but not enough runtime evidence to claim the requested universal actor
behavior works. More bounded owner-level StructuredLogger diagnostics and a real
fixed-60-Hz scenario are required before implementation or acceptance claims.

## Required proof scenario

Use one authoritative server with a player, NPC, and (if supported) a second
player. Exercise: head-on approach, stationary body block, actor standing on
actor, moving lower actor with supported upper actor, opposing equal movement,
and a small swarm. Record actor IDs/lifecycles, shapes, candidate pairs,
before/after transforms and velocities, contact normal/depth/point, support and
surface velocity, impulse/response policy, stable ordering, tick, and the first
divergence between expected and actual state. Keep per-pair logs sampled and
rate-limited; do not log every triangle every tick by default.

## Not performed

- No source or gameplay code was changed.
- No build, executable run, live `events.jsonl` capture, multiplayer run, or
  human gameplay acceptance was performed in this investigation.
- No regression was recorded because this is an architecture/evidence gap, not
  a confirmed newly introduced break.
