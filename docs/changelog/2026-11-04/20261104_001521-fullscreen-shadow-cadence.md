# Fullscreen shadow cadence regression recurrence

- Result: PASS_WITH_HUMAN_REVIEW
- Timestamp: 2026-11-04T00:15:21Z (2026-11-03 20:15:21 EDT)
- Branch: `20261011zt`
- HEAD: `c64d188e436139847ccb74d8ed4372d4a974fdc9`
- Scope: record the second mode 6 world-flicker report, restore full-rate shadow-map rendering, and publish the corrected source.

## Investigation and change

The current working copy had a pre-existing edit in
`src/engine/engine-tick-render.cpp`, `engineTickRender()`:

- Before this session: `constexpr int kShadowUpdateMode = 2;` selected alternating-frame shadow-map updates to reduce the depth pass cost.
- After this session: `constexpr int kShadowUpdateMode = 1;` selects full-rate shadow-map updates. The existing mode 2 branch remains available for future comparisons.

This matches the user's report and the prior Oct. 24 diagnosis: a shadow depth map reused between rendered frames can lag moving scene content and plausibly appear as flicker. The new code is the smallest correction, but the cause was not reproduced live in this session.

The append-only detail record is `docs/regressions/performance/fullscreen-shadow-cadence-REG-2026-11-04.md`; Entry 8 was appended to `docs/regressions/performance/README.md`. The user-reported occurrence is dated Nov. 3 local time; the canonical artifact folder follows UTC and is Nov. 4.

## Documents and review

- Read `AGENTS.md`, `docs/ROUTER.md`, `docs/regressions/README.md`, `docs/regressions/master/README.md`, `docs/regressions/performance/README.md`, `docs/specs/graphics/graphics-resolution.md`, `docs/specs/performance/performance.md`, `docs/skills/documentation-checker-v1.md`, `docs/architecture/time-and-formatting/time-and-formatting.md`, `docs/operations/build-and-exe/build-and-exe.md`, and `docs/operations/task-completion/task-completion.md`.
- Documentation review: the new detail record links to the prior correction, authoritative fullscreen spec, and session changelog. No spec behavior was changed.
- The worktree already contained many unrelated user changes. The renderer file itself already had changes to shadow cadence and SSBM presentation; only the cadence selection/comment were changed here. All other edits were preserved.

## Validation and remaining evidence

- Dev-loop build request succeeded and published `.dev/builds/2026-11-04T00-15-03.911565Z/mimita.exe`, source generation 9, build ID `2026-11-04T00:15:03.911565Z:3251d56b48e890950423f4d08416d5a099e70871baf81d469f24dbf1ea6cc37d`.
- The exact published executable's `--versioninfo` completed and reported `EVENTS_JSONL_PATH=logs/11-03-2026/20261103_191516/events.jsonl`.
- No live mode 6 scenario or visual review was performed. Dev-loop status showed an existing server process and no client process; this session did not launch or stop a game process. The visual correction remains pending human review.
- No tests were added or run. A repository-wide `git diff --check` surfaced trailing whitespace in pre-existing changes to `ROADMAP.md`, `docs/specs/open-gameplay-issues.md`, and `docs/specs/vehicles/vehicles.md`; these were not modified by this task.
