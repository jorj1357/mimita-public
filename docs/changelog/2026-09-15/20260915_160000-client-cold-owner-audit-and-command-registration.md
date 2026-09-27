# Client/presentation cold-owner audit + hot command-registration proof

Date: 2026-09-15 16:00 EST (UTC 2026-09-15T20:00:00Z)
Branch: `8292026stash` (worktree, uncommitted)
Result: `PASS_WITH_HUMAN_REVIEW`

## What this pass does

Architecture-first pass: audit the remaining client/presentation cold owners,
establish the new primary metric, and take one clean small slice (command
registration). It does not polish animation.

## 1. Repo-wide client/presentation cold-owner audit

Added a table to `hot-cold-audit.md` covering local player presentation, remote
NPC/actor, weapon presentation/animation, animation selection/pose/blending,
effects, decals/blood, camera shake, damage indicators, audio
selection/spatial/music, nameplates/health overlays, HUD, menus/UI behavior,
UI interaction, console commands, presentation resources (meshes/textures/
shaders/clips/skeletons/fonts/sounds), replay/spectator, and editor tools - each
with cold owner, hot owner, state authority, cold mechanism, fallback, whether a
bug still needs an EXE rebuild, and the next step.

Largest remaining client cold behavior owners: **effects, audio, weapon
presentation, nameplates, menus/UI interaction, and clip/skeleton/font/sound
resource generations.**

## 2. Primary metric: "BUGS THAT STILL REQUIRE A COLD EXE REBUILD"

Added the section to `hot-cold-audit.md` with 10 items (effects, audio, weapon
presentation, nameplates, menus/UI, blending, clip/skeleton resources,
font/sound/music resources, replay/spectator, post-startup command
registration), each with cold file/function, why cold, what must migrate, and
priority. Every architecture pass must shrink this list.

## 3. Clean slice taken: hot command registration

Commands are already registered generically by hot packages
(`MimitaHotPackage::CommandRegistrar` -> `GenericRuntime`), so `posedebug`,
`hotactor`, and `hotpresent` require no cold switch. Added a selftest proving
`hasCommand` for those names. (Truly dynamic post-startup add/remove-command is
still a missing primitive - recorded as priority low.)

## Evidence

- `python build_agent.py` -> `Status: SUCCESS`.
- `--hot-combat-selftest` -> PASS incl. "hot package registers commands without a
  cold switch".
- Full suite (9 selftests) -> PASS.

## Classification

- SELFTEST PROVEN: hot package command registration (no cold switch).
- COMPILED INTEGRATION: none new.
- LIVE HOT-EDIT PROVEN: no.
- HUMAN VERIFICATION NEEDED: none for this slice.

## Bugs deferred (per architecture-first policy)

- Animation feel/blending quality (cosmetic; ownership migration is what matters).
- Effect timing/perfection, UI spacing, sound tuning (deferred).

## Would a bug still require a cold restart?

Yes for the 10 items in the cold-restart list above; no for command registration.

## Files changed

`src/network/hot-combat-selftest.cpp`,
`docs/architecture/live-development/hot-cold-audit.md`,
`docs/architecture/live-development/hot-kernel-next-steps.md`.

## Next cold owner selected

Effects: a hot effect behavior consuming gameplay events and emitting generic
`effect.spawn`/`render.debug` commands with logical resources, with at least one
real shipping effect migrated end-to-end (rocket explosion or blood), and a
runtime-unknown effect proof.
