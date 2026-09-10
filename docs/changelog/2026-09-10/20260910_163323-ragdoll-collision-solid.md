# Ragdoll collision robustness (Phase A) + gold thought-process record

- Task ID: ragdoll-collision-solid
- Summary: Make ragdoll world collision solid by resolving existing overlap before
  motion and after constraints, and by substepping swept motion. Record the
  ragdoll thought process as a gold document.
- Status: PASS_WITH_HUMAN_REVIEW
- Date, time, timezone: `2026-09-10T16:33:23Z` (2026-09-10 12:33:23 EDT)
- Branch: `8292026stash`
- Base commit: working tree; no commit created
- Final commit: none (uncommitted)

## Pre-existing changes

- Work not created by this session (see `git status --porcelain`): the FFA/kill
  event and countdown changes in `config/analytics.json`,
  `config/gamemodes/ffa.json`, `config/gamemodes/tdm.json`,
  `src/engine/engine-tick-ui-overlays.cpp`, `src/gamemode/gamemode.h`,
  `src/network/community-match-client.cpp/.h`,
  `src/network/server-gamemode.cpp/.h`, and the other files under
  `docs/changelog/2026-09-10/`. Not claimed here.
- The prior ragdoll core work in this same session is recorded separately in
  `docs/changelog/2026-09-10/20260910_155338-ragdoll-mode-rigid-body-core.md`.

## Requested behavior

- The player must never phase through the floor, walls, or ordinary geometry
  while ragdolled; collision in ragdoll must be as solid as normal movement.
- Fix root candidates: bodies that start the tick already penetrating, single-
  triangle resolution, and fast parts tunneling.
- Also record the working ragdoll thought process in `docs/gold/`.

## Specification alignment

- `docs/specs/ragdoll-retrograd/ragdoll-retrograd.md` RAG-011: no ragdoll body
  part may freely tunnel through solid world geometry.
- `docs/specs/moving-physical-objects/moving-physical-objects.md`: reuse the
  generalized collision primitives rather than a second collision engine.
- `docs/architecture/collision/collision.md`: fixed 60 Hz domain, use the cached
  broadphase and the shared recovery/batched-correction helpers.
- `docs/skills/spec-behavior-review-v1.md`: PASS_WITH_HUMAN_REVIEW.

## Exact implementation changes

- `src/physics/physical-body.cpp`:
  - Added `resolveContactVelocity(body, point, normal)`: shared contact impulse
    with restitution and Coulomb friction, applied at the contact point so
    angular response is included.
  - Added `depenetrateStatic(body, world, passes)`: gathers triangles for the
    capsule's current AABB, collects contacts with the existing
    `collectCapsuleRecoveryContacts`, applies `solveBatchedCorrection`, and
    resolves contact velocities. Repeats a bounded number of passes. This is the
    missing step that handled bodies starting a tick already penetrating, which
    the swept phase cannot detect.
  - Rewrote `collideWithWorld`:
    1. static depenetration up front (3 passes);
    2. swept substepped motion, substep cap raised 8 -> 16 and substep size based
       on `capsuleRadius * 0.4` (min 0.05), resolving the earliest triangle and
       then static depenetration (2 passes) at the impact;
    3. a final static depenetration pass so constraints or the sweep cannot leave
       overlap.
- Added `physics/movement/physics-collision-shared.h` include to reuse the
  recovery collector (no new collision engine, no per-query allocation of a
  returned vector beyond the existing helper).

## Diagnostics

- No new logs. Reused existing `StructuredCategory::Ragdoll` tick events. Contact
  resolution stays low-volume.

## Validation

- Build: `python build_agent.py` -> `Status: SUCCESS`, return code 0, duration
  6.77s; `mimita.exe` relinked 2026-09-10 12:33. `physical-body.cpp` recompiled
  and linked. No compiler errors or warnings.
- Runtime: not performed this session. No collision, visual, or multiplayer
  acceptance claimed.

## Regression review

- No append-only entry added. This continues uncommitted ragdoll feature work;
  no previously-working behavior was broken.

## Human acceptance

- Gameplay review required: enter ragdoll, run/jump into blocks, fall from
  height, and confirm no part phases through the floor or walls.
- Gameplay review required: confirm grabs stay solid while collisions remain
  solid.
- Still unverified: all runtime behavior.

## Gold record

- Added `docs/gold/2026-09-10-ragdoll-rigid-body-thought-process.md`, which
  records the spec sections that helped, what could be clearer, and the human's
  model-choice hypothesis (`mimo v2.5` vs `deepseek v4.1 flash`).
