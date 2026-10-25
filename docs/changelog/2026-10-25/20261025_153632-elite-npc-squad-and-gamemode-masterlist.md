# Elite NPC squad behavior and gamemode masterlist

- Result: PASS_WITH_HUMAN_REVIEW.
- Timestamp: 2026-10-25 15:36:32 UTC.
- Branch: `main`.
- HEAD observed while finishing: `51f274ba`.
- Latest verified build: `3281` (`build-result` status SUCCESS). `--versioninfo`
  journal: `logs/10-25-2026/20261025_154650/events.jsonl`.
- Note: another concurrent workstream committed a mixed snapshot
  (`51f274ba`, "after colision fixes middle of map editor and npc bahvior edits
  untested") while this session was working. That commit includes part of this
  NPC work. The uncommitted changes listed below are the later refinements from
  this session.
- Skills read: `docs/skills/spec-behavior-review-v1.md`.
- Documents read: `docs/ROUTER.md`, `docs/specs/npc-behavior/npc-behavior.md`,
  `docs/architecture/player-npc-systems/npc-movement.md`,
  `docs/workflows/runtime-scenario-validation.md`,
  `docs/specs/open-gameplay-issues.md` (line 180 handoff),
  `docs/regressions/README.md`, and the relevant `docs/architecture` and
  `docs/operations` build/dev-loop guidance.

## What changed

### Code-owned elite skill tier

- `src/npc/npc-behavior.h/.cpp`: added `skill_tier` plus `turn_speed_deg_per_sec`,
  `aim_lead_seconds`, `prediction_seconds`, and `prediction_error_meters`.
  C++ now owns the complete elite baseline; explicit JSON fields override it.
- `config/behavior-profiles.json`: added `elite_squad` (elite tier, focus/swarm,
  Recast) plus `beginner` and `idle` control profiles.
- `src/game/game-cli.cpp`: the behavior-profile selftest now loads the shipped
  registry and proves elite tier values, focus/swarm, Recast, and near-zero
  prediction error.

### Combat aggression

- `src/npc/npc.cpp`: profile turn speed overrides global facing turn speed;
  behavior prediction and prediction error override the global perception
  values; committed steering now follows the current objective/focus destination;
  a visible legal target is now sufficient to request attack even if the current
  movement state forgot to set the attack flag.
- `src/npc/npc-combat.cpp`: projectiles lead by real travel time plus the
  profile lead; hitscan uses the tightened perception prediction. Added bounded
  `npc.fire-blocked` and `npc.attack-gate` diagnostics that name the exact gate.
- `src/npc/npc-utility.cpp`: a visible enemy now clearly outranks squad-slot
  movement, so elite NPCs shoot instead of sightseeing at a slot.
- `src/npc/npc-navigator.cpp`: a materially changed destination forces a new
  commitment instead of holding a stale direction for the old 10 s window.

### Squad behavior

- `src/npc/team-brain.h/.cpp`: added deterministic `selectFocus` by proximity and
  confidence; implemented cohesion/rally handling, squad focus actor id, and
  swarm-aware slot assignment.
- `src/network/server-gamemode.cpp`: focus now converges on the enemy nearest
  the squad centroid and the recorded focus actor id flows into slots.
- `src/network/server-npcs.cpp`: scored target selection (stickiness/threat/
  distance) now applies in opposite-team TDM instead of legacy nearest-target
  thrashing.

### Navigation

- `src/npc/recast-navigation.cpp`: the 8192-cell Recast bake limit no longer
  fails large maps; cell size/height scale to the world with an explicit warning.
- `config/npc-difficulty.json`: global navigation graph fallback enabled.
- `elite_squad` uses `navigation_backend: recast`; Recast is prewarmed because a
  loaded profile requests it.

### Mode wiring

- `config/roles.json`: added `elite_fighter` (health 100, Source movement,
  standard weapons, `elite_squad`).
- `config/gamemodes/duel.json`, `tdm.json`, `payload.json`: use `elite_squad`
  and the elite fighter role. TDM now has a real NPC role/profile instead of
  falling through to legacy behavior.
- TDM description now matches its 50-kill goal; added `npc_travel` squad hunting.

### Gamemode masterlist and validation

- `config/gamemodes/masterlist/gamemode-masterlist.json`: replaced the
  comment-only placeholder with the full schema/README, defaults, ranges, enums,
  nested team/objective/presentation/avatar/npc sections, and named avatar
  groups.
- `src/gamemode/gamemode.h/.cpp`: strict validation against the masterlist;
  unknown keys, wrong types, invalid ranges/enums, missing required id/name,
  duplicate ids, and filename/id mismatch are rejected. Initial load no longer
  registers a default `duel` after a parse failure. Reload keeps the last good
  mode. Validation errors are written to `events.jsonl` with the exact setting
  path and surfaced through the existing notification path at most once per
  second per file.
- Mode-wide `avatar` and legacy `victory_condition` are now consumed instead of
  silently ignored.
- `devscripts/config_selftest.py`: offline gamemode validation against the same
  masterlist.
- `src/game/game-cli.cpp`: new `--gamemode-config-validate` runtime check.

### Tooling

- `devscripts/dev-launch-modes.json`: added Duel NPC, TDM NPC, and Payload NPC
  launch profiles. TDM now uses `--mode team_deathmatch --gamemode tdm`.

## Evidence

### Build and selftests

- `python devscripts/dev-loop.py --request build --wait-for-build` published
  build `3284` with status SUCCESS.
- `.dev/builds/3284/mimita.exe --npc-behavior-profile-selftest`: PASS, including
  elite tier, focus/swarm, Recast, prediction, and idle control.
- `.dev/builds/3284/mimita.exe --team-brain-selftest`: PASS, including
  `focus_select=ok`.
- `.dev/builds/3284/mimita.exe --npc-utility-selftest`: PASS.
- `.dev/builds/3284/mimita.exe --gamemode-selftest`: PASS (16 modes loaded).
- `.dev/builds/3284/mimita.exe --gamemode-config-validate`: PASS
  (`masterlist=1 loaded=16 errors=0`).
- `.dev/builds/3284/mimita.exe --npc-navigation-selftest`: PASS.
- `python devscripts/config_selftest.py` gamemode subset: 16 modes, 0 gamemode
  errors. The pre-existing avatar-folder checks still report unrelated failures.

### Runtime evidence (older verified build 3261/3262, before the final attack
### fallback)

- Duel run `logs/2026-10-25/20261025_190350/events.jsonl`:
  `npc.roster-profile-resolved` shows actor `200000`, role `elite_fighter`,
  profile `elite_squad`, team 0. The journal shows `FocusTarget` and
  `KillTarget` decisions, `npc.squad-mode-changed`, movement, and visible-target
  transitions. It does not show a fired shot; this run is the reason the
  attack-flag fallback and `npc.attack-gate`/`fire-blocked` diagnostics were
  added.
- TDM run `logs/2026-10-25/20261025_192722_01/events.jsonl`:
  `gamemode.match.configured` shows mode `tdm`, policy `team_roster`, two teams
  of capacity 5. The journal shows ten roster profile resolutions with
  `elite_squad` (human + 4 red NPCs + 5 blue NPCs), squad focus events, and
  client-delivered `SHOT_EVENT_RECV` records proving NPC fire. That run used
  the older map/config and only a short engagement; it is not acceptance of the
  final balance.

### Not yet verified live

- Build `3284` has not been live-played for Duel or TDM because the shared
  dev-loop was running another contributor's mode-30 scenario. The final attack
  fallback, target-stickiness change, and atdm TDM profile are source/build
  verified only.
- The requested Duel benchmark (top-1% roughly 50/50 first-to-20; owner-level
  play around 3 kills to 20) is not measured. It remains a calibration target
  requiring human play.
- Payload squad/objective behavior is wired but Payload is not complete enough
  for acceptance.
- The full masterlist validates all existing mode files while allowing optional
  feature blocks to be omitted. It does not silently add optional blocks such
  as `payload` to unrelated modes, because that would change runtime behavior.

## Findings to carry forward

1. TDM previously had no NPC role/profile (`config/gamemodes/tdm.json` teams
   lacked `role`/`npc_role`, and no mode profile existed), so it could not use
   any squad or aggression profile. Fixed.
2. Group fields such as `cohesion`, `rally_distance_meters`, and focus actor id
   were parsed but unused. Implemented in the shared TeamBrain path.
3. Focus selection used the first max-confidence report rather than the enemy
   nearest the squad. Fixed with deterministic `selectFocus`.
4. Opposite-team targeting bypassed scored stickiness, causing
   `npc.target-changed` thrash with many NPCs. Fixed in source.
5. Recast bakes failed on large maps due to the 8192-cell cap, forcing weaker
   local navigation. Fixed with world-fit cell sizing.
6. A current concurrent map-editor/map-authored-geometry workstream was also
   being edited by another contributor. Its files were preserved; only the
   minimal missing `MapGLBObjectOverride` declaration needed by the shared
   compile was added where necessary. This session does not claim that
   workstream as its own.

## Human review still required

- Play Duel first-to-20 against the elite NPC and record kills/deaths, shots,
  accuracy, time-to-first-shot, and match duration.
- Then play TDM on an authored TDM map and verify squad cohesion, no solo
  wandering, no stuck loops, and no excessive target thrash.
- Confirm the difficulty target and whether elite should be tuned down after
  the first playtest.
- Review the masterlist wording and the optional-vs-required mode-setting
  policy, especially whether every mode file should physically include every
  optional block.
