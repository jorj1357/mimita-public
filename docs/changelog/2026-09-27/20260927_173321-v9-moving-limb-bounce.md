// 2026-09-27T17:33:21Z
/* purpose
* record the moving-limb/weapon bounce fix in the active repo C:\mimita-v9
* state what changed, what was validated, and what is marked TO-DELETE
* does NOT claim human gameplay acceptance
*/

# Task

- Task ID: v9-moving-limb-bounce
- Summary: Make a moving body part / held tool bounce the whole body off world
  geometry even when the root is still, add a configurable minimum low-speed
  push, and mark the pink body-contact spark TO-DELETE (not removed).
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
