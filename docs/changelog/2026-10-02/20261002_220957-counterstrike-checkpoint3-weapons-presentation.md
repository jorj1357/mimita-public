# Counter-Strike Checkpoint 3 — preset application, weapon overrides, presentation

Date: 2026-10-02
EST timestamp: 2026-10-02 22:09:57 EST
Branch: `afad20a-rebuild`

## Result

`PASS_WITH_HUMAN_REVIEW`

Source, build, and runtime-table evidence are proven. Live gameplay and visual
acceptance (hitmarker absence, blood, loadout) are NOT performed and remain
required.

## Scope

Checkpoint 3 of `docs/specs/20261002plan.md`: apply the Counter-Strike actor and
presentation presets, and add actor-preset weapon overrides (Stages 4, 5).

## Pre-existing edits (not mine)

Same camera-lens work as Checkpoints 1-2. Untouched.

## Files changed

### `src/gamemode/match-roles.h`

`ActorPresetWeaponOverride` gained `hasDamageScale`/`damageScale`,
`hasHeadshotMultiplier`/`headshotMultiplier`, `hasSpread`/`spread`,
`hasRecoil`/`recoil`. `ActorPresetPresentation` gained
`hasHitMarkers`/`hitMarkers`, `hasHitSounds`/`hitSounds`.

### `src/gamemode/match-roles.cpp`

`readWeaponOverride` parses the new fields and a per-weapon `presentation`
block. `readPresentation` parses `hit_markers`/`hit_sounds`.

### `src/combat/actor-preset-weapons.cpp` / `.h`

`applyOverride` applies damage scale, headshot multiplier, spread, and recoil.
Added `gHitMarkersEnabled`/`gHitSoundsEnabled` globals, set them in `apply`,
reset them in `clear`, and exposed `hitMarkersEnabled()`/`hitSoundsEnabled()`.

### `src/ui/hitmarker.cpp`

`hitmarkerVisualOnly` now early-returns when
`ActorPresetWeapons::hitMarkersEnabled()` is false.

### `src/audio/hitmarker-audio.cpp`

`playHitmarkerSound` now early-returns when
`ActorPresetWeapons::hitSoundsEnabled()` is false.

### `src/network/confirmed-damage-presentation.cpp`

Confirmed hitmarker/hit-sound presentation now also requires the actor-preset
globals.

### `src/network/server-npcs.cpp`

`adoptNewServerNpcs` now resolves and applies the role spawn profile (health,
movement preset, avatar, behavior profile, loadout) so roster NPCs get their
intended first life. Shared path; no Counter-Strike-only branch.

### `config/actor-presets/counter_strike.json`

- Kept revolver 100 dmg / 6 / 36 and shotgun 20 / 8 / 32.
- Added revolver `recoil: 0.0`; zeroed shotgun `beam_thickness` (was 2.0).
- Added `hitscan_rifle`: damage 30, damage_scale 1.0, headshot_multiplier 4.0,
  fire_delay 0.1, reload_time 2.4, magazine_size 30, reserve_ammo 180,
  spread 0.0, recoil 0.0, zero beam/world thickness.
- Presentation: kept damage numbers / hit effects / world impacts off; enabled
  `blood_effects` (was false) so it matches the Counter-Strike acceptance list.

### `src/game/game-cli.cpp` and `src/network/server-gamemode.cpp`

Extended `--actor-preset-selftest` to assert the new overrides and zero
thickness. Extended `serverCounterStrikeRoundSelfTest` to apply the preset and
verify the overrides reach the runtime ACTIVE weapon table while the base table
stays intact (loads builtin weapons + roles/presets first).

### `docs/features/gamemodes/counterstrike.md`

Appended Attempt 3.

## Reasoning

The actor preset remains the single presentation owner for Counter-Strike, so
hitmarker and hit-sound policy is expressed there and consumed at the two
central local functions plus the confirmed-damage path. The gamemode-level
`presentation` block is deliberately not applied to avoid a duplicate source of
truth. Overrides stay in memory; `config/weapons.json` is never written.

## Documents and skills

- Spec: `docs/specs/20261002plan.md` (Stages 4, 5; Checkpoint 3).
- Skill: `docs/skills/spec-behavior-review-v1.md` — one `NEEDS_SPEC_DECISION`
  (unused gamemode `presentation` block); no blocker.

## Validation

Build: `BUILD SUCCESS` (`Compiled: 1`).

Runtime (`mimita.exe`):

```text
[ACTOR PRESET SELFTEST] counter_strike=found revolver=100/6/36 shotgun=20/8/32 rifle=30/30/180 hs=4 thick=0.0/0.0
[ACTOR PRESET SELFTEST] PASS

[CS ROUND SELFTEST] rifle mag=30 reserve=180 beam=0.000000
[CS ROUND SELFTEST] PASS

[GAMEMODE SELFTEST] PASS
```

`git status --short config/weapons.json` is empty (unchanged).

## Known limitations / follow-ups

- The local human hitscan trace hard-caps range at 100 and ignores
  `customParams["range"]`; zero thickness is correct but a preset `range`
  override only reaches the authoritative trace.
- Hitmarker/hit-sound gating is an AND of the actor preset and per-weapon
  `config/weapon-hitfx.json`.

## Human review still needed

- In a live CS session confirm: revolver 6/36, shotgun 8/32, rifle 30/180,
  no damage numbers, no hitmarkers/hit sounds, blood visible, ragdolls and
  killfeed on, FOV 70, forced first-person, heavy movement.
- Confirm roster NPCs spawn with the preset loadout on their first life.
- Confirm leaving Counter-Strike restores ordinary presentation.
