# Free-for-all mode

## Purpose

Provide a reliable JSON-defined Free-for-all gamemode in which human players
and NPCs are equal participants. Every registered actor should spawn at the
current map spawn, use the selected weapon set, receive authoritative kills
and deaths, appear correctly in the leaderboard, and produce reliable
killfeed/chat output.

This document tracks the current FFA implementation and the remaining issues
found during human playtesting.

## Plain-language summary

The code is not bad because it has no FFA feature. It is bad because the same
job is done in several different places. Players and NPCs can use different
paths for damage, kills, spawning, weapons, leaderboards, and chat.

That means one part of the game can know something happened while another part
does not. For example, chat can say an NPC killed a player while the score code
does not add the NPC's kill. Or the new-map reset can know the new spawn while
the normal NPC respawn code still uses the old spawn.

The goal is one shared path for every actor and every gamemode. FFA, TDM,
duel, Bomb Tag, sandbox, and future modes should provide rules and settings;
they should not each have their own player, NPC, spawn, score, or GUI system.

## Desired behavior

- Players and NPCs participate in the same FFA scoring path.
- Every valid kill increments the killer's FFA kills and the victim's deaths:
  player → player, player → NPC, NPC → player, and supported NPC → NPC.
- NPC names use `NPC-1000`, `NPC-1001`, and so on.
- Killfeed/chat uses the real killer name and the weapon's `display_name` from
  `config/weapons.json`, for example `NPC-1001 killed admin with Revolver`.
- Killfeed events are reliable and do not overwrite one another when several
  kills happen close together.
- At round end, FFA shows a temporary ranked panel with a 0.5-alpha black
  background: positions 1–3 show the actor and kill count; remaining
  participants appear from position 4 onward in white or their configured VIP
  name color.
- NPCs and players use the current map's valid gamemode spawn anchor and the
  same offset policy. No actor may continue using a previous map's spawn.
- After a map change, authoritative damage, damage numbers, and hit feedback
  continue working immediately without waiting for a death or respawn.
- Shotgun damage feedback is condensed into one appropriate popup rather than
  producing a separate zero-damage popup for each pellet.
- The FFA leaderboard has the configured 0.5-alpha background and updates when
  actors join, leave, die, respawn, or are deleted.

## 9 9 2026 1451 est issues
i kill npc = no kill gained
npc kilsl me = no kill gained
i start the game and join the server and intermisison is at 12 sec but that is prob just bc i didnt join super quick so i think thats fine?

ingame countdown got to 3 but then didnt show 3, 2, 1, "GO!!!!!" i want to show all those sepcifically its for fun 

still, i kill the npc with weapons and i still am not  gaining a kill, what is wrong? 

can u add to "C:\mimita-priv-v8\logs\09-09-2026\Server_log_145018.txt", not the exact file but, use the debug logging spec, to log  exactly 
[STATS KILL TEST]: server tick that it detects a kill from npc to player, 
plr to plr, 
or plr to npc, 
the name of the attaaker,
 name of victim, 
 what weapon was used, 
 did it send a killfeed event to everyone, 
 how much points does each player in the game have now, like every single npc or plr in the server how many points they have, 
u suggest other things to log

as well as looking for if it has a debug logger already visible for the plr killing npc path, and if it doens texist, log it exactly like 
also log this to "C:\mimita-priv-v8\logs\09-09-2026\Server_log_145018.txt" not that file exact but hte same servre naming path 
[STATS KILL TEST PLRTONPC]:
server tick the plr killed the npc
how many points the plr has now , before vs after
what weapon the plr used to kill the npc
did the killfeed recieve that, like to the game does it think the killfeed got the event that a plr killed a n pc
does the gamemode manager think it got that event as well, gamemode manager knows we killed a npc and gives me  apoint
suggest other things to  log

C:\mimita-priv-v8\config\gui\gamemode-meta-gui.json this is good, add to gold that this is working good all the fields i tested work

specific log name for this: 

the countdown of 3 2 1 go just worked, this was like the 3rd ffa round i played, not sure why it didnt work the first times. can we also add to the server debug logs for when client thinks its time to  do countdwon, what countdown number to show at what client tick vs server tick, and the what mode the server thinks the game is in? 

add to regressions that i think we have debug logging specified to happen with each thing we do, new feature etc, but i don tknow if its 100% followed. we need to add to regressions 1. a new document following new format 2. for this ffa mode issues, the related issues with it  and stuff, and this file C:\mimita-priv-v8\docs\features\gamemodes\ffa mode issues.md should just put the relative paths to those importnat files not write in here, e.g. changelog path here and regression file relatin to this file path here .

and i freeze when the results mode is happenign but nothing shwos on the gui for the results so it just freezes me for no eraosn from my pov 
## 9 9 2026 1535 est 

still, the countdown on initial start doesnt show, it jsut freezes me then the ffa mode starts. we need to ensure the countdown always shows 

## Current behavior

Human playtesting reported:

1. NPC leaderboard kills are inconsistent. An NPC killed the player about five
   times while receiving only one leaderboard kill.
2. Player → NPC kills appear in chat but do not consistently increment the
   player's FFA kill count.
3. NPC killfeed/chat messages are sometimes missing. When present, the killer
   or weapon may appear as `unknown`.
4. The round-end screen does not show the requested ranked results panel.
5. An NPC spawned inside a cone on `coolplace`, suggesting a previous-map spawn
   position or invalid spawn anchor may still be used.
6. After changing the map during FFA, shotgun hit spheres appeared but damage
   numbers and server-authoritative damage did not work until a death occurred.
7. The FFA leaderboard needs its translucent background and layout refinement.
8. NPC shotgun attacks produce multiple zero-damage popups instead of one
   condensed shotgun result.

The repository contains partial implementation work for shared actor map reset,
session cleanup, NPC attribution, weapon-name resolution, and the Tab
leaderboard. The items above remain a checklist until directly observed in a
live server/client test.

## Code diagnosis: why current behavior does not meet the desired behavior

This section compares the desired behavior with the current implementation.
The causes below are based on the source path, not on assuming that the
executable is stale.

### 1. The kill queue is only partly converted to the new design

Desired: every valid actor kill increments the killer and victim counters.

Current owner: `src/network/server-gamemode.cpp`,
`serverGamemodeOnPlayerDeath`, `serverGamemodeOnNpcDeath`, and
`serverGamemodeOnPlayerKilledNpc`.

The older version wrote every kill into one shared pending slot. The current
code now has a `pendingKillEvents` queue, so that old description is no longer
fully current. However, the queue still copies one event at a time into the
old fields:

```cpp
d.hasPendingKill = true;
d.pendingKillerId = killerId;
d.pendingVictimId = victimId;
d.pendingKillerIsNpc = ...;
d.pendingVictimIsNpc = ...;
```

The tick still consumes the old fields one kill at a time:

```cpp
if (d.hasPendingKill)
{
    d.hasPendingKill = false;
    const uint32_t killerId = d.pendingKillerId;
    const uint32_t victimId = d.pendingVictimId;
    // one scoring operation follows
}
```

This is better than the original overwrite bug, but it is still a mixed old
and new design. Some event details can be lost when the event is copied into
the old fields, and other branches still have their own kill handling.

Correct direction: queue an ordered actor-neutral kill event containing killer,
victim, actor kinds, weapon, event ID, and correlation ID. Consume every event
once during the authoritative 60 Hz tick, and apply scoring only when the
phase is `ACTIVE`. Do not add separate NPC and player scoring paths.

### 2. The current FFA scoring branch is still mode-specific and assumes IDs

Desired: all registered actors use the same scoring service.

Current code in `serverGamemodeTick` branches by literal mode:

```cpp
if (d.matchMode == "duel") { ... }
else if (d.matchMode == "ffa") {
    ++d.ffaKills[killerId];
    ++d.ffaDeaths[victimId];
}
else if (d.matchMode == "tdm") { ... }
```

The FFA branch increments maps only if the event reaches this point and the
actor IDs are already present in `d.ffaKills`/`d.ffaDeaths`. There is no
validation or registration fallback for an actor that joined after the last
participant assignment, and an actor removed from the server can leave stale
score entries. This is why the desired “every registered actor” behavior is
not yet guaranteed even though NPCs are included during some assignments.

Correct direction: use one participant/score service keyed by actor reference;
membership changes must add/remove the actor's score row atomically with the
authoritative registry. FFA and TDM should consume the same kill event and
only select their rule calculation.

### 3. NPC killfeed reliability and identity are split across incompatible paths

Desired: one reliable event produces both scoring and presentation, with output
such as `NPC-1001 killed admin with Revolver`.

Current NPC→player path in `src/network/server-npcs.cpp` calls
`applyServerDamage`, records gamemode death, and sends a
`DamageConfirmedEventPacket`. The packet is processed in
`src/network/confirmed-damage-presentation.cpp`. This path now carries the NPC
kind and ID, but it is still a damage-confirmation presentation event, not the
same ordered generic kill event used by scoring and chat.

The player→NPC path uses a different `NpcDamageEventPacket` and is processed by
`mpProcessNpcDamageEventPacket` in `src/network/multiplayer-shots.cpp`.
Therefore the two kill directions can be deduplicated, predicted, and shown by
different logic. If prediction suppression or reliable-event deduplication
rejects one path, chat/killfeed can be missing even when server damage and
score state changed.

Correct direction: publish one authoritative `ActorKillEvent` after lethal
damage and feed that event to scoring, killfeed, chat, replay, and persistence.
The damage packets may remain for health/hit presentation, but they must not be
the only killfeed source.

### 4. Weapon display-name lookup is not consistently keyed

Desired: resolve display names from `config/weapons.json`, for example
`display_name: "Revolver"`, while killfeed verbs/colors come from
`config/killfeed.json`.

The current killfeed configuration lookup historically searched the configured
weapon map using the string passed to `onKillStyled`. Several callers pass the
display name, while `killfeed.json` weapon entries are keyed by weapon ID.
That makes a valid weapon miss its configured verb/color and can fall back to
`unknown` in paths that do not resolve through `WeaponRegistry` first.

Correct direction: carry both `weaponId` and resolved `weaponDisplayName` in
the generic kill event. Resolve the display name once through the weapon
registry loaded from `config/weapons.json`; use the ID only to select the
killfeed configuration entry.

### 5. Weapon-set filtering only rebuilds at selected lifecycle boundaries

Desired: GUI/community selection wins, and restricted inventories apply to
players, NPCs, respawns, attacks, hotbars, and synchronization immediately.

`resetPlayerForSpawn` in `src/network/server-players.cpp` rebuilds
`ownedWeaponIds` only when `isInitialSpawn` is true:

```cpp
if (isInitialSpawn)
    getInitialInventory(player.ownedWeaponIds);
```

Later respawns reset runtimes for the existing inventory but do not rebuild the
owned list. The managed gamemode boundary calls `resetPlayerForSpawn(p, true)`
in `resetGamemodeActorsAtMapSpawn`, which helps at countdown, but other map,
respawn, and runtime weapon-set paths call the ordinary spawn path. This leaves
a path where an old full inventory survives after selecting a restricted set.
NPC runtime filtering is also performed during the shared reset, but NPC spawn
and difficulty loadout initialization can occur outside that boundary.

Correct direction: maintain one resolved active loadout in the match/session
state and apply it through one shared actor inventory operation whenever the
selection changes and whenever an actor spawns or respawns. Attack validation
and snapshot construction must use that same resolved loadout.

### 6. Map transitions reset some actors but do not prove a complete lifecycle

Desired: after every map load, players and NPCs use the new map's anchor,
position, respawn position, velocity, health, and lifecycle epoch.

The shared operation `resetGamemodeActorsAtMapSpawn` now updates the simulated
NPC body and mirror. However, `server-npcs.cpp` still has a separate
`respawnServerNpc` path, and the map-only branch has separate NPC/player reset
logic. Those paths can reintroduce the old `body.respawnPosition` or an actor
that was not in the current participant assignment. This matches the reported
NPC stuck at the old `coolplace`/previous-map position and void-respawn loop.

Correct direction: route map load, countdown, active reset, void death, and
respawn through the same actor reset operation, using the current map anchor
and updating both the authoritative actor and its network mirror.

### 7. Results are replicated as phase state but not rendered as the requested panel

Desired: during `RESULTS`, render ranked FFA participants with the black
0.5-alpha background and configured colors.

The server enters `DUEL_PHASE_RESULTS` and broadcasts match state, but the
current community HUD block in `src/engine/engine-tick-ui-overlays.cpp` renders
mode title, score, intermission, countdown, and active match time. It does not
render an FFA results panel when `match.phase()` is `DUEL_PHASE_RESULTS`.
The existing `MatchLeaderboard` renderer only draws the live top three FFA
rows, not the complete results roster from rank 4 onward.

Correct direction: replicate a complete results snapshot and render a generic
results layout from `config/gui/gamemode-meta-gui.json`. The renderer should
use the same participant rows and VIP styling as the live leaderboard.

### 8. NPC deletion removes the replica only after missing-snapshot grace

Desired: `npc_delete_all` immediately removes the actor from server state,
leaderboard, and client presentation and adds `<name> left the room` to chat.

`npc_delete_all` clears the server `npcs` map. The client-side removal in
`remote-entity-lifecycle.cpp` waits for missing-snapshot confirmations and a
grace interval before erasing `remoteNpcs`. The leaderboard is populated from
the last replicated match state and has no generic membership-removal event to
remove a row immediately. No corresponding generic “left the room” chat event
is emitted by the command path.

Correct direction: send one reliable actor membership removal event, remove
the actor from score/team/leaderboard registries immediately, and use the same
display name for the chat line and the client row removal.

### 9. Map-change damage and shotgun popup symptoms have separate likely owners

The map-change damage report is consistent with stale actor/map synchronization:
the hit visuals are client-local, while authoritative damage uses the server's
current actor/world state. Until the new map, NPC body, snapshot epoch, and
spawn handshake are synchronized together, visuals can appear before the
server accepts the hit. This needs a packet/log trace to confirm the first
missing state transition.

The shotgun popup report is consistent with pellet-level presentation being
fed into a damage-number path that lacks a shotgun aggregation key. The correct
fix is to aggregate one authoritative shotgun hit by event/correlation ID
before rendering, not to hide zeroes in the renderer. This also needs a focused
runtime trace because the current document does not identify the exact popup
producer.

## Current status, from working best to not working at all

- Implemented, source/build verified: shared player/NPC map-spawn reset;
  session-state cleanup; compact NPC attacker attribution; canonical NPC name
  fallback; weapon display-name resolution; JSON-backed Tab leaderboard layout.
- Implemented, runtime confirmation pending: player/NPC FFA scoring; reliable
  NPC killfeed delivery; selected weapon-set filtering after map changes;
  repeated countdown/map-transition behavior.
- Not confirmed working: ranked FFA results panel, leaderboard membership
  removal, shotgun popup condensation, and complete live acceptance of all
  player/NPC kill directions.
- Human confirmation required as of `2026-09-09`: stop/start server-session
  isolation, repeated FFA rounds, map changes, NPC deletion, and live GUI hot
  reload.

## 2026-09-09 logging update

The latest server/client logs confirm that the logging system is currently too
verbose to be useful for FFA diagnosis. The following messages repeat every
main-loop or render iteration and should not be written to normal server or
terminal logs:

- `[LOOP TOP] ... entering main loop iteration`
- `[ICE POLL DONE] ... pkts=0 processed=0`
- `[NET RX] type=...` for every packet
- per-frame healthbar and skybox render messages
- repeated projectile correction and grenade-launcher verbose messages

The main-loop, empty-ICE-poll, and every-packet `printf` messages were removed
from the active paths. Remaining diagnostic output must use the central
categorized Debug logger, with event-focused names that are easy to search,
for example `[FFA_KILL_TEST]`, `[FFA_COUNTDOWN_STATE]`,
`[GAMEMODE_SPAWN_RESET]`, `[ACTOR_MEMBERSHIP]`, and
`[MAP_DAMAGE_HANDSHAKE]`. Each event should log the decision and result once,
while repeating state should be throttled or emitted only on change.

The FFA kill diagnostics still need to be verified end-to-end. They must make
the following visible without dumping a line every tick: server tick, session
generation, mode/phase, actor kind/ID/name, weapon ID/display name, event and
correlation IDs, score before/after, killfeed delivery result, and packet
accept/reject reason. Countdown diagnostics must log each visible `3`, `2`,
`1`, and `GO` transition with both client and server ticks.

This is also a broader logging regression: debug logging is specified for new
features, but current ownership and severity are not consistently followed.
The logging cleanup must separate normal server logs from opt-in verbose
terminal/debug output, preserve important transition/error records, and use
rate limits for high-frequency render, transport, and simulation state.

## Decisions

- FFA uses the same actor-neutral combat event and participant registry as
  players and other gamemodes; it does not get a separate NPC scoring system.
- Weapon precedence is GUI/community-menu selection, then explicit runtime
  selection, then the FFA JSON `weapon_set_id` fallback.
- Gamemode presentation is owned by `config/gui/gamemode-meta-gui.json`; Tab
  presentation by `config/gui/tab-leaderboard.json`; killfeed presentation by
  `config/killfeed.json`.
- Automatic map selection uses only `config/gamemode-good-maps.json`.
- `NEEDS_SPEC_DECISION`: the exact FFA results-panel duration is not separately
  specified here. Use the shared gamemode results duration unless FFA explicitly
  overrides it.

## Ownership

- Primary code owner: `src/network/server-gamemode.cpp/.h`
- Configuration owner: `config/gamemodes/ffa.json`,
  `config/gamemode-good-maps.json`, `config/weaponsets.json`
- Runtime/event owner: `src/network/server-damage.cpp`,
  `src/network/server-npcs.cpp`, `src/network/community-match-client.cpp`
- Network owner: `src/network/packets.h`, `src/network/multiplayer-tick.cpp`,
  `src/network/confirmed-damage-presentation.cpp`
- GUI owner: `src/engine/engine-tick-ui-overlays.cpp`,
  `src/gui/hud/match-leaderboard.cpp`, `src/killfeed/killfeed.cpp`
- Actor/spawn owner: `src/npc/npc-spawn.cpp` and the shared gamemode spawn
  reset in `src/network/server-gamemode.cpp`

## Related authoritative documents

- Specification: `docs/specs/gamemodes/gamemodes.md`
- GUI: `docs/specs/gui/guiv2.md`
- Weapons: `docs/specs/weapons/weapons.md`
- Networking: `docs/specs/networking/networking.md`
- JSON architecture: `docs/architecture/json-configuration/json-configuration.md`
- Ownership: `docs/architecture/code-ownership/code-ownership.md`
- Regression: `docs/regressions/2026-09-09/gamemode-session-actors-REG.md`
- Feature workflow: `docs/features/README.md`
- Focused reviews: `docs/skills/spec-behavior-review-v1.md`,
  `docs/skills/gui-hardcoding-checker-v1.md`,
  `docs/skills/chat-checker-v1.md`

## Relevant files

- [FFA rules](C:/mimita-priv-v8/config/gamemodes/ffa.json)
- [Good map pool](C:/mimita-priv-v8/config/gamemode-good-maps.json)
- [Gamemode GUI](C:/mimita-priv-v8/config/gui/gamemode-meta-gui.json)
- [Tab leaderboard GUI](C:/mimita-priv-v8/config/gui/tab-leaderboard.json)
- [Killfeed configuration](C:/mimita-priv-v8/config/killfeed.json)
- [Weapon definitions](C:/mimita-priv-v8/config/weapons.json)
- [Gamemode runtime](C:/mimita-priv-v8/src/network/server-gamemode.cpp)
- [Server NPC combat](C:/mimita-priv-v8/src/network/server-npcs.cpp)
- [Server damage events](C:/mimita-priv-v8/src/network/server-damage.cpp)
- [Killfeed manager](C:/mimita-priv-v8/src/killfeed/killfeed.cpp)
- [Tab/overlay rendering](C:/mimita-priv-v8/src/engine/engine-tick-ui-overlays.cpp)

## Tests and evidence

- Automated tests: still needed for all four kill directions, repeated event
  delivery, map-transition damage, results ranking, membership removal, and
  shotgun popup aggregation.
- Runtime commands: `npc_delete_all`, `npc_spawn`, `modestart`, `modestartnow`,
  and explicit map-change commands.
- Logs: inspect centralized server/NPC/network debug logs for session, map,
  actor, spawn, kill-event, score, and packet accept/reject diagnostics.
- Build evidence: the canonical build previously completed with `Status:
  SUCCESS`; this proves compilation/linking only.
- Human playtest: still required for repeated FFA rounds, NPC scoring,
  killfeed/chat identity, map transitions, results, leaderboard updates,
  weapon filtering, and GUI hot reload.

## Acceptance criteria

- [ ] Five consecutive NPC → player kills produce five NPC leaderboard kills.
- [ ] Five consecutive player → NPC kills produce five player leaderboard kills.
- [ ] Player → player and supported NPC → NPC kills use the same scoring path.
- [ ] Every killfeed line uses the correct actor and weapon display name; no
      valid NPC kill says `unknown`.
- [ ] Every valid kill produces one reliable killfeed/chat event.
- [ ] Round results show ranked positions 1–3 and all remaining participants
      with the required colors and 0.5-alpha background.
- [ ] NPCs spawned before a map change reset to the new map's player spawn.
- [ ] Damage numbers and authoritative damage work immediately after a map
      change.
- [ ] NPC shotgun attacks produce one condensed damage popup per shotgun hit.
- [ ] Removing an NPC removes its leaderboard row and adds `<name> left the
      room` to chat.
- [ ] Saving GUI JSON during a running match updates presentation without a
      restart and preserves the last valid layout after malformed JSON.
- [ ] Starting a new server after stopping the old one shows no old FFA
      countdown, score, killfeed, results, or leaderboard state.

## Changelog and regression links

- [Feature-record changelog](C:/mimita-priv-v8/docs/changelog/2026-09-09/20260909_174500-ffa-feature-record.md)
- [Gamemode session and actor regression](C:/mimita-priv-v8/docs/regressions/2026-09-09/gamemode-session-actors-REG.md)
