# Unified hot collision kernel (hot-side increment)

- EST timestamp: 2026-09-17 11:25:00 -04:00
- Branch: `8292026stash`
- Result: `PASS_WITH_HUMAN_REVIEW`

## Goal

Begin the unified hot collision kernel: one versioned interface and one solve
that owns the movement/smoothing capsule plus the head, torso, arms and legs,
with merged contact response and hot per-part impact events. This session
implements the hot-side increment only; weapons, projectiles, NPCs, cold-path
removal, and multiplayer authority remain for later phases.

## Changes

- `src/hot-reload/hot-collision-kernel.h` (new)
  - Versioned interface: `HotColliderV1`, `HotContactV1`, `HotImpactEvent`,
    `HotActorCollisionV1`, shape/policy/part enums.
  - Centralised `constexpr` response policy (`kBounceEnabled`, `kBounceStrength`,
    `kBounceFriction`, `kBounceMinSpeed`, `kBounceMaxSpeed`, `kBounceCooldown`)
    and separate `HotCollisionPolicyId` object policies.
  - `hotSolveActor`: swept substeps, contact gathering and merging, depenetration
    (capsule fully authoritative, limb push clamped), bounce/friction/slide,
    ground classification, one bounce cooldown per object, final position and
    velocity, contact output, and one `HotImpactEvent` per part per tick at the
    contact point with size `1.0` and lifetime one tick.
  - `spawnImpactSphere` places that sphere through the existing generic
    `effect.part` primitive (`replayType "impact_tick"`), so it renders as a
    filled sphere for one tick.
  - `runCollisionKernelSelfTest`: deterministic policy, merge, and impact-shape
    invariants for candidate validation.
- `src/hot-reload/hot-movement-collision.h`
  - Reduced to the world-geometry cache plus `gatherSphereContacts` narrowphase;
    removed the superseded capsule-only `hotMoveCapsule`/`resolveSphere`.
- `src/hot-reload/modules/movement-system.cpp`
  - Local player collision now calls the unified kernel with the actor entity and
    tick, solving body-part colliders and spawning impact visuals.
  - Gravity is skipped while grounded so the collision kernel does not receive a
    downward speed every tick and micro-bounce forever.
- `src/hot-reload/modules/collision-policy.cpp`
  - Reads bounce values from the kernel constants instead of a second copy
    (single owner).
- `src/effects/effect-part.cpp`
  - Candidate self-test also runs the collision-kernel invariants under
    `MIMITA_GAME_DLL`; the EXE branch is unchanged.
- `src/hot-reload/hot-modules.json`
  - Added `hot-collision-kernel.h` to the hashed headers.

## Evidence

- Hot build: `python devscripts/live-build.py` succeeded and produced
  `build/hotreload/mimita-live-g000027.dll`; it never writes `MiMITA.exe`.
- Headless load/validation: `mimita-20260917T110010.exe --live-code-selftest`
  reported `[LIVE CODE SELFTEST] PASS` with `[ok] GameAPI load + ABI + self-test`;
  the loaded package now includes the collision-kernel invariants.
- `git diff --check` passed (exit 0).
- No `.exe` build was required: no `hot-modules.json` `cold` source changed, and
  no running process was killed, restarted, or replaced.

## Specification and behavior review

Skill: `docs/skills/spec-behavior-review-v1.md`.

### Finding 1

- Severity: medium
- Type: spec-code disagreement (interface location)
- Specification: `docs/architecture/live-development/hot-kernel.md`
- Exact quoted requirement: "Create a versioned plain-data collision interface in
  the existing hot game API".
- Code path: `src/hot-reload/hot-collision-kernel.h` (new hot header) instead of
  `src/hot-reload/game-api.h`.
- Actual behavior: the interface and solve live in a hot, hash-tracked header.
- Expected behavior: the request names the hot game API; `game-api.h` is the
  kernel/hot ABI whose edits require a cold build.
- Evidence: the kernel is called only from hot code and needs no new EXE call
  site, so a hot header keeps it live-editable and avoids a cold window.
- Recommended wording or implementation action: keep it hot; move the structs to
  `game-api.h` only if/when EXE code must produce collider inputs or consume
  results.
- Human decision required: yes, confirm the placement.

### Finding 2

- Severity: low
- Type: unclear wording
- Specification: this task.
- Exact quoted requirement: `kBounceMinSpeed = 0.0f` and "The capsule will not
  cancel a limb bounce."
- Code path: `hot-collision-kernel.h::applyVelocityResponse` and
  `movement-system.cpp` gravity gate.
- Actual behavior: any contact with incoming speed bounces, so walking into a
  wall at full speed now bounces the actor away, and grounded gravity is skipped
  to avoid perpetual micro-bounce.
- Expected behavior: the request wants strong bounce on all listed contacts, but
  does not state how ordinary walking into a wall should read.
- Evidence: mechanism implemented as specified; live tuning is the intended
  control.
- Recommended wording or implementation action: raise `kBounceMinSpeed` or lower
  `kBounceStrength` live if ordinary wall contact should slide instead.
- Human decision required: yes, during live tuning.

## Documents and focused reviews

- `AGENTS.md`
- `docs/ROUTER.md`
- `docs/architecture/collision/collision.md`
- `docs/architecture/live-development/hot-kernel.md`
- `docs/specs/movement/movement.md`
- `docs/features/live-code-development/live-code-development.md`
- `docs/skills/spec-behavior-review-v1.md`

## Pre-existing unrelated work preserved

- `config/accounts/default.json`, `config/analytics.json`
- `src/config/collision-config.*`, `src/physics/movement/physics-collision-shared.h`,
  `src/engine/engine-tick-setup.cpp` (prior live-collision-bounce work)
- `src/hot-reload/hot-animation-clips.h`,
  `src/hot-reload/modules/presentation/animation-policy.cpp`,
  `src/render/presentation-entities.cpp` (prior animation work)
- Existing `docs/changelog/2026-09-17/` files

## Human review still required

- Runtime acceptance from the task: walk a limb into a wall, dash the capsule and
  limbs into a wall, fall onto the floor, and hit a corner; confirm strong bounce
  and a size-1 impact sphere at the contact point for one tick.
- Change `kBounceStrength` live and repeat to confirm behavior changes while the
  same process, world, EntityIds, players, chat, and network session stay alive.
- Confirm the local actor exposes a skeleton pose so body-part colliders resolve;
  if `socket.raw` returns no parts the kernel safely falls back to capsule-only.
- Multiplayer acceptance is out of scope this session: the server still uses the
  separate capsule solve, so the kernel is not yet active on client and
  authoritative server together.

## Not done (later phases)

- Weapon, projectile, and NPC colliders in the shared kernel.
- Projectile-specific policies and explosion-on-impact policy wiring.
- Removing cold post-solve velocity clearing/projection and the line-shaped
  `bodyContactSpark` authoritative path.
- Multiplayer generation agreement for the kernel.
