# Real hit/blood impact composition migrated hot

Date: 2026-09-15 20:00 EST (UTC 2026-09-16T00:00:00Z)
Branch: `8292026stash` (worktree, uncommitted)
Result: `PASS_WITH_HUMAN_REVIEW`

## SUBSYSTEM
Client effects - hit/blood/world impact composition.

## REAL SHIPPING PATH MIGRATED
`effects/hit-effects.cpp::HitEffects::onHit` (the real client hit/blood/world
impact composition owner, called from projectile/melee hit processing) now emits
a generic `effect.request` (`effect.hit.blood` / `effect.hit.world`). The hot
`hot.effect-composition` handler composes generic effect entities and sets
`handled = 1`, so the cold composition yields (exactly one owner).

## COLD OWNER REMOVED
Blood/impulse particle composition leaves `HitEffects::onHit` for the migrated
path.

## HOT OWNER ADDED
`effect.hit.*` branch in `modules/presentation/effect-composition.cpp` (generic
effect entities with color/size/lifetime). Reuses `EffectRequestV1`,
`hot.effect-lifecycle`.

## COLD MECHANISM REMAINING
`EffectPartSystem` particle/decal/textured draw; surface decals have no generic
primitive yet.

## COMPATIBILITY FALLBACK
Cold `HitEffects::onHit` composition runs only when no hot handler handles the
fact.

## RUNTIME-UNKNOWN PROOF
`hoteffect` + arbitrary `effect.hit.*`/`effect.explosion.*` type ids; no
enum/switch.

## SELFTEST PROVEN
"real hit/blood fact reaches the hot effect owner"; "real explosion fact reaches
the hot effect owner and composes generic effects"; "runtime-unknown effect
entity created"; "hot audio policy plays sounds via the generic command"; full
suite PASS.

## LIVE HOT-EDIT PROVEN
No (no screen).

## BUGS DEFERRED
Generic surface-effect/decal primitive (decals still cold mechanism); muzzle
flash; bullet-impact decal choice; screen flash; camera shake; remaining audio
policy; visual tuning.

## WOULD THIS BUG STILL REQUIRE COLD RESTART?
Hit/blood/explosion composition + lifetime: **no**. Decals/muzzle-flash/camera-
shake and remaining audio policy: yes until migrated. Backend/renderer bugs stay
cold.

## Update to the primary metric
"BUGS THAT STILL REQUIRE A COLD EXE REBUILD" item 1 reduced again: explosion and
hit/blood composition hot; decals/muzzle/camera still listed.

## NEXT COLD OWNER
Generic surface-effect/decal primitive, then bullet-impact decal choice, then
muzzle flash, then camera/screen effect policy, then weapon-fire/footstep audio.

## Files changed
`src/hot-reload/modules/presentation/effect-composition.cpp`,
`src/effects/hit-effects.cpp`, `src/network/hot-combat-selftest.cpp`,
`docs/architecture/live-development/hot-cold-audit.md`,
`docs/architecture/live-development/hot-kernel-next-steps.md`.
