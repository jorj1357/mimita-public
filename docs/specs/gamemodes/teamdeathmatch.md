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
- Players and NPCs share the mode's weapon set. The current TDM set is
  revolver, shotgun, rocket launcher, swordsword, chainsaw, mace, and
  deathball. Role entries must not override the mode set with a different
  loadout. The mode set applies automatically even when the launch UI still
  shows its default set; only an explicit host override or live command may
  replace it.
- RED and BLUE use separate `red_spawn` and `blue_spawn` groups for initial
  placement, round reset, and every respawn. Authored team points must carry
  the matching group tag. If those groups are unavailable, the runtime uses
  separated map spawn points; if fewer than two distinct points exist, it
  derives two fallback anchors around the map anchor.
- TDM keeps health bars visible and requests green teammate and red enemy
  player mesh outlines through its JSON presentation settings. Wireframes stay
  off for normal TDM play so the team color reads as a silhouette instead of a
  filled or dense mesh overlay. Death
  ragdolls are enabled for TDM.
- TDM selects player and NPC avatars from the masterlist's
  `competitive_no_headless` group. The allow list is authoritative; deny
  patterns only subtract from it. A rejected avatar is replaced with a permitted
  fallback at spawn and by the client presentation policy, so a monster or
  headless avatar cannot persist in TDM. Each NPC keeps its own selected avatar
  across deaths and respawns. A different mode may choose another named group
  and `reroll_each_spawn` through its `npc_avatar` block; the shared spawn owner
  applies both choices.

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

## Destructible map lifecycle (2026-10-30)

TDM uses the shared imported-map destruction rules in
`docs/specs/destructible-world/destructible-world.md` section 71.

- Every map in the configured TDM rotation uses the same rules. Dust2 is the
  first validation map, not a special case.
- Hitscan, projectile, melee, and explosion sources all damage the world through
  the single persistent destruction entry point.
- Player and NPC sources use the same physical path.
- The map is reset to its authored baseline at each 3-minute match/round start.
  Damage from one match must not carry into the next.
- Reset removes map-derived fragments. It does not disable destruction for the
  next match; cutting resumes from the restored baseline.
- The `features.destructible_world` mode override is nullable and inherits the
  global `config/destructible-world.json` `world.enabled` value.
- TDM sets its reset policy to `match`.
- The foundation material must resist sustained highest-energy TDM fire, while
  concrete walls, doorways, and corners cut normally.
- Clients must converge on the same cut, and the runtime must stay at or above
  240 FPS in the defined destruction stress scenario.

Human visual acceptance is still required for wall/door/corner cuts, physical
chunks, and reset readability.
