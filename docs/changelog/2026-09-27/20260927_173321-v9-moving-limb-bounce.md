// 2026-09-27T17:33:21Z
/* purpose
* record the moving-limb/weapon bounce fix in the active repo C:\mimita-v9
* state what changed, what was validated, and what is marked TO-DELETE
* does NOT claim human gameplay acceptance
*/

# Task

- Task ID: v9-moving-limb-bounce
- Summary: (1) Make a moving body part / held tool bounce the whole body off
  world geometry even when the root is still; (2) stop a limb from getting stuck
  inside a walkable slope; (3) make the minimum push work for embedded limbs;
  (4) replace the one-sphere-per-part body collision with the part's real mesh
  triangles vs world triangles; (5) keep the pink spark (its TO-DELETE notes were
  removed). See the Follow-up section below.
- Status: CODE_COMPLETE / BUILD_VERIFIED / DETERMINISTIC_TEST_PASS /
  HUMAN_REVIEW_REQUIRED
- Date, time, timezone: 2026-09-27T17:33:21Z, ISO 8601
- Branch: `afad20a-rebuild`
- Base commit: `23716e83` (pre deleting like old code that does the hit impact spark)
- Final commit: not committed in this session

# Pre-existing changes

- Exact status output (before this session's edits, already dirty):
  `M config/accounts/default.json`, `M config/analytics.json`.
- Files not created or modified by this session: `config/accounts/default.json`
  and `config/analytics.json` (left untouched).

# Requested behavior

The user plays/built `C:\mimita-v9` (branch `afad20a-rebuild`), not the
`C:\mimita-priv-v8` folder this chat was opened in. In v9 there is no hot
collision package; the active collision is the cold path. The user sees the pink
"body contact spark" and does not always bounce off the world. They asked to fix
the bounce in v9 and to mark old code TO-DELETE instead of deleting.

# Specification alignment

- Current specification paths: `docs/specs/movement/movement.md` (body/weapon
  collision, universal contact reset), `docs/architecture/collision/collision.md`
  (60 Hz fixed tick, one owner).
- Exact requirements: "A touching part pushes the whole player"; "Use the part's
  real swept speed"; "Apply a small minimum outward push when a valid contact
  happens at very low speed"; "The red spark remains only a visual effect".
- Why the change follows the specification: the cold response used only the root
  velocity (`respondVelocityAgainstNormal`), so a moving arm with a still root
  produced no bounce. The fix feeds each body-push contact's own `sweepDelta`
  into the response and adds the minimum push.
- Conflicts or decisions: v9's collision is a single cold owner, so there was no
  duplicate solver to condense. The pink spark is marked TO-DELETE, not removed.

# Exact implementation changes

## File: `src/config/collision-config.h`

- Added `bounceMinPush()` accessor and `mBounceMinPush = 0.1f`.

## File: `src/config/collision-config.cpp`

- Parse `bounce.minPush` (default `0.1`); include it in the load log line.

## File: `config/collision.json`

- Added `"minPush": 0.1` under `bounce`.

## File: `src/physics/movement/physics-collision-shared.h`

- `respondVelocityAgainstNormal(Player&, const glm::vec3& normal, const
  glm::vec3& partVelocity = glm::vec3(0.0f))`.
- Impact is now `max(root into, part into)` where `part into` comes from the
  passed part sweep. When the part dominates, the root body gets an outward
  velocity along the surface normal. A valid very-low-speed part contact applies
  `bounceMinPush`. The root-only path (default argument) is unchanged.

## File: `src/physics/movement/physics-collision-glb-body.cpp`

- `runBodyWeaponPass` last-pass response now passes `pc.sweepDelta`:
  `respondVelocityAgainstNormal(p, pc.normal, pc.sweepDelta)`.
- Added a dated TO-DELETE comment on the `spawnBodyContactSpark` call.

## File: `src/physics/movement/physics-collision-stress.cpp`

- `collisionStressSelfTest` now checks: (1) a moving limb bounces a still root,
  (2) a very-low-speed limb contact applies the minimum push, (3) a resting root
  contact adds no phantom push. Skips when bounce is disabled in config.

## Files marked TO-DELETE (comments only, nothing removed)

- `src/effects/effect-part.h` — declaration.
- `src/effects/effect-part.cpp` — `spawnBodyContactSpark` definition.
- `src/effects/effect-part-render.cpp` — `"body_spark"` render case.
- `config/hitfx.json` — `bodyContactSpark` block (added a `comment`).

# Diagnostics

- Owner/category: cold collision response (`respondVelocityAgainstNormal`),
  COLLISION category. No new diagnostics added.

# Validation

- Build: the running `devscripts/dev-loop.py` recompiled the edits live
  (`.dev/state.json`: source generation 10, latest successful build 40,
  `C:\mimita-v9\.dev\builds\0040`). `python build_agent.py` reported
  `Status: SUCCESS` / "Nothing changed" because the dev-loop had already
  compiled the sources.
- Tests (run from the repo root so `config/collision.json` loads):
  `.\mimita.exe --collision-selftest`
  - `PASS [COLLISION STRESS] moving limb bounces still root`
  - `PASS [COLLISION STRESS] low-speed limb contact min push`
  - `PASS [COLLISION STRESS] resting root contact adds no push`
  - `[COLLISION SELFTEST] PASS` (all existing cases still pass)
- Note: running the selftest from `.dev/builds/0040` skips the bounce checks
  because that directory has no `config/`; run from `C:\mimita-v9`.

# Measured evidence

- Before: a limb moving into a wall with a still root produced no root velocity
  change (impact derived from the zero root velocity).
- After: the same case yields a positive root velocity along the wall normal;
  a near-zero part speed yields the `minPush` (0.1) result.

# Regression review

- Regression entry appended: no.
- Why: no previously working behavior is known broken; the root-only response
  path is byte-for-byte unchanged (default `partVelocity = 0`).
- Related regression paths:
  `docs/regressions/2026-09-23/dash-down-dash-press-buffer-cooldown-REG.md`
  (no-cooldown rule; this change adds no new timer).

# Human acceptance

- Visual review: NOT performed.
- Gameplay review: NOT performed. Requires watching an arm/weapon bounce the
  body off a wall while the root is still.
- Multiplayer review: NOT performed.
- Still unverified: bounce feel and magnitude (`strength 0.35`, `minPush 0.1`),
  and whether the pink spark can now be deleted.

# Related feature record

- Feature path: none created yet.

# Cross-repo note

- Earlier this session, Phase A was implemented in `C:\mimita-priv-v8`
  (`collision-package-solver.cpp` etc.). That folder is an older clone and is
  not what the user runs. Those edits were not ported; this changelog records the
  v9 fix. The v8 working-tree edits can be discarded.

# Follow-up (same session) — A/B/C unstick + D mesh limbs

## Requested behavior

The user runs `C:\mimita-v9`. Symptoms: the pink body-contact spark fires but the
limb gets stuck inside the world (especially a walkable slope) instead of the
body bouncing out; `minPush` had no effect; and the limb visibly sinks into the
wall. Desired: keep the spark as a visual, fix the collision underneath, and
eventually make limbs collide with their real mesh triangles ("what you see is
what the hitbox is"), later applied to weapon shots too.

## Changes

### A — a walkable body contact is only ground when it is at the feet

- `src/physics/movement/physics-collision-glb-body.cpp` (`runBodyWeaponPass`):
  contacts are now split into `groundContacts` only when
  `normal.z > MAX_WALKABLE_SLOPE_DOT` **and** `point.z <= feetZ + 0.15f` (the
  same feet test `applyCollisionContact` uses). Everything else is a push
  contact. Previously any walkable body contact (e.g. an arm on a shallow slope)
  was treated as ground and never depenetrated.

### B — body push contacts always depenetrate

- `runBodyWeaponPass`: `solverContacts` is now
  `bwRootContacts + bodyPushContacts` where `bodyPushContacts` includes the
  walkable-non-foot contacts. A limb jammed into a walkable slope is pushed out
  by `solveBatchedCorrection` + `p.pos += correction`.

### C — minimum push works for embedded limbs

- `src/physics/movement/physics-collision-shared.h`:
  `respondVelocityAgainstNormal(Player&, normal, partVelocity, bodyContact,
  penetration)`. The low-speed branch now pushes at least `bounceMinPush` when a
  body contact is moving into the surface **or** is embedded
  (`penetration > 0.002`). Root contacts (`bodyContact == false`) are unchanged.
- `physics-collision-glb-body.cpp` passes `sweepDelta`, `true`, and
  `pc.penetration` for body push contacts.

### D — limbs use their real mesh triangles vs world triangles

- New `src/physics/movement/physics-collision-mesh.cpp`:
  `collectBodyMeshContacts(Player&, const World&)` transforms each part's real
  collider triangles (`Collider::triangles`, loaded from the model) by
  `worldTransform` (and `previousWorldTransform` for the sweep delta) and tests
  them **triangle-vs-triangle** against world triangles within the part's
  broadphase AABB. Contact normal comes from the world triangle oriented toward
  the actor; penetration is the deepest body vertex behind the surface. First
  version is brute-force with budgets (`kMaxPartTriangles = 512`,
  `kMaxContactsPerPart = 64`, `kMaxTriangleTests = 200000`); dedup keeps the
  deepest contact per (part, world triangle).
- `physics-collision-body.cpp`: `collectBodyWeaponSpheres(p, includeBodyParts)`
  so the mesh path can request weapon-only spheres.
- `physics-collision-glb-body.cpp`: when `bodyMeshCollision` is on, body contacts
  come from `collectBodyMeshContacts`; weapon contacts (spheres + capsule) are
  unchanged.
- `physics-collision-shared.h`: declaration added.
- `src/config/collision-config.{h,cpp}` + `config/collision.json`:
  `"bodyMeshCollision": true` (set false to fall back to the old sphere path).

### Spark kept

- Removed the TO-DELETE comments added earlier in `effect-part.h`,
  `effect-part.cpp`, `effect-part-render.cpp`, `physics-collision-glb-body.cpp`,
  and the `comment` in `config/hitfx.json`. `spawnBodyContactSpark` is unchanged.

## Validation

- Build: the running `devscripts/dev-loop.py` compiled every change live
  (`.dev/state.json`: source generation 33, build 50,
  `C:\mimita-v9\.dev\builds\0050`). `python build_agent.py` reported SUCCESS.
- `.\mimita.exe --collision-selftest` (run from `C:\mimita-v9`):
  - `PASS [COLLISION STRESS] moving limb bounces still root`
  - `PASS [COLLISION STRESS] embedded limb contact min push`
  - `PASS [COLLISION STRESS] embedded root contact no push`
  - `PASS [COLLISION STRESS] resting root contact adds no push`
  - `PASS [COLLISION STRESS] mesh limb triangle hits floor, pushes up`
  - `[COLLISION SELFTEST] PASS` (all original cases still pass)

## Human review still required

- Watch a limb against a walkable slope: it should push the body out, not stick,
  and the pink spark should still appear.
- Watch limbs vs walls/floors with the real mesh triangles: the visual limb
  should barely sink now. Confirm the frame rate is acceptable (first version is
  brute-force).
- Then decide whether to apply the same real-triangle approach to weapon shots
  (hitscan/hit resolution) and to consolidate the two body-collision phases
  (D follow-up / E).
