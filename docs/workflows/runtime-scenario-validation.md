# Runtime Scenario Validation

## Purpose

Use the real MiMITA executable and the canonical `events.jsonl` journal to
prove behavior. This workflow prevents a passing synthetic self-test from
being mistaken for working gameplay.

## Required loop

For every behavior change or investigation:

1. Read the specification, architecture owner, relevant changelog,
   regression record, and prior attempts.
2. Trace intent/configuration, action/input, fixed-tick simulation, networking
   where applicable, presentation, and logging. Find the first divergence.
3. Add bounded diagnostics at the existing owner using `StructuredLogger`.
   Records should include the scenario/action, tick, expected state, actual
   state, difference, and reason. Do not add a second file logger.
4. Build the newest timestamped executable.
5. Run:

   ```text
   mimita-<timestamp>.exe --versioninfo
   ```

   Save the printed `EVENTS_JSONL_PATH`. It is the authoritative journal for
   that process and must be named in the evidence report.
6. Run the actual game or a bounded runtime scenario using the same command,
   input, simulation, network, and presentation owners as normal play.
7. Inspect the journal live and after completion. Confirm that the changed
   path was reached and locate the first expected/actual mismatch.
8. Adjust only the responsible owner, rebuild, rerun, and compare the same
   scenario and evidence. For collision replay, preserve divergence evidence
   through the next human bookmark, then restore the full checkpoint and
   continue; divergence alone is not a stop condition.

## Scenario shape

When behavior needs deterministic timing, use an explicit seed, initial
state, and fixed-tick action schedule. For example:

```text
ticks 1-5: move forward
ticks 6-19: no movement input
ticks 20-25: move right
ticks 26-40: no movement input
```

The scenario must use the real fixed 60 Hz path and record input acceptance,
state before, state after, and final verdict. It must be bounded and exit with
an actionable process status.

## Test policy

Do not create a new synthetic test as the first response to a gameplay bug.
Use existing unit/component tests only for behavior they genuinely isolate,
such as serialization, path construction, or a pure mathematical invariant.
Classify those tests explicitly. For discovering gameplay failures, begin with
ordinary live play on the real game path and inspect the raw `events.jsonl`
evidence. Do not make an AI-authored fixed route or a short scripted scenario
the primary proof that general NPC or collision behavior works. Such a run can
only cover the actions and geometry it deliberately reaches.

Use a human-recorded route when a person has found a specific spot or action
that should be repeatable. Replay it through the real fixed-tick input and
collision owners to check whether that known failure returns. A recorded route
is a focused regression check, not broad proof: keep an unscripted live run for
behavior the recording did not exercise. A passing run that did not touch the
relevant map feature is `NOT_COVERED`, not a pass. Runtime logs prove only the
paths and events they actually capture; human visual or feel acceptance remains
a separate claim.

AI may observe and analyze ordinary gameplay and use human-recorded routes for
repeat checks. Any fixed setup, route, or assertion must be named in the result
so it is clear which parts were natural play and which were controlled.

## Recorded regressions and optional focused checks

Do not require a library of AI-authored fixed routes as the main collision
validation method. When a human finds a real failure, preserve that human
recording as a versioned route and replay it through the collision path that is
being tested. A route becomes a useful baseline only after its expected result
and relevant map feature have been reviewed.

A human-recorded route or other focused check may assert:

- a required event is present;
- a forbidden event is absent;
- an event field equals or stays within an expected range;
- a bounded journal metric such as unresolved NPC penetration, speed collapse,
  or seam oscillation stays below its accepted limit.

Known intentional exceptions, such as a real wall stop, a freeze, or a scripted
teleport, must be declared in the scenario rather than weakened globally.

Recorded-route checks run on the real executable through the dev loop. The
runner records the build, executable identity, `EVENTS_JSONL_PATH`, recording
identity, assertions, and terminal verdict. A current run fails when a
forbidden event appears, a required event disappears, or a metric leaves its
accepted range. Baseline changes are deliberate and recorded in the changelog.
The analyzer may classify events, but it must not silently excuse a failure or
replace the runtime journal.

For collision debugging, prioritize the human-recorded failure and its exact
map/features. Do not substitute random map selection or an NPC-navigation soak
for replaying the known failure. Report which actors and geometry were reached;
an event or map feature not reached is `NOT_COVERED`, not a pass. Longer
additional human-recorded cases may run in the background while short edit
checks finish; source/build checks and live gameplay evidence remain separate
results.

Complete a run as soon as its decisive runtime result or required coverage is
observed; do not wait out a fixed duration after the result. A fixed duration
does not itself prove the feature was exercised. This workflow uses
event-driven completion without a scenario-level timeout. If the expected event
never arrives, report the run as still waiting until an explicit stop or
process failure; do not convert elapsed time into a pass or a gameplay failure.

## Evidence report

Always separate:

- source/ownership evidence;
- build and executable identity;
- exact `EVENTS_JSONL_PATH`;
- runtime scenario inputs and exit status;
- JSONL records and first divergence or success condition; and
- human visual/multiplayer acceptance, if required.

## Record and replay player input

For movement, dash, edge, and weapon-collision investigations, record the real
input once and replay it after a code change:

```text
scenario_startrecord coolplace_edge
# play the exact route normally
scenario_endrecord
scenario_play devscripts/scenarios/user/scenario-YYYYMMDD_HHMMSS-coolplace_edge.jsonl
```

The recording is local-only under `devscripts/scenarios/user/`. Schema 2 also
records client-rendered actor tracks and a merged authoritative server track.
The player auto-restores the recorded start position, aim, weapon, and start
velocity, requires client/server spawn readiness plus matching map, weapon set,
and melee move-set revisions, and runs at fixed 60 Hz.

The server creates temporary Npc-backed proxies for recorded NPCs and remote
players, drives them from the authoritative track, disables normal AI for them,
and leaves live connected humans under their own control. Inspect
`scenario.playback.server_ready`, `scenario.playback.proxy_lifecycle`,
`scenario.contact.query`, `scenario.contact.weapon`, `weapon.contact.result`,
and `server.damage.applied` together to correlate an apparent hit with the
authoritative contact and damage outcome.

In the current legacy player, playback is invalid if readiness times out or
the player diverges by more than 2 m. The runner must treat
`scenario.playback.failed` or an invalid completion as a failed scenario; it
does not snap back. This is current implementation behavior, not the target
collision replay contract above. A checked-in template is
`devscripts/scenarios/recorded_input_playback.example.json`.

Schema 1 recordings remain input-only playable scenarios. Run playback on the
same map, mode, weapon set, and local dev server; with schema 1, NPC and
remote-player state can still differ.

### Collision replay limits and requirements

Human-recorded collision routes are useful because they preserve a real input
sequence and identify the map location where a person saw a problem. They do
not replace natural live gameplay or establish that unvisited geometry works.

For collision debugging, replay must exercise the collision behavior under
investigation. The desired human-recorded trace replays actor inputs/intents
through the shared authoritative movement/collision path; a human or NPC input
source does not select a different collision solver. Replaying recorded actor
transforms as authoritative proxy state does not prove that those actors'
movement or collision solver would produce the same transforms. The report
must identify whether each actor was re-simulated, simulated by live AI, or
followed as a recorded transform. Do not describe transform-follow playback
as collision re-simulation.

The desired collision recording evidence is per fixed server tick and includes
the real inputs/intents, actor identity and lifecycle, authoritative
position/velocity before and after movement, grounded and world-support state,
collision backend, contact identities/normals/penetration, and relevant
containment or recovery results. Recorded positions and velocities are
comparison evidence; playback must not force them onto an actor whose collision
response is being tested. Compare expected and actual outcomes and continue
through the current human bookmark after divergence, retaining the full tick
evidence. Then restore the complete recorded authoritative state at that
bookmark and continue. Never snap an actor back on every tick.

Use both kinds of evidence: a natural live run to discover failures that were
not anticipated, and a human-recorded route to repeat a known failure after a
change. Do not turn one successful recorded route into a claim that all NPCs,
actors, or maps are collision-safe.

## Separate client and server journals

A scenario journal is owned by one client. The server writes its own journal.
Do not accept a live journal containing more than one server owner or more than
one client under the same identity.

Evidence must name:

- the client journal path
- the server journal path, when authority evidence is required
- `run_id`, `server_instance_id`, and `client_instance_id`
- the exact client build
- the exact server build

Server events that the client received may appear in the client journal tagged
`source:"server"`. They are not a substitute for the server journal when
investigating server-only decisions.

## Actor and weapon matrix

A melee, hitscan, projectile, or deathball change is not accepted from one
human-versus-NPC run alone. The validation matrix must cover:

- NPC vs NPC
- NPC vs player
- player vs NPC
- player vs player

and the configured weapon families under local and bad-connection profiles.
Each client records its own journal. The scenario passes only when every client
and the server agree on the authoritative outcome.
