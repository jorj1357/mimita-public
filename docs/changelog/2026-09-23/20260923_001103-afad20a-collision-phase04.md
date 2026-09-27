# afad20a collision restoration — Phase 4 (per-limb physical body)

Date: 2026-09-23
Status: hot-code and cold-build succeeded; deterministic tests pass; human live
acceptance pending

Related specification: `docs/specs/movement/movement.md`,
`docs/architecture/collision/collision.md`

Related regression: `docs/regressions/2026-09-22/limb-contact-ability-reset-REG.md`
(remains UNRESOLVED; this work is a partial step, not a resolution)

Predecessor: `docs/changelog/2026-09-22/20260922_235824-afad20a-movement-phase01.md`

Reference commit: `afad20a` ("npc stuff its cool", 2026-09-11).

## Scope of this session

Phase 4 of the approved plan: restore one collision owner that uses the
`afad20a` per-limb collision source (`Player::physicalBody.parts`) behind the
hot `collision.main` interface, so collision follows the visible animated body
rather than guessed socket proxies. Local player path; NPC/server actors keep
the socket fallback. Animation restoration and the shared C++/JSON selector
remain deferred.

## Work performed

New body-parts capability (cold boundary, plain data):

- Added `GAME_CAP_BODY_PARTS` (`body.parts`) plus `GameBodyPartV1` /
  `GameBodyPartsV1` / `GameBodyPartsFn` to `src/hot-reload/game-api.h`.
- Implemented `capBodyParts` in `src/live-code/live-behavior.cpp`: for the local
  typed Player it returns each `Player::physicalBody.parts` entry's
  ROOT-RELATIVE world transform, its previous root-relative position, and the
  part-local collider AABB (`Collider::localMin/localMax`). The root is removed
  so hot policy composes its own current root, which is ahead of the cold
  Player during the hot movement step. Registered next to `socket.raw` /
  `mesh.part-bounds`.

Hot collision owner now consumes the physical body:

- `src/hot-reload/modules/movement-system.cpp` `buildPlayerCollision` now
  prefers `body.parts`: one sphere per part, centre = part world transform,
  radius = `max(local half-extents, 0.15)` clamped to `0.35` (the `afad20a`
  `computeBodyPartCenter` rule), flagged `COLLISION_COLLIDER_BODY_AUTHORITATIVE`.
  It sets the collider's sweep to `worldCenter - previousWorldCenter`. The
  socket + mesh-bounds path is kept as the fallback for remote/NPC actors or an
  unloaded model.
- The pose is published to the physical body before the read:
  `applyStoredPoseBeforeCollision` (`skeleton.apply` ->
  `Player::updateModelWorldTransforms`) already runs at the top of
  `resolveCollisions`, so `physicalBody.parts` match the visible pose.

Per-limb sweep and universal reset in the collision package:

- `src/hot-reload/packages/collision/collision-abi.h`: documented
  `CollisionColliderV1::velocity` as the world-space per-tick collider
  displacement (current centre minus previous centre) and added
  `CollisionContactV1::resetsAbilities`.
- `collision-package-solver.cpp`: a sphere collider with a non-zero sweep is
  cast as a capsule from its previous centre to its current centre, so a fast
  arm/leg/weapon cannot tunnel through thin geometry between ticks. Every
  returned contact is marked `resetsAbilities = 1` (afad20a universal reset).
- `movement-system.cpp` and `actor-movement-system.cpp`: the reset signal is now
  `worldContact || bodyContact`, so a limb/weapon/body contact restores
  abilities even when the actor is not grounded.

Deterministic tests added to the package candidate self-test
(`collision-package-selftest.cpp`):

- Test 12: a body-authoritative limb sphere penetrating a wall pushes the ROOT
  out and the contact is marked `resetsAbilities`.
- Test 13: the same rule for a weapon/tool collider.
- Test 14: a limb whose current centre is clear but whose per-tick sweep crossed
  the wall is still detected (movement-through-surface).

## Evidence

Source evidence:

- `afad20a` producer reference:
  `git show afad20a:src/physics/movement/physics-collision-body.cpp`
  (`computeBodyPartCenter`, `collectBodyWeaponSpheres` with
  `worldTransform`/`previousWorldTransform`).
- Body source: `src/entities/player.h` `PhysicalBodyPart`,
  `Player::updateModelWorldTransforms` (`src/entities/player.cpp`).

Build evidence (separate from runtime evidence):

- Hot DLL: `python build_game_dll.py` -> `DLL build success`
  (`build/mimita-game.dll`).
- Cold build: `python build_agent.py` -> `Status: SUCCESS`,
  `mimita-20260922T200604.exe`. No running executable was overwritten.

Test evidence (deterministic, headless):

- `--live-code-selftest`: PASS, including the DLL candidate self-test which runs
  the collision package self-test with the new limb/weapon/sweep cases.
- `--collision-selftest`: PASS.
- `--afad20a-reference-selftest`, `--afad20a-parity-selftest`,
  `--movement-selftest`, `--movement-parity-selftest`: PASS.

Runtime evidence:

- Not performed. No running game session was driven and no in-game 60 Hz trace
  was captured in this session.

Human acceptance:

- Pending. Arms/legs/weapons not entering walls, limb/tool/world contact
  resetting abilities, and the `afad20a` feel still require a human playtest
  before the regression can be marked resolved.

## Limits

- The local player now uses `Player::physicalBody.parts`. Remote/NPC/server
  actors still use the socket + mesh-bounds fallback because there is no generic
  EntityId -> typed-body lookup at the cold boundary. Converging them is
  remaining work.
- The per-limb sweep is a 3-sample capsule approximation of the swept sphere.
  It catches ordinary fast limb motion but is not a continuous TOI sweep; very
  thin geometry crossed between the three samples can still be missed.
- Weapon/tool colliders were already body-authoritative from the hot attachment
  pose; this session did not change their shape source
  (`config/weaponcollisions.json`).
- Ability reset still does not restore freeze *availability* (no
  `freezeAvailable` field in the runtime-state envelope); only the freeze timer
  resets. That state migration is deferred.
- Server authority, prediction, replay, animation restoration, and the shared
  C++/JSON `behaviorSource` selector remain unaddressed.

## Pre-existing changes preserved (not part of this session)

Unrelated working-tree edits left untouched: `config/collision.json` behavior
keys, `collision-policy.cpp`, and the untracked
`docs/regressions/2026-09-22/limb-contact-ability-reset-REG.md` and
`docs/changelog/2026-09-22/20260922_230452-limb-contact-movement-audit.md`.
