# HAFS — current behavior audit and specification decisions

**Status:** draft; implementation requirements need the user's review.

This document separates what the current source appears to do from what still
needs a product decision. HAFS is its own weapon (`id: hafs`); it is not
Godball, SwordSword, or Spy Knife. Do not infer its desired rules from those
weapons.

## Current source/configuration facts

- `config/weapons.json` defines HAFS with `behavior_type: hafs`, `pose_id: hafs`,
  and the model `assets/objects/weapons/mimita-hafs-v1.glb`.
- `src/combat/weapon-hafs.cpp` implements local slash/lunge state and sweeps
  blade sample spheres against local NPC physical-body parts. On a local hit it
  calls `npc.body.takeDamage()` and applies knockback/audio.
- `src/network/network-weapons.cpp` maps HAFS to a network weapon ID. That
  mapping alone does not establish that its attacks reach the server.
- `src/network/server-physical-contact.cpp` can construct a HAFS capsule, but
  it requires the attacker's server-side `physicalAttackTimeline` to be
  committed and active. The construction function alone does not prove that a
  HAFS request commits that timeline.
- The local HAFS NPC hit loop and the generic authoritative server contact
  path are separate code paths. The multiplayer attack-start/request path and
  authoritative player/NPC outcomes must be traced and proven before claiming
  HAFS works online.

Focused trace found a specific request-path gap in the current source:
`WeaponSystem::fire()` starts a HAFS slash but returns an empty result, while
the engine input sender only sends the generic request for a fired result.
For right-click, `WeaponSystem::fireAlt()` returns `fired`, but
`src/engine/engine-tick-combat.cpp` sends the attack-start request only when
the equipped weapon is SwordSword. Thus neither observed HAFS input path
starts the server-side timeline through that sender today. The server can
build a HAFS contact capsule after a committed timeline, but this path does not
appear to commit it for HAFS. This is a source-level cause candidate for
multiplayer attacks not applying; confirm with live request/timeline logs
before calling it the runtime root cause.

The current config reuses the HAFS model and pose ID for SwordSword. That is a
known configuration fact, not a decision that SwordSword should share HAFS's
model or animations. SwordSword's own asset contract remains in
[`swordsword.md`](swordsword.md).

## Decisions needed before implementation

Please review these as requirements rather than accepting current behavior by
default:

1. Are slash and lunge separate moves, and what input starts each one?
2. Which targets may HAFS damage: NPCs, players, teammates, owner?
3. Is damage continuous while touching, once per target per attack, or on a
   configured cadence? What should differ between slash and lunge?
4. Which authority owns hit detection and damage in multiplayer? The intended
   network contract should be client prediction followed by server validation
   and confirmation, with no client-owned health changes.
5. Which world, player, NPC, and weapon-on-weapon collision rules apply?
6. Which model, animation clips, sounds, and hit effects are HAFS-specific?
7. Which config values are approved, and what is the expected behavior at
   maximum swing speed and against long/irregular avatar limbs?

## Required implementation proof after decisions are approved

Use the real dev-loop executable and runtime journal to exercise slash and
lunge against NPCs and a second player on an authoritative server. Include
misses, repeated contact, disallowed PvP, collision with world geometry, and
spawn/respawn. Record client prediction, attack request, server acceptance or
rejection, authoritative health/knockback, confirmation, and visible outcome.
Keep build evidence separate from runtime and human feel/animation acceptance.
