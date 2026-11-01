# Versus Jason horror mode

Time created: 2026-11-01T15:13:23Z

Status: IMPLEMENTED IN SOURCE; FINAL BUILD 105 SUCCEEDED; RUNTIME JOURNAL
CONFIRMS MODE, JASON STATS, THREE LIVES, SCRIPTED AFTERMATH, 99 SOUND TICKS,
FATAL WINDOW, AND SERVER INTERMISSION; PRIVATE-PASSWORD, WIN LOOP, TIMEOUT
LOSS, MULTIPLAYER LAST-SURVIVOR, AND VISUAL ACCEPTANCE STILL PENDING

Branch: `20261011zt`
Base commit: `f757b936 disable destructible map world by default and document re-enable path`
Pre-existing edits: the worktree already contained many modified and untracked
files from earlier sessions. This session did not revert or claim them.

## Goal

Add a repeatable, room-hosted Versus Jason mode using the shared match, room,
NPC, lives, results, and replication systems. Keep it out of player-facing
documentation.

## Source and configuration changes

### Mode and content

- Added `config/gamemodes/vs_jason.json`:
  - `survivors` (human) versus `jason` (one NPC),
  - 300-second timer starting at GO,
  - `last_team_standing`,
  - three survivor lives,
  - three-second results,
  - data-driven aftermath block.
- Added `vs_jason_survivor` and `vs_jason_jason` roles in `config/roles.json`.
  Jason has `6666` HP, forced `jason` avatar, `vs_jason_chainsaw` loadout,
  `vs_jason_chase` behavior, and `vs_jason` movement.
- Added `vs_jason_chainsaw` role-only weapon set in `config/weaponsets.json`.
- Added `vs_jason_chase` profile in `config/behavior-profiles.json`.
- Added `config/movement/movement-vs-jason.json` with higher ground/max speed.
- Added `vs_jason` to `config/onlinemodes.json` with `private_by_default: true`.
- Added the mode to `docs/specs/gamemodes/vs-jason.md` and linked it from
  `docs/specs/gamemodes/gamemodes.md`.

### Masterlist and schema

`config/gamemodes/masterlist/gamemode-masterlist.json` now defines:

- `lives_enabled`,
- `lives_team`,
- `time_limit_starts_at_go`,
- `aftermath` (`enabled`, `target_team`, `scare_ticks`, `final_damage`,
  `move_speed`, `approach_distance_meters`, `scare_sounds`, `blackout`,
  `hide_hud`, `mute_audio`, `freeze_local_player`, `fatal_window`),
- `presentation.room_code_color`.

`python devscripts/gamemode_json_sync.py --check` passes. `melee.json` now sets
`lives_enabled: true` so its existing stocks use the generalized lives owner.

### Shared lives

- Replaced the melee-only stock map with `ServerGamemodeState::actorLives` and
  the generic helpers `serverActorHasLives`, `serverActorConsumeLife`,
  `serverActorNoteSpawn`, and `serverActorLivesApplyToTeam`.
- `serverPlayerRespawnsEnabled` and `updateActorStates` now consume and honor
  lives for any matching actor. A one-tick death between unlimited respawns is
  not treated as elimination.
- `reconcileTeamRosterNpc` now uses the role spawn profile health instead of
  hardcoded `3500`, so Jason receives `6666`.

### Timer, win, and loss

- `time_limit_starts_at_go` sets `matchStartTick` and `matchTimeLimitTick` at the
  GO transition; ACTIVE no longer overwrites them for that mode.
- Added `gamemode.time_limit.start` journal evidence.
- `last_team_standing` and the time-limit check trigger the aftermath when the
  configured loss team cannot continue. Killing Jason keeps the standard
  three-second results -> shared intermission/countdown flow.

### Aftermath server owner

- Added `DUEL_PHASE_AFTERMATH` and `ServerGamemodeState` aftermath state.
- `beginAftermath` freezes survivors at 1 HP, disables their movement/input,
  and creates one scripted approach effect per human survivor.
- The approach starts at the configured distance and moves at the configured
  speed, ignoring collision and AI. Each targeted client receives its own
  `DuelStatePacket` mover position.
- Final damage is applied through `applyActorDamage` with Jason as the NPC
  attacker and `chainsaw` as the weapon. Run evidence applied `999999`.
- The server emits exactly 99 scare ticks, increments a sound serial each tick,
  then broadcasts the done stage and returns the room to intermission.
- NPC simulation pauses during aftermath. Real Jason remains frozen.
- Added `src/network/network-weapons.h` include and fixed the aftermath kill
  state assignment.

### Replication and client presentation

- Protocol version 47 -> 48.
- `DuelStatePacket` now carries aftermath phase/stage, target actor, mover
  actor, mover position, sound serial/index, and presentation flags.
- `CommunityMatchClient` stores and resets the per-recipient aftermath state.
- Client input and camera freeze for the targeted local survivor.
- Gameplay HUD/overlays are skipped while the local HUD is hidden; a dedicated
  `engineTickAftermathUi` runs the audio/fatal path.
- Added `AudioManager::stopAll()`.
- The render pass draws a full-screen black overlay, clears depth, and draws
  the existing remote Jason body at the replicated mover position so Jason
  remains visible through geometry.
- The room-code HUD uses `presentation.room_code_color`, which Versus Jason sets
  red.
- The done stage shows the standard Windows fatal-error dialog and closes the
  client window. `MIMITA_NO_FATAL_DIALOG=1` suppresses the dialog for automated
  runs.

### Room privacy

- `CommunityServerConfig` parses `private_by_default`.
- The community menu forces Private for Versus Jason.
- Starting a protected room with an empty password is blocked in the GUI.
- `handleJoinRequest` now rejects any join to a protected room without the exact
  password, even if the configured password is empty.
- The host auto-join carries the selected password into direct and room-code
  connections.

## Validation

### Configuration

- `python devscripts/gamemode_json_sync.py --check`: PASS.
- `python devscripts/config_selftest.py`: the gamemode section loaded 19 modes;
  the overall command still reports the pre-existing avatar-normalization
  failures in `assets/avatars/**/avatar.json`. Those failures are unrelated to
  this change.

### Build

- Final dev-loop build 105 succeeded.
- Runtime evidence below came from build 96 (run `20261101_150945`) and build 97
  (run `20261101_151154`).
- Final build 102 contains the later approach-broadcast, queued-scare-sound, and room-privacy edits;
  it has not been re-exercised in a new runtime run.

### Runtime journal — build 96, run `20261101_150945`

Journal: `logs/2026-11-01/20261101_150945/events.jsonl`

- `gamemode.match.configured`: `mode=vs_jason`, `time_limit_seconds=300`,
  `time_limit_winner_team=jason`, `win_condition=last_team_standing`,
  `lives=3`, survivor team respawn `0.01`, Jason team respawn `0`.
- `gamemode.team-roster.resolved`: expected/actual `[1,1]`, one generated NPC.
- `npc.roster-profile-resolved` for actor `200000`: role `vs_jason_jason`,
  behavior `vs_jason_chase`, movement `vs_jason`, weapons
  `["chainsaw","nothing"]`, starting weapon `chainsaw`.
- Three `gamemode.actor.life_lost` records for actor `1`: lives `2`, then `1`,
  then `0` with `eliminated:true`.
- `gamemode.aftermath.begin` at tick `1874` with one target, mover `200000`,
  approach `18.0`, move speed `24.0`, 99 scare ticks, final damage `999999`.
- `gamemode.aftermath.final_damage` at tick `1919`: applied `true`, killed
  `true`, health after `0`. The 45-tick gap matches `18 m / 24 m/s = 0.75 s`.
- `gamemode.aftermath.scare_tick` records from `tick_index` `0` through `98`;
  `gamemode.aftermath.scare_sequence_complete` reports `sound_serial=99`.
- Client `horror.aftermath.scare_sound` records played the configured sounds.
- Client `horror.aftermath.fatal_window` recorded the fatal window.
- `gamemode.aftermath.finished` returned the server to intermission.

### Runtime journal — build 97, run `20261101_151154`

Journal: `logs/2026-11-01/20261101_151154/events.jsonl`

- `gamemode.time_limit.start`: `start_tick=358`, `time_limit_seconds=300`,
  `deadline_tick=18358`.
- Client `client.actor.render_state` for Jason reported `health:6666`.
- Server `server.npc.lifecycle.trace` reported actor `200000` with
  `avatar:"jason"` and `health:6666`.
- The same run's roster events confirmed the chainsaw-only role profile.

### Runtime journal — build 105, run `20261101_152051`

Journal: `logs/2026-11-01/20261101_152051/events.jsonl`

- `gamemode.time_limit.start`: `start_tick=351`, deadline `18351`.
- Jason lifecycle trace: `avatar:"jason"`, `health:6666`.
- Three `gamemode.actor.life_lost` records reached `0`.
- `gamemode.aftermath.begin` at tick `1822`; final damage at tick `1867`
  (45 ticks for the 18 m / 24 m/s approach), applied `true`, killed `true`.
- Exactly 99 `gamemode.aftermath.scare_tick` records and
  `scare_sequence_complete` with `sound_serial=99`.
- Client `horror.aftermath.audio_muted`: 1 record.
- Client `horror.aftermath.scare_sound`: 99 records.
- Client `horror.aftermath.fatal_window`: 1 record.
- Server `gamemode.aftermath.finished` returned to intermission.

### Not yet proven at runtime

- Private room: no-password and wrong-password rejection, correct-password join.
- Killing Jason and the win -> results -> countdown loop.
- Timeout loss as a separate aftermath path.
- Multiplayer loss only after the last survivor is out.
- Unlimited respawns with `lives_enabled: false`.
- Visual blackout, Jason-through-geometry presentation, red room-code text, and
  fatal-window appearance.

## Documents and skills read

- `AGENTS.md`, `docs/ROUTER.md`
- `docs/specs/gamemodes/gamemodes.md`
- `docs/specs/gamemodes/objective-hud.md`
- `docs/specs/npc-behavior/npc-behavior.md`
- `docs/specs/actor-presets/actor-presets.md`
- `docs/specs/networking/networking.md`
- `docs/specs/gui/gui.md`
- `docs/architecture/json-configuration/json-configuration.md`
- `docs/workflows/runtime-scenario-validation.md`
- `docs/specs/dev-loop/quality-gates.md`
- `docs/operations/task-completion/task-completion.md`
- `docs/skills/spec-behavior-review-v1.md`

## Skill results

`docs/skills/spec-behavior-review-v1.md`: PASS_WITH_HUMAN_REVIEW.

- Mode behavior is data-driven through lives, timer, aftermath, and room-code
  fields; no new mode-name branch was added to shared match logic.
- The aftermath target set and final-damage path are server-owned and use the
  shared damage event owner.
- Runtime and visual acceptance are separated above.

## Human review still required

- Create a Versus Jason room in the GUI with no password, then a wrong password,
  then the correct password; confirm the intended mode loads.
- Kill Jason and confirm results, three-second wait, and shared countdown.
- Force a timeout and confirm the aftermath path.
- Test two clients: one survivor dies, the other continues; both out starts the
  aftermath once.
- Toggle `lives_enabled` off and confirm unlimited respawns.
- Watch the red room-code text, blackout, visible Jason approach, scare sounds,
  fatal window, client exit, and clean host/server intermission.

## Documentation notes (not edited)

- `docs/specs/gamemodes/aooni.md:1`: expand the shorthand mode idea.
- `docs/specs/gamemodes/meta.md:8`: clarify the per-mode configuration
  direction.
- `docs/specs/gui/gui-general.md:1`: clarify the command-first GUI architecture
  note.
