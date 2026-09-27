# Limb contact and movement audit

Time: 2026-09-22T23:04:52Z (UTC)
Branch: `8292026stash`
Compared: `afad20a` to current `3150587` plus pre-existing working-tree edits.

## Result

`PASS_WITH_HUMAN_REVIEW` for the source comparison and regression record.
`UNRESOLVED` for the runtime behavior. No gameplay code was changed.

## Work performed

- Read `AGENTS.md`, `docs/ROUTER.md`, the movement specification, collision
  architecture, regression workflow, specification-review skill, and task
  completion procedure.
- Compared the old cold movement/contact path with the current hot movement
  policy and `collision.main` path.
- Recorded the exact ownership and ordering differences, the likely failure
  boundary, related risks, and the required falsification trace.
- Added `docs/regressions/2026-09-22/limb-contact-ability-reset-REG.md`.

## Evidence

- Source inspection only; no build was run.
- No runtime log, multiplayer proof, visual acceptance, or human fix
  confirmation was produced.
- Existing unrelated working-tree edits were preserved.

## Skills and documents

- `docs/skills/spec-behavior-review-v1.md`: used; findings recorded in the
  regression document.
- `docs/specs/movement/movement.md` and
  `docs/architecture/collision/collision.md`: used as behavior authorities.
- `docs/regressions/README.md` and
  `docs/operations/task-completion/task-completion.md`: used for record format.

## Human review still needed

Run the focused fixed-60-Hz input/contact trace and manually confirm ground
dash, down-dash bounce, freeze release, and limb-root collision one behavior at
a time. Only then identify a corrected code block and change the regression
status.
