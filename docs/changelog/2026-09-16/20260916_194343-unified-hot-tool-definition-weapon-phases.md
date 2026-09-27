# Unified hot tool definition + per-weapon animation phases (weapon anims)

Date: 2026-09-16 15:43 EDT (UTC 2026-09-16T19:43:43Z)
Branch: `8292026stash` (worktree, uncommitted)
Result: `PASS (weapon animation) / 2 CROSS-SESSION EFFECT FAILURES REMAIN`

## Task

Make one hot C++ tool definition own a weapon's gameplay data, held model,
sounds, and per-phase animations (idle, shoot, just-shot, reload, equip,
unequip, slash, lunge), authoritative for cold execution through a generic
capability. Focus: weapon animations. Out of scope: movement jitter, frame
pacing, collision.

## What changed

### 1. One hot `ToolDefinitionV1` (extends `ToolVisualRecipeV1`)
- `hot-tool-visual.h`: added `ToolDefinitionV1` (gameplay: behavior/fire/network
  mode, hitscan, slot, damage, headshot, fire/reload timing, equip/unequip pose
  time, ammo/mag/reserve/pellets/spread/recoil, projectile speed/radius/lifetime;
  `presentMask` marks authoritative groups) and `ToolAnimPhaseV1`
  (`phaseId/duration/loop/mask/frames`). `ToolVisualRecipeV1` now carries
  `definition`; `ToolSoundSetV1` gained `unequip`.
- `tool-visuals.cpp`: per-weapon procedural phase keyframe tables (revolver,
  shotgun, rocket launcher, grenade launcher, swordsword, spy knife) and the
  gameplay numbers mirrored exactly from `config/weapons.json` (no balance
  change). `WPN_ARMS`/`WPN_PH` compact authoring macros.

### 2. Generic gameplay authority capability
- `game-api.h`: `GAME_CAP_TOOL_DEFINITION` + `GameToolFieldFlags` +
  POD `GameToolDefinitionV1` + `GameToolDefinitionQueryFn`. No per-weapon ABI
  field; data is fixed arrays/char buffers only.
- `tool-visuals.cpp`: registers a `tool.definition` provider mapping tool key ->
  POD; a missing tool returns `found = 0`.
- `weapon-json-config.cpp` (cold): `applyHotToolDefinition()` resolves the
  capability and applies the marked fields onto `WeaponDefinition`; JSON/builtin
  remains fallback. `reloadBuiltinWeaponsIfChanged()` now re-registers weapons
  when the **hot generation** changes, so a live tool-definition edit applies
  without a JSON edit.

### 3. Per-weapon animation wiring
- `hot-action.h`: added `HOT_ACTION_JUST_SHOT`, `HOT_ACTION_UNEQUIP`.
- `animation-policy.cpp`: reads the previously ignored action-state timers —
  `EQUIP` from `equipTimer > 0` (fixes the never-set network `EQUIPPING` flag),
  `JUST_SHOT` from `fireCooldown > 0` after the muzzle window, and `UNEQUIP`
  from the tool-removal edge. `AnimationMemory` bumped to v2 (adds
  `prevWeaponKey`, `departingWeaponKey`) with a v1 -> v2 migration.
- `pose-generation.cpp`: resolves the equipped (or departing) tool's phase clip
  and composes it over the locomotion base, so legs keep moving while the tool
  shoots/reloads/equips/unequips; falls back to the generic body clips when a
  phase is absent.

### 4. Concurrent-session unblock (not my work)
- `live-behavior.cpp` failed to compile because another session's new
  `moveCapsuleStepHeadless` used `HeadlessWorld` unqualified. Qualified it as
  `MimitaNet::HeadlessWorld` (two lines) to let the cold build proceed.

## Validation

- Hot DLL: `python build_game_dll.py` -> `build\mimita-game.dll` success (57
  sources).
- Cold EXE: `python build_agent.py` -> `BUILD SUCCESS`.
- `mimita.exe --live-code-selftest` -> `PASS` (DLL candidate self-test).
- `mimita.exe --hot-combat-selftest` -> all weapon-animation tests PASS:
  `tool.definition` provider resolves; hot definition provides revolver
  gameplay and swordsword behavior; cold weapon registry reflects the hot
  definition; per-tool phase drives distinct arm poses; phase-2 just-shot,
  unequip-on-removal, unequip-return.

## Remaining failures (not weapon animation)

`--hot-combat-selftest` still reports two failures in the concurrent
effect-composition work: `real hit/blood fact reaches the hot effect owner` and
`explosion fact composes flash/smoke/debris`. Those files
(`modules/presentation/effect-composition.cpp`, `hot-effect.h`) are owned by the
other session and were not modified here.

## Evidence classes (separate)

- Source: yes. Hot build: yes. Cold build: yes.
- In-engine tests: weapon animation PASS.
- Live visual proof: no. Multiplayer proof: no. Human acceptance: pending.

## Human review needed

1. Start a match; equip each weapon and confirm per-weapon idle/shoot/reload/
   equip/unequip arms differ and read correctly.
2. Edit a phase keyframe in `tool-visuals.cpp`, save, confirm the change appears
   live in the same PID/session/EntityIds, then roll back.
3. Confirm the hot definition still matches `config/weapons.json` values
   (no balance drift) before trusting a hot gameplay edit.
