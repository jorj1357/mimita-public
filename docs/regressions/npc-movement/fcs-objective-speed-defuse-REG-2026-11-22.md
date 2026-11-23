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

## Pinned generation-500 runtime baseline

Run `20261122_150906` is an isolated, stopped Fun Counter Strike run on
`dust2cyberiav4`, profile `networking-validation`, launch mode 45. Its
`run.json` pins executable SHA-256
`4fa387f46e02a3a9d136abff3849b879306ecd354f07e145b35b10d20a8d2136` and game
DLL generation 500 SHA-256
`907486df17b9ed036abfeeb830a262626b42587321a513d9438e0c5cc2decbdc`. The
server journal is `logs/2026-11-22/20261122_150906/server-01/events.jsonl`;
runtime simulation time spans 0.000 to 1424.035 seconds (23.73 minutes).
This was another agent's pinned runtime, not this task's new measurement build.

Coarse speed estimate from `npc.attack-gate.fields.pos`: 19 NPC IDs,
4,236 consecutive-position segments spanning 6,136.9 sampled seconds, with
0.05–2.0 second gaps accepted and speeds above 150 units/second excluded.
Assuming the user's requested 1 world unit = 1 metre, the time-weighted mean
was 6.80 m/s in 3D and 6.56 m/s planar; median 3D segment speed was 5.54 m/s,
p90 15.23 m/s, and maximum retained segment 123.03 m/s. Per-actor sampled
means ranged from 5.44 to 8.82 m/s. These are sparse attack-event chord
measurements, not fixed-tick or per-life speed; the map's physical scale is not
independently calibrated, and bursts/turns between samples are missed.

Movement diagnostics counted 2,986 `npc.stuck-episode` events and 1,861
`npc.movement-progress-failed` events. Only 184 `npc.stuck-recovery` events
included `stuck_seconds`; those values had mean 0.30 s, median 0.30 s, p90
0.32 s, and maximum 0.32 s. This recovery-only subset does not measure the
full duration of the other stuck episodes. The run had no per-life movement
summary, no 10 m dwell duration, and no exact wall-contact count.

Combat/objective baseline: 32 `kill.reward` events, of which 7 had both
`killer_is_npc` and `victim_is_npc` true. NPC-v-NPC kills by completed round
were 3, 2, 0, and 2; the next round had only 17 seconds of journal time and is
incomplete. Round-start to first NPC-v-NPC kill was 137.609 s, 124.595 s, no
kill, and 421.264 s in those four rounds. The six intervals between the seven
NPC-v-NPC kills had mean 168.95 s and median 48.32 s. NPCs threw 138 grenades
across 18 actors (maximum 22 throws by one actor).

The journal recorded 9 `objective.plant-start`, 3 `objective.planted`, 6
`objective.plant-interrupted`, 8 `npc.defuse-assigned`, 1
`objective.defuse-start`, 1 `objective.defuse-interrupted`, 0
`objective.defused`, and 3 `objective.exploded`. The first plant start in the
whole run was t=412.735 s and the first defuse start was t=1180.690 s, a
cross-round interval of 767.955 s. For the round-4 bomb, its first plant
attempt was t=893.447 s, plant completion t=909.229 s, and CT defuse start
t=1180.690 s: 287.243 s from attempt and 271.461 s from completion. That
attempt was interrupted at t=1181.022 s after 0.332 s (`reason=released`); no
defuse completed. The pinned run therefore shows a CT defuse attempt but does
not prove a successful defuse.

## Instrumented measurement build

The task added the rate-limited `npc.movement-metric` event for elite/easy FCS
profiles. It reports per-life 3D and planar distance and mean speed, current
planar speed, net displacement, 10 m local dwell, movement intent and blocked
motion, near-wall-avoid exposure, pose-penetration contact samples, and
objective distance. The wall fields are explicitly proxies; they do not claim
to count exact physical wall contacts. The first candidate build is generation
503 and includes those metrics. A follow-on source edit adds active stuck state
and the current stuck timer to each one-second metric sample. Both changes need
a fresh pinned runtime journal before those values can replace the coarse
generation-500 baseline above.

## Pinned generation-507 measurement run

Run `20261122_174842` is pinned to EXE build
`2026-11-22T17:39:49.763492Z:3e52717b7ea5919fb05f896124b6b5ecf44eb67c72724707c3686c48585ead2d`
and game DLL generation 507 (SHA-256
`fa0e3f08b02856d2eaca7ba5dc97806511108defa221b112f9af755b818a9d6f`). It
ran Fun Counter Strike on `dust2cyberiav4` with 19 NPCs. The journal is
`logs/2026-11-22/20261122_174842/server-01/events.jsonl`.

At the 592.1 s journal checkpoint, the run had emitted 8,194 one-second
movement samples, 15 total kill rewards (4 NPC-v-NPC), 2,792 stuck episodes,
1,603 movement-progress-failed events, 178 stuck recoveries, and 59 NPC grenade
throws. Across each actor's latest current-life sample, mean 3D average speed
was 22.1 m/s (median 21.3, range 17.4-27.7); per-life planar average speed
mean was 20.2 m/s. These measurements use the requested 1 world unit = 1 m
scale, which has not been independently calibrated against the map geometry.
The longest 10 m local-dwell peaks were 62.2 s. Per-actor blocked-motion ratio
mean was about 0.07, with a maximum of 0.13. Near-wall avoidance exposure and
zero pose-penetration samples remain proxies, not exact wall-contact counts.

Two plants completed (A at 115.610 s, B at 467.011 s). CT defuser assignments
followed 0.037 s and 0.006 s later, but the chosen CT was respectively 32.1 m
and 212.7 m from the bomb. Neither attempt reached `objective.defuse-start`;
both bombs exploded. The first NPC-v-NPC kill was at 112.339 s; the four kill
intervals through 251.719 s were 16.238, 30.074, 93.067, and 316.565 s. These
values confirm that objective assignment/planting and NPC-v-NPC kills occur,
but do not prove a successful CT defuse or fast round-start combat.

A later sample exposed an invalid instantaneous-speed proxy: `current_planar_speed_mps`
reached 5,727.8 while the corresponding velocity magnitude was 79.5 m/s. Use
velocity magnitude for instantaneous speed and life distance divided by life
time for stable average speed; correct or remove the displacement-over-short-
interval field before using it as evidence. The capture continued after the
592.1 s checkpoint; this record reports that bounded checkpoint only.

## Phase 0 unit calibration and metric counters (2026-11-23)

The human's stated intention is **one world unit equals one metre** so gameplay
can reuse real-life physics. The current engine evidence disagrees with that
intention: the default capsule is 3.6 world units tall for a 1.8 m human, and a
10x10 authored block crosses in one second at the configured 20 unit/s ground
speed. Both observations imply **two world units per metre (0.5 m per world
unit)** under the current convention. This is recorded as the provisional metric
scale, not a decision to change the engine convention. The new
`npc.movement-metric` record keeps raw world-unit values and emits calibrated
metres using `meters_per_world_unit: 0.5`; if the live player probe later
disproves the scale, only the scale constant and derived fields change and the
raw data is preserved.

Source changes in this phase (game DLL, build note `f125a35d5099`):

- `src/npc/npc.cpp`, `src/npc/npc.h`, `src/network/server-npcs.cpp`: add
  per-life dash, air-dash, down-dash, jump, bhop, freeze, airborne, air-movement,
  max-planar-speed, and final-pose wall-contact counters to the existing
  `npc.movement-metric` event. Existing speed fields now report both raw
  world-units/second and calibrated m/s; the instantaneous-speed field now uses
  velocity plus external impulse instead of displacement over a short interval.
- `devscripts/quality/npc-match-report.py`: read-only journal analyzer for
  per-NPC/per-team movement, dash/jump/air/wall rates, NPC-vs-NPC kills, TTK,
  kill intervals, round objective timing, and diagnostics. It honours the
  `meters_per_world_unit` recorded in each event, so older 1.0-scale journals
  remain analysable.

Diagnostic baseline from the live FCS server journal
`logs/2026-11-23/20261123_144429/server-01/events.jsonl` (older DLL, legacy 1.0
scale, so calibrated metres are half these values under the new scale):

- 19 NPCs, 100,777 one-second movement samples, 5,705 s journal time.
- Team CT 9 actors: life-weighted 22.23 units/s; team T 10 actors: 21.37
  units/s. Per-actor max planar speed 40.9–121.6 units/s.
- 37,148 `npc.stuck-episode`, 17,393 `npc.movement-progress-failed`, 3,719
  `npc.nav-plan-failed`, 2,197 `npc.area-escape` events. Per-actor stuck
  episodes were about 20–24 per minute.
- Dash/jump/bhop/air/wall counters were absent in that DLL and `npc.dash` was
  not retained at the active category level, so those baseline rates cannot be
  read from this journal. This is exactly what the new counters fix.
- NPC-vs-NPC: 308 authoritative damage kills and 313 reward kills; TTK samples
  308, mean 5.04 s, median 0.83 s, p90 11.35 s; median kill interval 5.58 s;
  mean 5.60 NPC-vs-NPC kills per round with kills.
- Objective: plant latency from round go ranged from 17.1 s to 89.4 s and
  worsened in later rounds; several rounds had 1–2 plant interruptions. Some
  rounds did record `objective.defuse-start` and completions, but many planted
  bombs still exploded. CT defuse is therefore sometimes attempted on this
  generation but is not reliably completed.

The live dev-loop was source-stale during this baseline (`source_changed`,
`published_stale`, `active_source=None`), so this is a diagnostic baseline, not
proof for current source. Next required evidence is one pinned FCS run on the
new generation that reports the new counters, calibrated metres, wall-contact
time, and a CT defuse start and completion.

### Pinned generation-564 instrumented baseline (2026-11-23)

Run `20261123_164421` is an isolated `networking-validation` test run on launch
mode 45, map `dust2cyberiav4`, game DLL generation 564 SHA-256
`8cc5344e219a96b00e3a7b5cc75cf45284a2a7b24457cb748cda2e4bbdc15e4d` (EXE
`97b78d2ae01ed2dd665e39c9f5ac97ebf4acef801ee3597c4ecb0f7a584f7a8d`). Server
journal: `logs/2026-11-23/20261123_164421/server-01/events.jsonl`.

The new counters are live in this journal. Movement is reported in raw
world-units and calibrated metres at `meters_per_world_unit: 0.5`. Over 489 s
of journal time and the first six rounds:

- Team CT: 9 NPCs, 3,962 life-seconds, life-weighted **11.83 m/s** average,
  dash 17.52/min, down-dash 9.15/min, bhop 2.03/min, air 24.8%, near-wall
  avoidance 11.6%, blocked motion 1.1%, stuck 21.90/min.
- Team T: 10 NPCs, 4,400 life-seconds, **10.15 m/s** average, dash 10.85/min,
  down-dash 6.22/min, bhop 1.30/min, air 17.0%, near-wall avoidance 9.5%,
  blocked motion 2.6%, stuck 15.46/min.
- Per-actor average speed ranged roughly 10.7–12.6 m/s for CT and 7.1–12.5 m/s
  for T. Per-actor max planar speed ranged 40–71 m/s (dash/knockback bursts).
- Exact final-pose wall-contact seconds stayed 0 because the arcade capsule path
  does not populate final-pose contacts; `near_wall_avoid_seconds` and
  `blocked_motion_seconds` are the current wall proxy and are now measured.
- NPC-vs-NPC: 26 authoritative damage kills, 22 reward kills, 88 NPC deaths;
  TTK mean 2.40 s, median 0.81 s, p90 2.15 s; mean 4.33 kills per round with
  kills.
- Objective: T planted in rounds 1–5. CT defuse starts occurred in rounds 3, 4,
  and 5 (latency 12.8 s, 17.8 s, 11.8 s after plant) but no `objective.defused`
  completed; rounds 1–5 ended in detonation (`winner_team=1`, reason 2), and
  round 6 was still active. This is the exact Phase 1 defect: CTs reach the
  defuse sometimes but do not sustain it to completion.
- `npc.stuck-episode` 2,580 and `npc.movement-progress-failed` 1,742 in 489 s.
  CT stuck rate is about 40% higher than T.

This is the current-source baseline for the next tuning pass. It proves the
measurement instrument, not improved behavior. The stop request for this run
returned `status=stopped`, failures=[] and stopped only PIDs 27196/18764; the
shared dev-loop processes were left untouched.

### Generation-567 CT NPC defuse proof (2026-11-23)

Run `20261123_172947` pins game DLL generation 567 SHA-256
`3ce3673f656373f1c5b1c8b5206cb40434143bdd09336c91021f828ef95e092b` (EXE
`97b78d2ae01ed2dd665e39c9f5ac97ebf4acef801ee3597c4ecb0f7a584f7a8d`), launch
mode 45, map `dust2cyberiav4`. Server journal:
`logs/2026-11-23/20261123_172947/server-01/events.jsonl`.

The run recorded four T plants and four CT wins. Every recorded
`objective.defuse-start` in this window used `operator_kind: npc`. The first
three completed at t=69.83, 110.95, and 175.16 s; the fourth completed at
t=239.54 s. `objective.defused` therefore now appears in the live journal for
the first time in this regression record, and the assigned operators were NPC
IDs 100005, 100007, and 100001.

Implementation evidence in this generation:

- `npc.defuse-assigned` now records `actor_kind`, stable/nearest NPC and human
  candidates, and the reason. It prefers a living NPC over a nearer human and
  keeps a stable NPC while defuse progress is active.
- The designated CT NPC locks `DefuseObjective` while planted, and a carrier
  inside a site locks `PlantObjective`. Both hold position during the
  interaction; hazard avoidance still overrides.
- Movement metrics for this run: CT 12.31 m/s average, dash 15.56/min,
  down-dash 6.66/min, bhop 2.46/min, air 22.8%, near-wall avoidance 12.4%,
  blocked motion 1.2%, stuck 19.72/min. T: 12.15 m/s, dash 12.57/min,
  down-dash 7.95/min, bhop 1.54/min, air 21.1%, near-wall avoidance 8.3%,
  blocked motion 3.0%, stuck 20.02/min.
- NPC-vs-NPC: 15 reward kills, 10 authoritative damage kills, TTK mean 2.06 s,
  median 0.94 s, mean 3.33 kills per completed round.

Remaining defects exposed by the same run: a single defuse can still restart
several times (multiple `objective.defuse-start` records before one
`objective.defused`), so operator/range churn is reduced but not eliminated.
Plant/defuse latency and wall/stuck rates remain Phase 2/3 targets. This proof
covers CT NPC defuse completion only; it is not proof that the elite difficulty
or movement-tech targets are met. The isolated run was stopped by its own
`--stop-test-run` request with failures=[]; PIDs 31844/31072 only.
