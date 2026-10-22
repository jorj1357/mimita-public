# Duel

## Purpose and audience

This specification defines the first standard Duel community mode for players
and contributors. It records the agreed round rules, the shared actor-preset
setup, and the decisions that remain open after the initial implementation.

## Does not

- Define the later Rocket Launcher-only, Melee, or Revolver-and-Shotgun Duel
  variants. Each variant needs its own authored rules and preset references.
- Define rocket movement tuning. `config/movement/movement-source-rocket.json`
  is a candidate to improve for a later Rocket Launcher-only Duel; its values
  must not silently change standard Duel.
- Add a Duel-only lifecycle, damage system, spawn system, or NPC combat path.

## First-version player experience — decided

- Duel is a competitive one-versus-one match with two teams: RED and BLUE.
- There are two competitor slots total, one per team. At most two human players
  may join. With one human, one NPC fills the other team. When two humans are
  active, no mode NPC remains.
- Standard Duel is the first variant to author. Both competitors use the
  `duel_standard` actor preset, a third-person camera, the existing `source`
  movement preset, and the forced `monstertest1` avatar. RED and BLUE player
  roles and NPC roles each preserve their team's identity.
- The active map supplies the Duel spawn location. Both competitors use the
  same selected per-map Duel spawnpoint; they must not be assigned separate
  RED and BLUE spawnpoints. Keep the existing five-unit random XY offset around
  that shared origin.
- A round begins with a 15-second INTERMISSION, followed by the shared
  three-second `3, 2, 1, GO!` COUNTDOWN. Competitors are frozen during the
  numbered countdown and may move and attack once GO begins ACTIVE play.
- During ACTIVE, competitors respawn on the next fixed update after a confirmed
  kill and continue fighting until one competitor reaches 20 kills or three
  minutes elapse.
- Reaching 20 kills wins the round immediately. If three minutes elapse before
  either competitor reaches 20, the round is a stalemate and neither competitor
  wins.
- RESULTS shows the winner, or a stalemate result, for five seconds. The next
  round then begins with the 15-second INTERMISSION. This four-phase loop
  repeats continuously; there is no additional between-round wait or separate
  rematch choice.
- Round scores reset when the next round starts. Any lifetime/server series
  score is not defined by this first-version contract.
- Standard Duel uses the existing weapon set ID `1` (Stable weapons). Both
  competitors use the same standard Duel actor preset and the existing
  `source` movement preset. The later Rocket Launcher variant may have its own
  tuned actor preset and movement file.
- Duel presentation is controlled by `config/gamemodes/duel.json` and the
  `duel_standard` actor preset. Duel locks third-person perspective and FOV
  100. Damage numbers and ordinary actor hit effects start off. Blood remains
  enabled at 10% amount and 10% lifetime. World impact decals remain enabled
  with 10% alpha and lifetime. World hit spheres remain enabled at their full
  configured size and lifetime so they can be evaluated in play; their toggle
  and scales are editable in the mode JSON.
- Duel forces every player and NPC to use `monstertest1`. The preset allow list
  permits that avatar and has no denied patterns; each team role marks the
  avatar forced so personal avatar selection cannot replace it.

## Round phases

| Phase | Duration / exit | Behavior |
|---|---|---|
| INTERMISSION | 15 seconds | Prepare the next round and its two competitors. |
| COUNTDOWN | Three seconds | Show `3, 2, 1, GO!`; freeze competitors through 1. |
| ACTIVE | Until 20 kills or 180 seconds | Allow movement, attacks, scoring, and immediate respawns. |
| RESULTS | Five seconds | Show the winner or stalemate, then return to INTERMISSION. |

The three-minute timer starts when ACTIVE begins. If the timer expires while
neither competitor has reached 20, the result is a stalemate. Score-limit
evaluation runs before timeout evaluation on the authoritative tick, so a
20th kill on the timeout tick wins. The shared server lifecycle owns phase
timing and authoritative scoring.

## Spawn behavior

Each map used by Duel must identify the spawnpoint used for the shared Duel
spawn location. The server selects that point for both competitors at round
start and respawn. It remains the same map-owned point for the whole round.
This is intentionally a shared origin, not one team-specific point per
competitor.

The current `config/gamemodes/duel.json` uses `spawn_offset_radius: 5` around
that shared map-owned origin for both competitors at round start and respawn.
The origin stays fixed for the map; the existing shared random XY offsets
provide the separation.

## Actor presets and standard loadout

The Duel-specific actor preset assignment for both competitor roles is
`duel_standard`. Apply it through the shared role and spawn-resolution owners.
The preset provides the agreed third-person view and references the standard
movement identity. Use the existing actor-preset and movement systems; do not
put movement values in `behavior-profiles.json` or create a Duel-only physics
path.

The standard preset is `duel_standard`, shared by the RED and BLUE player roles
and the RED and BLUE NPC roles. Those roles are `duel_monster_test_red`,
`duel_monster_test_blue`, `duel_monster_test_red_npc`, and
`duel_monster_test_blue_npc`; all force `monstertest1`. The preset forces
third-person perspective and selects the existing `source` movement preset.
The gamemode's `weapon_set_id: 1` supplies the Stable weapons set because the
roles do not override it. Standard Duel must not depend on the proposed rocket-only tuning in
`movement-source-rocket.json`.

## Implementation owner map

| Need | Existing owner | Work still required |
|---|---|---|
| Match rules, capacity, score limit, time limit, and phase durations | `config/gamemodes/duel.json`; `config/onlinemodes.json`; shared lifecycle in `src/network/server-gamemode.cpp` | Configuration is authored; live mode selection and phase behavior still need runtime validation. |
| Team-to-actor assignment and third-person actor setup | `config/roles.json`, `config/actor-presets/duel_standard.json`, shared role/preset resolution | Shared roles and preset are authored; runtime role and camera application still need validation. |
| Per-map shared Duel spawn | Map GLB spawn list and shared authoritative spawn selection | Uses the shared map anchor with five-unit XY offsets; each supported map needs runtime validation. |
| Attacks, kills, score, and immediate respawn | Shared authoritative attack, damage, scoring, and spawn lifecycle | Generic score lifecycle is wired; real confirmed kills and respawns still need runtime validation. |
| Rocket-only Duel (future variant) | `config/movement/movement-source-rocket.json` and a future variant preset | Tune and validate separately; it is not part of standard Duel. |
| Duel camera and visual effects | `config/actor-presets/duel_standard.json`, `config/gamemodes/duel.json`, `GamemodePresentation`, and `EffectPartSystem` | JSON controls are wired; live visual review still needed to tune them. |
| Forced Duel avatar | `config/roles.json`, `config/actor-presets/duel_standard.json`, `MatchRoleRegistry`, and server avatar-manifest owner | Each RED/BLUE player and NPC role forces `monstertest1`; validate roster application in a live Duel. |

## Open decisions

- **Map availability:** Which maps must ship with a valid Duel spawnpoint, and
  what should happen if the selected map has none?
- **Stalemate presentation:** The result is decided as no winner. Exact result
  wording is `STALEMATE`; whether the screen includes each competitor's final
  score remains open.
- **Series wins:** Whether rounds contribute to a persistent or session-level
  RED/BLUE win total remains open; first-version round score resets are
  required.

## Acceptance

Before calling the first version implemented, verify from the authoritative
runtime that:

1. The mode admits at most two humans and assigns no more than one competitor
   to each of RED and BLUE.
2. Both competitors receive the selected standard Duel actor preset and
   third-person presentation.
3. The active map's same Duel spawnpoint is selected for both competitors at
   round start and respawn, with the agreed overlap/offset rule applied.
4. INTERMISSION lasts 15 seconds, COUNTDOWN is the shared three-second frozen
   sequence, and ACTIVE begins only at GO.
5. Confirmed opposing kills advance the correct competitor's score; the first
  to 20 wins. If neither reaches 20 by 180 seconds, RESULTS reports a
  stalemate. Score-limit evaluation precedes timeout evaluation on the same
  authoritative tick, so a 20th kill on that tick wins.
6. RESULTS lasts five seconds, round score resets, and the next 15-second
   INTERMISSION begins without an extra pause.
7. Duel applies its forced third-person FOV 100, hides damage numbers and actor
   hit effects, scales blood amount/lifetime to 0.1, and scales world impact
   decal alpha/lifetime to 0.1. The independent world-hit sphere switch and
   scales can be changed in JSON and take effect through the client mode profile.
8. Every human and NPC role forces `monstertest1`; the preset allows that avatar
   and the server accepts its manifest for the active roles.

The 2026-10-21 dev-loop run `20261021_082355` used matching build 2731 server
and client executables. Its journal confirms Duel mode, `weapon_set: 1`, a
team-roster NPC opponent, the configured 15-second intermission, and the
three-second countdown completing into GO. It does not yet prove a confirmed
kill, score-to-20, timeout stalemate, results display, repeated round reset,
avatar rejection, or visual acceptance of the camera/effect settings. Those
remain runtime acceptance requirements.

## Historical design notes

The informal notes below explain the origin and broader direction of Duel.
The decided rules above take precedence where the notes describe earlier
possibilities or values.

The initial idea was a third-person game with a gun and a moving NPC target.
The broader inspiration was improving on Roblox Da Hood-style duels, with
competitive restricted loadouts and fast repeated fights.

Earlier variant ideas included a TF2-style Soldier Rocket Launcher duel with
air strafing, a Melee duel, and Revolver-and-Shotgun-only duel. The Rocket
Launcher concept may use a custom movement preset with a speed limit, lower
gravity, and more air-strafe control. Its tuning belongs to the future variant
work described above, not the standard Duel contract.

The previous notes proposed an intermission, a frozen countdown, repeated
respawns, a first-to-20 win, a possible three-minute limit, and a short winner
screen. The present first-version values and stalemate rule are recorded in the
decided section above.
