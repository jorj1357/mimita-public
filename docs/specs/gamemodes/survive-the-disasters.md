# Survive the Disasters

**Purpose:** specify a casual, social survival mode built around short,
distinct disaster rounds.
**Audience:** gamemode, scripting/modding, physics, map, progression, audio, and
network contributors.
**Status:** desired redesign; disaster roster, map content, and balance need
playtesting. The current Survive the Disasters config describes a different
random-weapon, last-actor-standing mode.
**Does not:** require competitive ranking, community Lua execution in the
first playable version, or a separate lobby/social application.

## Core experience

Survive the Disasters is an arcade movement playground. Players see the next
disaster announced, get a short countdown, then try to survive a bounded event
with other players. The tone is playful and social; rounds should be tense or
silly without making the whole mode feel relentlessly fast.

## Match and round flow

Use the shared gamemode lifecycle. Starting targets for tuning are:

1. Initial intermission: 30 seconds.
2. Show the disaster name, then a 3-2-1-GO countdown.
3. Release players and begin the disaster on GO.
4. Run the disaster for a configured duration, initially chosen from 15, 30,
   60, 90, or 120 seconds.
5. Show how many players survived and identify them.
6. Use a short 15-second intermission before the next round.
7. Play ten rounds, then show match results, restore destructible map state,
   and start a new 30-second intermission.

Each round selects one disaster at random from the enabled, valid roster for
the current map. Invalid or unavailable entries are skipped with a reason;
they must not strand the lifecycle between rounds.

Times and round count are defaults for playtesting and remain mode data, not
hard-coded lifecycle branches. Gameplay timers use the fixed simulation tick;
the shared match lifecycle retains its documented countdown timing behavior.
Actors are held during countdown and released at GO.

## Disasters and world state

Disasters are authored behaviors that can spawn hazards, move geometry,
change safe areas, or alter the map. Reuse shared volume and physical-object
queries for spawn regions, safe zones, hoops, and triggers. Disasters must
declare their duration, affected region, cleanup behavior, and whether damage
to the world persists into later rounds.

Destructible world is enabled by the shared default. This mode preserves
damage and destruction across its ten rounds and restores the map to its
authored baseline between matches. The reset is owned by the destructible
world system, not a disaster-specific mesh rebuild.

## Community-created disaster packs

The long-term goal is to let creators publish disaster packs containing a
descriptor, assets, and bounded behavior. The descriptor should expose the
title, duration, presentation, and behavior inputs to the shared mode runtime.
Community Lua scripting is a future capability: arbitrary scripts must not
run until the project defines a sandbox, API permissions, resource limits,
network authority, and safe failure behavior. Early packs may use the
data-driven behavior surface that the engine already supports.

## Local mode points

The first progression pass is casual and local:

- Award 100 mode points for each disaster round survived.
- Keep these points separate from account-level kills, gold, XP, and public
  leaderboards.
- Store them on the player's device as unverified local progress. Do not sync
  or rank them until an integrity and trust model is designed.
- Optional upgrades may later cost example amounts such as 10 or 50 points.
  The first version must define the upgrade list and balance before purchases
  are implemented; no paid power or view-based reward is part of this mode.

## Social space

The lobby may eventually support voice and text chat, emotes, cosmetics,
group music or video viewing, and small social games such as Connect Four,
pool, chess, or karaoke. These are separate features with their own
accessibility, moderation, licensing, and performance requirements. They are
not prerequisites for the disaster-round loop.

## Acceptance

1. The mode announces the disaster, counts down, releases actors on GO, and
   ends the round at the configured duration.
2. The survivor count and names match the authoritative round outcome.
3. Ten rounds may damage the shared world; match completion restores the
   authored map baseline before the next match.
4. A player receives 100 local mode points for a survived round and no points
   for a round they did not survive; points never alter account-level
   statistics.
5. Missing, invalid, or over-budget disaster content fails safely and does not
   prevent the built-in round loop from completing.
6. Future community scripting passes a dedicated sandbox and network-authority
   review before execution is enabled.

## Related specifications

- [Gamemode runtime](gamemodes.md)
- [Destructible World](../destructible-world/destructible-world.md)
- [Accessibility and portability](../../architecture/accessibility/accessibility.md)
- [Distribution](../distribution/distribution.md)
- [Voice chat](../ingame-chat/voice-chat.md)
- [Zombie Tower](zombie-tower/zombie-tower.md) for possible later content reuse
