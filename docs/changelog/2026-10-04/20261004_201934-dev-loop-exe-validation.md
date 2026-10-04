# Dev-loop EXE Validation Fix

Time created: 2026-10-04T20:19:34Z  
Display time: 2026-10-04 16:19:34 EDT  
Branch: `afad20a-rebuild`

## Outcome

Fixed the development loop so it cannot publish or select a malformed
`mimita.exe` as a successful build. The observed build 1176 artifact had valid
PE headers but zero-filled entrypoint bytes and crashed with `0xC0000005` before
server startup.

## Changed

- `devscripts/dev-loop.py`
  - Added `_executable_snapshot_is_valid` to validate PE structure, executable
    sections, and nonzero entrypoint bytes.
  - Added `_copy_verified_executable` to detect source changes, copy through a
    temporary file, compare SHA-256 hashes, and atomically publish the EXE.
  - Updated `newest_valid_published_build` to skip invalid published artifacts.
  - Replaced the direct `shutil.copy2` executable publication in `publish_build`.
- `docs/regressions/2026-10-04/dev-loop-zero-filled-exe-REG.md`
  - Recorded the confirmed malformed-artifact regression and evidence.

Unrelated pre-existing worktree changes were preserved.

## Documents and review

Read:

- `docs/ROUTER.md`
- `docs/specs/networking/networking.md`
- `docs/specs/gamemodes/gamemodes.md`
- `docs/operations/build-and-exe/build-and-exe.md`
- `docs/operations/task-completion/task-completion.md`
- `docs/architecture/terminal-commands/terminal-commands.md`
- `docs/skills/terminal-command-checker-v1.md`
- `docs/regressions/README.md`
- `docs/architecture/time-and-formatting/time-and-formatting.md`

## Validation evidence

- `python -m py_compile devscripts/dev-loop.py`: PASS.
- `python devscripts/dev-loop.py --help`: PASS.
- Published artifact validation: build 1176 rejected for zero-filled
  entrypoint; build 1177 accepted.
- Build 1177 server probe: PASS. It loaded `dust2cyberiav4`, bound UDP on
  `127.0.0.1:1361`, spawned one NPC, registered room `NCY9XJT`, wrote the room
  file, and exited normally at the explicit three-second timeout.
- Human dev-loop/client join acceptance: still required.

## Status

`PASS_WITH_HUMAN_REVIEW` for the publisher fix and bounded startup probe.
The malformed build 1176 artifact should not be reused; the next dev-loop
launch should select the verified replacement or create a new verified build.
