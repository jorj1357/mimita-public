# Build-wait wake review and generation 405 compile failure

## Date

2026-11-20

## Agent

`build-wait-reviewer`

## Summary

Bounded agent-wake activation `wake-f62ce3ee3cb6` (assigned
`issue-b0ba8698acd7`). No source edits and no builds started.

- `issue-b0ba8698acd7` (`game-dll-build` at `game_build`, 60.024s) belongs to
  attempt `game-dll-20261120T163826804025Z-9b84b5`, generation 405, source
  generation 21. The wait was not an idle control path: the build ran
  `1466.52s` and then failed at `failed_phase: compile` with
  `worker_error: compile failed: 1 file(s) in 336.59s`.
- The compile error is real and specific:
  `src/network/server-gamemode.cpp:3949` (and 3950-3951) reference
  `npc.body.ground.groundNormal`, but `entities::GroundState` at
  `src/entities/player.h:410` has no `groundNormal` member. The ground normal
  lives on `movement-types.h:118` / `movement-step.h:22`.
- Filed `issue-b91438ac2972` (build-friction, high) with the exact compiler
  output, attempt path, reproduction, acceptance, and dependency on
  `issue-b0ba8698acd7`, and sent the owner (`arcade-collision-elite-proof`)
  an evidence message before the next build.
- Added another independent reproduction of `issue-62ab3daa615d`: the woken
  opencode session received only
  `"You were woken by MiMITA agent-wake. This is a bounded test activation."`
  while the prompt file held the full assignment. The actual task was
  reconstructed from `MIMITA_WAKE_ID` / `MIMITA_WAKE_ISSUE` and the on-disk
  prompt.
- Noted duplicate wait issues for the same build window
  (`issue-bb273ac1386c`, `issue-40dfaa2a803c`) as a wait-policy recurrence
  question; no duplicate issue created.

## Evidence

- `.dev/agent-wake/requests/wake-f62ce3ee3cb6.json`
- `.dev/agent-wake/logs/2026-11-20T16-39-29.241Z-build-wait-reviewer.prompt.txt`
- `.dev/build-timings/game-dll-20261120T163826804025Z-9b84b5.json`
- `src/entities/player.h:410`, `src/network/server-gamemode.cpp:3949`
- `agent-coord issue review issue-b0ba8698acd7`
- `agent-coord issue review issue-62ab3daa615d`
- `agent-coord issue add` -> `issue-b91438ac2972`

## Not proven

Build success is not behavior proof. The compile-failure fix still needs an
owner implementation and a root-dev-loop build whose build-timing summary
reports `success=true`; any user-visible payload telemetry behavior still
needs a pinned runtime journal proof.
