// 2026-09-30T20:42:05Z (display: 2026-09-30 16:42:05 EDT)
/* purpose
* Record the hole-size/cut-consistency fix and the smaller safe collision
* performance pass after the first human gameplay test of destructible crates.
* Human feedback: holes were tiny/inconsistent, and approaching a cut crate
  dropped FPS hard. Fracture left enabled by default (not yet tested).
*/

# Task

- Summary: Make one shot reliably cut one hole whose size is config-driven from
  both projectile force and projectile size, fix the duplicate-cut heuristic,
  and remove the obvious per-query allocation/scan costs in the entity collision
  and swept-projectile paths.
- Status: CODE_COMPLETE / BUILD_VERIFIED / SELFTEST_PASS /
  RUNTIME_VISUAL_AND_MULTIPLAYER_VALIDATION_REQUIRED
- Branch: `afad20a-rebuild`; base commit `61cd8e92`.

# Human feedback driving this pass

- Cuts do happen, but FPS drops badly when close to a cut crate (collisions).
- Hole size did not match the projectile.
- ~30 shots produced only 1-2 holes; shooting the same spot rarely deepened.
- Desired: 1 shot = 1 hole, repeated shots deepen/tunnel, projectile size drives
  hole size. `crate_spawn` used for testing. Fracture not yet testable.

# Changes

## Hole size and consistency

- `src/combat/weapon-types.h` + `src/combat/weapon-json-config.cpp`: added
  `cut_radius_scale` (`cut_radius_scale` JSON key) to the weapon definition; the
  size contribution to the hole is now config-driven.
- `src/impact/impact-event.h`: added `ImpactEvent::sizeScale`.
- `src/impact/impact-system.cpp` `calculateCutRadius`: the radius is now
  `sourceRadius + forceRadius + sizeRadius`, clamped to the material max, with
  the projectile radius as the floor. Force uses `cbrt(energy)`; size uses
  `sourceRadius * sizeScale`. A small fast projectile can still cut a big hole.
- `src/network/server-projectiles.cpp` and
  `src/network/multiplayer-projectiles.cpp`: both now pass the real projectile
  radius (`max(projectileRadius, projectileBaseRadius)`) and
  `cutRadiusScale` into the impact event instead of the 0.01 m base radius.
- `src/impact/impact-system.cpp` `submit` dedup: a source-less impact always
  cuts; a sourced impact dedups by exact `predictionKey` or by a
  radius-scaled spot match. This replaces the fixed 1e-4 tolerance that made
  repeated shots inconsistent.
- `config/weapons.json`: `projectile_rifle` gains `"cut_radius_scale": 1.0`.

## Collision performance (smaller safe fixes)

- `src/physics/physical-entity.cpp` `collectActorEntityContacts`: the per-entity
  surface cache now also builds an `AabbTree` over the cached world triangles
  and calls `collectActorMeshContactsInto(..., &entry.tree)` instead of the
  linear `collectActorMeshContacts`. The per-iteration narrowphase now prunes by
  tree rather than scanning every crate triangle.
- `src/network/server-projectiles.cpp` and
  `src/combat/client-collision-world-view.cpp`
  `queryEntityTrianglesSwept`: removed the per-entity `std::vector<...>`
  world-triangle allocation; each triangle is transformed and AABB-rejected
  inline. No heap allocation per entity per sweep substep.

## Tests and docs

- `src/impact/destructible-selftest.cpp`: added (19) force and projectile-size
  both scale the hole and `cut_radius_scale=0` removes the size term, (20) every
  rifle shot creates a hole, (21) repeated same-axis shots tunnel through.
- `docs/specs/manifold-destructible-integration-plan.md`: new 10.1 cut-size
  formula and an updated 11 performance status.
- Fracture left enabled by default per human request.

# Reasoning

- Spec alignment: `destructible-world.md` 15/18/50 (results emerge from
  mass/velocity/energy, not weapon-specific rules) and the human's explicit
  requirement that force and size both matter.
- The 1e-4 dedup tolerance was a fixed constant unrelated to cut size; scaling
  by radius and requiring a real source fixes both the missed double-apply and
  the over/under-culling of genuine repeated shots.
- Perf: the reflection of the prior session's own plan (13.2) was completed for
  entities using the existing `AabbTree` (`destructible-world.md` 43 requires
  spatial acceleration); the sweep allocation removal follows the "no heap
  allocation in the fixed-tick hot loop" invariant.

# Validation

- Build: `python build.py build-only` -> BUILD SUCCESS.
- `mimita.exe --destructible-selftest` -> PASS (incl. new hole-size and tunnel
  checks; all earlier checks still pass).
- `mimita.exe --moving-crate-selftest` -> PASS (no regression from the entity
  AabbTree path).
- `mimita.exe --destruction-replication-selftest` -> PASS.
- `python tools/check-debug-logging.py`: no new findings in the changed files.
- NOT performed: live FPS measurement near a cut crate and two-client visual
  confirmation. The tree + allocation fixes are the "smaller safe fix"; a
  per-entity projectile tree and a per-frame destruction budget remain if this
  is not enough.

# Pre-existing work

- All Manifold/destructible/replication/fracture work is uncommitted from
  earlier sessions and was extended, not re-authored. Pre-existing uncommitted
  edits across `config/*`, `devscripts/*`, `src/*` (other files), and dated docs
  were left untouched.

# Documents and skills reviewed

- `AGENTS.md`
- `docs/ROUTER.md`
- `docs/specs/destructible-world/destructible-world.md` (15, 18, 43, 44, 50)
- `docs/specs/manifold-destructible-integration-plan.md`
- `docs/architecture/time-and-formatting/time-and-formatting.md`
- `docs/skills/spec-behavior-review-v1.md`
- `docs/skills/efficiency-checker-v1.md`

# Human review still needed

- Shoot a crate once: exactly one hole, roughly as wide as the projectile.
- Shoot the same spot repeatedly: the hole deepens and can become a tunnel.
- Stand next to / touch a cut crate: confirm no large FPS drop. If still bad,
  the next step is a per-entity tree for the projectile query plus a per-frame
  entity-contact budget.
- Retune `cut_energy_scale` / `cut_radius_scale` in `weapons.json` to taste.
- Fracture (now testable) still needs acceptance and tuning.