# Ragdoll JSONL Process Ownership Regression

Time created: 2026-10-06

Status: OPEN

## Reported behavior

During a live Counter-Strike session, the user opened the path reported by the
game and found that the first record said `process=server`. The journal did
not contain the expected `death.ragdoll.requested` or `ragdoll.corpse.*`
records, so it was unclear whether the client was logging, whether the wrong
executable was running, or whether the ragdoll path had not been reached.

The user reports this is the fourth recurring debug-logging investigation in
which the selected journal was unclear or belonged to the other process.

## Evidence from occurrence 1

The supplied journal was:

`C:\mimita-v9\logs\10-06-2026\20261006_141256\events.jsonl`

Its first record was a server `logger.started` record, but its second record
was a client `logger.started` record. The file therefore was shared by both
processes; the first line alone was misleading. The journal contained client
avatar records but no `death.*` or `ragdoll.*` records.

## Confirmed causes

1. `log_open` opened the canonical shared file but did not prominently report
   the executable, process role, or whether death/ragdoll records existed.
2. `--versioninfo` printed the executable and path but did not print a clear
   `PROCESS_ROLE=client|server` line.
3. `logger.started` did not include the executable path or an explicit
   `process_role` field, making stale/wrong-executable diagnosis harder.

## Required behavior

Every canonical journal must make these facts easy to answer:

- Which executable wrote this record?
- Was the writer a client or server?
- Is the file shared by both roles?
- How many death and ragdoll events are present?
- Is the absence of ragdoll events a logging/path problem or a gameplay-path
  result?

## Narrow correction

- Add `process_role` and `executable` to `logger.started`.
- Add `PROCESS_ROLE` to `--versioninfo` output and its JSON record.
- Make `log_open` report client/server record counts and death/ragdoll counts,
  with an explicit warning when neither event family is present.

## Proof still required

Build a newly named executable, run `--versioninfo` for the client and server
roles as applicable, launch a real shared client/server session, use
`log_open`, and reproduce one death. The journal must show the two startup
roles and the death-to-ragdoll event chain, or show the first missing event.

## Regression Occurrence 2

### Observed

The user reported another Counter-Strike second-round session in which most
team deaths appeared to show blood but no visible ragdoll. The supplied path
used the displayed underscore form, but the actual filesystem path was:

`C:\\mimita-v9\\logs\\10-06-2026\\20261006_141756\\events.jsonl`

### Evidence

- File size at inspection: 11,172,746 bytes, approximately 10.7 MB.
- The file contained both roles and the same shared path:
  - server PID 8380, executable `C:\\mimita-v9\\.dev\\builds\\1546\\mimita.exe`
  - client PID 8180, executable `C:\\mimita-v9\\.dev\\builds\\1546\\mimita.exe`
- The client emitted 9 `ragdoll.corpse.spawn.attempt` records.
- All 9 were followed by `ragdoll.corpse.spawned` and
  `ragdoll.corpse.first_update` records.
- No `ragdoll.corpse.spawn.rejected` records occurred.
- The journal was readable and filterable; its size is not a reliability
  blocker for streaming JSONL inspection.

### Current conclusion

This occurrence proves that the client logger and ragdoll spawn/update
diagnostics are working. It does not prove that each corpse was submitted to
the screen or remained visible. The next diagnostic boundary is
`RagdollModeSystem::renderCorpses`, which currently calls
`renderNetworkPlayer(corpse.actor, camera, 0, false)` without a render-side
event. A bounded first-render or render-skip record is required to separate
spawn/update success from render visibility.

## Regression Occurrence 3

### Observed

The user reported another Counter-Strike round with several teammate deaths
that appeared to leave only blood and a persistent walking effect. The actual
path was:

`C:\\mimita-v9\\logs\\10-06-2026\\20261006_142343\\events.jsonl`

### Evidence

- The displayed path again contained a formatting underscore; the normalized
  filesystem path was used for inspection.
- The journal contains client and server records from `.dev\\builds\\1547\\mimita.exe`.
- The client emitted 9 `ragdoll.corpse.spawned` events.
- All 9 also emitted `ragdoll.corpse.render_submitted` and
  `ragdoll.corpse.first_update`.
- The position sampler emitted 180 samples, and the corpses later emitted
  `ragdoll.corpse.removed` at their 20-second lifetime.
- No corpse-cap eviction was recorded.

### Related walking-effect finding

`src/network/multiplayer-interpolation.cpp` runs remote walking VFX when the
replicated `NET_STATE_WALKING` flag is set, but that branch does not check
`player.dead`. The dead actor can therefore continue producing footstep and
walk-burst effects at its last position. This is a separate confirmed code
path from ragdoll creation, but it can make a dead actor look partially alive
and should be corrected or explicitly suppressed.

### Current conclusion

This journal proves that the instrumented client created and submitted nine
corpses to the renderer; it does not yet prove that the inner player renderer
actually drew visible meshes. It also confirms the walking VFX can persist
because the remote walking branch has no dead-state guard.
