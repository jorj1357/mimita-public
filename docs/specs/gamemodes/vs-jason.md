// 11 01 2026, 10 15
/* purpose
* Define the internal Versus Jason horror mode: roster, lives, timer, ending,
* aftermath, replication, and acceptance.
* Audience: gamemode, server, networking, client presentation, and content
* contributors.
* Does NOT belong in player-facing documentation.
* Does NOT add a mode-name branch to shared match code; behavior is driven by
* mode data.
* Does NOT claim runtime or visual acceptance that has not been performed.
*/

# Versus Jason

Status: implemented in source; runtime and visual acceptance are tracked in the
implementation changelog.

## 1. Mode identity

- Stable id: `vs_jason`
- File: `config/gamemodes/vs_jason.json`
- Community menu id: `vs_jason` in `config/onlinemodes.json`
- Teams: `survivors` (human players, unlimited slots) and `jason` (one NPC)
- Win condition: `last_team_standing`
- Timer: five minutes, starting at GO
- Results: three seconds, then the shared intermission/countdown loop

The mode is hosted through the normal community room flow. It uses the existing
generated room code and `Join a Room` password field. Rooms are private by
default and cannot start without a non-empty host password.

## 2. Roster and combat

- Jason uses the existing `jason` avatar and `vs_jason_jason` role.
- Jason has `6666` HP.
- Jason's role loadout is `vs_jason_chainsaw`, which contains only `chainsaw`.
- Jason's behavior profile is `vs_jason_chase`: persistent pursuit, nearest
  living survivor preference, chainsaw-only attack cycle.
- Jason's movement preset is `vs_jason`, a high-speed Source-family preset.
- Jason does not respawn. Team `jason` has `respawn_seconds: 0`.
- Human survivors use `vs_jason_survivor` with the existing standard weapons.

Killing Jason ends the match in the survivors' favor and enters the shared
results/intermission/countdown flow.

## 3. Lives

`lives_enabled` is the shared per-actor lives switch. `lives_team` scopes it to
one team; an empty value applies it to every participant.

For Versus Jason:

- `lives_enabled: true`
- `lives_team: "survivors"`
- `lives: 3`

Each survivor starts with three total lives. A death consumes one life and the
survivor respawns immediately while lives remain. At zero, the survivor no
longer respawns. When `lives_enabled` is false, survivors respawn according to
the normal team rule with no stock limit.

A brief death between respawns is not elimination. A team is still in the match
while at least one member is alive/respawning or can still return. In
multiplayer, the loss ending starts only after no survivor can return.

## 4. Timer and endings

`time_limit_seconds: 300` and `time_limit_starts_at_go: true`. The timer starts
when GO is shown, not at the ACTIVE transition.

- Jason eliminated: standard results for three seconds, then the shared
  intermission/countdown.
- Time limit expires with Jason as the configured winner: loss ending.
- No survivor can return: loss ending.

The loss ending does not use the standard results screen. It uses the
data-driven aftermath block.

## 5. Loss aftermath

`aftermath.enabled` and `aftermath.target_team` select the losing team. Every
connected human survivor receives a local aftermath presentation. The server
authors one scripted presentation effect per target; the real Jason NPC stays
frozen.

Server flow:

1. Freeze the match and all NPC simulation.
2. Hold each survivor alive at 1 HP, movement disabled, input ignored, position
   pinned, and no respawn.
3. Create an approach effect per survivor. The effect starts at
   `approach_distance_meters` from the frozen body and moves at `move_speed`
   toward it, ignoring collision, AI, and final-pose containment.
4. When every effect reaches its target, apply the final `final_damage` event
   through `applyActorDamage`, with Jason as the attacker and `chainsaw` as the
   weapon. The normal damage-confirmed event, kill event, and death presentation
   remain authoritative.
5. Emit exactly `scare_ticks` (99) server ticks of random scare sounds. Each tick
   increments a serial so every targeted client plays one overlapping sound.
6. Broadcast the done stage, wait briefly, then close the match into the shared
   intermission. The room remains alive; the targeted client shows the fatal
   window and exits.

Client presentation for a targeted local survivor:

- hide the gameplay HUD,
- stop existing game audio and music,
- black out the world and render the existing Jason body at the server-authored
  mover position,
- freeze local input and keep the local camera,
- play the mode's scare sound for each replicated sound serial,
- at the done stage, show the standard fatal-error window and close the client.

## 6. Replication

`DuelStatePacket` carries `DUEL_PHASE_AFTERMATH` and the per-recipient effect:

- active/stage,
- target actor and mover actor,
- mover position,
- sound serial/index,
- hide-HUD, mute-audio, blackout, freeze-local, and fatal-window flags.

Non-target clients receive no active local aftermath. The server tick owns all
timing, damage, sound choice, and the transition back to intermission.

## 7. Presentation

- In-room room-code text uses `presentation.room_code_color`. Versus Jason sets
  red.
- Enemy healthbars are enabled so Jason's 6,666 HP is visible.
- The mode remains internal: do not add it to player-facing documentation.

## 8. Ownership

| Concern | Owner |
|---|---|
| Mode rules and roster | `config/gamemodes/vs_jason.json` |
| Jason role/loadout/behavior | `config/roles.json`, `config/weaponsets.json`, `config/behavior-profiles.json` |
| Jason movement speed | `config/movement/movement-vs-jason.json` |
| Room privacy default | `config/onlinemodes.json`, `CommunityServerConfig`, online menu |
| Lives | `ServerGamemodeState` generic actor-lives map and helpers |
| Match phases, timer, results, aftermath trigger | `server-gamemode.cpp` |
| Scripted effect, final damage, sound cadence | `server-gamemode.cpp` |
| Replication | `packets.h`, `broadcastDuelState`, `CommunityMatchClient` |
| Local HUD/audio/camera/blackout/fatal exit | engine tick modules |
| Password enforcement | `server-packets.cpp`, GUI server-start path |

## 9. Acceptance

Source and build acceptance require:

- `python devscripts/gamemode_json_sync.py --check` passes,
- the canonical build succeeds,
- the new mode registers with the expected supplied fields.

Runtime journal acceptance requires:

- private room rejects no password and wrong password; correct password joins,
- mode id `vs_jason` is replicated,
- Jason reports 6,666 HP, chainsaw-only equipment, and nearest-survivor pursuit,
- the five-minute timer starts at GO,
- three lives and immediate respawns while lives remain,
- unlimited respawns when lives are disabled,
- multiplayer loss only after the last survivor is out,
- win results/intermission/countdown loop,
- timeout and lives-exhausted aftermath separately,
- 99 sound ticks, blackout, fatal window, client exit, and clean server
  intermission.

Visual acceptance remains human work: red room-code text, blackout readability,
Jason's approach through geometry, scare presentation, and the fatal window.

## 10. Related documents

- `docs/specs/gamemodes/gamemodes.md`
- `docs/specs/npc-behavior/npc-behavior.md`
- `docs/specs/actor-presets/actor-presets.md`
- `docs/specs/networking/networking.md`
- `docs/specs/gui/gui.md`
- `docs/specs/dev-loop/quality-gates.md`
- `docs/workflows/runtime-scenario-validation.md`

## 11. Documentation TODOs outside this change

These nearby notes are not edited here:

- `docs/specs/gamemodes/aooni.md:1`: expand the shorthand mode idea.
- `docs/specs/gamemodes/meta.md:8`: clarify the per-mode configuration
  direction.
- `docs/specs/gui/gui-general.md:1`: clarify the command-first GUI architecture
  note.
