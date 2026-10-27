# Melee and animation issue intake

Time: 2026-10-27T06:03:43Z (2026-10-27 02:03:43 EDT)
Branch: `20261011zt`
HEAD at session start: `eb79b2de vehcile and 4 new melee wepaon attemptetd to add in this commit not tested first ai pass etc`

## Result

Added one dated intake section to `docs/specs/open-gameplay-issues.md` (new lines 1250-1290; heading `New intake — 2026-10-27`). It records: airborne attacks for every melee weapon; visible melee contact feedback and broader visual/sound coverage; brighter Chainsaw tint and distinctive sounds; Blender authored clips composing with camera-relative procedural aim; expanding the existing reusable animation state/clip catalog to actor and weapon states; and camera-influenced ragdoll arm poses as an exploratory request.

The intake says reports are not reproduced defects, routes follow-up to existing owner specs, and calls out open details such as hit/block/miss feedback, aim layer order and affected parts, state transitions, observer consistency, and how camera control interacts with ragdoll physics. It explicitly avoids creating a duplicate animation tracker.

## Exact change

Before: `docs/specs/open-gameplay-issues.md` ended with the existing implementation note:

```md
fix.
```

After: immediately following that ending, added:

```md
## New intake — 2026-10-27

- **Melee attacks should work while airborne:** The player wants every melee
  weapon to be attackable in the air. This is a desired behavior request, not
  a reproduced failure. Review the shared melee action rules and per-weapon
  exceptions, then verify input acceptance, server authority, and animation
  timing for attacks started and continued while airborne. Keep fixed-tick
  movement and damage behavior consistent across player, NPC, and networked
  actors; exact action restrictions, if any, remain open.
- **Melee contact needs visible feedback, with more complete weapon
  presentation:** The player reports that melee contact has no visible effect
  and asks for more expressive visuals, animations, and sounds. Define readable
  contact feedback for hit, block, and miss outcomes and check it against the
  existing shared contact and confirmation presentation. Treat the reported
  absence as unverified until reproduced; avoid confusing predicted contact
  with confirmed damage. The Chainsaw should also have a brighter tint and a
  broader, more distinctive sound set; general melee sound and animation
  coverage should be reviewed across weapons rather than assuming one recipe
  fits all.
- **Blender-authored animation should compose with procedural camera aiming:**
  The player wants to author animations in Blender and have them continue
  playing while the arms, head, and other relevant parts follow camera look
  direction up and down. The existing Blender exporter and shared runtime
  provide a starting point, but the current animation spec orders procedural
  and authored pose layers without yet defining this requested aim composition
  for every rig. Define the layer order, affected parts, pitch limits, and
  first-person/remote/NPC consistency before implementation; verify that aim
  adjustment does not alter hitboxes or make melee contact misleading.
- **Expand the reusable animation catalog for actors and weapons:** The player
  wants a master list of reusable animation states and sets that can keep
  growing: idle, walking, running, jumping, falling, crouching, crawling, light
  and heavy hurt reactions, dying, and weapon-specific idle, walking, attack,
  and blocking poses. This overlaps the existing shared animation state
  machine and clip library, so extend their catalog/coverage rather than
  creating a parallel tracker. Review state names, transition/blend rules,
  per-weapon action poses, general versus weapon-specific blocking, and JSON
  ownership for player, NPC, remote, and first-person presentation.
- **Explore camera-influenced ragdoll arm poses:** The player wants ragdoll
  arms to respond somehow to camera control. This is an early design request:
  define how camera-driven arm control can coexist with physical ragdoll motion,
  authority, and other observers before selecting behavior or implementation.
```

## Documents and review

- Read `docs/ROUTER.md`, `docs/operations/task-completion/task-completion.md`, `docs/architecture/time-and-formatting/time-and-formatting.md`, `docs/specs/animations/animations.md`, `docs/specs/animations/blender-animation-authoring.md`, `docs/specs/avatar/avatar-editor.md`, `docs/specs/weapons/weapons.md`, `docs/specs/weapons/melee/melee-weapons.md`, `docs/operations/asset-management/asset-management.md`, `docs/skills/documentation-checker-v1.md`, `docs/skills/spec-behavior-review-v1.md`, `docs/skills/asset-checker-v1.md`, and `docs/regressions/README.md`.
- Router references `docs/doc-review-09-03-2026.md` and `docs/regressions/regressions-v1.md`, but neither file exists at that path in this checkout. The current regression guide is `docs/regressions/README.md`; archived `regressions-v1.md` is under `docs/regressions/archive-ignore/` and was not treated as current authority.
- Documentation Checker: PASS for focused changed-file clarity and link targets; no new links were added. Full docs TODO inventory was not run because this intake did not edit existing TODOs.
- Specification and Behavior Review: PASS_WITH_HUMAN_REVIEW. The intake aligns with shared animation and melee owner documents, marks unverified reports as such, and leaves material behavior choices open.
- Asset Checker: NOT_APPLICABLE; no assets or sound files changed.

## Validation and remaining review

- `git diff --check`: PASS.
- Focused Markdown review and target-path checks: PASS.
- No code, gameplay configuration, or assets changed. No build, runtime scenario, or playtest was performed; those remain necessary for later implementation and human acceptance.
- Pre-existing worktree changes were preserved. At session start, modified files included `config/accounts/default.json`, `config/animations.json`, `config/audio/music-settings.json`, `config/weapons.json`, combat, player-animation, and networking source files. The untracked earlier changelog `docs/changelog/2026-10-27/20261027_015346-deathball-melee-poses.md` was also preserved.
