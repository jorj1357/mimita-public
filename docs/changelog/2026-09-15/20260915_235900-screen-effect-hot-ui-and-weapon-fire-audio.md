# Screen effect via hot UI + hot weapon-fire audio

Date: 2026-09-15 23:59 EST (UTC 2026-09-16T03:59:00Z)
Branch: `8292026stash` (worktree, uncommitted)
Result: `PASS_WITH_HUMAN_REVIEW`

## SUBSYSTEM
Client presentation - screen effects + weapon-fire audio.

## REUSED EXISTING PRIMITIVE OR ADDED NEW ONE
Screen effect: **reused the existing hot UI path** (`render.ui` / `ui.frame`).
No new compositor primitive was added because there is no dedicated cold
damage-flash/vignette owner (audit: `PostFX.vignette` is static config only) and
hot UI can express a full-screen tinted panel with hot-owned fade.
Weapon-fire audio: reused `audio.play`; added a generic `char text[64]` logical
name field to `EffectRequestV1`.

## REAL SHIPPING PATH MIGRATED
- Weapon-fire sound: `WeaponAudio::playShootSound` (the common cold seam for all
  weapon fire sounds) now emits `effect.weapon.fire.sound`; hot policy owns
  volume/pitch/falloff and emits `audio.play`; cold playback yields.
- Screen effect: `hotscreenfx` triggers a hot `ui.frame` system
  (`hot.screen-fx`) that emits a fading full-screen UI panel.

## COLD OWNER REMOVED
Weapon-fire sound volume/pitch/falloff decision leaves
`WeaponAudio::playShootSound`.

## HOT OWNER ADDED
`effect.weapon.fire.sound` branch + `hot.screen-fx` system +
`hotscreenfx` command in `modules/presentation/effect-composition.cpp`.

## COLD MECHANISM REMAINING
UI immediate-mode draw; audio device/mixer/playback.

## COMPATIBILITY FALLBACK
Cold `playWorldSound` in `playShootSound` when no hot handler handles the fact.

## RUNTIME-UNKNOWN PROOF
`hotaudiotest` (runtime sound), `hotscreenfx` (runtime screen effect), and the
generic `effect.weapon.fire.sound` with an arbitrary logical name - no enum.

## SELFTEST PROVEN
"hot screen-effect command registered (reuses hot UI)"; "real weapon-fire sound
policy is hot (audio.play)"; full suite PASS. `build_agent.py` -> SUCCESS.

## LIVE HOT-EDIT PROVEN
No (no screen).

## CONCURRENCY BOUNDARY STATUS
Clean (no movement/network files touched).

## BUGS DEFERRED
Footstep/landing audio; UI/NPC/ambient/music policy; sound resource generations;
weapon presentation; tuning.

## WOULD THIS BUG STILL REQUIRE COLD RESTART?
Weapon-fire audio policy and screen-effect policy: **no** (hot). Footstep/UI/NPC/
music/weapon-presentation: yes until migrated. UI/audio backend bugs stay cold.

## NEXT COLD OWNER
Footstep/landing audio (consume generic movement/contact state read-only), then
weapon presentation (tool entity + PresentationState/AnimationState + attachment
+ effect/audio facts).

## Files changed
`src/hot-reload/game-api.h`, `src/combat/weapon-audio.cpp`,
`src/hot-reload/modules/presentation/effect-composition.cpp`,
`src/network/hot-combat-selftest.cpp`,
`docs/architecture/live-development/hot-cold-audit.md`,
`docs/architecture/live-development/hot-kernel-next-steps.md`.
