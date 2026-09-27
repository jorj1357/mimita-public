# Dev-loop stale-build guard and disk restoration

- time_utc: 2026-09-27T19:54:00Z
- display_timezone: America/New_York
- display_time: 2026-09-27 15:54:00 EDT
- branch: `afad20a-rebuild`
- result: `PASS_WITH_HUMAN_REVIEW`

## Request

Fix `devscripts/dev-loop.py` so `[1]` does not relaunch a stale executable,
queues a build/retry when needed, shows compiler errors in its output, and
restores the newest published build from disk at startup.

## Implementation

- `devscripts/dev-loop.py:254-263` adds `newest_valid_published_build()`, which
  scans numbered `.dev/builds` artifacts newest-first and accepts only a
  non-empty `mimita.exe`.
- `devscripts/dev-loop.py:313-324` restores that artifact at startup and marks
  it stale until the current source tree passes a build.
- `devscripts/dev-loop.py:430-452` marks the previous artifact stale after a
  failed build and prints an explicit compiler/build diagnostics section. The
  full child output continues to stream before the filtered diagnostics.
- `devscripts/dev-loop.py:455-460` refuses direct stale launches.
- `devscripts/dev-loop.py:597-614` prevents automatic process recovery from
  relaunching stale artifacts and waits for `[1]`.
- `devscripts/dev-loop.py:698-707` makes `[1]` queue a build retry when the
  artifact is stale or missing; fresh artifacts retain the switch/relaunch
  behavior.
- `devscripts/dev-loop.py:652` updates the status text to describe both
  retry/build and switch behavior.

## Root cause and reasoning

The old loop stored `latest_build` only in memory, left it unchanged on build
failure, and routed both `[1]` and automatic recovery directly to
`launch_latest()`. A failed current-source build therefore left the prior
published EXE looking like the newest build. The new state explicitly treats
that artifact as inspectable but stale and requires a successful build before
launch.

## Documents and focused skills

Read and followed:

- `AGENTS.md`
- `docs/ROUTER.md`
- `docs/operations/build-and-exe/build-and-exe.md`
- `docs/operations/task-completion/task-completion.md`
- `docs/architecture/terminal-commands/terminal-commands.md`
- `docs/skills/terminal-command-checker-v1.md`
- `docs/regressions/README.md`

Regression record:
`docs/regressions/2026-09-27/dev-loop-stale-build-launch-REG.md`

## Validation and limits

- Python syntax compilation passed.
- `--help` passed.
- Non-launching state-machine check restored build `0058` and reported it as
  stale with a pending validation build.
- `git diff --check` passed; only existing line-ending normalization warnings
  were present.
- No game build or process restart was performed. The currently running
  dev-loop process must be restarted to load this Python fix, and the next
  build must still pass before the reset-menu C++ behavior can appear.

## Pre-existing edits

Unrelated config/NPC/physics changes and existing untracked changelogs were
preserved. The earlier ESC reset-menu edits were also preserved.

## Human review still needed

Restart the dev-loop, confirm it restores the newest artifact, intentionally
observe a failed build's compiler diagnostics, press `[1]`, and confirm a
successful build publishes and launches a new numbered artifact without
relaunching the stale one.
