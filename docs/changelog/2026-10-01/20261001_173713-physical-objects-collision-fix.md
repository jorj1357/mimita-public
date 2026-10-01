// 2026-10-01T17:50:34Z (display: 2026-10-01 13:50:34 EDT)
/* purpose
* Record the session that began the physical-object / destructible-world
* correctness+feel+perf pass: terminal word wrap and Shift+Tab reverse
* autocomplete, rounded entity-vs-entity contacts, generic face settling with
* configurable object gravity/damping, fragment lifecycle config, and the first
* performance fixes (no per-entity allocation in the sweep, per-entity GPU mesh
* cache). Remaining work is tracked in the linked regression record.
*/

# Task

- Summary: physical-object feel/correctness and destruction performance, with
  the terminal UI fixes first.
- Status: PASS_WITH_HUMAN_REVIEW
- Branch: `afad20a-rebuild`; base commit `cce04d99`.
- Result states: build PASS; all four destruction selftests PASS; runtime
  frame-time and two-client visual acceptance NOT performed.

# Human feedback driving this pass

The full grouped list is recorded in
`docs/regressions/2026-10-01/physical-objects-collision-REG.md`. Highlights:
crate phasing into floor/cylinder/other and player phasing; density-dependent
settling that never rests flush; too-strong drag; no momentum on fast contact;
floating/tiny fragments; large FPS drops when shooting/being near holey crates;
terminal needs word wrap and Shift+Tab reverse autocomplete.

# Changes

## WE — terminal UI (done first)

- `src/devtools/terminal-input.cpp`: `Tab` autocompletes; `Shift+Tab` now
  reverse-cycles the autocomplete list.
- `src/devtools/terminal.{h,cpp}`: added `mWrapColumns` and a greedy
  `wrapTerminalLine`; `addLog` stores pre-wrapped lines (scroll offsets stay
  line-based, incrementing by the number of wrapped lines added).
- `src/devtools/terminal-render.cpp`: refreshes `mWrapColumns` from the live
  draw width each frame.

## WA — collision correctness (partial)

- `src/physics/physical-entity.cpp` `collectPairDirection`: entity-vs-entity
  contacts now pass `contactSkin = -1`, enabling the rounded feature shell
  (point→sphere, line→capsule, face→triangle) so crates cannot phase through a
  corner or curved surface.
- `src/physics/physical-entity.cpp` `applyPlayerPush`: includes
  `player.externalImpulse` so a dash transfers the contact-speed impulse.
- Actor-vs-entity (`collectActorEntityContacts`) intentionally stays at skin 0
  (exact triangles) because `-1` changed player-carry support behavior; that
  migration remains tracked in the regression record.

## WB — generic face settling + configurable object physics

- `src/impact/destructible-geometry.{h,cpp}`: added `restAxes` and
  `computeRestAxes` — the object's distinct local face orientations (sign
  agnostic, deduped), recomputed on initialize/rebuild. Boxes use the 3 axes;
  imported/cut meshes use their real faces.
- `src/physics/physical-entity.cpp`: replaced `isBoxRestingUpright` /
  box-axis righting with `bestRestAxisAlignment` / `isRestingOnFace` /
  `applyRestingRightingTorque` using `restAxes`. Righting is an angular-velocity
  impulse, not a torque through inertia, so settling does not scale with
  density.
- `src/impact/destructible-world-config.{h,cpp}` + `config/destructible-world.json`
  `physics`: `objectGravity`, `objectLinearDamping`, `objectAngularDamping`,
  `supportedFrictionRetain` — hot-reloadable. `advanceKinematics` uses
  `objectGravity` (players keep `PHYS.gravity`) and the config damping/friction.
  Code defaults preserve the legacy feel so self-tests are stable; the JSON
  holds the tuned in-game values (`objectGravity -70`, damping `0.08`/`1.5`,
  friction retain `0.8`).

## WC — fragment lifecycle

- `src/physics/physical-entity.h`: `isFragment`, `fragmentAge`.
- `src/impact/impact-system.cpp` `applyFracture`: children are flagged
  fragments; the primary piece is woken and cleared of sleep/support state
  (fixes the floating chunk).
- `src/physics/physical-entity.cpp` `advanceKinematics`: fixed-tick lifecycle
  pass removes fragments smaller than `minFragmentVolume`, older than
  `fragmentLifetimeSeconds`, or beyond `maxTotalFragments`.
- `config/destructible-world.json` `fragments`: `minFragmentVolume` 0.001 m^3
  (~0.1 m cube), `fragmentLifetimeSeconds` 12, `maxTotalFragments` 64.

## WD — performance (partial)

- `src/physics/physical-entity.cpp` `advanceKinematics`: the entity-vs-world
  sweep no longer allocates per entity per collision pass (thread-local
  `s_objectMeshes` / `s_objectCandidates` / `s_objectContacts` +
  `collectActorMeshContactsInto` instead of the value-returning
  `collectActorMeshContacts`).
- `src/impact/destructible-render.{h,cpp}`: per-entity GPU mesh cache keyed by
  `(entity id, geometryRevision, vertexCount)`; `releaseGeneratedEntityMesh` is
  called from `PhysicalEntitySystem::remove`. Previously one shared VBO was
  re-uploaded on every entity switch (every fragment/holey crate every frame).

# Reasoning

- Spec alignment: `docs/architecture/collision/collision.md` (fixed 60 Hz,
  cached broadphase, no per-query allocations in hot loops) and
  `destructible-world.md` 44-46 (bounded work). Rounded features are the
  documented "point/line/face thickened" model.
- Smallest changes with the largest correctness/perf effect first; the larger
  local-space collision refactor is deferred and recorded rather than rushed.

# Files changed (since cce04d99)

- `src/devtools/terminal-input.cpp`, `terminal-render.cpp`, `terminal.{h,cpp}`
- `src/impact/destructible-geometry.{h,cpp}`, `destructible-render.{h,cpp}`,
  `destructible-world-config.{h,cpp}`, `impact-system.cpp`
- `src/physics/physical-entity.{h,cpp}`
- `config/destructible-world.json`
- `docs/regressions/2026-10-01/physical-objects-collision-REG.md` (new)

# Pre-existing work (not authored this session)

`config/analytics.json` and `docs/specs/20261001plan.md` were already modified
and were left untouched. The prior crash-safe / shared-collision / momentum work
was committed by the human as `cce04d99`.

# Documents and skills reviewed

- `AGENTS.md`, `docs/ROUTER.md`
- `docs/specs/destructible-world/destructible-world.md`
- `docs/architecture/collision/collision.md`
- `docs/operations/build-and-exe/build-and-exe.md`,
  `docs/operations/task-completion/task-completion.md`
- `docs/skills/spec-behavior-review-v1.md`,
  `docs/skills/efficiency-checker-v1.md`
- `docs/regressions/README.md`

# Validation

- Build: `python build.py build-only` -> success; relinked `mimita.exe`.
- `mimita.exe --moving-crate-selftest` PASS (incl. momentum exchange,
  projectile momentum/torque, rest/sleep).
- `mimita.exe --destructible-selftest` PASS.
- `mimita.exe --destruction-replication-selftest` PASS.
- `mimita.exe --destruction-stress-selftest` PASS.
- Cold-build debt: appended `Cold-build occurrence 39`.

# Human review still needed

- Terminal: Shift+Tab reverse autocomplete; long lines wrap.
- Crates: no phasing at corners/curved surfaces; settle flush on a real face at
  any density; drag/gravity feel (tune `config/destructible-world.json`).
- Fragments: fall, no floating chunk, tiny/old debris disappears.
- Performance: no frame over 4 ms render with ~10 holey crates; no 0.5 s shoot
  freeze; `perf_top` whole-frame/gameplay/collision/boolean/render numbers.
- Two-client destruction/fragment visual match.

# Remaining work (tracked in the regression record)

- Actor-vs-entity rounded shell without breaking carry; deep-depenetration pass
  for floor+wall corners; edge jitter with a wall partly inside the floor.
- Cached entity-vs-world triangle gather; local-space entity triangles + tree;
  entity broadphase for the pair pass; profile the shoot freeze.
- Two-client fragment collision/replication verification.
