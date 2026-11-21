# Room-timeout context acknowledgement and progress-gap review

## Task

- Task ID: `wake-669d2c2fc167`
- Session ID: `session-f9e9b7c4a437`
- Owner: `opencode`
- Focus: review high-priority user context `ctx-4cff4a73ed21`
- Summary: confirmed routing, acknowledged context, reviewed the in-progress
  progress-aware room-code timeout, and appended one correctness finding to the
  issue discussion
- Status: complete, no source edits, no build request
- Date, time, timezone: 2026-11-21T15:40:00Z, ISO 8601
- Branch: `checkpoint/one-checkout-condensation-20261111`
- Base commit: `d651e133`
- Final commit: uncommitted

## Pre-existing changes

- The worktree already had modifications and untracked files from other agents,
  including an in-progress edit to `src/gui/gui-main.cpp` that adds a
  progress-aware room-code deadline and a change to `config/*`, devscripts, and
  issue/changelog files.
- This session did not modify or revert any of them.

## Requested behavior

The wake prompt file
`.dev/agent-wake/logs/2026-11-21T14-36-14.233Z-opencode.prompt.txt` requested
review of user context `ctx-4cff4a73ed21` and an acknowledgement after routing
or repairing ownership. The user reported that creating a room timed out while
the server terminal printed slowly during a build or high CPU/RAM load, and
asked that the timeout not fire only because a build or slow terminal is
happening.

## Specification alignment

- Context `ctx-4cff4a73ed21` was already linked `primary` to
  `issue-c8ac61cf07fc` ("Room creation times out while server starts under
  system load"), so no ownership repair was needed.
- `root` is the active working agent for that issue
  ("investigate room creation timeout during build / slow terminal").
- No code was edited, consistent with the wake instruction to not edit code
  without human approval.

## Findings

- The timeout is driven by the room-code file written by the server after
  `network.room.registered`; server console print speed is not on the timeout
  path.
- The in-progress `src/gui/gui-main.cpp` change makes the deadline
  progress-aware via `gServerStartupProgressMs`, but the journal reader is still
  gated by `if (!gServerLoggerReady ...)`. Once `logger.started` is seen, later
  `server.startup.stage` events stop updating progress, so a legitimately
  progressing `world_load`/`network_bind`/`room_register` phase under load can
  still trip the 45 s stall after the 60 s minimum.
- Appended the finding to `issue-c8ac61cf07fc` and sent it to `root` on thread
  `thread-room-timeout`.

## Evidence

- `src/gui/gui-main.cpp` room-code poll and timeout path.
- `src/network/server.cpp` room-file write after registration.
- `src/network/server-ice.cpp` registration stages (TURN, ICE gather,
  `coordinatorIceHost`).
- Coordination message `msg-9730bdbe95be`, thread `thread-room-timeout`.
