# Revert failed slope response attempt and restore bookmark JSONL events

Time: `2026-09-29T19:18:00Z`

## Changes

- Reverted the support-priority and static-landing-bounce suppression from
  `actor-triangle-solver.cpp`.
- Restored the existing JSON-enabled bounce behavior for downward landings.
- Kept the established JSONL collision tracing.
- Changed `bookmark.created` and `bookmark.annotated` to the enabled
  `COLLISION` structured-log category so they are written to the canonical
  `events.jsonl` beside collision records.

## Evidence

The referenced run `logs/09-29-2026/20260929_150709/events.jsonl` contains
collision records but zero `bookmark.created` or `bookmark.annotated` records.
The bookmark code was writing those events as `GENERAL`, while
`config/debuglogger.json` has the general category at `off`; the terminal UI
therefore showed success while the JSONL write was filtered out.

The failed support-response attempt is recorded in
`docs/regressions/2026-09-29/slope-edge-snag-REG.md` as Attempt 6. No claim is
made that the slope/corner issue is fixed.
