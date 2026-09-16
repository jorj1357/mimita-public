# Weapon anims: runtime shoot/reload selection, exaggerated upper-body phases, hot new weapons

Date: 2026-09-16 16:11 EDT (UTC 2026-09-16T20:11:28Z)
Branch: `8292026stash` (worktree, uncommitted)
Result: `HOT PART LIVE; PART C COLD BUILD PENDING (mimita.exe running)`

## Task

Make weapon shooting/reloading actually play, exaggerate the phase poses, and
make adding a brand-new weapon hot (no cold edit). Parts A/B are hot; Part C
needs one cold build to install the enumeration/ABI.

## Part A — shoot/reload selection from runtime (hot)

Root cause: `HOT_ACTION_FLAG_SHOOTING`/`RELOADING` come from
`player.networkWeaponState`, which is only written for remote players, so the
local player never selected SHOOT/RELOAD (equip worked because it used the
runtime `equipTimer`). `animation-policy.cpp` now derives from the runtime
values the bridge already published:
- `f.shooting = flags.SHOOTING || shootEffectTimer > 0`
- `f.reloading = flags.RELOADING || isReloading || reloadTimer > 0`
- melee (slash/lunge) is now checked before generic shooting so a weapon that
  also sets a shoot-effect timer still plays slash/lunge.

## Part B — exaggerated upper-body phases (hot)

- `tool-visuals.cpp`: added `WPN_UPPER` (torso+head+arms) alongside `WPN_ARMS`;
  `WPN_PH` now takes an explicit mask. `idle` phases are `MaskArms` (keep
  locomotion torso/legs); `shoot`, `just_shot`, `reload`, `equip`, `unequip`,
  `slash`, `lunge` are `MaskUpper` with strong torso/head lean and larger arm
  swings, per weapon (revolver, shotgun, rocket, grenade, sword, knife).
- `just_shot` kept as a short settle tail (falls back to the `shoot` phase when
  a weapon has none).

## Part C — hot new weapon registration (cold install)

- `game-api.h`: `GameToolParamV1`, and `GameToolDefinitionV1` extended with
  `enumerateIndex`, `id`, `displayName`, `soundHit`, `soundDryFire`, and a
  bounded `params[8]` customParams list.
- Moved the `gameHash` constexpr to the top of `game-api.h` (a concurrent edit
  used it above its definition, breaking the build).
- `hot-tool-visual.h`: `ToolDefinitionV1` gained the same identity/params fields.
- `tool-visuals.cpp`: recipes set id/displayName/sounds; the provider now
  enumerates (`toolKey == 0` + `enumerateIndex`); added a hot-only
  `hot_selftest_gun` recipe that has no cold builtin.
- `weapon-json-config.cpp`: `applyHotToolDefinition` also applies
  id/displayName/soundHit/soundDryFire/params; new `registerHotTools()`
  enumerates hot tools and adopts unknown ids into `WeaponRegistry` +
  network-id registration. `weapon-data.cpp` calls it after builtins.
- Result: adding a weapon = add a `makeXVisual()` in `tool-visuals.cpp`; no cold
  edit. Loadout membership is data (cloud/config) or the generic `equiptool`
  path, not a code list.

## Validation so far

- `python build_game_dll.py` -> `build\mimita-game.dll` success (57 sources).
- Cold syntax check (`g++ -fsyntax-only`, project flags + PCH) on
  `weapon-json-config.cpp`, `weapon-data.cpp`, `hot-combat-selftest.cpp`,
  `live-behavior.cpp`, `weapon-system.cpp` -> exit 0.
- New selftest assertion added: `hot_selftest_gun` appears in the cold registry
  after `registerBuiltinWeapons()` (proves hot new-weapon adoption).

## Blocker

`python build_agent.py` refuses: two `mimita.exe` are running (started
15:50). The cold build is required for Part C's ABI/enumeration. No process was
killed and no force-cold was used. Parts A/B are live in the running session via
the hot DLL.

## Concurrent-session fixes (not my work)

- `live-behavior.cpp` used `HeadlessWorld` unqualified (fixed as
  `MimitaNet::HeadlessWorld`).
- `game-api.h` used `gameHash` before its definition (moved it up).

## Next

Close both `mimita.exe`, then: `python build_agent.py` ->
`mimita.exe --live-code-selftest` -> `mimita.exe --hot-combat-selftest`
(expect the two known cross-session effect failures to remain). Then visual
tuning of the phase values in `tool-visuals.cpp` (all hot).
