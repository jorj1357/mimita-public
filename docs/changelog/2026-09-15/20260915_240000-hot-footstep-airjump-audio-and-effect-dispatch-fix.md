# Hot footstep/air-jump audio + effect-dispatch one-owner bug fix

Date: 2026-09-15 24:00 EST (UTC 2026-09-16T04:00:00Z)
Branch: `8292026stash` (worktree, uncommitted)
Result: `PASS_WITH_HUMAN_REVIEW`

## SUBSYSTEM
Client presentation - movement audio (footstep, air-jump) + effect dispatch.

## REUSED EXISTING PRIMITIVE OR ADDED NEW ONE
Reused `effect.request` (cold->hot fact), the new `EffectRequestV1.text`
logical-name field, and `GAME_CAP_AUDIO_PLAY` / `audio.play`. No new ABI.

## REAL SHIPPING PATH MIGRATED
- Footstep: `entities/player.cpp:313` (local player walk sound) now dispatches
  `effect.footstep.sound`; hot owns sound choice/cadence/volume/pitch/falloff;
  cold `playWorldSound` yields.
- Air-jump: `entities/player.cpp:258` (`playAirJumpSound`) dispatches
  `effect.jump.sound`; hot emits `audio.play`.

## COLD OWNER REMOVED
Walk-sound selection in `Player::updateAudio` footstep block; air-jump sound
selection in `playAirJumpSound` (fallback retained).

## HOT OWNER ADDED
`effect.footstep.sound` and `effect.jump.sound` branches in
`hot-reload/modules/presentation/effect-composition.cpp`.

## GENERIC STATE/PRIMITIVES USED
`ground.stableOnGround`, `jump.didAirJump`, actor position (read-only movement
state); `effect.request` + `audio.play` + `EffectRequestV1.text`.

## COLD MECHANISM REMAINING
miniaudio device/mixer/spatial playback; movement state remains movement-owned.

## COMPATIBILITY FALLBACK
Cold `playWorldSound`/`playAirJumpSound` run only when the fact is unhandled.

## RUNTIME-UNKNOWN PROOF
Arbitrary logical footstep id (`mod/custom_step`) accepted via `EffectRequestV1.text`;
no enum/switch.

## SELFTEST PROVEN
"grounded footstep audio policy is hot (audio.play)"; "arbitrary logical footstep
sound id is hot (no enum)"; "air-jump audio uses the same movement-fact
substrate"; "unmigrated landing audio stays cold-owned (no duplicate owner)";
full suite PASS. `build_agent.py` -> SUCCESS.

## LIVE HOT-EDIT PROVEN
No (no screen).

## CONCURRENCY BOUNDARY STATUS
Clean (movement state consumed read-only; no movement/network files touched).

## BUGS DEFERRED
Footstep cadence/quality tuning; landing sound does not exist (VFX only);
UI/NPC/ambient/music audio policy (recorded for later category cleanup).

## BUGS FIXED NOW
One-owner dispatch: `effect-composition` previously set `handled = 1` for every
fact and defaulted unknown facts to an explosion. It now marks handled only on
real branches and leaves unknown facts unhandled so the cold owner runs. This
prevents silent suppression of unmigrated cold presentation and spurious
explosions on arbitrary facts.

## WOULD THIS BUG STILL REQUIRE COLD RESTART?
Footstep/air-jump audio policy: **no** (hot). Weapon presentation,
socket/attachment, weapon mesh resources: yes until migrated (and blocked on
deliberate ABI design). Audio/GPU backend bugs stay cold.

## NEXT COLD OWNER
Weapon/tool presentation. First target `swordsword` (third-person, then
first-person). Blocked on two deliberate generic ABI primitives: (a) socket/bone
world-transform query; (b) weapon GLB as a logical presentation mesh resource.

## Files changed
`src/entities/player.cpp`, `src/hot-reload/modules/presentation/effect-composition.cpp`,
`src/network/hot-combat-selftest.cpp`,
`docs/architecture/live-development/hot-cold-audit.md`,
`docs/architecture/live-development/hot-kernel-next-steps.md`.
