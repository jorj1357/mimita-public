# Projectile Rifle Audio and Impact Terminal

Date: 2026-09-26

## Scope

Updated the existing projectile rifle path without creating a parallel projectile
or effect system.

## Changes

- Added reusable audio-event start/end offsets and retrigger behavior to the
  existing miniaudio voice owner.
- Added weapon-configurable rifle fire pitch, start/end seconds, and retrigger
  settings. The rifle uses pitch `0.5`, plays `0.0` through `0.1` seconds, and
  restarts its existing voice on each automatic shot.
- Increased the rifle projectile speed from `180` to `900` in
  `config/weapons.json`.
- Set the rifle's shared projectile policy to `maxBounceCount: 0`, so its first
  valid world collision becomes the existing terminal `WorldImpact` result.
- Carried the existing projectile collision normal through the authoritative
  terminal packet and into the existing staged rifle impact effect. The effect
  is offset outward by `0.01` meters to avoid embedding in the surface.

## Ownership

- Projectile movement/collision remains owned by
  `src/combat/projectile-simulation.cpp`.
- Server terminal authority remains in `src/network/server-projectiles.cpp`.
- Client reconciliation remains in `src/network/multiplayer-projectiles.cpp`.
- Rifle impact presentation remains in `src/combat/explosion-fx.cpp`.
- Audio voice creation and lifecycle remain in `src/audio/audio.cpp`.

## Validation

- `config/weapons.json` parsed successfully.
- `git diff --check` passed; only existing line-ending conversion warnings were
  reported.
- `python build_agent.py` compiled the touched translation units successfully.
- Final link was blocked by pre-existing unrelated missing server symbols,
  including `broadcastServerChatMessage`, `handleChatMessage`, and
  `handleClientTimeout`.
- Runtime shot/audio/impact acceptance was not performed in this session.

## Focused review

Applied `docs/skills/spec-behavior-review-v1.md`: the implementation reuses the
existing fixed-tick projectile collision result, shared terminal packet, staged
effect spawner, and audio manager. Runtime and human visual acceptance remain
required.
