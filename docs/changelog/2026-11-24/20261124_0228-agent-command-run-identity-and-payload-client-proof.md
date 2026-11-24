# Agent command run pin and Payload client diagnosis

## Request

Use in-game agent/console commands, without computer-use automation, to connect
as a client to Funworld3 Payload, verify NPC gameplay, diagnose the current
connection failure, and improve direct communication between ChatGPT and
OpenCode agents.

## Findings

- Run `20261124_005533` had a live Payload/Funworld3 server (PID 19056) and a
  stale client (PID 27052). The client received `PACKET_JOIN_ACCEPT` at
  `2026-11-24T01:04:16.161Z`, then logged
  `network.server-identity-mismatch`: expected run `20261124_004333`, actual
  server run `20261124_005533`. The server repeatedly sent join acceptance and
  marked the player spawned; the old client rejected the server identity and
  disconnected. This is the first confirmed failure in that attempt.
- Dev-loop launch mode 57 started Payload on Funworld3 with `--npcs 19` in run
  `20261124_014450` (EXE build
  `2026-11-24T01:44:42.533263Z:1327fa12ce01906a90e6a843b83fbaa2b0a24bbbe6ea2acee68507a714e6c7df`,
  game generation 577). Server PID 18640 recorded 19 NPC roster profiles and
  ongoing NPC spawn/decision/respawn events. Its client did not reach snapshot
  readiness: the direct connection timed out, and the room-code retry failed
  at coordinator join because local room `PTTPR2D` was not visible. No playable
  client proof was obtained.
- `agent.gui_scan` and `agent.terminal help` completed through the dev-loop
  command queue. A later active-run switch made `client-01` ambiguous: an
  `agent.leave` and `agent.join 127.0.0.1:54868` command were delivered while
  the current run was Bloodtest (`20261124_015025`), not the Payload test. I
  notified the likely owner and stopped all runtime commands. The effect of
  those two commands on that client was not confirmed.
- The attached status showed the CLI status path responding while
  `--command "team_pick 1" --client client-02` produced no output and was
  terminated by the outer shell after 40 seconds. This points to a stuck or
  delayed command/ack path; its exact timeout stage is not yet proven.
- The external Payload PID 19056 was alive in an earlier process sample but
  absent from a later sample. Its current state is unknown. The root dev-loop
  subsequently pointed at run `20261124_022440_01`, mode 44 Juggernaut Horde;
  I did not issue commands into that run.

## Changes

- Added `--expected-run-id` for one-shot dev-loop commands. When `--client` is
  specified, the run ID is required. The CLI checks the selected run before
  sending; the daemon rechecks it before queueing so a run switch cannot silently
  retarget `client-01` in another task's run. Runtime-targeted commands compare
  against the runtime's run ID.
- Documented the run-pinning rule in the multi-instance contract.
- Added four pure CLI tests for required pins, matching and mismatched run IDs,
  and runtime-targeted commands. All four pass; both changed Python modules
  passed `py_compile`.
- Linked the user's ChatGPT/OpenCode communication request to existing issue
  `issue-70cbf4d13c02` and suggested a shared inbox/thread UI with auto-wake,
  issue/run/build context, and an explicit acknowledgment or surfaced timeout.
  A direct message was sent to the OpenCode agent; no reply had arrived when
  this record was written. Another agent did reply directly through the shared
  coordination brain, so cross-agent messages do not require user copy/paste.

## Remaining proof

The run-ID guard is covered by CLI tests but has not been exercised against a
live daemon after its Python code reload. The server identity mismatch is
diagnosed; a fresh client attached to the matching Payload run and visible NPC
gameplay remain unverified. No C++ or game-DLL build was requested.
