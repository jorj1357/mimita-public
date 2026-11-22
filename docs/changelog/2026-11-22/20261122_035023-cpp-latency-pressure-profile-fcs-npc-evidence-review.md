// 2026-11-21 22:50 EST
/* purpose
* record one bounded independent evidence review of the Fun Counter Strike NPC behavior
* preserve run, build, generation, journal, and exact event facts
* this file DOES NOT edit gameplay source or claim human acceptance
*/

# FCS NPC evidence review (ctx-c00ad9484fa1)

# Task

- Task ID: wake-a612d8d0ae56
- Session ID: session-a5d300229c10
- Agent: cpp-latency-pressure-profile
- Focus: NPC-on-NPC kill or bomb-plant evidence for Fun Counter Strike
- Status: complete (evidence review only)
- Date, time, timezone: 2026-11-22T03:50Z
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 07ba9003a6be33556ff350eb97be042fd18a99dc
- Final commit: 07ba9003a6be33556ff350eb97be042fd18a99dc

# Requested behavior

Preserve ctx-c00ad9484fa1: "NPC-on-NPC kill or bomb-plant evidence get thids stuff work until this is the ingame behaiot with events proof".

# Exact implementation changes

None. This activation only read journals and coordination state, appended one issue review, and acknowledged the context. No source file, config, build input, or runtime was modified or started by this agent.

# Evidence read

- Run 20261122_032407_fcs-route-proof, runtime fcs-route-proof, launch mode 45 fun counter strike, map dust2cyberiav4, EXE build 2026-11-22T02:37:06.972018Z, game DLL generation 444 sha256 fa4d17a590241ac22bb8d0ab2e1b41ebb3bb895376df30639ab4b0af5f542390, server PID 35960.
  - Service journal: logs/2026-11-22/20261122_032407_fcs-route-proof/server-01/events.jsonl
  - Client journal: logs/2026-11-22/20261122_032407_fcs-route-proof/client-01/events.jsonl
- Generation 441 plant chain in logs/2026-11-22/20261122_023327_fcs-proof/server-01/events.jsonl, already recorded as issue evidence-8572793982d1 by issue-review-sweeper.

# Measured evidence

- gen444 retained server window: 134 server.damage.applied events with attacker_type=npc, target_actor_type=npc, killed=true, all cross-team. First sampled: tick 19938 at 2026-11-22T03:37:25.064Z, attacker 100003 (team 0) killed 100013 (team 1), source physical_contact, 22 damage.
- NPC-vs-NPC kill.reward includes ranged and melee: tick 4514 100003 shotgun to 100014; tick 5145 100002 revolver to 100009; later chainsaw and swordsword kills.
- Client journal: client.npc.death.received shooter_id=100017 (team 1), victim npc 100005, position [-600.7,40.2,2361.1].
- CT traversal on gen444 between 03:24:07Z and about 03:30Z: all ten sampled CT NPCs (team 0) started near ct_spawn [-875,122,2349] and ended at [-600..-637, 1..68, 2360], 298-319 units away on the T side. Actor 100006 first [-880.7,118.8,2343.1] to last [-609.5,29.0,2360.3]; actor 100003 first [-880.9,124.8,2343.1] to last [-614.5,3.3,2361.1].
- gen444 objective window: objective.carried (100012, team 1), objective.dropped, objective.pickup (100018) but no objective.plant-start/planted in the retained window.
- gen441 plant chain: objective.plant-start t=871.552 operator 100018 npc site B; objective.planted t=882.928; objective.exploded t=929.200; round.result round 9 winner team 1.

# Validation

- Read-only journal analysis with local JSONL parsers. No build requested. No test run started. No compiler invoked.
- Active runtime and build queue owned by other agents were left running.

# Limitations

- The server journal is a rolling/compacted window; early lines from the gen444 run were dropped before the review was written, so the gen444 first-position CT lines are not retained.
- Source has advanced past gen444 (generation 448 published during this activation). A single newest-generation run holding objective.planted plus cross-team kills is still pending.
- No human visual or gameplay review was performed.

# Agent handoff context

- Owner: fun-cs-npc-objective
- Durable record: issue review appended to issue-be1adb51b6ac (verdict evidence) at 2026-11-22T03:47Z.
- Next exact step: keep one newest-generation FCS runtime alive past the plant hold and copy the objective.planted plus cross-team kill lines before journal rotation drops them.
