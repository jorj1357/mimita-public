# Team Deathmatch

Status: current first-version behavior

## Match contract

- RED and BLUE each have a capacity of five actors. The server fills open slots
  with NPC teammates or opponents so a one-player match still starts as 5v5.
- The team is the scoring unit. Each confirmed kill of an actor on the opposing
  team adds one point to the killer's team. Same-team kills do not score.
- The first team to the JSON-configured `goal_value` wins. The current value is
  30.
- A match lasts the JSON-configured `time_limit_seconds`. The current value is
  180 seconds. If the score goal was not reached when time expires, the result
  is a stalemate, including when scores are tied.
- The repeat lifecycle is: 10-second intermission, three-second frozen
  countdown (`3`, `2`, `1`), `GO`, active match, five-second results, then the
  next intermission.
- Actors remain frozen during the countdown. They and NPC simulation resume
  as soon as `GO` is shown. The active timer starts when the match enters
  ACTIVE.
- Team capacity, score goal, timer, respawn and heal rules, countdown, GO
  display, intermission, results, spawn settings, maps, and NPC targeting are
  owned by `config/gamemodes/tdm.json`.
- RED and BLUE use separate `red_spawn` and `blue_spawn` groups for initial
  placement, round reset, and every respawn. Authored team points must carry
  the matching group tag. If those groups are unavailable, the runtime uses
  separated map spawn points; if fewer than two distinct points exist, it
  derives two fallback anchors around the map anchor.
- TDM keeps health bars visible and requests green teammate and red enemy
  player mesh wireframes through its JSON presentation settings. Death
  ragdolls are enabled for TDM.

Death ragdolls are shown by default through the global ragdoll-death setting.
A gamemode may explicitly set `ragdoll_enabled: false` to suppress them; TDM
explicitly enables them so its mode override cannot turn off the default.

## Runtime ownership

`GamemodeRegistry` loads and polls `config/gamemodes/tdm.json`. The shared
server match lifecycle owns phases and authoritative score changes. The
configured `team_roster` policy fills team capacities; player and NPC actors
use the same team identity and kill scoring. The community match client reads
the replicated team score, winner, and phase to update the HUD.

A build proves only that the source compiled and linked. Runtime proof still
requires a fresh executable and journal evidence for a ten-actor roster, one
point per opposing-team kill, countdown freeze and GO release, score-limit
victory, time-limit stalemate, five-second results, and repeated intermission.
Human confirmation is required for visible phase and result presentation.
