# Hot C++ movement preset registry (phases 1-3)

- EST timestamp: 2026-09-17 22:30:00 -04:00
- Branch: `8292026stash`
- Result: `PASS_WITH_HUMAN_REVIEW`

## Why

Movement presets were owned by two duplicated 12-field C++ tables plus JSON
side-loaders. `source`/`default` lived in `movement-system.cpp` and
`hot-actor-movement.h` (which disagreed on jump buffer: 0.15 vs 0.20), and role
presets (`heavy`, `retrograd_fast`, `counterstrike`) were parsed from
`config/movement/*.json` and then discarded - `movementProfileId` was never
consumed by physics. This migrates every supported preset into one hot C++
registry and routes all callers through it.

## Changes

### Phase 1 - one hot registry (one authority)

- New `src/hot-reload/hot-movement-presets.h` (added to `hot-modules.json`
  headers):
  - `enum class MovementPresetId { Source, Default, Heavy, RetrogradFast,
    Counterstrike, Count }`.
  - Full `MovementPreset` field set (speeds, accel, friction, stopspeed, air
    projection cap/gain, air strafing/blending, gravity, jump/buffer/coyote/air
    jumps, ground snap/velocity clip, surface friction, landing bleed/retention,
    dash/down-dash/grace/friction, freeze duration/curve, impulse decay/max/mode/
    carry, speed caps/limit/steering, free-fly).
  - `getMovementPreset(id)`, `getActiveMovementPreset()`,
    `movementPresetIdFromName`, `movementPresetIdFromHash`,
    `movementPresetNameExists`.
  - `constexpr MovementPresetId kActiveMovementPreset = Source;` (edit + rebuild
    the DLL to change; the EXE is never relinked for tuning).
- Values are copied from the JSON files with the loader's clamps applied
  (e.g. `source.air_input_mouse_threshold_degrees 0.0 -> 0.1`,
  `default.minimum_camera_yaw_delta_degrees 1110.25 -> 180`). Preset names are
  `source`, `default`, `heavy`, `retrograd_fast`, `counterstrike`; the
  `movement-source.json` file (whose internal name was `cs16`) is now `source`.
- `GameMovementTuningV1` (`hot-movement-policy.h`) extended to carry the full
  preset plus an in `presetId`. `movement.tuning` now fills the requested preset
  from the registry instead of a hardcoded subset.
- Deleted both duplicate `MovementMode`/`kModes`/`kActorModes` tables.
  `hot-actor-movement.h` is now a thin include of the registry.
- Both hot systems pass the preset's real fields to the shared policies (air
  projection cap/gain/surface friction, ground stopspeed/friction by walk mode,
  jump buffer/coyote/air jumps, ground-vs-air dash impulses, freeze
  duration/enabled, speed limit), instead of constants.
- `game-api.h`: `GameMovementRuntimeStateComponentV1` gained `freezeTimerSeconds`
  so the freeze curve actually advances (append-only, one cold ABI build).

### Phase 2 - route every caller

- `movement-conversion.cpp`: `makeMovementConfigForPreset(id)` derives the whole
  cold `MovementConfig` from the registry; `makeCurrentRuntimeMovementConfig()`
  is now the active-preset wrapper. The duplicated `sourceTuningFallback()`
  literal block is deleted; the no-DLL fallback reads the same registry header.
- `RoleMovementCache` resolves names from the registry and no longer reads JSON
  (`pollReload` is a no-op).
- `NpcDifficultyConfig` resolves `movementPreset` from the registry.
- Per-actor overrides are active: the hot actor system and `movement.main` read
  the generic `ActorProfileState` `movementPresetHash` and select that preset,
  falling back to the active preset. `server-gamemode` now lets the mode-level
  preset apply when a role declares none.
- Terminal commands (`movement_presets`/`movement_preset`/`movement_reload`/
  `movement_print`/`movement_velocity`/`movement_air`/`movement_debug`) read the
  registry/`makeCurrentRuntimeMovementConfig()`; the JSON selector writes are
  gone.
- `debug-visuals-scene.cpp` bhop overlay reads the active preset.
- JSON loader (`movement-config.{h,cpp}`) marked comparison-only.

### Phase 3 - pre-activation self-test + comparison harness

- `gameSelfTest` (DLL) now runs `movementPresetSelfTest`: every preset named and
  finite, active id valid, shared ground policy deterministic. A malformed
  registry/policy is rejected before activation, so the previous generation stays
  live.
- New `--movement-preset-selftest`
  (`src/physics/movement/movement-preset-selftest.{h,cpp}`): for each of the five
  presets it loads the JSON reference comparison-only, builds the C++ preset,
  requires every `MovementConfig` field to match, runs an identical 60-tick
  fixed-60Hz air script through the shared step, and requires matching position,
  velocity, and movement events. It also checks the preset-selection API
  (name/hash round-trip, unknown fallback).

### JSONL observability (mirrors collision logging)

- New `src/hot-reload/hot-movement-preset-log.h` (hot, in the manifest headers),
  a copy of the collision-log bridge (`log.event` capability + `GameLogEventV1`):
  - `movement.preset.tuning` when a preset's tuning is requested/changes.
  - `movement.preset.actor` when an actor's selected preset changes, with
    entity/actor identity and `source=active` vs `source=actor-profile`.
  - Both carry the effective values (`groundSpeed`, `airSpeed`, accel, friction,
    `airCap`, `airGain`, gravity, jump, dashes, freeze, ability enables) and are
    change-gated plus a 5s heartbeat so they never spam.
- Wired into `movement-system.cpp` (`movement.main` + `movement.tuning`) and
  `actor-movement-system.cpp` (server/NPC actors).
- `config/debuglogger.json` gained an explicit `movement` category at
  `important` (like `collision`).
- Fixed a real logger bug in `src/debug/structured-log.cpp`: aggregated hot
  events (`capLogEvent`) stored `message`/`reason` in the sample fields, but
  `buildRecord` skips those keys when appending fields, so hot messages were
  silently dropped. `flushBucket` now carries `message`/`reason` onto the emitted
  event. Collision/tool hot messages benefit too.

## Behavior delta (intentional)

- Migrating the JSON values makes previously-ignored tuning effective:
  `source` now has `stop_speed 1.0`, air projection cap `2.0`, air speed-gain
  `2.0`, dash grace `1.0`/friction `0.0`, `landing_overspeed_bleed 0.0`,
  `impulse_carry 0.1`, jump buffer `0.2`, and a global `50 / fixed` speed limit
  (the JSON file's comment says the limit is disabled, but its literal value is
  `speed_limit_enabled:true`; the literal value was migrated for equivalence).
- Per-actor overrides are newly active: `config/roles.json` `hunter`/`skirmisher`
  = `retrograd_fast`, `juggernaut` = `heavy`, and mode-level `counterstrike`.
  This is a deliberate `PASS_WITH_HUMAN_REVIEW` behavior change. Human roles in a
  network match still predict with their own active preset; only NPC actors and
  the local listen-server actor read `ActorProfileState` today.

## Evidence

- Cold build: `python build_agent.py` -> `Status: SUCCESS` (final
  `mimita-20260917T222053.exe`).
- Hot DLL: `python build_game_dll.py` -> `DLL build success: build\mimita-game.dll`.
- `mimita.exe --movement-preset-selftest` -> `PASS`. All 5 presets:
  `[ok] ... JSON reference loaded`, zero field `[FAIL]` lines, and
  `scripted 60-tick parity posDev=0.000000 velDev=0.000000`; selection API
  round-trips ok; `[ok] movement preset log reached events.jsonl`,
  `[ok] hot movement.main emitted movement.preset.actor to events.jsonl`,
  `[ok] hot movement.tuning emitted movement.preset.tuning to events.jsonl`.
- Runtime JSONL proof from the last run (`logs/2026-09-18/20260918_025444/events.jsonl`):
  - `{"category":"MOVEMENT","event":"movement.preset.actor",... "source=active ... walkMode=2 groundSpeed=20.00 ... airCap=2.00 airGain=2.00 gravity=40.","actor_type":"player","result":"source"}`
  - `{"category":"MOVEMENT","event":"movement.preset.tuning",... "source=tuning-request ...","result":"source"}`
  - `{"category":"MOVEMENT","event":"movement.preset.selftest","message":"registry active=source presets=5 json-vs-c++ parity all match","result":"ok"}`
- `mimita.exe --movement-selftest` -> `PASS`.
- `mimita.exe --movement-parity-selftest` -> `PASS`.
- `mimita.exe --air-movement-parity-selftest` -> `PASS` (test updated to start
  below the real source projection cap and to use the active preset rather than
  hardcoded `airMaxWishspeed=0`).
- `mimita.exe --movement-algorithm-selftest`, `--generic-integrator-selftest`,
  `--npc-actor-state-selftest`, `--counterstrike-selftest`,
  `--gamemode-hot-selftest`, `--live-code-selftest` -> `PASS`.
- Search: the only `MovementJsonConfig` references now are the loader itself, its
  header, a comment, and the comparison harness.

## Not done (JSON removal gates)

Phase 4 is intentionally deferred until the removal gates pass. `config/movement.json`
and `config/movement/*.json` are still present and still read by the comparison
harness only. No active runtime movement path reads them.

## Human verification still required

- Run the final EXE and confirm `source` still feels right (and decide whether the
  migrated `speed_limit_enabled:true / 50 / fixed` should stay or be disabled; it
  can be flipped in `hot-movement-presets.h` with a DLL rebuild).
- Confirm NPC role movement (`heavy` juggernaut, `retrograd_fast` hunter) is the
  intended live change.
- Live hot-edit proof: edit `hot-movement-presets.h`, save, confirm the DLL
  candidate activates at a tick boundary with the same running game, and that an
  intentionally-broken edit keeps the previous generation active.

## Notes

- `game-api.h` changed (new component field), so one cold EXE build was required;
  `mimita.exe` is otherwise never relinked for movement tuning.
- Pre-existing unrelated working-tree changes (analytics/accounts configs,
  collision package files, docs/gold) were left untouched.
