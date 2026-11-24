# Server room startup progress investigation

## Request

Keep room creation and the initial join in a truthful connecting/loading state
while the server progresses; show named stage durations in the client; make
in-game room creation and joining dependable. The user also asked that AI work
keep returning to its stated goal after coordination and measure whether each
action advances it efficiently.

## Source change

In `src/gui/gui-main.cpp`, `pollPendingServerRoomCode()` previously stopped
reading the server journal as soon as `logger.started` was observed. That also
stopped consuming later `server.startup.stage` events, freezing
`gServerStartupProgressMs` during world loading and room registration. Removed
the logger-ready condition from the journal-reader guard; the logger latch
still changes only when the matching server PID emits `logger.started`.

This addresses progress accounting for the in-game room-code watchdog. It does
not implement the requested stage list/checkmarks UI, alter the client's
initial-connect deadline, or fix the separate post-join snapshot stall.

## Specification and trace findings

The networking loading contract in `docs/specs/networking/networking.md`
requires initial join/map synchronization to remain **Loading**, show the
current step and total elapsed time, report completed-step durations, and name
the stalled step with Retry/Cancel. Reconnecting is reserved for an established
session that the health owner has judged lost. The current GUI room-code wait
does not render the requested stage history.

The matching current run available for inspection was
`20261124_004333`, server PID 32360, EXE build
`2026-11-23T19:22:15.804284Z:2e24d312603e4135cdd9be0d870a11ce4be98042e253296de6ca16ca189d512b`,
game DLL generation 576:

- Server `world_load`: 17,436.645 ms; map `funworld3`.
- Server `room_register`: 1,075.777 ms; room `R99C54P` registered.
- `network.server.local-gameplay-ready` preceded public room registration.
- Server received and accepted player 2's join at about 00:49:25 UTC.
- Server marked the peer stale after 2,512 ms silence (2,500 ms threshold),
  then expired it after 70,058 ms silence at about 00:50:36 UTC.
- Exact journals: `logs/2026-11-24/20261124_004333/server-01/events.jsonl`,
  `logs/2026-11-24/20261124_004333/client-01/events.jsonl`, and
  `logs/2026-11-24/20261124_004333/dev-loop/events.jsonl`.

This proves successful startup, room registration, and server-side join
acceptance for that run, followed by loss of peer responsiveness. It does not
prove public/community-browser discoverability, human-visible client
acceptance, or resource-pressure causality. The console excerpt supplied by
the user has no run/build identity and was not attributed to this run.

## Coordination and validation

- Added the user's report to coordination context `ctx-5c14d94d3bdb`, linked
  to `issue-c8ac61cf07fc`; recorded stage-display follow-up there.
- Added the goal-following feedback to existing autonomy issue
  `issue-91e3384b219b`.
- Added run evidence to `issue-c8ac61cf07fc` and cross-referenced snapshot
  investigation `issue-453d0596e294`.
- Added proof task `room-code-progress-reader-logger-gate`; it remains
  unverified.
- Source diff is one condition change. No tests were added or run.
- No build was requested: the shared checkout has pending compiler inputs and
  Mode 3 requires the root coordinator to confirm owners ready before sealing
  one batch. The published EXE is stale for this edit, so source/build/runtime
  proof and in-game room-creation acceptance remain pending.

## Focused review

The requested loading-state distinction is consistent with the networking
specification. This change restores stage progress to the existing room-code
deadline without changing timeout thresholds or reporting a successful room
code as an error. Full completion still requires a coordinated build, an exact
in-game create/join run, stage UI, and a separately traced fix for the observed
post-join snapshot loss.
