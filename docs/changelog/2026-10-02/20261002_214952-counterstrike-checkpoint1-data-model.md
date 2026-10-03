# Counter-Strike Checkpoint 1 — data model, roles, actor preset, team commands

Date: 2026-10-02
EST timestamp: 2026-10-02 21:49:52 EST
Branch: `afad20a-rebuild`

## Result

`PASS_WITH_HUMAN_REVIEW`

Source and build are proven. Runtime acceptance (visual/gameplay) is NOT
performed and is still required.

## Scope

Checkpoint 1 of the Counter-Strike plan (`docs/specs/20261002plan.md`): the
data model, roles, actor-preset reference, and team commands. No round
lifecycle, objectives, or AI changes yet.

## Pre-existing edits (not mine)

Working tree already had modifications before this session:
`config/accounts/default.json`, `config/analytics.json`, `config/camconfig.json`,
`shaders/post.frag`, `src/config/camera-config.cpp/.h`,
`src/engine/engine-tick-setup.cpp`, `src/render/post-fx.cpp/.h`, and untracked
`docs/changelog/2026-10-02/20261002_211037-camera-lens-distortion.md`,
`docs/changelog/2026-10-02/20261002_211936-bodycam-lens-reference.md`,
`docs/specs/20261002plan.md`. These were left untouched.

## Files changed

### `src/gamemode/gamemode.h`

Added data structures before `struct Gamemode`:
- `GamemodeTeam { id, displayName, capacity, role, spawnGroup }`
- `GamemodeSpawnGroup { id, team, points }`
- `GamemodeObjectiveDefinition { id, kind, carrierTeam, siteGroup, plantSeconds, defuseSeconds, explosionSeconds }`
- `GamemodeRounds { maxRounds, roundsToWin, roundSeconds, freezeSeconds, countdownSeconds, intermissionSeconds, resultsSeconds }`
- `GamemodePresentation { has*/value flags for damageNumbers, hitEffects, worldImpactEffects, hitMarkers, hitSounds, blood, killfeed, ragdolls, enemyHealthbars }`

Added fields to `Gamemode` (after `maps`):
`teams`, `spawnGroups`, `objectives`, `rounds`, `presentation`, `victoryCondition`.

### `src/gamemode/gamemode.cpp`

In `loadFile` (after `next.maps = ...`), added optional parsing:
- `teams` array -> `GamemodeTeam` list; when non-empty, rebuilds `next.teamNames`
  from `display_name` for backward compatibility.
- `spawn_groups` object -> `GamemodeSpawnGroup` list.
- `objectives` array -> `GamemodeObjectiveDefinition` list.
- `rounds` object -> `GamemodeRounds`.
- `victory.type` -> `victoryCondition`.
- `presentation` object -> `GamemodePresentation` flags.

Extended the load log line to include `teams=`, `rounds_to_win=`, `objectives=`.

### `src/gamemode/match-roles.h`

`MatchRoleDefinition` gained: `actorPresetId`, `teamId`, `spawnGroup`,
`objectivePermissions` (plus doc comments).

### `src/gamemode/match-roles.cpp`

`readRole` now parses `actor_preset`, `team_id`, `spawn_group`,
`avatar_forced`, and `objective_permissions`.

### `config/gamemodes/counterstrike.json`

Rewritten: ordered `teams` (ct="Counter-Terrorists", t="Terrorists", capacity 5,
roles, spawn groups), `rounds` (max 15, win 8, round 115s, freeze 0, countdown 3,
intermission 30, results 5), `spawn_groups`, one `bomb` objective, `victory.type
= rounds`, `actor_preset = counter_strike`, `hide_healthbars/ragdoll_enabled/
blood_enabled = true`, a `presentation` block (damage numbers/hit effects/world
impacts/hit markers/hit sounds off; killfeed on), map `dust2cyberiav3`.

### `config/roles.json`

Added `counter_terrorist` (team 0, team_id ct, actor_preset counter_strike,
avatar jason forced, defuse permission) and `terrorist` (team 1, team_id t,
actor_preset counter_strike, avatar abusiveboy forced, plant/pickup permissions).

### `src/terminal/debug-commands.cpp`

Replaced `teamlist`/`teampick` registration with `team_list`/`team_pick` as
primary names carrying `aliases {"teamlist"}` / `{"teampick"}` in the
`ConsoleCommand` aggregate, category `CommandCategory::Duel`.

### `src/network/server-packet-chat.cpp`

`handleServerCommand` now matches both `team_list`/`teamlist` and
`team_pick`/`teampick`.

### `src/network/server-gamemode.cpp`

`serverRequestTeamChange`: rejects when `phase` is not WAITING/INTERMISSION/
RESULTS ("team selection locked"), and enforces ordered-team `capacity` (when
declared) with "team is full".

### `src/game/game-cli.cpp`

Added a focused `--gamemode-selftest` harness (plus `gamemode/gamemode.h`
include) proving the new schema parses at runtime.

### `docs/features/gamemodes/counterstrike.md`

New stable feature record with desired behavior, decisions (including the
weapon-values `NEEDS_SPEC_DECISION`), ownership, and an append-only attempt log.

## Reasoning

The plan requires a generalized, JSON-controlled data model. All new gamemode
keys are optional so other modes are unaffected; `teams` intentionally
repopulates `teamNames` so existing HUD/team code keeps working with no
CS-only branch. `team_list`/`team_pick` are added as primary names with the old
names kept as aliases, per the human decision, so nothing breaks.

Presentation keys were deliberately limited to genuinely new flags; healthbars/
blood/ragdolls remain owned by the existing `hide_healthbars`/`blood_enabled`/
`ragdoll_enabled` keys to avoid duplicate sources of truth.

## Documents and skills

- Spec: `docs/specs/20261002plan.md` (Stages 1, 2 commands; Checkpoint 1).
- Router: `docs/ROUTER.md`, `AGENTS.md`.
- Skill: `docs/skills/spec-behavior-review-v1.md` — no blocker findings; one
  `NEEDS_SPEC_DECISION` recorded (weapon values).
- Skill: `docs/skills/terminal-command-checker-v1.md` — commands registered with
  name/usage/description/category, validated input, clear failure messages,
  aliases retained.

## Validation

Build evidence (`python build.py build-only`):

```text
[LINK] mimita.exe
 BUILD SUCCESS
Compiled: 1
Skipped : 507
Time    : 11.16 sec
```

Runtime schema evidence (`mimita.exe --gamemode-selftest`):

```text
[GAMEMODE SELFTEST] loaded=10
[GAMEMODE SELFTEST] team[0] id=ct display=Counter-Terrorists capacity=5 role=counter_terrorist spawn=ct_spawn
[GAMEMODE SELFTEST] team[1] id=t display=Terrorists capacity=5 role=terrorist spawn=t_spawn
[GAMEMODE SELFTEST] rounds_to_win=8 round_seconds=115 countdown=3
[GAMEMODE SELFTEST] victory=rounds objectives=1 spawn_groups=2
[GAMEMODE SELFTEST] role counter_terrorist=found actor_preset=counter_strike avatar=jason team_id=ct
[GAMEMODE SELFTEST] role terrorist=found actor_preset=counter_strike avatar=abusiveboy team_id=t
[GAMEMODE SELFTEST] PASS
```

Actor-preset evidence (`mimita.exe --actor-preset-selftest`): PASS.

Pre-existing, unrelated: `python devscripts/config_selftest.py` fails on avatar
asset contracts and a `weapons.json` comment entry; these were failing before
this change and were not touched.

## Human review still needed

- Run `start counterstrike`, `team_list`, `team_pick 1` in a live session and
  confirm the team assignment message and ordering.
- Confirm `teamlist`/`teampick` aliases still work.

## Explicitly not done yet

Round lifecycle, 5v5 roster creation, objectives/bomb, AI, grenades, and the
remaining checkpoints. CS currently still routes through the legacy lifecycle.
