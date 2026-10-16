# Play-and-speak gameplay issue intake

Time created: 2026-10-16T09:19:44Z
Time last updated: 2026-10-16T09:20:14Z

## Purpose

Document how to capture gameplay problems while the player is playing and
speaking with an AI helper, route each report to the existing behavior spec,
and learn from cases where implementation does not produce the expected
result. This change updates the open gameplay issue index; it does not change
gameplay code or claim a runtime root cause.

## Findings and changes

- Added a Griever row to the issue table linking its existing detailed spec,
  movement owner, and collision owner. Its reported server-position error,
  hovering, and insufficient bounce remain separate symptoms with no newly
  confirmed cause.
- Added a row for custom animation work. `animations.md` owns the runtime
  contract; `blender-animation-authoring.md` remains a proposal with open
  decisions.
- Added a play-and-speak issue-capture workflow: preserve the player's exact
  words, separately record context/intent/observed behavior, locate the
  existing owner, label evidence by confidence, and attach runtime identity
  and journal evidence when available.
- Added an implementation-failure trace checklist for input/command routing,
  active configuration/assets, server authority, simulation, replication, and
  presentation.
- Added an explicit work-focus note for 2026-10-16: custom animation work,
  improving Zombie Tower through actual play, and recording spoken issues as
  they occur. It is recorded as a plan, not completed work.

## Evidence and validation

- Read `docs/ROUTER.md`, `docs/skills/documentation-checker-v1.md`,
  `docs/architecture/time-and-formatting/time-and-formatting.md`, the current
  issue index, runtime animation spec and Blender proposal, Zombie Tower spec,
  Griever spec, and runtime scenario workflow.
- Confirmed the referenced Griever, animation, Zombie Tower, and runtime
  workflow documents exist.
- The Griever example quotes its existing attempt note, “good rolling but
  doesnt bounce, needs to bounce a lot lot lot”; this is a prior user report,
  not a new live reproduction. The Griever spec still marks the earlier
  server-position/hovering cause as unconfirmed.
- Relevant existing TODOs found by the documentation scan were left intact:
  `docs/architecture/player-npc-systems/npc-movement.md:22` says
  `9 8 2026 1019 est jorj todo - explain`; suggested expansion: describe the
  intended general NPC movement/record-and-replay direction and link its owner.
  `docs/specs/gamemodes/zombie-tower.md:5363` says
  `2026 10 06 1603 jorj todo - explain zombie tower`; suggested expansion:
  clarify which part of Zombie Tower needs explanation, then keep mode goals
  in that spec and link the shared systems rather than duplicating their rules.
  Other previously scanned documentation TODOs are summarized in
  `docs/changelog/2026-10-16/20261016_082206-gameplay-spec-issue-map.md`.
- No code changed, so no build or game session was run. No current
  `EVENTS_JSONL_PATH` or new runtime evidence is claimed.
- The router references `docs/doc-review-09-03-2026.md` and
  `docs/regressions/regressions-v1.md`, which are absent at those exact paths in
  this checkout; the documentation review file was not required for this
  focused issue-index edit. `docs/regressions/README.md` is present.
