# Fun Counter Strike, grenade configuration, and Deathball contracts

Time created: 2026-10-31T14:15:31Z
Time last updated: 2026-11-01T01:15:00Z

Status: IMPLEMENTED; RUNTIME EVIDENCE PARTIAL; HUMAN VISUAL REVIEW PENDING

## Goal

Implement the requested `funcounterstrike` respawning bomb mode on the shared
actor, weapon, damage, networking, NPC, and map-authoring systems; move grenade
policy into per-weapon configuration; and update the Deathball contract without
accepting configured radius as proof of world size.

## Changed

### Docs and routing

- Added `docs/specs/gamemodes/funcounterstrike.md`.
- Routed it in `docs/ROUTER.md`.
- Added a scope note to `docs/specs/gamemodes/counterstrike.md` keeping the
  one-life tactical rules distinct.
- Updated `docs/specs/weapons/deathball.md` (999-round clip, 1 s fire delay,
  1 s reload, runtime scale-measurement contract).
- Updated `docs/specs/weapons/weapons.md` with the shared `grenade_effect`
  schema and retirement of `config/grenades.json`.
- Updated `docs/specs/entity-editor/entity-editor.md` with bomb-site authoring.

### Mode configuration

- `config/gamemodes/funcounterstrike.json`:
  - 90-second unplanted round timer (follow-up; first pass used 60).
  - 3-second countdown and 3-second result break.
  - Endless rounds by default; match length configurable through
    `rounds_to_win` / `max_rounds`.
  - Non-zero respawn seconds for instant respawn during active rounds.
  - CT/T/spec teams and bomb objective.
  - `features.objective_only_round_wins: true`.
  - Dedicated TDM-plus-grenades loadout via the new `fun_counter_terrorist` /
    `fun_terrorist` roles (weapon set `funcounterstrike`).
- `config/weaponsets.json`: added the `all_weapons` key to set 5.
- `config/onlinemodes.json`: updated the Fun Counter Strike description and set
  its score limit to 0 (endless).
- `src/gamemode/gamemode.{h,cpp}` and the gamemode masterlist:
  - Added `features.objective_only_round_wins`.
- `src/network/server-gamemode.cpp`:
  - The elimination-round branch is skipped when the feature is enabled.
  - Objective carrier logging now records `bomb_item_id` and
    `bomb_item_equippable`.

### Grenades

- Added `WeaponGrenadeEffect` to `WeaponDefinition` and a shared top-level
  `grenade_effect` schema to the weapon masterlist and JSON parser.
- Migrated frag/smoke/fire/darkbang values into their per-weapon configs.
- Deleted the competing `config/grenades.json` policy file.
- `GrenadeRegistry` now publishes definitions from the weapon registry and
  reloads with the weapon snapshot; startup registration calls it.
- Added frag center/edge damage interpolation:
  - `splashEdgeDamage` support for server projectiles and local persistent
    physics.
  - Frag defaults to 1000 center / 25 edge.
- Smoke defaults to 20 m diameter, 10 s lifetime, outer 5% fade band, and a
  client countdown label.
- Fire defaults to 10 m diameter, 10 damage every 5 fixed ticks, 7 s lifetime,
  with a client fire cylinder + label.
- Darkbang applies exposure-scaled intensity/duration, plays a generated
  brown-noise asset, and logs exposure.
- Added `grenade.smoke.spawn`, `grenade.fire.spawn`,
  `grenade.darkbang.exposure`, `grenade.effect.damage`, and
  `grenade.effect.expired` events.
- Wired the NPC early-use grenade policy with self-blast, teammate-blast,
  ammo/reload, range, and utility-score checks. NPCs log
  `npc.grenade-decision`.

### Deathball

- `config/weapons/deathball/config.json`: magazine 999, fire delay 1.0 s,
  reload 1.0 s.
- Added `deathball.config.resolve`, `deathball.local_scale`, and
  `deathball.scale.first-divergence` logging.
- Local/offline deathball contact tests now use the weapon projectile radius
  instead of a separate hardcoded player-sized threshold.
- Configured radius remains 5.0 (10 m diameter); it was not retuned because the
  runtime measurement contract requires visual/collision proof first.

### Map editor

- Added bomb sites as an editor-selectable object kind.
- Added `MapConfigRegistry::createBombSite`, `deleteBombSite`, and
  `setSiteRadius`; site position/visibility edits now mark the authored data
  dirty.
- Added `mapeditor add bombsite [id] [radius]`, outliner listing, selection,
  nudge/set-position, delete, and `select_kind bombsite`.
- Added `devscripts/dev-launch-modes.json` mode 45 for Fun Counter Strike on
  `dust2cyberiav4`.

### Assets

- Added `assets/sound/weapon/darkbang/brownnoise.wav` for the Darkbang audio
  hook.

## Validation

### Source/config

- Config selftest: weapons, gamemodes, roles/weaponsets, and the new schema
  validate. The run still exits 1 only on the pre-existing avatar-schema
  findings (`assets/avatars/...`), which are unrelated to this change.
- Build: dev-loop build `4008` succeeded.
- Exact executable: `.dev/builds/4008/mimita.exe`.
- `--versioninfo`:
  `EVENTS_JSONL_PATH=logs/10-31-2026/20261031_141243/events.jsonl`.
- `--grenade-selftest`: PASS (4 definitions).
- `--area-effect-selftest`: PASS.

### Runtime journal

Run `20261031_181249`, build `4008`, mode `funcounterstrike`, map
`dust2cyberiav4`:

- `round.start` -> `round.go` -> `round.active` with
  `round_end_tick = 4681` for round 1.
- Round 1 ended by timeout (`reason: 3`, Counter-Terrorists win); round 2
  started with a new synchronized countdown.
- `objective.dropped` and `objective.pickup` show the bomb dropping and being
  recovered by Terrorists.
- Death/respawn presentation reset events occur during active play, confirming
  instant respawn is active.
- NPC grenade decisions include smoke (radius 10 m) and fire (radius 5 m)
  throws, plus a rejected throw with `friendly_near: 1` and score `-1`.
- `grenade.smoke.spawn` recorded 20 m diameter / 10 s.
- `grenade.fire.spawn` recorded 10 m diameter / 10 damage per 5 ticks / 7 s.
- `grenade.effect.damage` recorded 10 damage on the configured cadence.
- `grenade.effect.expired` recorded the configured fire lifetime.
- `grenade.darkbang.exposure` recorded full exposure at 4.08 m and partial
  exposure (0.429) at 9.24 m.
- `deathball.config.resolve` recorded collision radius 5.0, visual radius 5.0,
  visual diameter 10.0, magazine 999, fire delay 1.0 s, reload 1.0 s, and the
  3.6 m player capsule reference.

### Not observed in this bounded runtime window

- A successful plant, defuse, or detonation.
- A Deathball contact trace.
- The reported player-sized Deathball appearance. The runtime data shows a 10 m
  rendered diameter, so this remains a human visual-measurement item rather
  than a tuning change.

### Human acceptance still required

- Smoke boundaries, fade band, and countdown readability.
- Darkbang screen blackout and brown-noise recovery.
- Fire/frag readability and sustained exposure feel.
- Deathball measured visual size and knockback.
- Map-editor bomb-site authoring flow.
- Multiplayer presentation.

## Follow-up: TDM-plus-grenades loadout, team spawns, and objective HUD

### Changed

- Added dedicated weapon set `30` key `funcounterstrike`: the TDM set plus
  frag, smoke, fire, darkbang, bomb, and nothing. TDM set `29` is unchanged.
- Pointed `fun_counter_terrorist` and `fun_terrorist` at the new set.
- Changed `funcounterstrike.spawn_strategy` from `shared` to `team_shared`.
- Limited the mode map pool to `dust2cyberiav4`, the current shipped map with
  authored bomb sites and explicit team spawns.
- Added authored `ct_spawnpoint` and `t_spawnpoint` entities to
  `dust2cyberiav4.json`.
- Set the active round timer to 90 seconds, editable in gamemode JSON.
- Added a dedicated `funcounterstrike` HUD layout with phase, score, round,
  round/bomb timer, objective state, team hint, prompt, result, and countdown.
- Added generic HUD behavior for:
  - phase label,
  - team-local objective hint,
  - `BOMB CARRIED`,
  - round timer replaced by bomb timer when planted,
  - `BOMB DEFUSED`, `BOMB EXPLODED`, and `TIME EXPIRED` results.
- Added `phaseText`, `objectiveHint`, and `objectiveResultText` to the
  Counter-Strike layout as well.
- Made gamemode objective timers authoritative, with map `bomb` timers as
  fallback only. Added `objective.timers.resolved` effective-value logging.
- Added `docs/specs/gamemodes/objective-hud.md` and routed it.
- Updated the gamemode index and Fun Counter Strike spec.

### Validation

- Build: dev-loop build `0001` succeeded.
- Exact executable: `.dev/builds/0001/mimita.exe`.
- `--versioninfo`:
  `EVENTS_JSONL_PATH=logs/10-31-2026/20261031_210853/events.jsonl`.
- `--grenade-selftest`: PASS.
- `--area-effect-selftest`: PASS.
- Runtime run `20261101_010944`, mode `funcounterstrike`, map
  `dust2cyberiav4`:
  - loadout resolved as set `30` with 13 weapons including all four grenades
    and the bomb;
  - CT spawns resolved through `authored-team-group` at the CT authored point;
  - T spawns resolved through `authored-team-group` at the T authored point;
  - `objective.timers.resolved` reported plant/defuse/explosion from `mode`
    values 3/5/40 seconds;
  - round 1 became active with `round_end_tick` equal to roughly 90 seconds of
    fixed ticks;
  - no missing Fun Counter Strike GUI layout error was emitted.

### Still required

- Human visual review of the new HUD and countdown presentation.
- Runtime confirmation of plant, defuse, and explosion outcomes with the new
  HUD.
- Map-editor create/move/save/reload confirmation in a live session.

## Notes and pre-existing findings

- `config/weapons/masterlist/weapon-masterlist.json` requires every definition
  to carry every top-level field; the new `grenade_effect` field is therefore
  present as `null` on non-grenade weapons.
- `config_selftest.py` still reports unrelated avatar-schema failures and the
  consolidated Counter-Strike acceptance selftest still reports the unrelated
  pre-existing `spawn-tags` failure.
- The two Fun Counter Strike roles were necessary because NPCs use role/loadout
  resolution rather than the mode's community weapon-set id.
