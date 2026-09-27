# Real rocket/grenade explosion composition migrated hot

Date: 2026-09-15 18:00 EST (UTC 2026-09-15T22:00:00Z)
Branch: `8292026stash` (worktree, uncommitted)
Result: `PASS_WITH_HUMAN_REVIEW`

## SUBSYSTEM
Client effects - rocket/grenade explosion composition + lifetime.

## COLD OWNER REMOVED
`combat/explosion-fx.cpp::spawnExplosionFx` no longer owns the visual
composition for the migrated path: after the (temporary cold) sound it emits a
generic `effect.request` and returns when a hot handler handles it. The flash /
smoke / debris config-driven composition is now hot.

## HOT OWNER ADDED
- `EffectRequestV1` (generic payload) + `LiveBehavior::dispatchEffectRequest`
  (cold -> hot fact).
- `modules/presentation/effect-composition.cpp` (`hot.effect-composition`): on
  `effect.request` creates generic effect entities (flash expanding+fading,
  smoke rising+growing, debris) and sets `handled = 1`.
- `hot.effect-lifecycle` (previous round) owns age/integration/growth/fade/
  destroy.

## COLD MECHANISM REMAINING
Particle/quad/textured draw (`EffectPartSystem`), audio device/playback
(temporary), mesh draw for effect entities.

## STATE AUTHORITY
Generic dynamic components `PresentationState`, `EffectLifetime` on ordinary
entities; logical resource ids only.

## COMPATIBILITY FALLBACK
`spawnExplosionFx` cold composition runs only when no hot handler handles the
fact (hot unavailable/unregistered).

## REAL SHIPPING PATH MIGRATED
Yes: the real client rocket/grenade explosion (network explode handler at
`multiplayer-projectiles.cpp:1037`/`:1885` and local launcher) flows through
`spawnExplosionFx` -> `effect.request` -> hot composition. Exactly one owner
(hot yields cold).

## RUNTIME-NEW PROOF
`hoteffect` (previous round) plus a new explosion type id (`effect.explosion.*`)
require no enum/switch/ABI.

## SELFTEST PROVEN
"real explosion fact reaches the hot effect owner and composes generic effects";
"runtime-unknown effect entity created (no enum/switch)"; "hot effect ages,
integrates, and grows"; "hot effect expires and is destroyed"; full suite PASS.

## LIVE HOT-EDIT PROVEN
No (no screen).

## BUGS DEFERRED
Remaining effect types (bullet impact, blood, muzzle flash, decals, screen
flash, camera shake); sound stays cold temporarily; effect visual polish.

## WOULD THIS BUG STILL REQUIRE COLD RESTART?
For rocket/grenade explosion composition and lifetime: **no** (hot). Remaining
cold effect types: yes until migrated. Cold restart remains only for low-level
renderer/backend issues.

## Update to the primary metric
"BUGS THAT STILL REQUIRE A COLD EXE REBUILD" item 1 reduced again: explosion
composition/lifetime hot; remaining effect types listed.

## NEXT COLD OWNER
Blood impact using the same substrate, then a generic decal primitive if needed,
then audio policy migration (sound choice/volume/pitch/falloff/variation as hot
behavior over a cold mixer).

## Files changed
`src/hot-reload/game-api.h`, `src/live-code/live-behavior.{h,cpp}`,
`src/hot-reload/modules/presentation/effect-composition.cpp` (new),
`src/combat/explosion-fx.cpp`, `src/network/hot-combat-selftest.cpp`,
`docs/architecture/live-development/hot-cold-audit.md`,
`docs/architecture/live-development/hot-kernel-next-steps.md`.
