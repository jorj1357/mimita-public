# Healthall 500 left lethal body-part health at 100

- Recorded: 2026-11-07T15:44:30Z
- Status: MODE-SCOPED FIX IN SOURCE; BUILD AND RUNTIME VALIDATION IN PROGRESS
- Related: [healthall-live-npc-REG-2026-11-06.md](healthall-live-npc-REG-2026-11-06.md)
- Changelog: [20261107_154430-healthall-fcs-runtime-investigation.md](../../changelog/2026-11-07/20261107_154430-healthall-fcs-runtime-investigation.md)

## Expected behavior

When a player uses `healthall 500`, the server, client, and shared damage/death
rules should agree on what 500 HP means. Actors should not die after only
roughly 100 points of accumulated body-part damage while the global health
counter is still hundreds of points above zero, unless that separate lethal
body-part rule is an intentional mode rule clearly communicated to players.

## What the live journal proves

Run `20261107_154026_01` used build
`2026-11-07T15:40:16.360512Z:032addb5b338935afeda24da592c38af785dc5e33316a71888a53fb24b1adb77`.
The server accepted `healthall 500` at sequence 7160, tick 2384. Per-actor
`server.healthall.actor.applied` rows at sequences 7161–7180 cover both players
and all 18 NPCs; every row records `current_hp=500`, `max_health=500`, and
`dead=false`, and all NPC rows also record `mirror_health=500`. The summary at
sequence 7181 records `players_healed=2`, `npcs_healed=18`, and
`override_hp=500`. Client 01 sequence 3370 receives actor 1 at 500/500 after
98/100; client 02 sequence 3423 receives actor 2 at 500/500 after 100/100.

The same run then proves the mismatch between global HP and lethal body-part
health. For client actor 1, server damage sequence 7977 records a 15-point
hitscan hit taking global health from 405 to 0, with `body_part=torso`,
`part_health_after=0`, `part_destroyed=true`, and `killed=true`. Client 01
sequence 3640 independently records that local death as `health_before=405`,
`damage=15`, `health_after=0`. Earlier hits in that sequence drained the torso
part pool while global health fell only from 500 to 405.

Other damage in this run also shows that individual hits can be much larger
than the displayed “100” impression: server sequence 8860 logs a
`grenade_explosion` hit of 457, taking actor 1 from 485 to 28 and destroying a
right arm; sequence 73810 later logs a 1000-point explosion killing actor 2.
Those are separate from the torso-destruction case.

The dev-loop run was stopped after evidence collection. Journals remain at:

- Server: `logs/2026-11-07/20261107_154026_01/server-01/events.jsonl`
- Client 01: `logs/2026-11-07/20261107_154026_01/client-01/events.jsonl`
- Client 02: `logs/2026-11-07/20261107_154026_01/client-02/events.jsonl`

## First incorrect step

`handleServerCommand` in `src/network/server-packet-chat.cpp` correctly updates
player current/max HP and each live NPC's authoritative body current/max HP.
It does not reset `ActorBodyState::partHealth`. The body-part config in
`config/actor-body.json` sets `part_max_health` to 100. The shared damage owner
`applyPlayerDamageLegacy` in `src/network/server-damage.cpp` applies every hit
to both global health and the selected body part; when a head or torso is
fully destroyed, `lethalPartDestroyed()` forces global health to zero. The
same lethal-part check is present in the NPC damage path. Thus the HUD can
truthfully display 500/500 immediately after the command while the torso/head
still has at most 100 independent hit points.

The logs do not show a single ordinary 100-point hit killing actor 1. They show
multiple hits accumulating against the torso, then a final 15-point hit that
breaks it and kills the actor at 405 global HP. Separately, this mode's logged
grenade explosions include 457- and 1000-point damage events.

## Resolution chosen

The mode owns whether actor body parts are destructible. The masterlist now
defines `features.destructible_body_parts` as a Boolean with default `false`.
Only Bloodtest sets it to `true`; Fun Counter Strike and all other modes use
`false`. The shared damage owner applies per-part health and lethal head/torso
rules only when this mode feature is enabled. With it off, weapon hits still
reduce ordinary actor HP, but cannot break a part or kill an actor while actor
HP remains.

This does not change the meaning of `healthall`: it updates actor current/max
HP, while mode-specific body-part destruction is controlled separately.
Validation and live per-mode evidence are recorded in the session changelog.
