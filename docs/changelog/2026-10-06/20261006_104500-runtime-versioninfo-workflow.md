# Runtime versioninfo and scenario workflow

Time (EST): `2026-10-06 10:45:00 -04:00`
Branch: `afad20a-rebuild`
Status: `PASS_WITH_HUMAN_REVIEW`

## Request

Make `--versioninfo` a bounded command-line probe that exposes the exact
canonical `events.jsonl` path, and document a runtime-first validation workflow
so agents run the real executable and inspect live JSONL evidence before
creating synthetic tests.

## Changes

- `src/game/game-cli.cpp:81-132`: added `runVersionInfoCli()`. It initializes
  `StructuredLogger`, records `versioninfo.executed`, prints executable path,
  PID, working directory, events path, run ID, and JSON metadata, then exits.
- `src/game/game-cli.cpp:257`: dispatches `--versioninfo` before normal game
  startup.
- `docs/workflows/runtime-scenario-validation.md`: added the required
  diagnose, instrument, build, versioninfo, real-runtime, inspect-JSONL, and
  iterate workflow plus scenario/test policy.
- `docs/architecture/terminal-commands/terminal-commands.md:90-119`: added
  the AI runtime validation workflow to the terminal/action architecture.
- `docs/ROUTER.md:16,82`: routed runtime behavior validation to the new
  workflow and existing logging/terminal specifications.
- `AGENTS.md:24-28`: added the mandatory runtime-evidence-before-new-tests
  gate.

## Evidence

- Initial canonical build attempt could not link `mimita.exe` because the
  pre-existing `C:\mimita-v9\mimita.exe` process held the destination lock.
  The process was preserved.
- Safe unique-output build succeeded with:
  `MIMITA_EXE_NAME=mimita-20261006T104300-versioninfo.exe python build_agent.py`.
- Runtime command succeeded and exited without graphics startup:
  `mimita-20261006T104300-versioninfo.exe --versioninfo`.
- Console output exposed:
  `EVENTS_JSONL_PATH=logs/10-06-2026/20261006_104305/events.jsonl`.
- That journal contains `logger.started`, `versioninfo.executed`, and
  `logger.stopped`; the `versioninfo.executed` record identifies the exact
  executable and PID.
- `git diff --check` found only pre-existing whitespace warnings in unrelated
  files; no new whitespace error was introduced by the targeted changes.

## Scope boundary

This proves the bounded identity probe and documentation path. It does not yet
implement the future deterministic gameplay scenario runner or prove any
particular gameplay behavior. Human review remains useful for confirming that
future agents can follow the printed path and that the workflow is discoverable
from the router.

## Routed documents and focused review

- `AGENTS.md`
- `docs/ROUTER.md`
- `docs/architecture/terminal-commands/terminal-commands.md`
- `docs/specs/debug-logging/debug-logging.md`
- `docs/skills/spec-behavior-review-v1.md`
- `docs/skills/logging-checker-v1.md`
- `docs/operations/build-and-exe/build-and-exe.md`
- `docs/operations/task-completion/task-completion.md`

Specification/behavior review: `PASS_WITH_HUMAN_REVIEW`.

## Pre-existing edits

The worktree already contained broad unrelated edits and deletions across
configuration, audio, networking, NPC, collision, rendering, tests, and
documentation. They were preserved. The existing `mimita.exe` process was not
stopped or overwritten.
