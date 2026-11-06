2026 10 06 1728 jorj todo 

OK RPG LIKE
THE TALE OF KITTEHS CAPE

start at level 1

do /load 
and get to level 182 with all these ridiculous  colorful balls making the plastic bits falling soundns i loev that sound 
and u have like
35,000 hp
instead of 100
and like 
Shard of Vymmkhlr: does like 3500 dmg per intersecting tick, minimum, more with speed

BUT
GO HARD ON THE HUUUUGE MAP
AND LIKE
MOVEMENT 
AND STUFF
CUZ NOW WE HAVE LIKE GUNS AND SWORD AND REOCKTE LAUNCHER AND STUFF ITS SO SICK LIKE NOT JUST  STUCK FROZEN IN PLACE  RPG GAME BORING WALK SPEED U CAN JUST GO FLYING BRO ok 

 u shoudl be able to get to like
 level 999,999

and like 
in 100 years u could get there
but u prob should hmmmmmmmmm im not sure 
juts
that means there is just that much content
combine super super strong primtiives together so that combinaitons allow for a bunch bunch bunch of stufff mhm ok 

also writing C:\mimita-v9\docs\specs\gamemodes\zombie-tower.md this  2026 10 08 1342 jorj 
this is good for 
data saving
bc this rpg mode
i loooove d
being able to be lik elevel 482
and doing 

/save
/load
then i get all the items and stuff back and i acn move super fast
we need to do that
but like auto load dont require commands and stuff
ALSO 
do a local auto save as well 
send the data to the server like 60 * 60 ticks so like 3600? 
60 seconds between
so low rate limit
but save it loaclly to the device in like a json a  rolling event log
maybe
and the result of what u killed and how much gold/xp u have now
todo how to  handle multiple gammeodes?
and like

kill a monster in rpg mode = get a cosmetic/item/emote/hat/etc = can be worn in other modes such as juggernaut, survive the disasters, duels, etc
or, duels, e.g.  become top 500 player = get special badge/award/achievemnt/cosmetic = can use that in other modes as well
as well as, inspecting another user's avatar
and how they got those items and how u could get those items too 
thats sick

but relate it to monetization C:\mimita-v9\docs\specs\monetization\monetization.md

so we do not do enshittification 

## RPG direction and sequencing — 2026-11-05

The RPG takes inspiration from 2012 Roblox role-playing games: a recognizable
social world, clear places and activities, and visible long-term progression.
It may use a more mature tone, difficult fights, and blood. It is not a
medieval-only RPG. The intended starting kit is a revolver, shotgun, and sword;
movement is fast and expressive, and monsters use varied, readable approaches.

Build RPG foundations through creator tools first. The first product priority
is world projects and the in-game map/entity editor: open, fork, save, browse
versions, author reusable world content, edit the live world collaboratively,
place NPC spawns, and tune world-specific rules. Start with an authored grassy
area and nearby easy enemies as the first content target, but do not make
leveling, shop balance, or reward implementation a prerequisite for the
storage/editor contracts.

Worlds may use handmade regions, procedural zones, or both. Content should be
assembled from reusable authored structures and connected chunks with explicit
connection points. Infinite generation can be enabled globally or only in
authored zones. A future world may use recursive rooms, fractals, or
non-Euclidean connections while preserving a stable room/layer identity.
Procedural generation, server authority, and performance requirements are
specified in
[`../procedural-infinite-world/procedural-infinite-world.md`](../procedural-infinite-world/procedural-infinite-world.md)
and
[`../entity-editor/entity-editor.md`](../entity-editor/entity-editor.md).

World data and player data are separate. Each world lineage owns its custom
schema and progress; a fork explicitly chooses whether to preserve or reset
that progress. Adding a field such as diamonds must preserve existing values.
The player account ID identifies the player, while recovery data remains
player-controlled. The distributed-storage comparison and recommendation live
in [`../world-projects/world-projects.md`](../world-projects/world-projects.md).

Future progression direction: defeating monsters grants world-defined gold and
XP; gold buys gear and XP levels open later zones. Some cosmetic items may be
portable across compatible modes. Platform-wide inventory/economy rules need a
separate design. Do not implement a cryptocurrency or shared token as part of
the first editor slice.

NPCs should reuse the existing actor, role, weapon, animation, movement, and
damage systems. Early fights should be solo-capable with unchanged NPC stats as
more people join: larger groups contribute more attackers, not automatic
enemy scaling. Presets may select different speed, size, weapons, and attack
style. Per-world NPC appearance, animation, sounds, and reward values are
authored data. Existing NPCs can accept a live preset edit using the editor's
explicit apply-to-existing or future-spawns choice.

The RPG design inherits the no-pay-to-win direction from the Zombie Tower
specification. No random paid rewards, daily login pressure, or required
platform purchase is part of this direction.

### extra

2026 11 05 1626 jorj

# MiMITA RPG Mode: Full Gameplay Plan

## Summary

Build the RPG as a fast, social PvE world with optional PvP arenas. Players arrive in a grassy starter region, choose where to explore, and fight readable enemies using MiMITA’s movement, guns, and melee systems. Combat earns world-scoped XP, currencies, gear, and cosmetics; levels open the forest at 10 and desert at 25.

Assume the world editor and project/version system work. Use the existing map, actor, weapon, NPC, animation, chat, and persistence owners wherever possible. Add RPG-specific rules through data and shared systems rather than creating parallel combat or account-stat systems.

## Gameplay and Progression

- **The core loop:** Spawn in a recognizable social hub with nearby easy enemies, NPCs, and a shop. Players can start fighting immediately; optional quests and bounties give direction. Fighting grants rewards, gear supports new builds, and XP unlocks further zones.
- **Starter kit and movement:** Start at level 1 with the revolver, shotgun, and sword equipped in quick-switch slots. Reuse the existing weapon, ammo, movement, jump, dash, and melee paths. Add and hot-reload equip/carry poses for these weapons through the animation system.
- **Action combat:** Monsters telegraph attacks and use distinct approaches: slow heavy attacks that can be avoided, fast rushes or flanks that demand movement, and ranged pressure that encourages repositioning. Reuse existing NPC movement, behavior profiles, attack definitions, collision, and damage; don’t make combat a stationary click loop. Bosses build on these same actions with readable multi-step patterns.
- **Power scale:** Keep the intentionally oversized RPG numbers. Use roughly 35,000 HP as an early tuning anchor and about 3,500 damage per fixed tick as a high-tier weapon reference; progress beyond those values in later zones. Level and earned gear increase power, while each zone keeps a readable enemy band. Use numeric types and UI formatting that safely support large HP, damage, XP, and currency values.
- **Progression:** Keep levels uncapped, with 999,999 as a long-horizon ambition rather than a promised time-to-reach. Use a world-authored XP curve that supports extrapolation and milestone rewards. Level 10 opens the forest, level 25 opens the desert; later zones use explicit milestones. Progression must not silently scale monster stats with group size.
- **Flexible builds:** Don’t lock players into permanent classes. Weapons, gear, and earned abilities form changeable builds; healing and support abilities can support non-DPS play. Gold buys earned weapon variants, upgrades, and other gear. World-defined currencies are supported from the start; the starter world uses gold and XP, with additional currencies such as diamonds added through the project schema.
- **Co-op rewards:** Keep NPC stats fixed as more players join; groups face more attackers where appropriate. Allocate each monster’s reward in proportion to contribution. Count direct damage plus effective healing, shielding, buffs, or control using authored contribution weights. Avoid crediting a support player with an ally’s entire damage total. Keep NPC rewards separate from PvP statistics.
- **Drops and economy:** Combine predictable quest/boss rewards with gameplay-only random drops. Show drop sources and odds; never sell attempts, keys, or randomized crates. Defer player-to-player trading. Optional purchases are direct cosmetics only, with no paid power or daily-login pressure.
- **Death:** Respawn quickly without losing XP, currency, or gear. Preserve challenge through enemy behavior, positioning, and time spent returning to the fight rather than progress loss.

## World and Social Experience

- **Starter world:** Make the first playable region a readable authored grassland with a safe hub, spawn area, simple roaming enemies, one shop, and a small set of NPC interactions. Add at least one optional bounty or quest that teaches combat and rewards. Keep the hub safe from PvP.
- **Expansion:** Build the forest and desert as distinct authored zones with reusable structures and connectors. Use seeded generation where it adds replayable space; pin the seed and generator version. The host confirms canonical generated content. Recursive/fractal worlds remain a later experiment after ordinary zones meet determinism and performance budgets.
- **Interactions:** NPC dialogue, shops, quest acceptance, and world objects use the shared interaction action system and map-authored press/hold settings. No arbitrary downloaded scripts.
- **Social presence:** Use the existing global chat path for server-generated level-up and zone-unlock messages. Show every level-up with a local toast and orb/sound celebration; send its level announcement through chat. Extend avatar inspection to show earned RPG cosmetics and how each was obtained.
- **PvP:** Add marked, opt-in casual duel arenas after the PvE loop. Keep PvP out of the hub and normal exploration. Defer ranked play, F-to-S rank bands, top-500 awards, and their cross-mode cosmetics to a later phase.
- **Cross-mode rewards:** Keep RPG levels, currencies, and gear world-scoped. Later, add an account-wide cosmetic interface for earned hats, emotes, badges, and similar appearance items that can be worn in compatible modes. Preserve item provenance so players can inspect where an item came from. Competitive top-player rewards use this cosmetic path when ranked play exists.

## Data and Existing Systems

- **World-scoped saving:** Store RPG levels, XP, currencies, inventory, unlocks, and reward history by player identity and world lineage. Do not write RPG currencies into the existing account-wide gold/XP totals. Reuse the existing persistence pipeline’s server authority and idempotent reward-event patterns, but keep RPG state in its own schema and history.
- **Autosave and recovery:** Save each host-confirmed reward event to a rolling local event log and update a compact snapshot periodically, including the existing one-minute cadence idea. Recover from the latest confirmed event after disconnect. Adding a field such as diamonds must preserve prior fields and values; a fork explicitly chooses whether to inherit or reset progression.
- **Reward authority:** The host confirms kills, contribution, drops, purchases, level changes, and zone access. Reward event IDs prevent duplicate XP or currency after retries and reconnects. Host-awarded rewards retain world/version provenance; they are not presented as trustless competitive rewards.
- **Content ownership:** The map/project owns encounter placement, zone rules, shops, NPC dialogue, reward definitions, and world-specific overrides. Existing weapon, actor-preset, role, NPC behavior, animation, and chat systems remain owners of their respective behavior. Use editor previews and hot reload for authored data.

## Delivery and Acceptance

1. **Playable grassland loop:** Add the RPG mode configuration and starter role/loadout; connect the grassland spawn, hub, enemies, NPCs, shop, first optional bounty, combat rewards, autosave, and level-up feedback.
2. **Progression and zones:** Add the uncapped world XP curve, world-defined currency support, earned gear/drop tables, forest and desert unlocks, and zone-specific encounters. Verify high-value numeric storage, formatting, and network replication.
3. **Co-op and social polish:** Validate contribution-based rewards for damage and support, level announcements, avatar/item inspection, disconnect recovery, and host-confirmed persistence.
4. **Casual PvP and later platform links:** Add opt-in arenas without changing the PvE reward path. Keep ranked rewards, cross-mode cosmetics, peer-distributed saves, and account backup recovery as later work.

Acceptance uses real runtime scenarios: solo and co-op fights against different enemy behaviors; unchanged monster stats across group sizes; damage and support reward attribution; quick death/respawn with no loss; duplicate reward rejection; save/reconnect restoration; level 10 and 25 zone gates; seeded-zone repeatability; and chat/visual level-up feedback. Use the dev loop’s exact build and per-process journals for multiplayer checks, and report build, runtime, and human gameplay acceptance separately.

## Assumptions and Defaults

- The RPG uses PvE exploration with optional casual PvP arenas; no open-world PvP.
- Starter weapons are revolver, shotgun, and sword. The first tuning build uses the oversized RPG number anchors above.
- Forest 10 and desert 25 are the initial level gates. Levels remain uncapped; exact curve and combat values are tuned in playtests.
- Monsters reuse and extend MiMITA’s existing monster/NPC assets and behavior systems.
- World-defined currencies are supported, while the starter world begins with gold and XP. Trading and ranked arenas are deferred.
- Random rewards are earned through gameplay only, with visible odds. Purchases are direct cosmetics only; no paid power, loot crates, or login-pressure rewards.
