# Griever spec slices 1-3 implemented and validated live

Time created: 2026-10-15T20:20:00Z
Time last updated: 2026-10-15T20:20:00Z

## Scope

Implement slices 1-3 of `docs/specs/griever-monster/griever-monster.md` and
validate them with the real executable and the live shared journal per
`docs/specs/debug-logging/AI-fix-live.md` and
`docs/workflows/runtime-scenario-validation.md`.

## Routed documents read

- `AGENTS.md`, `docs/ROUTER.md`
- `docs/specs/griever-monster/griever-monster.md`
- `docs/specs/debug-logging/AI-fix-live.md`
- `docs/workflows/runtime-scenario-validation.md`
- `docs/specs/dev-loop/dev-loop.md`
- `docs/skills/spec-behavior-review-v1.md`, `docs/skills/logging-checker-v1.md`

## Focused review result

- `spec-behavior-review-v1`: compared the Griever spec against the current code.
  Confirmed spec-code disagreements: two roll formulas (render-frame vs fixed
  tick), gravity applied with an inverted sign in both roll paths, a fixed-tick
  readiness gate that rejected rig-only avatars, four `monstergriever` name
  gates, and pitch-dropping roll input. All are addressed below.
- `logging-checker-v1`: all new records use `StructuredLogger` with existing
  categories, carry decision/state fields, and are tick- or transition-bounded
  (roll step every 30 ticks; capability/network/manifest on state change).

## Pre-existing working-tree changes (not authored here)

Present before this session and preserved: `config/accounts/default.json`
(volume), `config/analytics.json`, `config/audio/music-settings.json`,
`config/ragdoll.json`, `docs/changelog/2026-10-14/20261014_003000-avatar-limb-render-origin.md`,
`docs/jorj-docs/20261014plan.md`, `src/physics/movement/physics-collision-shared.h`
(roll response guards), `src/entities/player.h` (`rollBounceScale`), the
`rollBounceScale` line in `src/entities/player.cpp`, and the earlier
`engine-tick-net.cpp` vertical-cleanup band-aid. Note:
`config/accounts/default.json` gained `settings.avatar_name = "monstergriever"`
as a runtime side effect of the `avatar.load monstergriever` test command.

## Exact changes

### Slice 1 — one shared fixed-tick roll kernel

- New `src/physics/movement/roll-movement.h`: `rollIntegrateVelocity` (force/mass
  + signed gravity + speed clamp), `rollCleanupContactVelocity`, and
  `rollRotationDelta`. Pure helpers; no camera, avatar-name, transport, or GLB.
- `src/physics/physics-mini.cpp`: the roll branch now builds the intent from the
  full camera basis (pitch included) and calls the shared kernel, then the shared
  contact cleanup and roll rotation, and logs `griever.roll.step`.
- `src/engine/engine-tick-net.cpp`: removed the duplicate render-frame roll
  integration (force/gravity/speed/collision/rotation). The fixed-tick
  `physicsMainUpdate` roll path is now the single owner.
- Gravity sign fix: both old paths did `vel.z -= PHYS.gravity * scale * dt` with
  `PHYS.gravity = -58`, which accelerated the ball upward. The shared kernel now
  does `v.z += gravity * gravity_scale * dt`.

### Slice 2 — data-driven capability and lifecycle resolution

- Removed the `monstergriever` name gates in `server-players.cpp`,
  `movement-validation.cpp`, `multiplayer-reconcile.cpp`, and
  `engine-tick-net.cpp`; all now key on `AvatarMovementCap.mode == "roll"`.
- `src/network/multiplayer-tick.cpp`: `mpLocalGameplaySimulationReady` accepts a
  rig-only avatar (`physicalBody.extraParts` non-empty), so the fixed-tick roll
  path actually runs for the spike-ball instead of the render-frame path being
  the only mover.
- Capability-resolution evidence in `Player::applyAvatarMovementCap` and the
  server re-resolution path (`griever.capability.resolved`).
- Avatar-change propagation: the client republishes its manifest when the local
  avatar changes (`avatarNetworkName`, `griever.manifest.republish`); the server
  parses a completed manifest's `avatarName` and updates the player so the
  existing capability re-resolution runs (`griever.manifest.arrived`).

### Slice 3 — stable spherical world body

- The Griever already collides as GLB triangles from `physicalBody.extraParts`
  with `actorTriangleSolver` enabled; the pre-existing roll response guards
  (no ground bounce, no embedded upward push) are retained. This session proved
  the resting body is stable and does not hover.

### Regression fixed

- `src/network/server-packets.cpp::isKnownPacketType` rejected the avatar
  manifest/asset packet types (62-64), so the whole avatar-manifest receive path
  was dead. Accepted them. Recorded in
  `docs/regressions/2026-10-15/avatar-manifest-packets-rejected-REG.md`.

## Build evidence

- Canonical build (`python build.py build-only`) reported `BUILD SUCCESS`.
- Dev-loop published and launched build `2401`
  (`C:\mimita-v9\.dev\builds\2401\mimita.exe`,
  build_id `2401:769caf4393dc136e2f0cb0a28e8562c2dd2ab841dc7d35210a8f1fffafc1b6d3`,
  build_date `Oct 15 2026 15:52:06`).
- Build proves compilation/linking only.

## Runtime evidence

Authoritative dev-loop journal for the acceptance run:
`C:\mimita-v9\logs\2026-10-15\20261015_201502\events.jsonl`

- `griever.capability.resolved`: client and server both resolve
  `avatar=monstergriever mode=roll force=5000 mass=10 gravity_scale=3.0 radius=3.5`.
- `griever.roll.step` now originates in `physicsMainUpdate_Internal` (fixed tick),
  not the render path. Idle: the ball falls and settles (z 107.86 -> 78.16,
  then `vel.z=0`, `grounded=true`, stable across many samples; no upward drift).
- Movement: `intent=[-0.0,-0.98,0.17]` (pitch included in the W direction) and
  `vel=[-44.69,15.33,-2.78]`, i.e. force applied and position changed through
  physics.
- `server_pos_error` from `agent.snapshot`: `0.00001` (idle) to `0.00025`
  (moving). No growth; server accepted the roll reports.
- `griever.network.accepted` (reason none) and `griever.network.rejected`
  (reason `stale-client-tick`) alternate; the accepted reports carry the
  predicted transform to the server.
- Avatar switch (run `20261015_201147`): client republish -> server
  `griever.manifest.arrived` -> server `griever.capability.resolved jason`
  (mode walk), then switch back -> server `griever.capability.resolved
  monstergriever previous=jason` (mode roll). Both directions resolve.

Standalone `mimita.exe --versioninfo` reported its own journal path
`logs/10-15-2026/20261015_161543/events.jsonl`; the dev-loop run journal above is
the authoritative one for the scenario.

## First divergence before / closed boundary after

Before: the Griever was moved only by a render-frame roll formula with inverted
gravity and six target checks; the fixed-tick path was skipped for rig-only
avatars and the server could not learn an avatar change.

After: one fixed-tick shared roll kernel runs; gravity points down; the ball
settles; the server accepts roll reports with tiny position error; and the server
resolves avatar changes.

## Still requires human review

- Visual confirmation that the ball convincingly rolls and tumbles on multiple
  axes (root rotation is applied but was not visually reviewed).
- Feel at high render FPS, wall bounce quality, and corner behavior.
- Two-client remote rendering of the Griever and avatar switch.
- Whether the alternating `stale-client-tick` rejects should be reduced.

## Related records

- Regression: `docs/regressions/2026-10-15/avatar-manifest-packets-rejected-REG.md`
- Specification: `docs/specs/griever-monster/griever-monster.md`
