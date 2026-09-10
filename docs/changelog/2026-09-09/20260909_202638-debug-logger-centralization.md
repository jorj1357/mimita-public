# Debug logger centralization

Time: 2026-09-09 20:26:38 -04:00 (America/New_York)
Branch: `8292026stash...origin/8292026stash`
Status: PASS_WITH_HUMAN_REVIEW

## Goal

Route the legacy debug facade and the highest-volume diagnostic records through
`StructuredLogger`, with `config/debuglogger.json` as the filter source and the
normal process run log as the primary output.

## Changes

- `src/debug/structured-log.h/.cpp`: expanded structured categories to cover the
  legacy debug categories, added `DBG(Category, ...)`, added formatted legacy
  adapter entry points, and routed structured records to `LogManager`.
- `src/debug/debug-log.cpp/.h`: legacy `Debug::log`, `warn`, `error`,
  `logOnce`, `logThrottled`, and `logAuto` now use structured category/level
  filtering instead of the old `DebugConfig` flags.
- `src/debug/log-manager.h/.cpp`: added direct console output to the saved
  stdout descriptor so structured records do not duplicate through stdout
  capture.
- `src/network/server.cpp`: dedicated and listen-server loops now poll the
  debug logger configuration while running.
- `config/debuglogger.json`: all diagnostic categories are off by default,
  category files are disabled by default, and summary generation is disabled.
- Converted the pasted high-volume paths in camera, main-menu, animation,
  audio, NPC combat, projectile receive, client snapshots/entities, server
  damage/packet rejection/NPC projectile/spawn, coordinator polling, and
  rocket aim diagnostics to `DBG`.
- Added `tools/check-debug-logging.py` to inventory likely raw diagnostic
  output and direct diagnostic file writers.

## Evidence

- `python build_agent.py`: `Status: SUCCESS`, return code 0, 2026-09-09
  20:22:30 build result.
- `config/debuglogger.json` parses successfully.
- `git diff --check` reported no whitespace errors.
- The static inventory reports `diagnostic_bypasses=1026`; this is an explicit
  remaining migration list, not a claim that the whole repository is already
  consolidated.

## Remaining work and human review

- Remaining raw diagnostic `printf` calls and direct diagnostic file writers
  still bypass `StructuredLogger`; they must be migrated or placed in a small,
  reviewed intentional-output allowlist before strict repository-wide
  consolidation is complete.
- No live client, dedicated-server, or listen-server hot-reload trial was
  performed in this session.
- No visual or multiplayer acceptance is claimed.

## Documents and focused review

- `AGENTS.md`
- `docs/ROUTER.md`
- `docs/specs/debug-logging/debug-logging.md`
- `docs/skills/logging-checker-v1.md`
- `docs/operations/build-and-exe/build-and-exe.md`
- `docs/operations/task-completion/task-completion.md`

## Pre-existing worktree changes

Unrelated modifications were present and preserved, including account,
analytics, ragdoll, regression, camera/engine, simulation, ragdoll-mode, and
`src/debug/npckillfeed-log.*` changes. The final worktree was not clean before
this session.
