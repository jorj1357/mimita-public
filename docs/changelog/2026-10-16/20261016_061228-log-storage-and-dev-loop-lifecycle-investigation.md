# Log storage safeguards and dev-loop lifecycle findings

- Timestamp: 2026-10-16T06:12:28Z (2026-10-16 02:12:28 EDT)
- Branch: `20261011zt`
- HEAD: `3a345200`
- Result: `PASS_WITH_HUMAN_REVIEW`

## Goal

Reduce unbounded structured-log growth while recording the user's reported
dev-loop disappearance, server-room availability, and one-command `ss` issues
without weakening existing process/build identity safeguards.

## Documents and source reviewed

- `docs/ROUTER.md` and the user-provided `AGENTS.md`.
- `docs/specs/debug-logging/debug-logging.md` and
  `docs/skills/logging-checker-v1.md`.
- `docs/specs/dev-loop/dev-loop.md`.
- `docs/specs/networking/networking.md` and
  `docs/architecture/terminal-commands/terminal-commands.md`.
- `docs/architecture/time-and-formatting/time-and-formatting.md`,
  `docs/operations/task-completion/task-completion.md`, and
  `docs/skills/documentation-checker-v1.md`.
- `docs/skills/spec-behavior-review-v1.md`.

The routed `docs/regressions/regressions-v1.md` path does not exist in this
checkout; the available current entry point is `docs/regressions/README.md`.
No new regression was added because the reported room failure was not
reproduced in this session.

## Changes made

- `src/debug/structured-log.cpp` and `src/debug/structured-log.h`: added a
  100,000,000-byte cap for each canonical event journal and a 1,000,000,000-byte
  regular-file ceiling for the `logs` tree; use shared Windows coordination
  for concurrent processes; prune eligible completed runs while protecting
  active run directories; reject writes that cannot fit and attach an
  accumulated dropped-record count to a later accepted record.
- `src/physics/movement/physics-collision-glb-main.cpp`: rate-limit repeated
  NPC collision-fallback diagnostics to one per 1,200 ticks per actor, with a
  suppressed-count summary and reset on recovery.
- `src/ragdoll/ragdoll-mode.cpp`: avoid repeating the hybrid pose-input event
  and sample reset on every frame when the same avatar still has no body
  parts.
- `docs/specs/debug-logging/debug-logging.md`: changed its obsolete
  “planning only” status to partial implementation and recorded known gaps:
  no active-file segment rotation, no runtime quota proof, and remaining
  legacy/direct-file logging sinks.
- `docs/specs/dev-loop/dev-loop.md`: added section 19.7 documenting the daemon
  lifetime/status mismatch, `ss` path, server identity guard, observed stale
  server/build evidence, and the exact runtime reproduction still required;
  added section 22 as the current CLI and interactive-key reference.
- `docs/specs/networking/networking.md`: added section 67.1 specifying the
  one-command local preset start/rejoin behavior, including idempotence and
  safe handling of stale server identity.
- `docs/gold/2026-10-16-doc-first-dev-loop-and-ss-plan.md`: captured the user's
  documentation-first prediction, the current spec/code map for `ss`, and a
  safe sequence for the eventual implementation.

## Findings

The dev-loop specification says a host supervisor is required for daemon
resurrection. The script itself runs as a foreground Python process and its
shutdown path writes stopped status; `--auto-restart` is for the game client,
not the Python daemon. Current `state()` reports `run_alive` as the client OR
server state and provides no daemon PID/heartbeat/script hash, so the status
does not prove the command owner is alive.

`config/command-aliases.json` maps `ss` to
`server_start_preset sandbox_1`. The terminal handler refuses while a
connection is active; `launchServerPresetAndJoin()` refuses while its local
listener or server-process flag remains active. The multiplayer welcome path
rejects mismatched run/build identities. These guards should be preserved;
the launch/rejoin owner needs to reconcile the existing connection/server.

At inspection, `.dev/state.json` showed latest build 2429, adopted server PID
33140 on build 2427, `builds_match=false`, and no client PID. This is concrete
stale-server evidence, but not proof it caused the earlier “room identity not
found” report. The game leave + immediate `ss` sequence remains unverified.

## Validation and evidence

- Canonical dev-loop build published build 2429 successfully. `.dev/state.json`
  reported `build_status=success` and `latest_build_stale=false`.
- `git diff --check` passed for the changed source and documentation paths.
- No tests were run.
- No fresh client/gameplay runtime test was performed: the adopted server is
  still running build 2427 while build 2429 is current, and there was no client
  process. No live quota, rotation, room leave/rejoin, or `ss` behavior is
  claimed as validated.
- The current `logs` tree measured 12,730,573,761 bytes at inspection. The
  new logger limits have not yet run in a fresh game/server process, so this
  existing storage has not been reduced by the source changes in this session.

## Preserved pre-existing changes

The checkout already contained unrelated edits in assets, config, dev launch
modes, engine/gui/network/gamemode files, and the untracked
`config/gamemodes/juggernaut_duel.json`,
`docs/specs/gamemodes/juggernaut-duel.md`, and existing changelogs. They were
left intact and are not claimed as part of this work.

## Human review still required

- Reproduce the daemon-close path and verify host-supervised single-instance
  recovery, durable exit reason, and visible supervisor behavior.
- With a fresh matching build, run `ss`, leave once, immediately run `ss` once,
  and inspect the exact canonical journal for disconnect, room publication,
  identity validation, and joined gameplay state.
- Exercise logger caps with concurrent client/server writers, old-run pruning,
  per-file rejection, and recovery of a reported dropped-record count.
- Decide whether full active-file rotation and migration of the remaining
  direct-file log sinks belong in the next logging phase.

## Follow-up specification clarification

The user clarified that they expect repeated AI invocations of
`dev-loop.py --command ...` to keep the service available and to recover it if
it is absent, without a separate supervisor application. Updated section 19
and the AI workflow to make the same script's CLI entry point the
single-instance bootstrap: reuse a healthy daemon, start one background
daemon if missing/stale, wait for readiness, then send the work request once.
`--status` remains read-only. The spec also states that no program can prevent
external closure absolutely; availability is provided by detection and
recovery on the next work call.

Expanded the `ss` explanation in section 19.7: `ss` starts/joins the sandbox
preset; `leave` disconnects the client but may leave the server alive; and the
target `ss` behavior is to reconnect to the verified existing sandbox or
safely replace a proven stale server, then join once. Calling `ss` twice must
not create duplicate servers or connections. These are desired contracts, not
implemented or runtime-proven behavior.

`git diff --check -- docs/specs/dev-loop/dev-loop.md`: passed after the
clarification. No game or server was launched or stopped.

## Documentation-first command/spec follow-up

At the user's request, created a gold reference recording their prediction
that discussing and writing the behavior first makes large fixes more
manageable. The user supplied `GPT-6 Luna, Medium` as the expected model
context; this is recorded as their prediction, not verified telemetry.

Added the exact currently implemented `dev-loop.py` command-line options and
interactive keys to section 22 of the dev-loop spec. The reference separates
these from game-terminal commands: `ss` is an in-game alias, not a Python
option. Added networking section 67.1 rather than creating a new spec file:
existing networking sections 62 and 67 already own disconnect/server commands,
while the terminal-command architecture is generic. The missing desired rule
is that a preset start/rejoin should be idempotent and complete in one action.

The documentation checker scan found no actionable “todo: explain/clean/add an
explanation” item in the reviewed docs; matches were instructional examples or
historical changelog references. `git diff --check` passed for the updated
dev-loop/networking specs and the new gold document. No source code changed in
this follow-up and no gameplay/runtime test was run.

## Exact `ss` and leave/rejoin specification review

At 2026-10-16T06:27:41Z, the user asked for exact file paths/sections and a
plain explanation of why `ss` and leaving/rejoining are not yet “just go into
it and it works.” Re-traced the current preset code and refined the specs:

- `config/command-aliases.json:6` expands `ss` to
  `server_start_preset sandbox_1`.
- `src/terminal/network-commands.cpp:359-394` rejects the preset if
  `MP_CONTEXT.active`, then delegates to `launchServerPresetAndJoin()`.
- `src/terminal/network-commands.cpp:516-523` implements `server_leave` by
  calling `mpShutdown()` and leaves the separate server process alive.
- `src/gui/gui-main.cpp:355-383` rejects starting the preset when the
  in-process listener or external server-process flag is still active. It has
  no branch to rejoin that existing child; on its start path it queues a
  localhost `directAddress`.
- `src/network/multiplayer-tick.cpp:1029-1044` preserves expected run/build
  identity checks and reports `Server identity mismatch` for a mismatch.

This is a source-level explanation for why “leave once, run `ss` once” may fail:
leave can keep the server alive, while `ss` tries to start a new one and the
GUI launch owner refuses because that existing server flag remains set. This
matches the lifecycle shape the user describes, but it does not prove the
exact runtime message they saw. The exact literal “room identity not found”
was not present in the inspected owners. Also clarified that the local preset
launch collects a room code through a separate server handshake but currently
passes localhost direct-address as the client join address; those identities
must not be conflated.

Updated networking section 67.1 with the source-level disagreement and the
distinction between the published room code and the local direct join address.
Updated dev-loop section 19.7 to label `ss`'s current behavior versus desired
start-or-rejoin behavior. A standalone new spec file is not needed: networking
sections 62 and 67 already own disconnect and server commands, so section 67.1
is the focused addition. The terminal-command architecture stays generic.

The proposed spec-first workflow is sound, with one addition: approval of a
clear spec must be followed by a scenario acceptance matrix and observable
event sequence. A spec reduces implementation ambiguity but does not prove
the implementation; runtime scenarios must test normal, repeat, leave/rejoin,
server-dead, and identity-mismatch cases through the real executable and live
journal. `git diff --check` passed; no gameplay runtime was changed or run.

## Follow-up: clarify what “no localhost” means

The user asked whether localhost means the old non-networked game mode and
whether it is used today. Re-read networking section 48.4, which contains the
user's clarification: the concern is local-only implementations being
maintained or described as online. Corrected the previous doc wording that
could be read as “localhost is never valid.” A localhost address names this
same computer; a separate client and server can still exchange normal packets
there. That only proves a local route, not that a remote computer can connect.

The `ss` acceptance path remains room-code/coordinator/ICE because the
2026-09-27 regression documents a real dev-loop shortcut that bypassed room
registration and normal join. A narrow localhost test may be useful for packet
isolation, but cannot count as online acceptance. Updated networking 67.1,
dev-loop 19.7, and the gold note to preserve that distinction. Also clarified
that player gameplay commands come from the client over the network while the
server remains authoritative.

No code or running process was changed; no game was launched. `git diff
--check` validates documentation formatting only.

Correction to validation note: `git diff --check` reports trailing whitespace on
pre-existing/user-authored networking notes at the top of the file and in the
section 48.4 clarification. This follow-up did not clean those unrelated note
lines. The check therefore is not a clean pass; the newly edited paragraphs
were visually reviewed for whitespace instead.


## Follow-up: dev-loop responsiveness and GUI-equivalent preset startup

Read the dev-loop performance path, the server-preset command, GUI startup,
and the routed performance, terminal-command, documentation, and runtime
scenario guidance. Added a low-priority future investigation to dev-loop spec
section 5.1. The user says the loop currently works and is frustrated by
perceived update delay; this is recorded as an optional later measurement, not
a rewrite request. The note identifies the current 0.50 s watcher interval,
0.30 s build debounce, recurring source-tree snapshot (comment-estimated at
about 60 ms, not measured in this run), canonical incremental build, and
post-build artifact validation/copy. It recommends timing each stage before
tuning or considering modules and preserves one persistent entry point/daemon.

Expanded dev-loop section 19.7 and networking section 67.1 to specify that all
server-preset commands are shortcuts for the same GUI server setup: apply the
preset settings, start a registered dedicated server, wait for the room code,
and join through coordinator/ICE. The desired gameplay path forbids direct
`127.0.0.1` client addresses and loopback-only server binds. Explained the
separate local dev-loop control socket is AI-to-script IPC, not gameplay
transport.

Current source mismatch found: `src/terminal/network-commands.cpp` passes preset
settings to `launchServerPresetAndJoin()`. That helper in
`src/gui/gui-main.cpp:355-384` rejects an existing listener/server flag and
sets `gPendingConnect.directAddress` to `127.0.0.1` for both external-server
and in-process fallback paths. In contrast, the GUI's room-file poll path at
`src/gui/gui-main.cpp:655-667` takes the registered code and sets
`gPendingConnect.roomCode`. The preset helper is not called from GUI button
code, so it is not currently equivalent to the button-driven room-code flow.
The leave handler disconnects the client and preserves the server, but the
preset helper has no rejoin branch; the terminal command and GUI helper can
reject based on stale active/listener/process flags.

The dev-loop itself launches a server with `--room-file`, waits for the
coordinator-issued code, and launches the client with `--room`; however its
`npc-navigation` profile binds the game server to `127.0.0.1:1357`, so that
profile cannot prove remote reachability. The control socket also uses
loopback, but that is local process-control and should not be confused with
gameplay networking.

No implementation, build, or game scenario was run. This was source/document
investigation only. The runtime proof still needs the real GUI-equivalent preset
path, non-loopback server bind, room registration, client room-code join, and
leave/rejoin scenario; a second remote client is needed to claim cross-machine
reachability. This entry follows `docs/skills/spec-behavior-review-v1.md`,
`docs/skills/efficiency-checker-v1.md`, and
`docs/skills/terminal-command-checker-v1.md`.


## Follow-up: explain the control socket and investigate the build-2430 launch

Expanded dev-loop spec section 19.8 with a plain-language explanation of the
local control socket: the short-lived CLI sender asks the persistent Python
loop to do work; the loop writes a command to the client queue; the game client
executes it. The `127.0.0.1` control address is private Python-to-Python IPC,
not the gameplay endpoint. The spec now contrasts current behavior (requests
require an existing loop) with desired self-start behavior (one daemon starts
if absent, then receives exactly one request).

Investigated the user's build-2430 launch report and journal
`logs/2026-10-16/20261016_074515/events.jsonl`. Build 2430 passed
`--versioninfo`; the dev-loop then attempted to start server PID 10820 with
bind `127.0.0.1:1357`. It did not find that exact PID/role's `logger.started`
event. Source `wait_for_logger_started()` waits up to 20 seconds. The
`server_logger_missing` failure branch saves state, calls `stop_server()`, and
returns before the later client launch block. The PID was no longer running at
inspection; no client PID was present. Thus the first confirmed divergence is
server readiness, and the game client was never launched. The underlying
server startup error is unknown: the captured journal has no matching logger
start/failure event or exit code, and the child stdout/stderr are not copied to
a durable diagnostic record by this launch path.

Found a status freshness issue on that branch: it saves `state.json` before
`stop_server()` clears the server fields and does not save a new snapshot after
cleanup. The retained status may still show the failed child as alive. Also,
`run_alive` means server alive OR client alive, not client-ready. Added a
specification finding requiring post-cleanup status and separate daemon/server/
client states.

Expanded networking spec section 67.1 to answer where
`launchServerPresetAndJoin()` originates and to preserve the existing-server
reuse pattern as a future shared GUI/terminal lifecycle. Current source declares
the helper in `src/gui/gui-main.h`, defines it in `src/gui/gui-main.cpp`, and
calls it from the terminal preset handler. The dev-loop's `launch_latest()`
checks owned server/configuration/room-code reuse and starts the client with
`--room`; if no reusable server exists, it starts with `--room-file`, waits
for registration, then joins by room code. This is source evidence only, not
live acceptance of either path.

No code was changed and no new launch was attempted. No game or server was
stopped by this investigation; the failed server PID observed in the supplied
run had already exited through the dev-loop's failure cleanup. No tests/build
were run. The required behavior is documented only.

## Follow-up: preset GUI parity and launch stall investigation

- Time: 2026-10-16T08:01:00Z (2026-10-16 04:01:00 EDT)
- Branch: `20261011zt`
- HEAD: `3a345200`
- Result: `PASS_WITH_HUMAN_REVIEW`

### Changes in this follow-up

- `src/gui/gui-main.cpp`, `launchServerPresetAndJoin()`: replaced immediate
  `directAddress=127.0.0.1` connection with the GUI's room-code registration
  path. New external servers wait for `pollPendingServerRoomCode()`; the
  in-process fallback queues its registered room code. A live external server
  is reused only when its process is active and map/mode/name/port match the
  selected preset. A stale child handle is cleared without terminating any
  other process.
- `src/terminal/network-commands.cpp`, `server_start_preset`: while already in
  a multiplayer game, now prints that the player is already in a game and
  leaves state unchanged.
- `src/debug/structured-log.cpp`, `pruneCompletedRuns()` and
  `StructuredLogger::init()`: removed redundant full `logs`-tree reconciliations
  around startup pruning. The shared quota total is initialized once and
  protected by the cross-process mutex. The quota-boundary write path still
  performs a full reconcile before pruning; pruning and file/tree ceilings
  remain enabled.
- `devscripts/dev-loop.py`, `wait_for_logger_started()` and
  `launch_latest()`: source now checks for an early child exit during readiness
  and records `dev-loop.server.startup_failed` with the exit code, PID, build
  path, journal path, and the failure stage. A child that remains alive but
  does not announce logger readiness is distinguished from an early exit.
  The already-running daemon had loaded the older Python source, so this new
  diagnostic was not active during the runtime reproduction.
- Updated `docs/specs/networking/networking.md` section 67.1 and
  `docs/specs/dev-loop/dev-loop.md` section 19.7 with current code status,
  runtime evidence, the likely startup bottleneck, and remaining acceptance.

### Investigation evidence

- User-provided run `20261016_075225` (build 2431) recorded server launch then
  `server_logger_missing`; it did not record a client launch.
- A controlled attempt through the existing dev-loop published build 2436 and
  run `20261016_075833`. The child server PID 33560 remained alive during the
  20-second `logger.started` wait. The journal had launcher events but no
  server `logger.started`; at the timeout the dev loop stopped its own child
  and returned before constructing the client. No other game/server process
  was stopped by this follow-up.
- `runServer()` calls `StructuredLogger::init()` before any later server
  initialization. That logger function held the shared events-file mutex while
  doing full-tree reconciliation, pruning (including additional tree/run
  scans), and another reconciliation. Given the previously measured logs tree
  above 12 GB, this is a likely startup stall. No stack trace or per-stage
  timing was available, so the exact cause remains unproven.
- During this work two dev-loop Python processes were present in the checkout.
  They alternated `.dev/state.json` and published different build/run snapshots.
  They were not stopped. The shared status eventually showed build 2438,
  source generation 3, current, but the duplicate loop owners make that status
  insufficient for a clean post-fix runtime claim.
- Existing processes also included a long-running `.dev/builds/2251` server
  bound to `127.0.0.1:1357`, plus a `mimita-v2.0.7` dedicated server. These
  were preserved. No post-fix server/client launch was attempted against the
  contested server port.

### Validation and limits

- Source was published by the existing loop as build 2438 with
  `latest_build_stale=false` and source generation 3. This is build/publish
  evidence only; the duplicated daemon status means it is not a cleanly
  attributable live runtime proof.
- `git diff --check` found no issue in the edited source paths. It reports the
  pre-existing trailing whitespace in the user's clarification at
  `docs/specs/networking/networking.md:3350`; that note was preserved.
- No tests were run. The last launched server attempt happened before the
  logger-scan optimization and was stopped by its owning dev-loop at the
  readiness timeout. The new `ss` room-code path and startup optimization
  still require runtime acceptance using one dev-loop owner and a fresh journal.
- Focused reviews: `docs/skills/spec-behavior-review-v1.md`;
  `docs/skills/terminal-command-checker-v1.md`;
  `docs/skills/efficiency-checker-v1.md`;
  `docs/workflows/runtime-scenario-validation.md`.

### Human review still needed

With exactly one dev-loop owner and no conflicting listener on the selected
server port, launch the current published build. Confirm that server
`logger.started` appears promptly, the server publishes a room code, the client
window reaches normal readiness, and the client joins. Then run `ss`, leave
once, run `ss` once, and confirm the existing matching room is reused without a
localhost gameplay route. Compare `events.jsonl`, `state.json`, build identity,
and process PID/exit code at each stage.

Correction to the startup optimization detail: `StructuredLogger::init()` now
performs one full-tree reconciliation so legacy/external files are still
counted, then passes that exact byte total into pruning and subtracts the
reclaimed bytes. It removes the redundant full-tree scan inside pruning and
the post-prune re-scan. The live quota-boundary path retains its reconcile
before pruning. This preserves startup accounting while reducing repeated
traversal.

Final validation update (2026-10-16T08:02:22Z): the existing watcher published
build 2439 with source generation 4 current after the logger optimization. The
`dev-loop.py` source passed `python -m py_compile`. No post-fix runtime launch
was possible: `python devscripts/dev-loop.py --request launch --launch-mode 1`
reported `no running daemon control endpoint`, and another dev-loop Python
process still existed without a responsive control endpoint. I did not start a
competing loop. The previously observed `.dev/builds/2251` server still
occupied `127.0.0.1:1357`, so a new run against that port would not establish a
clean client/server acceptance test. The current build is compile/publish
evidence only; runtime acceptance remains open.
