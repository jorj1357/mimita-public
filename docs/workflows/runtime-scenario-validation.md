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
   scenario and evidence.

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
Classify those tests explicitly. Prefer a runtime scenario for gameplay,
networking, configuration loading, animation, collision, audio, and visual
behavior. A runtime log proves that the path executed; human visual or feel
acceptance remains a separate claim.

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

Playback is invalid if readiness times out or the player diverges by more than
2 m. The runner must treat `scenario.playback.failed` or an invalid completion
as a failed scenario; there is no snap-back path. A checked-in template is
`devscripts/scenarios/recorded_input_playback.example.json`.

Schema 1 recordings remain input-only playable scenarios. Run playback on the
same map, mode, weapon set, and local dev server; with schema 1, NPC and
remote-player state can still differ.

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
