# Documentation: record ragdoll bind regression and model-power lesson

- Task ID: ragdoll-regression-model-record
- Summary: Added an append-only regression for inheriting the animated pose at
  ragdoll activation, and a permanent gold lesson that capable model selection
  materially affected the day’s ragdoll outcome.
- Status: PASS_WITH_HUMAN_REVIEW
- Date, time, timezone: `2026-09-10T22:20:48Z` (2026-09-10 18:20:48 EDT)
- Branch: `8292026stash`
- Base commit: working tree; no commit created
- Final commit: none (uncommitted)

## Pre-existing changes

- This session did not create, touch, or claim the large concurrent
  corpse-ragdoll / impact-decal changes or their
  `20260910_220023-impact-decals-ragdoll-corpse.md` changelog.
- It also did not modify the earlier activation-rest-bind source fix or its
  `20260910_220958-ragdoll-activation-rest-bind.md` changelog; those records
  were used as evidence.
- Other uncommitted repository changes remained untouched.

## Changes made

### 1. `docs/regressions/regressions-v1.md`

Inserted one entry after the `newest at top 9 3 2026` marker:

- New entry:
  `2026-09-10T22:19:14Z — Ragdoll activation inherited the animated pose;
  rest-pose bind corrected it (source-built, awaiting playtest)`
- Recorded the expected behavior, animated-pose symptom, `initParts /
  meshLocal` owner, wrong and corrected binding behavior, observation time,
  build-only proof status, and the link to
  `docs/changelog/2026-09-10/20260910_220958-ragdoll-activation-rest-bind.md`.

### 2. `docs/gold/2026-09-10-ragdoll-model-power-lesson.md`

Created a new permanent lesson recording the human’s observation that ragdoll
progress qualitatively moved from approximately 5% to approximately 95%, and
that using `deepseek v4.1 flash` rather than cost-preferred `mimo v2.5` was the
decisive process change. The document explicitly preserves the percentages and
model claim as human assessment rather than a controlled comparison or finished
acceptance.

## Documents and skills

- `docs/skills/documentation-checker-v1.md` — result:
  `PASS_WITH_HUMAN_REVIEW`.
  - Both new records have explicit purpose statements, nonspecification
    disclaimers, real repository paths, and separate source/build/runtime
    evidence.
  - The gold claim is intentionally qualified so it does not assert a
    controlled model benchmark or finished ragdoll acceptance.
- `docs/operations/task-completion/task-completion.md`
- `docs/architecture/time-and-formatting/time-and-formatting.md`

## Validation

- No source, configuration, or executable changed; therefore no canonical build
  or test rerun was warranted.
- Verified the intended two documentation artifacts with repository paths:
  modified `docs/regressions/regressions-v1.md` and new
  `docs/gold/2026-09-10-ragdoll-model-power-lesson.md`.

## Human acceptance

- Confirm that the regression wording accurately preserves the reported limb
  orientation fix without overstating dedicated runtime verification.
- Confirm that the gold lesson accurately preserves the human’s 5%-to-95% and
  DeepSeek-over-Mimo assessment as a lesson rather than a benchmark result.
