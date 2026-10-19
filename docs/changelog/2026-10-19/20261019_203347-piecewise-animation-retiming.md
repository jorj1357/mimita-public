# Piecewise animation phase retiming

- Status: `PASS_WITH_HUMAN_REVIEW` (specification updated; runtime implementation and gameplay acceptance remain pending).
- Timestamp: 2026-10-19T20:33:47Z (2026-10-19 16:33:47 EDT, America/New_York).
- Branch: `20261011zt`.
- Base commit: `35c9b12a` (`stuff   for a nimations`).
- Pre-existing work: `config/weapons.json` was already modified before this task and was not edited here.

## Request and result

The user asked for a clear contract where changing Melee phase marker ticks in an exported animation JSON also speeds up or slows down the visible Blender-authored motion. The animation spec now defines separate authored `sourceTick` anchors and runtime `tick` positions. The playback curve is mapped piecewise-linearly between those boundaries. The authoring spec explains export, direct JSON edits, and re-export preservation.

Example: source active interval 20 ticks, runtime active interval 5 ticks = 4x speed. If only active moves from 20 to 2 and recovery stays at 40, windup becomes 10x faster while active motion is stretched over 38 runtime ticks. Move recovery closer as well to speed up active motion.

## Exact spec changes

### `docs/specs/animations/animations.md`

- Lines 289-315 replace the former statement that pose keyframes remain independent and marker edits change only gameplay timing.
- Before: “Pose keyframes remain independent; they do not need to be added for every marker unless a specific pose is desired at that boundary.”
- After: “Each required Melee marker has two tick values: `sourceTick` is the Blender-authored pose boundary, and `tick` is where that boundary plays in the engine.” The new section further requires piecewise-linear mapping and sampling the existing keyframe curve at the mapped source position, including fractional ticks.
- Lines 321-329 add a JSON example with runtime ticks 0, 10, 15, 60 and source ticks 0, 20, 40, 60; it states the active 20-to-5 span plays at 4x speed and explains the separate windup-only change.
- Lines 403-412 replace “Playback samples the clip directly at elapsed clip tick; there is no normalized phase retiming.” with the rule that each runtime phase interval maps linearly to its `sourceTick` interval and changing runtime `tick` updates gameplay timing and visible phase speed.
- Lines 436-440 require swept hitbox queries to use the retimed fixed-tick poses and server/client to share the same mapping.

### `docs/specs/animations/blender-animation-authoring.md`

- Lines 303-310 add that Blender exports `tick` and `sourceTick` initially equal.
- Lines 386-389 update the version-2 example with both fields.
- Lines 448-455 replace the old instruction to mirror JSON marker edits in Blender before re-export. The new rule edits runtime `tick`, preserves `sourceTick`, and requires re-export to refresh source anchors while retaining playback overrides by marker name, with an explicit reset option and report preview.
- Before: “Re-exporting from Blender writes the scene's marker positions again, so mirror JSON timing edits in the Blender timeline before the next export.”
- After: “When re-exporting an existing clip, the exporter must refresh `sourceTick` from Blender while preserving the existing playback `tick` values by marker name; an explicit reset option restores all playback ticks to their source ticks.”

## Review and evidence

- Read `docs/ROUTER.md`, `docs/specs/animations/animations.md`, `docs/specs/avatar/avatar-editor.md`, `docs/specs/animations/blender-animation-authoring.md`, `docs/skills/spec-behavior-review-v1.md`, `docs/skills/documentation-checker-v1.md`, `docs/architecture/time-and-formatting/time-and-formatting.md`, and `docs/operations/task-completion/task-completion.md`.
- The router-listed `docs/doc-review-09-03-2026.md` is absent in this checkout. The avatar-editor file exists, but its opening content is an unrelated Brute investigation note; it did not define clip retiming.
- Code comparison: `src/entities/player-animation-config.cpp` parses keyframes but not marker `sourceTick`; `src/entities/player-animation.cpp` samples the active clip from elapsed animation time. `src/combat/weapon-melee-config.cpp` reads phase markers for move timing. The new source-tick mapping is therefore a desired contract, not a current runtime feature.
- `git diff --check`: passed; Git emitted only its existing working-copy line-ending conversion warnings.
- No code, config, build, or runtime behavior changed. No gameplay or visual acceptance is claimed.
- Documentation TODO scan found unrelated notes, including `docs/features/TEMPLATE.md:11` (“todo make this nicer and clearer cleaner”), `docs/features/spawnvelocity/spawnveloc.md:11` (same note), and `docs/architecture/player-npc-systems/npc-movement.md:22` (“9 8 2026 1019 est jorj todo - explain”). Suggested future action: replace each with a short statement of the specific missing explanation or remove it after that section is clarified. These are outside this animation request and remain unchanged.

## Remaining work

Implement `sourceTick` export and preservation in `tools/mimita_animation_exporter.py`, parse and validate the anchors in the runtime clip loader, and apply the same piecewise-linear pose-time mapping on server/client animation paths. Then validate retimed keyframe sampling and the move's hitbox alignment in the real runtime. Human review is required to accept the kick's speed and feel.
