# Fun Counter-Strike team read: teammate healthbar, no enemy healthbar, outlines off

Written 2026-11-27 by `rocket-league-input-fix` for
`issue-e873cbbdbeeb` (context `ctx-7386cfc693ef`).

## Goal

Honor the user's stated team-read preference: "teammate = healthbar, enemy = no
healthbar", and stop using red/green outlines as the primary team read.

## Observed target

Live run `20261127_174216` (client pid 32452, server pid 23116) was **Fun
Counter Strike** on `dust2cyberiav4`. Its client journal carried active
`player_visuals.outline_pose` traffic and a
`client.gamemode.presentation.applied` record with
`friendly_healthbars=false`, `enemy_healthbars=false`, `player_outlines=true`,
`friendly_outlines=true` — exactly the red/green-outline, no-teammate-healthbar
state the user described.

## Exact changes

| File | Before | After | Why |
|---|---|---|---|
| `config/gamemodes/funcounterstrike.json` | `enemy_healthbars=false`, `friendly_healthbars=false`, `player_outlines=true`, `friendly_outlines=true` | `enemy_healthbars=false`, `friendly_healthbars=true`, `player_outlines=false`, `friendly_outlines=false` | Teammates get a healthbar; enemies get none; red/green outlines are removed. |
| `docs/specs/gamemodes/funcounterstrike.md` | no team-read statement | new "Team readability" subsection | Record the policy so outlines are not reintroduced. |

`tdm.json` already matched (`friendly_healthbars=true`, `enemy_healthbars=false`,
`player_outlines=false`) and was left untouched. `bloodtest.json` already has
all four flags off.

## Evidence separation

- Source/config: the diff above. Per-mode JSON, hot-reloadable through
  `GamemodeRegistry::pollReload()`; no compile or DLL build is required.
- Validation: `config/gamemodes/funcounterstrike.json` parses (comments
  stripped); `gamemode_json_sync.py --check` reports the same generic
  field-order/missing-setting drift every mode already has, no new issue from
  this edit.
- Runtime: **not yet proven.** The accepted event is
  `client.gamemode.presentation.applied`
  (`src/network/community-match-client.cpp:1018-1072`), which already fingerprints
  and emits `friendly_healthbars`, `enemy_healthbars`, `player_outlines`, and
  `friendly_outlines`; after a healthy Fun Counter Strike run it should read
  `true/false/false/false.` Visual/screenshot acceptance and `[HEALTHBAR]`
  render/cull rows are still pending. No run was available because the dev loop
  was in `game_build_failed` on an unrelated changed-source preflight
  (`src/engine/engine-tick-camera.cpp`).

Proof task `team-read-policy-fcs` remains unverified.
