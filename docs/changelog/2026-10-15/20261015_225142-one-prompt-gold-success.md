# Changelog: one-prompt real-human release gold reference

- Date: 2026-10-15 18:51:42 EDT / 2026-10-15 22:51:42 UTC
- Branch: `20261011zt`
- Change type: documentation only
- Result: `PASS_WITH_HUMAN_REVIEW`

## What changed

Added:

- `docs/gold/2026-10-15-one-prompt-real-human-release.md`

The document records the user-reported one-prompt success, the `40m 43s`
real-human play session, the user’s recollection of GPT-5.6 Terra High and
approximately 55% of a 5-hour usage window, and the independently recorded
v2.0.7 release/package/Juggernaut evidence. Reported model, usage, and human
acceptance facts are explicitly separated from repository verification.

## Review and validation

- Read `AGENTS.md`, `docs/ROUTER.md`, and
  `docs/skills/documentation-checker-v1.md`.
- Reviewed nearby gold references for structure and evidence boundaries.
- Checked the new document for clear purpose, audience, reusable lesson, and
  `Does not claim` boundaries.
- Verified the release links and identifiers against the already-published
  v2.0.7 evidence from this release workflow.
- `docs/doc-review-09-03-2026.md` is referenced by the router but is missing in
  this checkout; it was not invented or modified.
- Documentation TODO scan found the following instruction/reference matches,
  not actionable TODOs in the new gold document:
  - `docs/skills/documentation-checker-v1.md:22` — “todo: explain this better”
    appears as the checker’s search example.
  - `docs/ROUTER.md:116` — “todo: explain this better” appears as the router’s
    handling instruction.
  - Existing changelog mentions at
    `docs/changelog/2026-09-12/20260912_011456-grenade-types-smoke-frag-fire.md:139`
    and `docs/changelog/2026-09-10/20260910_220023-impact-decals-ragdoll-corpse.md:147`.
- No source, build, runtime, or release artifact was changed in this turn.
- Pre-existing user edits were preserved and excluded:
  `config/analytics.json`, `config/audio/music-settings.json`, and
  `docs/jorj-docs/20261014plan.md`.

## Evidence limits

This documentation change does not independently replay the user’s human
multiplayer session or inspect account telemetry. The gold document preserves
those details as clearly labeled user reports while retaining the separate
repository and release evidence.

## Regression status

No regression was identified or added. No regression entry is required.
