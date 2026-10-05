# Counter-Strike hit feedback overrides

Time created: 2026-10-05T16:47:43Z
Display time: 2026-10-05 12:47:43 EDT (America/New_York)
Branch: `afad20a-rebuild`

## Result

Added Counter-Strike actor-preset overrides that disable both local hit-marker
visuals and hit-marker sounds. The existing actor-preset presentation loader
and central runtime gates already owned these behaviors, so no new runtime
owner was added.

## Changes

`config/actor-presets/counter_strike.json:34-35`

Old:

```json
"world_impact_effects": false,
"blood_effects": true,
```

New:

```json
"world_impact_effects": false,
"hit_markers": false,
"hit_sounds": false,
"blood_effects": true,
```

`src/game/game-cli.cpp:310-313,323-328`

The existing `--actor-preset-selftest` now requires and reports both parsed
flags: `hasHitMarkers && !hitMarkers` and `hasHitSounds && !hitSounds`.

## Authority and code path

- `src/gamemode/match-roles.cpp:131-132` parses the actor-preset fields.
- `src/combat/actor-preset-weapons.cpp:74-75` resolves the active preset
  policy and exposes the runtime gates.
- `src/ui/hitmarker.cpp:39` suppresses the visual marker.
- `src/audio/hitmarker-audio.cpp` uses the same actor-preset sound policy.
- `src/network/confirmed-damage-presentation.cpp:124-136` gates confirmed
  network hit markers and sounds.
- `config/weapon_hitfx.json` remains the shared/per-weapon policy and is not
  changed; the actor preset now overrides it through the existing AND gates.

## Documents and focused review

Read and followed:

- `docs/ROUTER.md`
- `docs/features/gamemodes/counterstrike.md`
- `docs/specs/20261002plan.md`
- `docs/architecture/json-configuration/json-configuration.md`
- `docs/skills/spec-behavior-review-v1.md`
- `docs/operations/build-and-exe/build-and-exe.md`
- `docs/operations/task-completion/task-completion.md`

## Validation

- `python build_agent.py`: `BUILD SUCCESS`; compiled `src/game/game-cli.cpp`.
- `mimita.exe --actor-preset-selftest`: loaded the preset and reported
  `hit_markers=0 hit_sounds=0`, but overall `FAIL` remains due to the
  pre-existing unrelated revolver assertion expecting damage `100` while the
  current preset contains `35`.
- `mimita.exe --cs-round-selftest`: `PASS`.
- `git diff --check`: no new whitespace errors; existing unrelated trailing
  whitespace warnings remain in `config/behavior-profiles.json`.

## Existing work preserved

The working tree contained unrelated edits before this session, including
changes in `counter_strike.json` and `game-cli.cpp`, plus many other files.
Those changes were preserved and not attributed to this task.

## Human review

No live Counter-Strike playtest was performed. Human review should confirm
that shooting an enemy in Counter-Strike produces neither a hit-marker visual
nor a hit-marker sound; the built/runtime self-test evidence confirms the two
parsed override values and the Counter-Strike self-test remains green.
