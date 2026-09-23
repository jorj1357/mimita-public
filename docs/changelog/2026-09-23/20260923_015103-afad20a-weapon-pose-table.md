# afad20a per-weapon arm pose table (hot, json/cpp selectable)

Date: 2026-09-23
Status: hot-code built; deterministic self-test passes; human acceptance pending

Related specification: `docs/features/live-code-development/live-code-development.md`
Predecessor: `docs/changelog/2026-09-23/20260923_014705-afad20a-animation-1to1.md`
Reference commit: `afad20a` ("npc stuff its cool", 2026-09-11).

## What afad20a did

`git show afad20a:src/entities/player-animation.cpp` + `player-animation-config.cpp`:

- `config/animations.json` held `weapons.<id>.poses.<state>` with
  `leftArm`/`rightArm` `translation` + `rotation` and a `useWeaponPose` gate,
  plus `active_pose`.
- The animator selected a state from the weapon runtime facts, in precedence
  order: `equipping` (-> `equip`), `reloading` (-> `reload`),
  `shooting`/`slash`/`lunge` (-> `fire`), `just_shot` (-> `cooldown`), then
  `idle` (-> `equipped`).
- For arms only it **replaced** `rotationEuler` and **added** `translation`.
- A pose with `useWeaponPose: false` was skipped and the fallback/next state was
  tried.

## What changed

New `src/hot-reload/hot-animation-weapon-poses.h`:

- Parses `config/animations.json` `weapons` (mtime-refreshed, hot) into
  `gameHash(weaponId) -> { active_pose, poses.<state> }`.
- `primaryState(actionId)` maps the hot action id to the afad20a state:
  `EQUIP -> equipping`, `RELOAD -> reloading`, `SLASH -> slash`,
  `LUNGE -> lunge`, `SHOOT -> shooting`, `JUST_SHOT -> cooldown`, else `idle`.
- `fallbackState` implements the afad20a fallback chain
  (`equipping -> equip`, `reloading -> reload`, `shooting -> fire`,
  `cooldown -> just_shot`, `idle -> equipped`).
- `poseFor(weaponKey, actionId, out)` tries the primary state, its fallback, the
  weapon's `active_pose`, then `idle`, skipping `useWeaponPose == false`.
- `apply(target, pose)` replaces the arm rotations and adds the arm
  translations (arms only), matching afad20a.
- `weaponPoseSource` (`"json"` | `"cpp"`) selects the implementation:
  `"json"` uses this table; `"cpp"` leaves the hot tool-phase / compiled carry
  stance in charge. Both are hot: the selector and the JSON are read live.

`src/hot-reload/modules/presentation/pose-generation.cpp` (afad20a path):

- Arm poses now come from the afad20a per-weapon table when
  `weaponPoseSource: "json"` and the weapon resolves; otherwise the hot tool
  phases / JSON `weaponArms` run as before.

`config/animations.json`: added `"weaponPoseSource": "json"`.

`src/hot-reload/modules/presentation/animation-selftest.cpp`: runs the weapon
pose self-test in the candidate gate.

`src/hot-reload/hot-modules.json`: registered the new header.

## Evidence

Build evidence:

- Hot DLL: `python build_game_dll.py` -> `DLL build success` (82 sources). No
  cold rebuild required.

Test evidence:

- `--live-code-selftest`: PASS, including the weapon pose self-test (state
  mapping, fallback mapping, replace-rotation/add-translation, arms mask, and a
  known-weapon resolution when the table is present).
- `--afad20a-parity-selftest`, `--movement-selftest`,
  `--movement-parity-selftest`, `--collision-selftest`: PASS.

Runtime evidence:

- Not performed. The running game applies the new hot DLL on the next reload.

Human acceptance:

- Pending. Per-weapon idle/shooting/just_shot/reloading/equipping/slash/lunge
  arms need a human playtest.

## Limits

- `weaponPoseSource: "cpp"` uses the compiled carry stance / tool phases, not a
  compiled copy of the full per-weapon pose table. The JSON source is the exact
  afad20a table.
- The state is derived from the hot action id, which the animation policy already
  computes from the weapon runtime; there is no separate hot read of
  `equipTimer`/`shootEffectTimer`/`swordPoseState`/`fireCooldown`.
- `weaponPoseSource: "cpp"` is not wired to a compiled per-weapon table; it is
  the existing fallback path.
