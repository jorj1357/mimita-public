// 09 06 2026, 16 22
/* purpose
* record the canonical universal time and filename standard added this session
* preserve exact documentation scope, evidence, and migration boundaries
* distinguish this documentation change from unrelated existing worktree edits
* this file DOES NOT claim that C++ log generation has been migrated
* this file DOES NOT rename historical logs or changelogs
* this file DOES NOT modify authentication or gameplay behavior
*/

# Task

- Task ID: DOC-TIME-FORMAT-2026-09-06
- Summary: add universal UTC ISO 8601 time and generated filename standard
- Status: PASS_WITH_HUMAN_REVIEW
- Date: 2026-09-06
- Time: 16:22:26
- Timezone: EDT (America/New_York)
- Branch: not re-read as a clean branch because unrelated worktree changes were present
- Final commit: not committed in this session

# Pre-existing changes

The worktree already contained unrelated modified, deleted, and untracked files,
including authentication changes and a login-fix changelog. Those files were
not edited, deleted, or overwritten by this session.

# Requested behavior

Use one universal standard for new generated data and documentation artifacts:

- UTC ISO 8601 for stored timestamps.
- `yyyy-mm-dd` for date folders.
- `yyyymmdd_hhmmss` for generated filenames.
- Examples: `combat_20260906_193200.log` and
  `20260906_193200-camera-fov.md`.

# Exact documentation changes

## `docs/architecture/time-and-formatting/time-and-formatting.md`

Added the authoritative cross-repository standard for canonical UTC timestamps,
date folders, generated filenames, local display context, historical files, and
future migration work.

## `docs/ROUTER.md`

Added the universal time-and-formatting document to the documentation hierarchy
and routed time, date, filename, and generated-artifact tasks to it.

## `docs/specs/debug-logging/debug-logging.md`

Declared the existing `mm-dd-yyyy` and `mmddyyyy_hhmmss` examples historical
compatibility formats and pointed new log generation to the universal standard.

## `docs/operations/task-completion/task-completion.md`

Updated new changelog path guidance to use `yyyy-mm-dd` folders and
`yyyymmdd_hhmmss-three-word-summary.md` filenames.

# Validation

- Read `docs/skills/documentation-checker-v1.md` before editing.
- `git diff --check`: passed; Git reported only existing line-ending normalization warnings.
- Confirmed the new standard is linked from the router, debug-logging specification, and task-completion procedure.
- No C++ or JavaScript code was changed, so no build or executable test was required.

# Migration boundary

Existing `09-06-2026` folders and old-format files remain historical evidence.
The C++ logging path, scripts, and tests still require a separate implementation
task before runtime-generated files actually change format.

# Human review

- Confirm the chosen UTC/local-display wording.
- Confirm the filename separator and whether fractional seconds are needed in any generated filenames.
- Confirm the later code migration should update logs, crash reports, test artifacts, and build artifacts together.
