# Fun Counter-Strike NPC speed, wall-stuck and defuse behavior

- Recorded: 2026-11-22 (human report; exact time not supplied)
- Status: REPORTED / INVESTIGATING; regression cause and historical baseline unverified
- Related issue: [issue-be1adb51b6ac](../../issues/bug/20261120-fun-counter-strike-npc-objective-behavior-regres-issue-be1adb51b6ac.md)
- User's exact request excerpt: “also prove that CT npcs will defuse”

## Reported behavior and target

The user reports that FCS NPCs behaved closer to the desired objective flow
before the DLL hot-reload effort, and requests recording a regression. This
historical comparison is user-reported; no pre-change pinned journal has yet
been identified, so the DLL-reload change is a timing correlation, not a
confirmed cause.

Target behavior: Ts agree on one site at round start, travel there using the
fastest valid shared movement options, plant, then defend the carrier/bomb.
CTs initially search or defend without omniscient T positions; a legitimate
sighting is shared through team reports, and a planted bomb is known to CTs.
CTs coordinate a defuse attempt while teammates cover; Ts prioritize stopping
the defuser. Elites should use the same authoritative movement and combat
rules as players, with fewer navigation mistakes and high useful progress.

The user's 85–100+ m/s numbers are aspirations only. The distance scale, human
player speed under the same movement physics, and shared speed limits must be
measured before treating those numbers as achievable targets.

## Current diagnostic evidence

Run `20261122_081330_fcs-route-proof-applied-fallback`, generation 478,
`dust2cyberiav4`; authoritative server journal:
`logs/2026-11-22/20261122_081330_fcs-route-proof-applied-fallback/server-01/events.jsonl`.

The run recorded 2,223 `npc.stuck-episode`, 1,156
`npc.movement-progress-failed`, 316 `npc.nav-plan-failed`, 311
`npc.nav-objective-fallback`, and 1,321 `npc.dash` events. It also recorded
48 `npc.team-report`, 101 `npc.grenade-thrown`, 3 `objective.plant-start`, 2
`objective.plant-interrupted`, and 1 `objective.planted` event. These are raw
single-run counts, not per-NPC rates; some event types can recur for one actor
and must be correlated by actor, tick, round, and life before comparing.

A coarse speed baseline can be estimated from consecutive `npc.attack-gate`
position samples. Across 19 actor IDs, after keeping intervals from 0.05 to
2.0 seconds and excluding four apparent >150-unit/s teleport/outlier steps,
3,758 sample pairs spanning 4,500 seconds give a time-weighted average of
8.43 world-units/s in 3D (8.13 planar), median 6.83, p90 20.39, and a highest
retained segment of 96.01. Per-actor 3D averages range from 6.84 to 10.92
world-units/s. This is not a measured per-life velocity average: chord distance
misses turns between samples, the sparse attack-gate cadence misses bursts,
the actor IDs include respawns, and map world units have not yet been
calibrated to meters. It is a useful low-motion signal, not proof that 100
m/s is currently achievable or a speed-cap recommendation.

After the plant at t=296.685, actor 100002 was assigned as nearest CT defuser
at distance 34.01 world units. There was no `objective.defuse-start` or
`objective.defused` in the journal before `objective.exploded` at t=397.101.
This proves assignment without attempt/completion in this run; it does not
prove that current source cannot defuse. `kill.reward` (52) and `npc.death`
(37) counts do not establish NPC-v-NPC kill attribution, and grenade throws
do not alone establish useful tactical outcomes.

The current dev-loop status reports the latest successful artifact stale
against source and no active FCS runtime. Generation 478 is diagnostic only,
not proof for current source. A fresh pinned run is required after the current
shared source changes settle.

## Attempted defuse-priority correction and runtime status

Source trace at `src/npc/npc-utility.cpp` showed that the designated CT's
`DefuseObjective` relevance was gated only by `canDefuse`, which becomes true
only within the interaction range. In the gen478 journal the assigned CT's
utility goal alternated between `MoveToObjective` and `DefendSite` while
`can_defuse=false`; `DefuseObjective` did not appear. The scorer now treats
`isDefuserId` as sufficient to keep `DefuseObjective` relevant during travel.
This preserves the existing objective route and shared movement owner; it does
not bypass distance, interaction, LOS, or defuse progress rules.

The root dev-loop automatically built DLL generation 500 successfully. Its
summary at `.dev/build-timings/game-dll-20261122T144944189873Z-31ded0.json`
lists only `src/npc/npc-utility.cpp`, code hash
`907486df17b9ed036abfeeb830a262626b42587321a513d9438e0c5cc2decbdc`, and
queued build note `4b3d4ffda2ea`.

Isolated run `20261122_145137` used that DLL hash on `dust2cyberiav4`, but
its run manifest says `pinned_exe=false`; the exact executable was the older
published artifact. It failed readiness with Windows `WinError 5` replacing
`run.json.tmp` with `run.json`, at server journal t=52.873 before a plant
attempt. The journal recorded no plant/defuse events. A second independently
started FCS run, `20261122_145240`, on the same DLL also failed the same
manifest rename. These runs do not prove the goal correction, CT defuse, or
other desired behavior. The manifest-writer path belongs to a concurrent
dev-loop investigation; no changes were made to it here.

As a fallback, root-dev-loop runtime `fcs-speed-defuse-500` was requested on
launch mode 45 and the same generation 500 DLL. It created run metadata
`logs/2026-11-22/20261122_150859_fcs-speed-defuse-500/run.json`, but did not
create a server journal or establish a client/port; server PID 7348 exited
before runtime evidence became available. Its own runtime entry was stopped
through the dev-loop. The existing shared Juggernaut server/client pair was
left running throughout.

Status remains REPORTED / INVESTIGATING. The next proof run must reach a plant,
show the designated CT selecting `DefuseObjective` while out of range, then
record `objective.defuse-start` and `objective.defused` (or retain the exact
failure path for another iteration). No source/build result substitutes for
the live journal evidence.

## Measurements required for acceptance

- Per NPC and per life: actual horizontal/full speed over fixed ticks,
  requested speed, movement-intent duty time, mean and percentile speeds,
  peak speed, path length, net displacement, and progress toward the active
  objective/target. Report both all-life and purposeful-movement averages.
- Calibrate map units and compare NPC speed to a player using the same shared
  physics and movement caps before setting a numeric elite speed target.
- Wall blocking: unique contact episodes per NPC-minute, duration, intent
  directed into the contact normal, pre/post-contact velocity and speed loss,
  and ticks with movement intent but no route progress. Deduplicate repeated
  contact samples into episodes.
- Stuck/dwell: continuous time within the user's 10 m anchor with no
  meaningful objective progress; time per area; route distance reduction;
  and net movement per life/round. Separate intentional plant/defuse, combat,
  cover, and bomb-defense holds from accidental inactivity.
- Objective timing per round: round start to site choice and first plant
  attempt; plant attempt to completion; plant start to first CT defuse start;
  plant completion to first defuse start; and plant completion to defuse
  completion. Keep the attempt-to-attempt metric requested by the user and
  these separate phases so delays are attributable.
- Combat: first NPC-on-NPC kill latency, actor-attributed kill count, interval
  between kills, deaths, role/site context, weapon, and whether kills enabled
  plant/defuse/defense. Compare across rounds, not only aggregate counts.
- Utility and information: grenade inventory and uses by type per life,
  throw reason and damage/utility result; CT first sighting, report fan-out,
  teammate receipt/arrival, and bomb knowledge after plant.

Use bounded fixed-tick accumulators and periodic/per-life summaries at the
existing NPC movement/combat/objective owners; do not emit an unbounded event
for every movement tick. Preserve the fixed 60 Hz gameplay path, shared player
physics and combat legality. Runtime proof must use the exact pinned
dev-loop executable and game generation. Human visual/feel acceptance is a
separate evidence layer.
