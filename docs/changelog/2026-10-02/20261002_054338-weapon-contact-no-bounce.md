# Weapon contacts: no launch, JSON-tunable bounce

## Change

- Weapon contacts with the static world no longer bounce the player. They block
  movement (velocity projection + positional depenetration) without turning the
  weapon's own sweep velocity into root player velocity. This implements the
  documented rule in `docs/specs/20260927plan.md` ("weapon contacts: no bounce",
  "Do not use arm/weapon sweep velocity as player bounce").
- Added a per-weapon `"player_bounce"` field to `config/weaponcollisions.json`.
  It scales `config/collision.json` `bounce.strength` for that weapon's contacts
  with the world (default `0.0` = no launch; raise toward `1.0` to rebound).
  The value is hot-reloaded with the rest of the file and is stored on
  `WeaponCollisionRuntimeDebug::playerBounce`.
- `respondVelocityAgainstNormal` (`physics-collision-shared.h`) gained a
  `bounceScale` parameter; the existing call sites keep `1.0`. The actor
  triangle solver and the legacy body/weapon pass pass the weapon's
  `player_bounce` when the contact label is `"weapon"`.
- The weapon hitbox geometry itself is unchanged (still `source: "boxes"`); only
  the velocity response changed. Boxes remain editable in the JSON as before.

## Evidence

Source:

- `src/entities/player.h`: `WeaponCollisionRuntimeDebug::playerBounce`.
- `src/combat/weapon-collision-config.{h,cpp}`: parse/clamp `player_bounce`
  (accepts legacy `bounce`), reset per weapon in `applyCollisionConfig`.
- `src/physics/movement/physics-collision-shared.h`: `bounceScale` multiplies
  `bounce.strength`; `0` falls through to `projectVelocityAgainstNormal`.
- `src/physics/movement/actor-triangle-solver.cpp`: weapon contacts use
  `player.weaponCollisionDebug.playerBounce`; selftest #9 now asserts the weapon
  contact does not launch the player.
- `src/physics/movement/physics-collision-glb-body.cpp`: same scale on the
  legacy (non-triangle-solver) weapon push path.
- `config/weaponcollisions.json`: `player_bounce: 0.0` added to every weapon
  with an explanatory header comment.

Build:

- `python build_agent.py` -> `Status: SUCCESS`, return code 0, linked
  `mimita.exe` (2026-10-02 01:41:17 local).
- Note: build output ended with `Nothing changed.` after the link; object files
  for the changed translation units and `mimita.exe` were both regenerated
  (verified by mtime). This is not treated as a failed relink.

Test (automated, headless):

- `--actor-triangle-solve-selftest` PASS, including the new
  `weapon contact does not launch the player`; the root-body bounce test
  (`200 m/s normal momentum rebounds when bounce is enabled`) still PASSES.
- `--actor-collision-mesh-selftest` PASS.
- `--moving-crate-selftest`, `--destructible-selftest`,
  `--destruction-replication-selftest`, `--destruction-stress-selftest` PASS.
- `--physical-perf-selftest` PASS (moving holey crates ~2.27 ms/tick, target
  4 ms MET; full-auto avg 0.19 ms / max 0.42 ms per flush).

Runtime:

- None. No live run or log capture was performed in this session.

## Human acceptance

Not performed. In game, hold a weapon and walk into a wall/crate: the weapon
should stop the body without launching it; set a weapon's `"player_bounce"` to a
nonzero value and confirm the rebound returns. The "falls through the world"
report needs the same playtest to confirm it is gone.
