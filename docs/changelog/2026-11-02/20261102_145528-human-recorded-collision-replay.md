Time created: 2026-11-02T14:55:28Z
Time last updated: 2026-11-02T14:57:28Z

Purpose:
Record the human-directed design for repeatable collision debugging: capture a
real failure at high per-tick detail, replay it through normal movement and
collision owners, compare exact actor/tick evidence, and keep runtime records in
canonical `events.jsonl` journals.

This is a documentation-only update. It does not implement recording,
playback, containment diagnostics, scenario evaluation, TAS controls, or a
runtime logger migration. It does not claim that the current recorder meets the
new target.

## Session context

- Branch: `20261011zt`
- HEAD at start: `20578d11`
- Existing unrelated working-tree edits in config, coordinator/networking,
  website, deployment, router, persistence, and the open gameplay issues were
  preserved. Previous documentation edits in this conversation were also
  preserved and remain part of the same documentation request.
- The user chose human-recorded failure scenarios as the first validation
  priority. AI should replay those real failures; AI-authored collision
  scenarios and random map coverage are not the initial strategy.

## Documentation changes

Updated `docs/specs/dev-loop/scenario-input-recording.md`:

- Reframed separate scenario files as the current implementation and specified
  the target: scenario telemetry goes through `StructuredLogger` into the
  canonical per-process `events.jsonl` journal, linked by run/scenario/process/
  actor/tick identity. No separate scenario telemetry sink is added.
- Specified per-fixed-tick capture for every relevant actor and dynamic object:
  identity/lifecycle, intent/input, pose and velocity, movement backend,
  equipment/role identity, health/action state, limb/weapon collision geometry,
  triangle contact pairs, grenades, and containment/recovery evidence.
- Required `inside` / `outside` / `unknown` to distinguish a validated closed
  solid from open, unclassified, or missing geometry. Lack of an intersection
  alone must not claim an entire body part is outside an enclosing object.
- Required complete capture or an invalid/incomplete result when journal
  quotas/drop limits prevent writing every tick. Slower wall-clock capture must
  keep gameplay simulation at fixed 60 Hz.
- Required replay matching by scenario-relative frame and stable actor track,
  start-state verification, explicit numeric invariants at the known failure
  frame, and `NOT_REPRODUCED` when initial conditions cannot be matched.
- Specified replay of recorded inputs/accepted NPC intents through normal
  collision, never forced recorded positions. Added a diagnostic continue mode
  that records divergence and continues without position snap-back, while
  marking later contacts unaligned after significant spatial drift.
- Clarified that current playback stops beyond 2 m position error; increasing
  that threshold alone only delays the stop and does not snap the actor back.
- Marked TAS save states and speed controls as optional future work, not a v1
  prerequisite.

Updated `docs/workflows/runtime-scenario-validation.md` to prioritize the exact
human-recorded failure on the reported map/features, and to avoid random map
selection or NPC-navigation soak as a substitute for the known reproduction.
The human-reviewed recording is the baseline source; AI executes and analyzes
it rather than inventing collision routes for v1. Soaks are supplemental and
must not wait out a fixed duration after a decisive event. The current
readiness timeout, recording cap, and runner timeout format remain identified
as current implementation details; the collision-debugging target uses event
completion and marks resource-limited capture incomplete.

Updated `docs/specs/tas/tas.md` with a short structured section connecting TAS
to human-recorded collision regressions and explicitly deferring complete save
state support until it is needed for reliable reproduction.

The recording spec also requires each AI iteration to identify its scenario,
source revision, executable, run, iteration, starting-state match, first
failure tick, expected/actual values, and exact journal event; repeated runs
that disagree must be reported as unstable, not called fixed.

## Key before/after decisions

- Before: scenario capture was described as separate local scenario JSONL files
  and server sidecars. After: that remains documented as current behavior, but
  collision telemetry's target is the canonical per-process `events.jsonl`;
  scenario identity and event ranges select the evidence.
- Before: current position divergence above 2 m ended playback. After: that
  current behavior is stated accurately, and the target collision diagnostic
  run logs divergence and may continue without forcing the recorded position.
- Before: NPC proxies could be mistaken for a full NPC collision replay. After:
  transform-follow proxies are explicitly not collision re-simulation; the
  actor under test must receive recorded inputs/intents through the live path.

## Validation and review

- Read `docs/specs/tas/tas.md`,
  `docs/specs/dev-loop/scenario-input-recording.md`, and inspected
  `src/devtools/scenario-player.cpp` for the current 2 m divergence cutoff and
  no-snap behavior. Current schema 2 server playback follows NPC/remote-player
  transforms rather than rerunning their AI movement decisions.
- `git diff --check` passed on the documentation files changed in this
  session. The repository has unrelated pre-existing trailing whitespace in
  `docs/specs/open-gameplay-issues.md`; it was not changed.
- No build, tests, game launch, or scenario run was performed; this update only
  records the requested design.
- Documentation TODO review found the directly relevant note
  `docs/architecture/player-npc-systems/npc-movement.md:22`:
  `9 8 2026 1019 est jorj todo - explain`. Suggested expansion: describe NPC
  goal-to-route-to-shared-movement ownership and its runtime evidence path.
  The referenced TAS document also begins with `docs/specs/tas/tas.md:1`:
  `2026 11 02 0939 jojr todo eplxina`. Suggested expansion: replace the raw
  note with a concise TAS purpose and current decision boundary. Neither TODO
  was rewritten as part of this request.

## Remaining work and acceptance

The recorder and `events.jsonl` writer need a separate implementation task to
support full per-tick actor/object capture, high-detail collision identity,
lossless scenario capture within the journal quota, repeatable initial state,
and numeric per-tick comparison. Human review must confirm that the captured
failure corresponds to actual play and that a passing replay still permits
normal collision behavior rather than teleporting around the failure.
