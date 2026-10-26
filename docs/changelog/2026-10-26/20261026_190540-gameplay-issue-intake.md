# Gameplay issue intake — 2026-10-26

- **Timestamp:** 2026-10-26T19:05:40Z (2026-10-26 15:05:40 EDT)
- **Branch:** `20261011zt`
- **HEAD before this documentation change:** `4d96b119f54c3e08cfc840c7f41ae77709ee4fdd`
- **Result:** PASS_WITH_HUMAN_REVIEW

## Change

Updated `docs/specs/open-gameplay-issues.md` in the existing 2026-10-26
intake, adding six user reports at lines 64–112:

1. TDM join requiring about nine attempts, with the screenshot treated as
   evidence of repeated join messages rather than proof of a specific cause.
2. Falling through slopes on `tdmtest3` after collision changes, with seam
   tolerance kept as an unconfirmed possibility.
3. A proposed automatic-solid-block behavior or mode option, explicitly left
   as a design question pending scope and authoring decisions.
4. Repeated position-correction snapping, kept separate from a proven network
   cause and related to (but not conflated with) the existing high-speed report.
5. Less accurate NPC aim with firing speed and other difficulty held constant,
   plus a separate request to improve NPC movement and verify live JSON edits.
6. Distant gunshot audibility targets: 10% at 50 m, 5% at 500 m, 1% at
   2,500 m, and inaudible at 3,000 m, recorded as proposed values rather than
   a settled attenuation curve.

The exact old state at the insertion point was the end of the existing
`Live NPC weapon-profile updates across roles and modes` bullet, ending:

> resolution, and when already-running NPCs adopt updates before changing the
> owner.

The new content starts with `- **TDM join requires repeated attempts:**` at
line 64 and ends with the gunshot distance/falloff review instructions at line
112. No earlier intake text was removed or rewritten by this change.

## Evidence and limits

- The attached screenshot visibly contains several `[system] Guest(6281)...
  joined the game` messages. This supports repeated join activity but cannot
  distinguish distinct connections, retries, or displayed identity variants.
- The supplied journal `logs/2026-10-26/20261026_145155/events.jsonl` identifies
  build 3371, Sandbox mode, and map `funworld3`. It contains connection and
  collision-summary events, but is not the reported TDM `tdmtest3` scenario.
  The inspected journal does not establish the repeated correction behavior
  or a network disagreement cause. It is not used as proof of the TDM reports.
- No code, settings, or runtime behavior changed. No build or gameplay test was
  run for this documentation-only intake.
- Human reproduction and owner-level review remain open for all six reports.

## Documents and focused review

Read `AGENTS.md`, `docs/ROUTER.md`, `docs/architecture/time-and-formatting/time-and-formatting.md`,
`docs/skills/documentation-checker-v1.md`,
`docs/skills/spec-behavior-review-v1.md`,
`docs/specs/open-gameplay-issues.md`,
`docs/regressions/regressions-v1.md`, the routed movement, collision, TDM,
networking, NPC behavior, NPC movement, JSON configuration, asset management,
hot-audio contract, and live-code development documents, plus
`docs/operations/task-completion/task-completion.md`.

- Documentation checker: PASS for the added wording; it distinguishes reports,
  evidence, uncertainty, and design choices. Existing inbox purpose and scope
  remain unchanged.
- Specification/behavior review: PASS_WITH_HUMAN_REVIEW. This is intake only;
  no desired behavior was promoted to a settled specification or code change.
- Documentation TODO scan: no actionable TODO comment was found. The matches
  are examples/instructions in `docs/ROUTER.md:120` and
  `docs/skills/documentation-checker-v1.md:22`, not an unresolved document
  TODO. Suggested correction: none for this task.
- The router names `docs/doc-review-09-03-2026.md`, but that file is absent in
  this checkout. It was not recreated or replaced; the focused documentation
  checker was used.

## Working tree

The repository was already broadly dirty before this task, including
`docs/specs/open-gameplay-issues.md` and unrelated configuration, source,
specification, and changelog edits. Those edits were preserved. This session
adds only the six intake bullets above and this one required changelog file.

## Human review still needed

Reproduce the one-join TDM flow and the `tdmtest3` spawn/slope failure on a
confirmed TDM run. For repeated corrections, capture paired client/server
state and identify the first divergence. Decide the scope and authoring rule
for automatic-solid blocks. Confirm NPC live-profile behavior before tuning
accuracy, then assess movement separately. Evaluate the proposed gunshot
attenuation points against actual source levels and in-game audibility.
