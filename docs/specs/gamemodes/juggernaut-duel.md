# Juggernaut Duel

## Purpose and audience

This specification defines the selectable Juggernaut Duel community mode for
players and contributors. It records the desired player experience, existing
owners, required extensions, and acceptance evidence.

## Does not

- Change the existing team-based **Juggernaut** mode.
- Add a new movement, weapon, damage, or NPC combat implementation.
- Use finite team tickets or end the match when one actor dies.

## Player experience

- **Juggernaut Duel** is a separate community mode beside Juggernaut, Team
  Deathmatch, and the other selectable modes.
- The match has two teams and accepts at most two human players.
- Both competitors use the existing Juggernaut movement and
  `juggernaut_mode` weapon set through the shared `juggernaut_arcade` actor
  preset.
- Team 1 is forced to the Jason avatar. Team 2 is forced to the Clown avatar.
- With one human connected, one Juggernaut NPC fills the opposing team and
  fights using that team's Juggernaut role, actor preset, movement, weapons,
  and forced avatar. When a second human joins, the mode NPC is removed and
  the two humans take opposite teams at the next intermission. Do not create an
  NPC when two humans are active.
- Joining or starting a round enters a 3-second intermission, then the shared
  `3, 2, 1, GO!` countdown. Scoring and combat begin only after GO.
- During active play, deaths respawn immediately. There is no finite lives
  counter; the score limit decides the round.
- The first competitor to 20 kills against the opposing competitor wins. A
  human or the NPC can win. The results screen names the winner, then the mode
  returns to a 3-second intermission and repeats with scores reset.
- Match scoring is per competitor. A kill on the same team must not count.

## Dev Loop launch

The mode is available as Dev Loop launch mode **13** (`juggernaut_duel`) in
`devscripts/dev-launch-modes.json`. It starts the server with both
`--mode juggernaut_duel` and `--gamemode juggernaut_duel`, disables the
default NPC population so the mode can create its own team-roster opponent,
and selects `dust2cyberiav4`.

To select it in the running Dev Loop, press `2`, enter `13`, and press Enter;
then press `1` to launch the selected mode. To start the loop with this mode
and have it build and launch automatically, run:

```powershell
python devscripts/dev-loop.py --profile npc-navigation --launch-mode 13 --auto-restart
```

If the Dev Loop daemon is already running, select and launch mode 13 through its
control endpoint:

```powershell
python devscripts/dev-loop.py --request launch --launch-mode 13
```

Selecting a mode changes the next server/client launch. In the interactive
loop, press `2`, enter `13`, and press Enter, then press `1` to launch it.

## Implementation and owner map

The mode uses the shared gamemode, role, actor, scoring, respawn, and HUD
systems. Duel-specific behavior is selected by configuration and the generic
team-roster policy.

| Need | Existing place | Current fit |
|---|---|---|
| Mode selection and cap | `config/onlinemodes.json`; `CommunityMode.maxPlayers` in `src/network/community-server-config.h`; parsing in `community-server-config.cpp`; join enforcement in `handleJoinRequest` (`src/network/server-packets.cpp`); capacity in `src/network/server.cpp` and `src/gui/gui-main.cpp` | Separate picker item. The cap applies to human joins and host/advertised capacity. |
| Team roles and profile | `config/gamemodes/juggernaut_duel.json`; `config/roles.json`; `assignMatchParticipants` and `serverResolveActorSpawnProfile` in `src/network/server-gamemode.cpp` | Both roles use movement `juggernaut`, actor preset `juggernaut_arcade`, weapon set `juggernaut_mode`, and their forced team avatar. |
| Solo NPC roster | `reconcileTeamRosterNpc` in `src/network/server-gamemode.cpp`; NPC adoption and respawn in `src/network/server-npcs.cpp` | `npc_spawn.policy: team_roster` creates one opposing NPC for one human and removes it when two humans play. |
| Round lifecycle and respawn | `updateGamemodeLifecycle`, `serverMatchRespawnsEnabled`, and `serverMatchRespawnSeconds` in `src/network/server-gamemode.cpp`; NPC respawn in `src/network/server-npcs.cpp` | Empty lobby waits; first join starts intermission; countdown and GO start scoring; active deaths use the shared immediate respawn lifecycle. |
| NPC aim and combat behavior | `config/roles.json` role `juggernaut_duel_clown` / `juggernaut_duel_jason`; `config/behavior-profiles.json` profile `juggernaut_limited`; profile resolution in `resolveNpcBehaviorProfileId` / `serverResolveActorSpawnProfile` (`src/network/server-gamemode.cpp`) and NPC think/attack in `src/network/server-npcs.cpp` | Duel NPC needs a Duel-specific accuracy/reaction tuning owner; do not silently make the shared Juggernaut role more accurate in every mode. Preserve perception, line-of-sight, weapon, cooldown, and authoritative hit validation. |
| Score, result, and repeat | `checkMatchWinConditions`, `updateGamemodeLifecycle`, and `broadcastDuelState` in `src/network/server-gamemode.cpp`; `CommunityMatchClient::onState` | JSON-selected `first_to_score` awards opposing kills, records winner, shows results, resets scores, and repeats. |
| HUD | `config/gui/gamemode-meta-gui.json`; `MatchLeaderboard` in `src/gui/hud/match-leaderboard.cpp`; `CommunityMatchClient::onState` | Shared leaderboard displays competitor scores, configured goal, and winner. |
| Attack acceptance and confirmed damage | `handleAttackRequest` in `src/network/server-attack.cpp`; legacy `handleShotRequest` in `src/network/server-packet-handlers.cpp`; packet dispatch in `src/network/server-packets.cpp`; attack-result reconciliation in `src/engine/engine-tick-net.cpp` | The mode must use the normal authoritative weapon path. A predicted shot/hit is not proof of accepted attack or damage; the server must accept the request, apply enemy damage, and replicate health. See the attack rejection flows in `docs/specs/weapons/weapons.md` and `docs/specs/networking/networking.md`. |

## Mode contracts

1. `juggernaut_duel` is in the community catalog with `max_players: 2`;
   `CommunityMode` parses the cap, the server enforces it on joins, and launch
   and advertised capacity are clamped to it.
2. `config/gamemodes/juggernaut_duel.json` defines two teams of capacity one,
   compatible team roles forcing Jason/Clown while sharing
   `juggernaut_arcade`, `juggernaut_mode`, a score limit of 20, immediate
   respawn, 3-second intermission, 3-second countdown, GO presentation, and a
   3-second result display.
3. The generic `first_to_score` win rule is selected from gamemode JSON. It counts
   confirmed opposing kills for human and NPC competitors, stores a winner actor
   ID, and ends the round on the first score reaching the configured goal. Do
   not branch on the `juggernaut_duel` ID.
4. The shared team-roster policy gives one human exactly one opposing
   Juggernaut NPC; two humans are placed on opposite teams and no opponent NPC
   remains. The NPC must use the same participant, team-role, damage, score,
   avatar, loadout, and respawn paths.
5. Competitor scores and winner actor ID replicate through existing match-state
   data and render them through the shared, JSON-positioned gamemode HUD. Once
   the results interval expires, reset scores and restart at intermission.
6. The solo Juggernaut NPC should be substantially more accurate against its
   one human opponent than the current `juggernaut_limited` profile, whose
   configured `aim_error_deg` is 10.0 and `reaction_delay` is 0.55 seconds.
   Give this tuning a Duel-specific profile/override so standard Juggernaut
   behavior does not change as a side effect. The intended improvement is
   steadier target tracking, lower aim error, and a shorter but nonzero reaction
   delay. Keep weapon cadence and weapon-defined spread separate from NPC aim
   error; do not remove legitimate weapon spread to make the NPC accurate.
7. Accuracy must come from the NPC's permitted perception and observed target
   state. It may aim at a visible target and predict from already observed
   movement/velocity, but it must not read future player input, see through
   geometry, retain forbidden target information, bypass weapon/cooldown rules,
   or bypass authoritative shot tracing and damage. Follow the shared NPC
   behavior rules in `docs/architecture/player-npc-systems/npc-movement.md`.
8. Active Duel deaths have no intentional waiting period: resolve the shared
   respawn at the first lifecycle-safe fixed-tick boundary after death is
   confirmed. Do not wait for a death animation, storage operation, or a second
   user action. The local player should show the respawn as soon as death is
   confirmed locally; server authority still owns the spawn and life generation.
   At 60 Hz, one simulation tick (about 16.7 ms) is the maximum server-side
   scheduling delay, excluding transport and rendering latency. NPC opponents
   follow the same immediate-life intent. The current `respawn_seconds: 0.01`
   is a timer value, not a guarantee of a 10 ms respawn, and should become the
   shared zero-delay representation when implementation is updated.

### Acceptance additions: accuracy and time spent dead

- Compare the Duel NPC against a human-controlled target with the same weapons
  and map, using the same visible range and movement scenarios before and after
  tuning. Record shots fired, shots accepted by authoritative tracing, hits,
  target visibility/occlusion, aim error, and reaction delay. The Duel profile
  must show a clear accuracy improvement over `juggernaut_limited` without
  firing through cover or using unavailable information.
- Exercise close, medium, and long range; a moving target; a target moving behind
  cover; and a death/respawn transition. Verify weapon spread/cadence, cooldown,
  and damage validation remain active.
- For both human and NPC deaths during ACTIVE, inspect the live journal from
  confirmed death through the new life generation. There must be no configured
  dead-time timer, death-animation wait, or missed respawn transition. The
  authoritative life should return at the first eligible fixed tick; report
  server scheduling, network delivery, and visible client delay separately.
- Verify the respawn does not carry stale health, weapon pose, cooldown,
  prediction, or prior-life packets into the new life, using the existing
  spawn-generation contract.

### Storage question: unresolved scope

The request mentions “storage issues,” but does not identify what is stored or
what failure is happening. The current Duel specification has no progression or
round-result persistence requirement; its score and respawn/life state are
match-runtime state. Do not add a database, save-file, inventory, or persistence
change based on this phrase alone. **NEEDS_SPEC_DECISION:** clarify whether this
means saved player progress, inventory/loadout persistence, game files/disk
space, or that death/respawn state is incorrectly being saved/restored. If the
intended issue is that a dead state survives longer than intended, the immediate
respawn contract above addresses match-runtime behavior; it does not imply a
durable save-system change.

## Acceptance

Source/configuration acceptance establishes the picker entry, the mode 13 Dev
Loop launch, a hard two-human join cap, the correct role/avatar/loadout on both
teams, solo NPC roster and two-human roster behavior, and JSON-selected
first-to-20 scoring. Build success does not prove configuration load or
gameplay. Runtime evidence must show
`INTERMISSION (3s) → COUNTDOWN (3, 2, 1) → GO → ACTIVE`, an immediate respawn
after death during ACTIVE, accepted attacks that apply enemy damage, the correct
winner at 20, and a fresh repeating intermission with reset scores. Multiplayer,
NPC combat, avatar appearance, and human gameplay feel remain separate review
evidence.

## Reported incomplete implementation attempt: 2026-10-16 00:24 (Jorj)

Jorj reports that an implementation attempt got substantially far, but firing
still produced a hit-rejected result (“all hits are rejected”). The user gave
the attempt time and attribution; the journal below independently captures
matching Juggernaut Duel attack rejections. The result is an unresolved blocker,
not completed combat acceptance.

The current server also has distinct rejection gates in `handleAttackRequest`
(`src/network/server-attack.cpp`): inactive spawn state, unknown or disallowed
weapon, stale spawn generation, dead shooter, slot mismatch, cooldown, and
invalid hitscan geometry can reject an attack request. A request can be
accepted while its claimed target hit is separately rejected by the
authoritative trace. These are different outcomes and must not be collapsed
into a generic “hit rejected” diagnosis.

The relevant existing requirements are the attack-rejection and prediction
correction flows in `docs/specs/weapons/weapons.md` (“Attack rejection flow”)
and `docs/specs/networking/networking.md` (“40. Request rejection” and “45.4
Hitscan weapons”). For Juggernaut Duel, acceptance must distinguish:

1. whether the server accepted the attack request;
2. whether the authoritative trace accepted the claimed target hit;
3. whether authoritative enemy health changed; and
4. whether the client received and applied the confirmed damage.

Correlate one request/shot identifier through client prediction, server receive
and validation, hit verdict/rejection reason, authoritative damage, and client
health replication. Preserve the exact first rejection reason. Do not widen hit
geometry or weaken server validation until that chain identifies the first
divergence.

### Evidence from the supplied live journal

`logs/2026-10-16/20261016_042126/events.jsonl` contains 321
`weapon.attack.slot_mismatch` events associated with `mode: juggernaut_duel`
and participant `actor_weapon_set: 11`. They cover the role's three listed
weapons: 287 Large Machine Gun requests, 26 Heavy Shotgun requests, and 8
Force Punch requests. For example, request 61 for `large_machine_gun` asked for
logical slot 2 while the server expected slot 9; the same event reports mode
weapon set 1 and actor weapon set 11. Heavy Shotgun requests also mismatch
(requested slot 1, expected slot 24).

The source path explains this divergence. `config/roles.json` assigns the
Juggernaut roles weapon set 11, while `config/gamemodes/juggernaut_duel.json`
does not select a mode weapon set and the loaded mode state reports set 1.
`serverCommunityWeaponLogicalSlot(const ServerPlayer&, ...)` in
`src/network/server-gamemode.cpp` consults the participant's role set only for
objective-round modes; this score-limit mode falls back to the global mode set.
`handleAttackRequest` in `src/network/server-attack.cpp` compares the request
slot with that result and sends a rejected attack result on mismatch. This is a
specific confirmed cause for the logged Juggernaut role weapon requests.

Networking TODO: give role-based score-limit modes one consistent weapon-set
owner for loadout assignment, allowed-weapon checks, logical/native slot
mapping, equip requests, and attack validation. Evaluate selecting set 11 in
the mode JSON versus making the shared resolver honor the participant role set
for these modes. Then repeat the real Duel scenario and prove each weapon's
attack result is accepted, enemy health changes authoritatively, and the
confirmed health reaches the client. Do not call the mode combat-complete until
that live check passes.

### Follow-up investigation: slot identity and rejection path

Jorj reports another networking-fix attempt on **2026-10-16 00:34**. The
specific source edits and a run tied to that timestamp were not independently
identified, so this is recorded as a reported attempt, not evidence that a fix
landed. Keep it distinct from the reported 00:24 attempt above and from the
04:21:26 log file timestamp below.

The word “slot” currently refers to three different values; they are not
interchangeable:

| Value | Meaning | Example / owner |
|---|---|---|
| Logical weapon-set slot | Ordered position in the active community weapon set; the equip request uses this selection. | `weaponsets.json`, set 11; LMG appears at logical slot 2 in the observed request. |
| Native weapon slot | `WeaponDefinition::slot`, used by runtime weapon storage and older equipment paths. | The observed LMG definition reports native slot 17; Heavy Shotgun reports 24. |
| Weapon identity | Stable definition/network identity for the actual weapon being fired. | `weaponDefNetworkId` in `AttackRequestPacket`, resolved to a weapon definition on the server. |

An ordered slot is useful for hotbar selection, input, equip intent, and
checking that a player can select a weapon from their assigned set. It can
change if set ordering changes, so it should not be the weapon's durable
identity. The server should not accept an arbitrary client-supplied weapon
identity either: it still needs to establish that the role owns the weapon and
that it is currently equipped according to server state. Attack validation
uses stable weapon identity, confirms membership in the participant's assigned
weapon set, and compares the weapon definition's native slot with the server's
equipped native slot. It does not use the request's client slot as authority.
Equip intent still uses a set position; for older clients, a native slot is
accepted only if it resolves to a weapon in the participant's set. Convert to
native slot at the runtime boundary.

The current request/validation path is:

1. `mpSendAttackRequest` in `src/network/multiplayer-packets.cpp` writes both
   `weaponDefNetworkId` and `equippedSlot` into `AttackRequestPacket`, then
   stores both in pending prediction state. The slot is not the only identity
   sent.
2. Equip intent is checked in `applyEquipIntentFromInput` in
   `src/network/server-packets.cpp` resolves a logical position against the
   player's set, then converts it to the native slot stored in
   `player.equippedSlot`. A native-slot compatibility form is accepted only
   when the player's set owns the resolved weapon.
3. `handleAttackRequest` in `src/network/server-attack.cpp` resolves the
   packet's weapon identity and, after the fix, checks that the identity is
   allowed by the player's assigned set and matches the server's equipped
   native slot. The client slot field is no longer used to authorize firing.
   The previous comparison emitted `weapon.attack.slot_mismatch` and returned
   before authoritative shot tracing; that explained the supplied log but is
   no longer the attack contract.
4. The player-aware `serverCommunityWeaponLogicalSlot` in
   `src/network/server-gamemode.cpp` previously looked up `player.weaponSetId`
   only when `state.objectiveRounds` was true. Duel uses score-limit victory,
   so it fell back to the global mode weapon set. In the supplied log, that was
   set 1, while the role loadout and `actor_weapon_set` were set 11.

### Follow-up live check: initial equipped weapon is not synchronized

The approved Duel run on build 2427 (`run_id: 20261016_050752`) reached the
server but did not reach attack validation. Its journal records 2,411
`weapon.equip.rejected` events in about 46 seconds. They all carry
`requested_slot: 10`, `resolved_native_slot: -1`, `weapon_set: 11`, and reason
`not-owned-or-unknown`. Slot 10 is the native HAFS weapon slot; it is not a
logical slot in Duel's set 11. No `weapon.attack` outcome was recorded, so this
run does not prove the slot-agnostic attack fix or hit acceptance.

The source path reveals two sides of one missing initial-equipped-weapon
contract:

1. `serverResolveActorSpawnProfile` resolves `startingWeapon`, but
   `resetPlayerForSpawn` in `src/network/server-players.cpp` rebuilds owned
   weapons/runtimes without setting `ServerPlayer::equippedSlot` from it. The
   Duel role declares `heavy_shotgun` as its starting weapon.
2. `PlayerRespawnedPacket` (`src/network/packets.h`) carries the authoritative
   community weapon-set ID and runtime/ammo entries, but no active equipped
   weapon identity or slot. `applyAuthoritativeSpawn` in
   `src/network/multiplayer-tick.cpp` therefore cannot synchronize it.
3. `engine-tick-net.cpp` sends `player.equippedSlot`, converting it to a
   logical slot only if the current local weapon belongs to the active set.
   Its stale HAFS selection remains native slot 10.
4. `applyEquipIntentFromInput` in `src/network/server-packets.cpp` correctly
   rejects slot 10 because it does not resolve to a set-11 weapon. The server
   and client consequently have no shared starting equipped weapon; repeated
   retries flood the journal and client attack identity is not a Duel-owned
   weapon.

Required implementation before another combat acceptance run:

- On every authoritative player spawn, set the server equipped weapon from the
  role's `starting_weapon` after confirming it belongs to the player's active
  set and initialized inventory. Include that active weapon definition identity
  in reliable spawn state alongside the existing loadout/set ID; apply it
  through the normal client equip owner before sending the next input. Do not
  infer the active weapon from unordered inventory entries or trust an
  unverified client slot.
- Keep invalid equip requests rejected, but rate-limit or deduplicate repeated
  diagnostics for an unchanged invalid selection; do not authorize the stale
  weapon to quiet the log.
- In the real Duel run, verify the client selects the server's starting
  Heavy Shotgun from set 11, equip requests resolve to its native slot 24,
  subsequent set selections map correctly, and attack requests reach the
  authoritative trace. Then verify confirmed enemy damage and health
  replication separately.

The journal evidence is concrete: 321 `weapon.attack.slot_mismatch` events
for Duel/set 11 included LMG requests with requested logical slot 2 versus
expected slot 9, and Heavy Shotgun requests with requested slot 1 versus
expected slot 24. The 24/17 values are native definition slots, while 1/2/9
are logical positions. Comparing across those namespaces is invalid. The
logged mismatch does **not** prove every attack in every run fails for this
reason; it proves this rejection for those requests. A separate accepted
request can still later miss the server trace or be rejected by another
validation gate.

**Should attack validation ignore the slot entirely?** It now ignores the
client slot for attack authorization while keeping server-side ownership and
equipped-state validation:

- Resolve the packet weapon identity to a registered definition.
- Confirm that weapon belongs to the participant's authoritative role/loadout
  set and its native definition slot matches the server's current equipped
  native slot.
- Do not compare `AttackRequestPacket.equippedSlot` with a logical position or
  native slot; it is client selection metadata, not attack identity.
- For equip input, resolve a logical position against the participant's set.
  Accept a native-slot compatibility form only when the resulting weapon
  belongs to that same set, then store the native slot.
- Keep the independent authoritative trace, cooldown, spawn, and damage
  validations intact.

The shared player-aware resolver now applies to role-based score-limit modes;
it is not a Duel-ID exception. Since both Duel roles share set 11, the mode
also selects set 11 and Dev Loop mode 13 passes `--weapon-set 11` so the client
and server start with the same ordered set. The packet slot field remains for
protocol compatibility and equip/prediction metadata, but it no longer decides
whether an attack identity is valid.

The exact “ALL HITS REJECTED” phrase in the legacy `handleShotRequest` debug
branch remains separate. That branch currently returns before the phrase and
`config/serverdisagree.json` has `rejectAllHits: false`; the supplied Duel
requests instead show the structured `slot_mismatch` reason. Do not use the
legacy phrase alone as the diagnosis.

### Work required to make the mode work

The shared role-set resolution fix is now implemented in source:
`serverCommunityWeaponAllowed(const ServerPlayer&, ...)`,
`serverCommunityWeaponNativeSlot(const ServerPlayer&, ...)`, and
`serverCommunityWeaponLogicalSlot(const ServerPlayer&, ...)` use the player's
assigned set whenever one exists, rather than restricting that behavior to
objective-round matches. Attack validation, equip validation, and generic
physical-contact weapon selection now use the player-aware permission check.
This keeps role loadout membership authoritative while making logical slot
lookup, native conversion, and permission checks agree. Players without an
assigned set retain the previous mode-level behavior.

`handleAttackRequest` now validates the stable weapon identity against the
role-set permission and the server's currently equipped native slot; it no
longer rejects based on the request's slot field. Equip intent in
`src/network/server-packets.cpp` resolves the role-set logical position first,
then permits a native-slot form only when that weapon belongs to the same role
set. The first follow-up run showed slot-10 equip input while the server held
role set 11; this compatibility path preserves ownership checks while handling
that client representation.

Juggernaut Duel's two roles intentionally share set 11. The mode config now
also selects set 11 as its mode-level weapon set, and Dev Loop mode 13 passes
`--weapon-set 11` so the client and server start with the same ordered set
before per-participant role state arrives. Runtime `--versioninfo` and journal
checks still need to confirm that the launched process loads this configuration
and accepts equip/attack requests.

1. Rebuild with the canonical Dev Loop, launch mode 13, capture that
   executable's `EVENTS_JSONL_PATH` with `--versioninfo`, and use the real Duel
   scenario. Confirm requests for Heavy Shotgun, Large Machine Gun, and Force
   Punch pass equipped-identity/ownership validation, then separately confirm authoritative
   enemy damage and client health replication. Inspect the first failure if
   any; do not infer trace failure from a pre-trace slot rejection.
2. After combat acceptance, exercise both roster paths (one human plus the
   opposing NPC; two humans on opposite teams and no NPC), the two-human join
   cap, team avatars/loadouts, immediate active-round respawn, first-to-20
   winner/results, score reset, and repeating 3-second intermission/countdown.
   Record source/config, build, live-journal, multiplayer, and human-visible
   evidence separately.
3. Add bounded structured fields to the attack outcome path only if the live
   journal cannot correlate one `request_id` with weapon identity, role/mode
   set IDs, requested and resolved slots, equipped state, validation reason,
   trace verdict, damage, and client reconciliation. Reuse categorized
   `StructuredLogger` events; don't create unmanaged log files.

Before the change, the concrete pre-trace slot mismatch was confirmed from the
supplied journal and source. The source alignment is implemented. The live
follow-up still needs accepted attacks, authoritative damage, and client health
replication before combat can be considered verified.

The exact phrase “ALL HITS REJECTED” is also present in a legacy debug branch in
`handleShotRequest` (`src/network/server-packet-handlers.cpp`), but that
function currently returns before reaching the branch, and
`config/serverdisagree.json` has `rejectAllHits: false`. The supplied journal's
actual pre-fix Juggernaut Duel rejection reason was `slot_mismatch`.

## Evidence recorded during implementation

- Source and configuration define the mode, role pair, two-human cap,
  team-roster NPC policy, immediate respawn, first-to-20 scoring, result loop,
  and configured HUD.
- The canonical dev loop published build 2413 from the current source
  generation. Its server journal at
  `logs/2026-10-16/20261016_041202/events.jsonl` records the selected mode,
  friendly-fire and NPC policy, creation of solo actor 200000 on team 1, and
  role resolution for the human Jason and NPC Clown using the Juggernaut actor
  and movement presets. `--versioninfo` was run against that exact executable.
- The live check used one connected human and verified solo opponent creation
  and profile resolution. It did not exercise a second human, death and
  respawn, accepted attack and damage, 20 kills, visible HUD/avatar appearance,
  or repeated rounds; those still need multiplayer gameplay review. The
  reported hit-rejection issue is unresolved.
- The later supplied journal identifies a role/mode weapon-set slot mismatch,
  but does not establish that a correction has been implemented. At follow-up,
  the active Dev Loop was on launch mode 1 (`current`, sandbox), not mode 13;
  its tracked server was dead and a separate sandbox server process was active.
  `versioninfo` confirmed the live client executable and journal identity. No
  mode switch or process replacement was performed, so corrected Duel combat
  still needs an isolated live run.
