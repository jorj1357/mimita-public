# Gamemodes

**Purpose:** define shared match behavior and how mode-specific rules are
selected and consumed.
**Audience:** gamemode, UI, networking, actor, and content contributors.
**Does not:** require a fixed maximum number of modes or give each mode a
separate copy of the match lifecycle.

## General design

Gamemodes have stable IDs and data-defined rules. The shared runtime owns
match phases, participants, spawns, scoring, results, maps, and inventory.
Adding a mode should use those shared services instead of branching on a
mode's name, first letter, or a dedicated manager.

Mode data may request generic presentation such as timers, health bars, and
world text. The shared UI and presentation owners render those elements from
the declared state. The aspiration to support a very large community of
distinct modes is a direction, not a literal capacity guarantee.

## Mode specifications

- [Skatepark](skatepark.md)
- [Survive the Disasters](survive-the-disasters.md)
- [Need for Madness](need-for-madness.md)
- [Cops vs. Robbers / Police Chase](vehicles/police-chase.md)
- [SWAT](swat.md) and [Bodycam](bodycam.md)
- [Zombie Tower](zombie-tower/zombie-tower.md)

## General gamemode runtime — authoritative as of 2026-09-09

The server lifecycle is owned by a general gamemode manager. Its state,
participant, spawn, scoring, countdown, results, map, and inventory functions
must describe shared behavior and must not be named or structured as duel-only,
Bomb Tag-only, or another single-purpose mode implementation. Individual modes
are JSON-defined rule sets and optional narrowly scoped rule data consumed by the
shared runtime.

All gamemodes currently use `config/gamemode-good-maps.json` as their automatic
map pool. Random selection and automatic rotation may choose only valid entries
from that file. An explicit map selected in the GUI or an explicit `changemap`
command may choose a map outside the configured pool; invalid or unloadable
automatic candidates are skipped safely.

When the GUI presents a map choice before starting a match, that choice is
resolved and loaded before the match enters its intermission. Intermission is
not a second map-picker phase. The chosen map remains the match map through
countdown, active play, respawns, and results unless a documented mode/map
rotation transition selects another map.

The active lifecycle is `WAITING → INTERMISSION → COUNTDOWN 3 → COUNTDOWN 2 →
COUNTDOWN 1 → GO → ACTIVE → RESULTS → INTERMISSION`. `PRE_MATCH` is not used by
the active runtime.

The configured countdown duration (three seconds for standard matches) uses
monotonic wall time, even if server simulation stalls. Each match or round
shows one forward-only `3 → 2 → 1 → GO` sequence. Pause simulation for all
world NPCs during COUNTDOWN. By default, also hold them during GO and resume at
ACTIVE. A mode may set `features.freeze_during_go` to `false` to resume actor
movement, NPC simulation, and team AI as soon as GO is shown. The server enters
GO on the first available simulation tick after the wall-clock deadline.

`modelist` lists modes and their numeric IDs only. `modestart N` starts or live-
switches to mode N through the results screen and then the new mode's default
15-second intermission. `modestartnow N` starts or live-switches to mode N and
enters its countdown after the five-second live-switch results screen. There is
no `modepick` command.

### `startmelee` command contract

`startmelee` is a named Melee launch action, not a numeric alias for
`modestart`. If the local player is already connected, it requests the
authoritative server to switch that room to the stable gamemode ID `melee`.
If disconnected, it starts the configured Melee server preset and connects the
local host client as soon as that authoritative server is ready; coordinator
registration continues in parallel so remote players can join by room code.
The path must never fall back to sandbox or report joining before the
connection is confirmed. Its initial map comes from the Melee preset; the
normal GUI map picker remains the way to choose another map. Command receipt, handler result,
server acceptance/rejection, room registration, connection transition, active
mode, and actor roster are separate evidence; sending the command is not proof
that the server applied it.

The first playable Melee preset starts one human and exactly one Melee NPC.
Both are assigned the `falcon` role and resolve the same `captain_falcon`
weapon set and `captain_falcon_moveset` equipped definition. The NPC must not
retain or fall back to ordinary guns. Do not add a second NPC in a launch
scenario unless that scenario is explicitly testing rejection of an extra
spawn. A mode change must reconcile generic startup NPCs to the mode's declared
roster; while the first-version Melee roster is active, reject or prevent
additional NPC spawns instead of silently adding unassigned, gun-equipped
actors. Reapply and log the same effective loadout on respawn. The launch
scenario must assert one human, one NPC, the active `melee` mode, and matching
role, weapon-set, and equipped definition identity for both fighters.

An explicit weapon set selected in the GUI or at runtime takes precedence over
the `weapon_set_id` fallback in a gamemode JSON file.

## Engine-wide actor and session rules

The shared runtime is an engine service. Players and NPCs are participants in
the same actor registry and use the same map-spawn reset, inventory, damage,
score, membership, leaderboard, killfeed, and phase-transition services.
Gamemode JSON supplies rules and data; it does not own a shared lifecycle.
Generic names such as actor, participant, gamemode, match, results, and spawn
are required so adding a new mode does not require another duel-only or
Bomb-Tag-only lifecycle implementation.

Every gamemode uses `config/gamemode-good-maps.json` for automatic and random
map selection as of `2026-09-09`. Explicit GUI map selection and explicit
`changemap` may use another map. Missing, invalid, or unloadable automatic
candidates are skipped safely.

The session boundary resets replicated mode, phase, countdown, timers, match
identity, scores, results, Bomb Tag state, killfeed, and leaderboard state. A
new server session has a new generation, so packets from a stopped server
cannot revive its countdown or presentation state.

## Generic match timeout outcomes

`time_limit_outcome` optionally selects the result when the active time limit
expires before the configured score goal. Supported values are `high_score`
(the default, preserving existing behavior) and `stalemate` (no winner). The
shared score-limit check runs before the timeout check, so reaching the goal on
the timeout tick wins. This setting belongs to mode data and must not require a
mode-ID-specific code path.

## Per-mode friendly and enemy healthbars

The gamemode masterlist exposes `presentation.friendly_healthbars` and
`presentation.enemy_healthbars` as independent nullable boolean overrides for
all modes. A concrete `true` or `false` controls that team's world-space
healthbars; `null` keeps the shared HUD behavior. `hide_healthbars` remains the
broader switch that hides all live-world healthbars. The local player's own
healthbar is controlled separately by `config/healthbar.json`. Payload enables
friendly healthbars explicitly so its map can show teammate health without
requiring an enemy-healthbar change.

## Per-mode aim, body, and victim feedback (2026-10-29)

The gamemode masterlist defines a nullable `aim` block and one additional
presentation flag. Values live in each `config/gamemodes/*.json`; `null` or an
absent value inherits the shared base file.

| Mode field | Inherits from | Meaning |
|---|---|---|
| `aim.mode` | `config/gameplay.json` `aim_mode` | Client firing solution: `crosshair`, `world_hit`, `farpoint`, `camforward`, or `physical`. |
| `aim.body_mode` | `config/aimbody.json` `mode` | Body simulation: `default`, `smooth`, `physical`, or `hybrid`. |
| `aim.body_enabled` | `config/aimbody.json` `enabled` | Enables or disables the aim body. |
| `presentation.hit_flinch` | `config/camconfig.json` `hitFlinch.enabled` | Camera punch when the local player is hit. |
| `presentation.blood_client_feedback` | `config/impact_decals.json` `blood.clientFeedback.enabled` | Camera-locked blood particles when the local player is hit. |

`presentation.blood` stays the all-blood switch: it controls world blood,
decals, spray, and the client feedback together. `blood_client_feedback` only
controls the victim-screen particles and does not turn world blood back on.

Example for Juggernaut:

```jsonc
"aim": {
  "mode": "physical",
  "body_mode": "hybrid",
  "body_enabled": true
},
"presentation": {
  "hit_flinch": true,
  "blood_client_feedback": true
}
```

These fields are resolved and applied by the single
`GamemodeRuntimeResolver` described in
`docs/architecture/json-configuration/json-configuration.md`. A mode must not
require editing `gameplay.json`, `aimbody.json`, or `impact_decals.json` to
change its own aim or victim-feedback behavior.

## Authored map spawnpoint consumption

`MapConfigRegistry` owns authored map entities. The Entity Editor owns how a
creator adds, positions, tags, enables, saves, and reloads a `spawnpoint`; this
shared gamemode specification owns how an authoritative spawn request chooses
one. All spawn entry paths must use the same server-side selection policy:
initial player join, respawn, map change, team/role assignment, and NPC spawn
commands or waves. They must not independently pick from different local
lists.

Selection order for ordinary spawns:

1. Preserve a higher-priority lifecycle rule already in effect, such as a
   valid checkpoint spawn or explicit server-approved teleport.
2. Select an enabled map-config spawnpoint eligible for the actor's active
   mode/team/role. A point with a configured tag must match the actor's
   configured spawn group; an untagged point is the generic candidate.
3. If the active map JSON has a complete GLB-spawn override manifest, its
   enabled resolved entries replace the raw GLB spawn list, including disabled
   source points; never resurrect a disabled GLB point as a fallback. For a
   legacy map without a complete manifest, use the raw map-GLB spawn list for
   compatibility.
4. Validate the candidate with the server's existing safe-spawn checks. If no
   candidate is safe, use the existing documented safe fallback and emit a
   reasoned diagnostic.

The selected position is applied through the existing authoritative spawn
lifecycle, which updates actor generation/transform identity and replicates
the spawn. Creating or moving an authored point changes future spawn selection;
it does not teleport actors who are already alive. An editor change is active
in the session after the authoritative server accepts it. In a remote session,
the client must submit an authorized edit request to the server; a client-local
JSON write is not a remote server update. `entity_save` persists accepted map
data, while the in-memory change should be available for the next spawn in the
same session.

Implementation audit (2026-10-16): `gamemodeSpawnPoint()` already consumes
enabled authored points for some gamemode-managed spawn events, while initial
join and `npc_spawn` still follow GLB/local-world paths. Consolidate those
callers into this contract instead of adding another spawn-point owner. This
is a proposed alignment; runtime acceptance is still required.

### 2026-10-16 runtime evidence: Dust2 Cyberia spawn report

The selected journal `logs/2026-10-16/20261016_134002/events.jsonl` records
the server loading `mimita-aabb-only-interior-small-v4.glb` with zero spawn
points, while the client loads `dust2cyberiav4.glb` with two spawn points.
The client then reports repeated failures to load the non-v4
`mimita-aabb-only-interior-small.glb`. Later server and client map events
report `funworld3.glb`. A local actor/corpse presentation event records
position Z≈−501 while the successfully loaded map bounds in this journal begin
at Z≈−5 or higher. This supports the reported below-world condition and a
map/session divergence investigation; it does not yet identify which spawn
caller wrote that position or prove the complete join sequence.

Acceptance: on the same fresh server session, initial player join, respawn,
map change, and NPC spawn all use the selected authoritative map and the same
safe authored-spawn policy. Record expected and actual map/session identity,
chosen spawnpoint, actor generation, and final transform in the live journal.
Before a player is admitted or spawned, the server and client must agree on a
canonical map ID/content revision, and the server must confirm that collision
world and spawn data are ready. The client must not independently load a
fallback or stale map while joining the server's room. If map identity differs
or no safe spawn exists, stop the join/spawn transition with a visible reason;
never apply a stale-world coordinate or silently continue below/outside the
loaded map. Diagnostics connect session/run ID, selected map ID and content
revision, requested/resolved asset path, process role, load readiness, spawn
source/candidate, validation result, actor ID/generation, and final
authoritative transform.

### 2026-10-17 resolution: map-id truncation and safe-spawn policy

Root cause of the 2026-10-16 below-world report was identified and fixed. The
`mapId` field in `WelcomePacket`, `JoinAcceptPacket`, `MapChangePacket`,
`DuelStatePacket`, and `ClientMapReadyPacket` was `char[MAX_NAME_BYTES]`
(32 bytes), which silently truncated the content id
`mimita-aabb-only-interior-small-v4` (34 bytes) to
`mimita-aabb-only-interior-small` (`src/network/packets.h`). The client then
requested a nonexistent base map and still reported ready on its previous world.
The map id now uses `MAX_MAP_ID_BYTES = 64`, the client only reports ready when
its loaded world matches the required id, and `serverPickSafeSpawn`
(`src/network/server-players.cpp`) is the single authoritative spawn owner:
authored points are ground-snapped and validated, maps with no authored spawn
points derive a validated standing position by probing collision geometry, and
a genuinely unsafe world refuses with a visible `spawn.server-no-safe-spawn`.

Live evidence: scenario `devscripts/scenarios/priority1_map_spawn_v1.json` on run
`logs/2026-10-17/20261017_002823/events.jsonl` passed all 10 assertions on the
reported map (server `spawn_points=0`; client received the full map id and
loaded the same file; `spawn.server-selected source=derived-probe
validated=true` in bounds). Human visual confirmation of the in-bounds spawn
remains outstanding; see
`docs/regressions/2026-10-17/map-id-truncation-stale-ready-REG.md`.

## General NPC intent contract (objective / focus / patrol)

## General NPC intent contract (objective / focus / patrol)

A gamemode does not own NPC artificial intelligence. It selects roles, presets,
and behavior profiles; the shared NPC brain turns those into intent. To keep the
brain mode-agnostic, every mode NPC intent reduces to one of three general
categories:

- **Objective** — the team must reach or interact with a mode-declared
  objective (for example a bomb site). The mode supplies objective positions via
  the existing objective/TeamBrain path.
- **Focus** — the team must converge on a hostile actor set or area as a unit
  (for example Juggernaut Fighters converging on the Juggernauts). Focus is
  configured through behavior profiles, not through mode code; see
  `docs/architecture/player-npc-systems/npc-group-behavior.md`.
- **Patrol** — no objective and no focus context exists; the NPC walks the map.

Rules:

- The mode JSON must not contain `if (mode == ...)`-style decisions; it declares
  data (teams, roles, behavior profiles, optional objective/focus intent), and
  the shared runtime consumes that data.
- A behavior profile owns the per-actor group parameters (focus, cohesion,
  swarm, slots). A mode/role owns which profile an actor uses. Neither duplicates
  the other.
- Focus is a travel target, never an aim permission. Aiming always requires the
  shared perception owner to report a visible target.
- Adding a new team mode that needs a swarm must require only configuration and
  behavior-profile data, not a new NPC subsystem.

## Shared vehicle and world-physics policy

Gamemode data may select shared physical behavior without introducing a
gamemode-specific physics owner:

- `vehicle.control_profile` selects the input-to-action profile for occupied
  vehicles. The default car profile is manual transmission; skateboard, BMX,
  and arcade race modes can select the shared arcade trick profile.
- `features.destructible_world` is a nullable mode override. An absent or
  null value inherits the global destructible-world setting, whose desired
  default is enabled. A mode may explicitly set it to false when its design
  or performance budget requires intact world geometry.
- A mode may select a shared actor-contact profile, including the proposed
  close-space soft-fit behavior. It tunes the shared collision owner and may
  not bypass world collision or allow tunneling.

World-geometry destruction and physical contact are separate. Disabling
destructible world geometry does not disable ordinary collision between
actors, vehicles, weapons, or props. Vehicle damage is controlled by the
shared damage and physical-object rules.

These are desired mode-data interfaces; the current gamemode JSON schema and
runtime do not yet implement all of them. Add schema validation and runtime
consumption together when implementation begins, and retain safe inheritance
for modes with no override.

9 2 2026

- End goal  
  - Clearly explain what each gamemode we are doing   
    - Bc we are moving away from duels queue and just doing community server browser from now on  
  - How each gamemode behaves, how it is to use the gamemodes etc  
- Meta  
  - Generalization of functions  
    - Gui elements should be made with general functions  
    - These gamemodes are just different definitions of the same json structure, no functions should be written directly for hjust 1 mode, all functions should be general so later we can expand the modes 

Commands, for host only, 9 2 2026 0922 move this into its own commands spec 

- “Modelist”  
  - Lists all modes that are json defined   
  - As of right now, the path is here  
    - "C:\\mimita-priv-v8\\config\\gamemodes\\ffa.json"  
    - "C:\\mimita-priv-v8\\config\\gamemodes\\tdm.json"  
  - And the team death match json for example looks like thsi   
    - {  
    -   "id": "tdm",  
    -   "name": "Team Deathmatch",  
    -   "description": "Red vs Blue. First team to 30 kills wins.",  
    -   "team\_names": \["RED", "BLUE"\],  
    -   "goal\_value": 30,  
-   "time\_limit\_seconds": 180,
-   "time\_limit\_outcome": "stalemate",
    -   "respawn\_seconds": 0.01,  
    -   "kill\_heals": true,  
    -   "countdown\_seconds": 3,  
    -   "go\_seconds": 0.75,  
    -   "rematch\_seconds": 5,  
    -   "spawn\_offset\_radius": 5.0,  
    -   "spawn\_strategy": "shared",  
-   "intermission\_seconds": 10,
-   "results\_seconds": 5,
    -   "maps": \["mimita-duels-map-v3", "atdm", "funworld"\]  
    - }  
- “modestart”   
  - “Modelist” lists tdm as 1, ffa as 2  
  - “Modestart 1”  starts tdm, at the intermissionstage  
- “modestartnow”   
  - “Modestartnow 1” starts tdm, at the 3,2,1, countdown stage  
- “Changemap”  
  - “Changemap 1” changes the server’s map to whatever shows up as the first map   
  - “Changemap 25” changes the server to whatever map is the 25th map in the map list   
- “maplist”   
  - Lists all maps, with a number, so u arent typing “Superlongmapname12345” u just type “changemap 17” and it loads that map  
- “Weaponsetlist”  
  - List all weaponsets  
- “weaponsetpick”   
  - Pick a new weapon set for all plauers , and apply it instantly   
    - And npcs?  
- Npc behavior console commands  
  - Todo, difficulty,   
    - maybe npc names defined in a json,   
    - And each npc can have behavior like  team deathamtch, what team theyre on, etc  
  - Also is npcs good enough to hav elike 15 in a server? And it runs fine? For liek team death match and stuff   
- Npclist  
  - Lists all npcs,   
    - 1: npc-bob  
    - 2: npc-ben  
    - 3: npc-carson  
    - 4: npc-david  
- Npcteam (n) (team, red/blue)  
  - Npcteam 3 red  
    - Means npc-carson is now on team red  
  - Npcteam 1 blue  
    - Means npc-bob is now on team blue  
  - They stay on these teams until they are deleted  
- Npcdelete 1  
  - Means npc-bob is deleted  
- Npcdelete 1,2,3  
  - Means npc-bob, npc-ben, npc-carson is deleted 

Commands, for host or player

- Changeteam  
  - For now behavior, u can join whatever team u want to join, ever. No team limits, etc.  
  - Ideal behavior: the teams are equally sorted by skill.  
    - If u get like 2 kills per whole 5 min round, u are gonna go on the team with  better plauers to balance it out  
    - If u get like 25 kills anad u win games by urself, u are gonna get a team with not so good players  
    - I know, it sucks, and def needs a better solution, bc it should not be their fault that u lose, it should be your fault, but this is a casual ish mode for now, so its fine. Later we have better team sorting, or no skill taken into consideration at all, idk.   
  - Teams for now: red, blue  
  - If you just write “changeteam” \= prints all teams that u can switch to, and how many people are on each team, and their usernames  
  - “changeteam red” \= you requests to join the red team

- 

Weapon set

- Todo explain this 9 2 2026 0848   
- And also the hotbar behavior 

Full ideal flow of behavior

- Start mimita.exe, im on teh main menu screen  
- I press play   
  - I instant go to the communitt server browser, and also community server creator. Bc thats all in 1 screen  
  - I do not go to duels queue, that button is not even in the gui at all.  
- I make a server with these settings  
  - Name: my super cool server  
  - Gamemode: team deathmatch  
  - Weapon set: rocket launcher only   
  - Map rotation: enabled  
    - Auto picks a random map to rotate to  
  - Notify discord that the server started: enable  
    - Todo fix this so online servers are shown to be online, not just left there for no reason 9 2 2026 0853  
  - Map: xxity2 because its a new cool map  
  - Privacy: public  
    - Private \= just a password needed to join  
    - Todo make more cool later but for now thats cool  
  - Npcs spawned: true, just 1 spawned  
- I press start server  
- Im instantly in the server, and im getting shot at by the npc  
- Im fighting the npc using the rocket launcher, bc the npc onlt uses the rocket launcher and i can only use the rocket launcher  
  - Which is the only weapon in my hotbar, becasue the weapon set is rocket launcher, so thats the only one i can even equip.  
  - And its on slot 1 , not slot 7 like it was before   
- As soon as i join thte server, because i picked team deathamtch as the mode for the server, i see  
  - “Mode: team deathmatch, intermission: 15”  
  - “Mode: team deathmatch, intermission : 14”  
  - Etc  
  - Counting down , in big text in the center of my screen, not in the chat, because the chat is not big enough  
- As soon as the 3,2,1, countdown before the gamemode starts, teams are picked.  
  - For example, previous player list, all just on the same team/no team at all  
    - Jorj1357  
    - Admin  
    - Jojo  
    - Rickets  
    - Andrew  
    - Npc-derek  
    - Npc-jimbob  
    - Npc-smoothmovement  
  - After team picking  
    - RED  
      - Jorj1357  
      - Jojo  
      - Npc-derek  
      - npc-jimbob  
    - BLUE  
      - Admin  
      - Rickets  
      - Andrew  
      - npc-smoothmovement  
- intermission timer  
  - It counts down from 3, to 0 , with the 2nd decimal place.  
  - So its like 3.00, 2.99, 2.75, 2.17, 1.53, 0.85, 0.21  
  - As soon as it gets to 0 it is replaced with “GO\!\!\!”  
- Then, team death match starts.   
  - A timer at the top , that does not obscure other gui elements at the top of the screen, has the match time, for example 5 min, and counts down   
    - Hh:mm:ss.miliseconds  
  - So like  
    - 00:04:59.982  
    - 00:02:12.255  
    - 00:00:15.174  
    - Etc etc   
  - Also, it starts \= every single person regardless of team, is spawned at 1\. The same spawn point, with a random offset alongn x/y , because x/y is thehorizontal plane in this engine, z is up in this engine   
  - For the rest of the match, everyone spawns at taht same spawn point, with a random offset, and instant respawns  
    - Every single time  
    - I know this is unbalanced  
    - We will add forcefields or some sort of thing to push you out like  
    - Spawn pos \= x  
    - Your random offset \= \+1x \+2y  
    - So , the exact line between spawn pos and your random offset \= your push out direction. So you get shot out from it and are hard to predict where ur gonna go , ties into movement of the game  
- Gui   
  - At the top of the screen   
    - RED on left  
    - BLUE on right  
    - “RED: 12K                        BLUE: 10K”  
  - When u get a kill   
    - You personallt get the popup reward of like   
      - If plauer kill  
        - \+100 xp in turqoise  
        - \+50 gold in gold color  
        - This is tracked to ur account   
      - If npc kill  
        - \+10 xp in turquoise  
        - \+0 gold in gold  
        - This is also tracked to ur account  
      - If no account  
        - No popup, bc nothing to track?  
        - Or do a popup of \+0 gold \+0xp, because its satisfying and gets u used to it  
    - Your team gets a green “+1”   
      - It starts at the kills part of the gui, like   
      - If ur on blue  
      - As soon as the kill is confirmed  
      - Show a little green “+1” under the team’s total kills , it shows at like 0.5 alpha 50% opaque, then fades to 0.0 alpha 0% opaque over 30 ticks. This is json editable, and should be controlled by a generalized json function   
  - When you press tab  
    - RED  
      - All players on red  
      - All players kills, deaths, assists  
      - Top \= most kills  
    - BLUE  
      - All players on blue  
      - All players kills, deaths, assists  
      - Top \= most kills   
- Win conditions  
  - Either, kill enouhg epople on the other team to get to 30 (?) i think thats the score u need  
  - Or, have more score than the other team by the time the timer is done  
- When a round is won  
  - Show “RED TEAM WINS\!\!\!\!”  
  - Or “BLUE TEAM WINS\!\!\!\!”  
    - Show how many times that team has won in this server so far  
    - E.g. a new ish server will have like, red team’s total wins: 3  
    - Blue team’s total wins: 2  
    - For now, 9 2 2026 0916 bc i cant think of the right gui places to put that, just put that in the chat every time that the game is over  
  - Show all players on that team, like put their scores in the gui as well, so itll be like  
  - At the top: “RED TEAM WINS\!\!\!\!”  
    - Jorj1357: 13K, 5A, 8D  
    - Admin: 10K, 1A, 2D  
    - Etc, for all players, and npcs too   
  - This screen is shown for 5 seconds, with a timer counting down how long its shown,   
  - Then we immediately repeat from the intermission step above, where it starts at 15 seconds.   
- This repeats indefinitely  
  - The entire flow should repeat indefinitely. I should be able to
