# Intermittent gameplay stalls after long uptime

- Time UTC: `2026-10-25T15:06:05Z`
- Display: `2026-10-25 11:06:05 EDT`
- Branch: `20261011zt`
- HEAD: `0f33025c` plus the existing uncommitted worktree
- Result: `PASS_WITH_HUMAN_REVIEW`

## Scope

Added the player's 2026-10-25 report to the existing open gameplay issue inbox.
The issue remains unresolved. The report records the approximate two-to-three-
day PC uptime, launch of a new `mimita.exe`, variable FPS and timing displays,
the reported zero-byte journal, dev-loop launch failure, and the player's
observation that closing older MiMITA processes seemed to help. That observation
is explicitly treated as correlation, not a proven cause or fix.

## Changed file

### `docs/specs/open-gameplay-issues.md`

Added a row under the existing performance/frame-pacing intake table, titled
“Intermittent severe choppiness and launch failure after long PC uptime.” It
includes the player's supplied 11:04 EDT incident time and corresponding UTC
time, the screenshot's visible values, relevant earlier journal/dev-loop
observations, candidate owner documents, and a contributor handoff to capture
process/build/journal identity and frame-time threshold distribution on a
controlled dev-loop reproduction.

No implementation or gameplay requirement was added. No claim is made that old
processes caused the performance issue or that closing them fixed it.

## Documents and review

- Read `docs/ROUTER.md`, `docs/skills/documentation-checker-v1.md`,
  `docs/architecture/time-and-formatting/time-and-formatting.md`,
  `docs/specs/performance/performance.md`,
  `docs/specs/open-gameplay-issues.md`,
  `docs/regressions/README.md`, and
  `docs/operations/task-completion/task-completion.md`.
- `docs/ROUTER.md` links `docs/doc-review-09-03-2026.md`, which is absent from
  this checkout. This was noted; the focused documentation checker was read.
- The repo has `docs/regressions/regressions-v1.md` in the router but that path
  is absent; `docs/regressions/README.md` was used for regression guidance.
- No regression file was created: the report is unresolved and current evidence
  does not establish a reproducible confirmed regression.

## Validation and limits

- `git diff --check -- docs/specs/open-gameplay-issues.md docs/changelog/2026-10-25/20261025_150605-intermittent-gameplay-stalls.md`: passed.
- Documentation-only change; no build, tests, or new runtime reproduction was
  run.
- Earlier inspected journals and dev-loop state are partial evidence and may
  belong to separate attempts from the player's stated 11:04 incident. The
  exact cause, whether process accumulation contributed, and whether the
  zero-byte journal belonged to the same launch remain unknown.
- Existing worktree edits and the prior dev-loop diagnostics changelog were
  preserved.
