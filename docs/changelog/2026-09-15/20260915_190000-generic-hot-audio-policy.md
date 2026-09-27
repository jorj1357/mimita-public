# Generic hot audio policy (explosion sound + runtime-unknown sound)

Date: 2026-09-15 19:00 EST (UTC 2026-09-15T23:00:00Z)
Branch: `8292026stash` (worktree, uncommitted)
Result: `PASS_WITH_HUMAN_REVIEW`

## SUBSYSTEM
Audio behavior policy (explosion sound; runtime-unknown sound).

## REAL SHIPPING PATH MIGRATED
Rocket/grenade explosion sound: `explosion-fx.cpp` no longer plays it before the
hot dispatch. The hot `effect.request` handler (`hot.effect-composition`) chooses
the sound, volume, pitch, and spatial falloff and emits a generic `audio.play`
command. The cold `playWorldSound` call moved into the compatibility fallback
(only when no hot handler handles the fact).

## COLD OWNER REMOVED
Sound choice/volume/pitch/falloff for the explosion path leaves
`combat/explosion-fx.cpp` (now fallback).

## HOT OWNER ADDED
`GLAME_CAP_AUDIO_PLAY` (`audio.play`) + `GameAudioCommandV1`; kernel
`capAudioPlay`; hot policy in `modules/presentation/effect-composition.cpp`;
`hotaudiotest` command for runtime-unknown sounds.

## COLD MECHANISM REMAINING
Audio device, mixer, source/buffer lifetime, `playWorldSound`/`playSoundPitched`
(the actual playback), name-keyed `soundPath` resolution.

## COMPATIBILITY FALLBACK
Cold `playWorldSound` + cold composition in `spawnExplosionFx` when hot is
unavailable.

## RUNTIME-UNKNOWN PROOF
`hotaudiotest [logical sound]` plays a logical sound the EXE never knew about
through the generic command - no audio enum/switch.

## SELFTEST PROVEN
"hot audio policy plays sounds via the generic command"; "real explosion fact
reaches the hot effect owner and composes generic effects"; full suite PASS.

## LIVE HOT-EDIT PROVEN
No (no screen).

## BUGS DEFERRED
Remaining sound policy (weapon fire, footstep, UI, NPC, ambient, music);
generation-aware sound resources; sound tuning.

## WOULD THIS BUG STILL REQUIRE COLD RESTART?
Explosion sound policy: **no** (hot). Other audio policy: yes until migrated.
Backend/device bugs remain cold.

## Update to the primary metric
"BUGS THAT STILL REQUIRE A COLD EXE REBUILD" item 2 reduced: explosion sound
policy hot; remaining audio policy listed.

## NEXT COLD OWNER
Blood impact (+ generic decal primitive) then bullet impact / muzzle flash /
camera-shake policy, then remaining audio policy, then weapon presentation.

## Files changed
`src/hot-reload/game-api.h`, `src/live-code/live-behavior.{h,cpp}`,
`src/hot-reload/modules/presentation/effect-composition.cpp`,
`src/combat/explosion-fx.cpp`, `src/network/hot-combat-selftest.cpp`,
`docs/architecture/live-development/hot-cold-audit.md`,
`docs/architecture/live-development/hot-kernel-next-steps.md`.
