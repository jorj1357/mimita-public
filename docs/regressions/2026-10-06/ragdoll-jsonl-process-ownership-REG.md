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
