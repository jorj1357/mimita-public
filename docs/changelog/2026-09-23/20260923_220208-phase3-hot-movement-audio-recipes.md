# Phase 3: hot audio policy owner and movement sound recipes

Date: 2026-09-23T22:02:08Z
Status: recipes + movement audio migration landed; build + headless selftests pass; no live human acceptance

## Change

- `config/audio-recipes.json` (new): logical-sound recipes for `ground_jump`,
  `air_jump`, `dash`, `down_dash`, `landing`, `footstep`, `freeze`, and `hurt`,
  with sound lists, volume/pitch base + jitter ranges, distance/falloff,
  category, spatial mode, priority, and overlap policy.
- `src/hot-reload/hot-audio-policy.h` + `src/hot-reload/modules/presentation/audio-policy.cpp`
  (new): the hot audio-policy owner. Loads the recipe snapshot from
  `config/audio-recipes.json` with a throttled `last_write_time` poll and
  last-valid preservation on parse failure, picks a variant and volume/pitch
  jitter deterministically from `(recipe, owner, seed/tick)`, and emits one
  `GameAudioCommandV2` through `audio.play`. Also handles the generic sound-only
  `audio.fact` event.
- `src/hot-reload/game-api.h`: added the POD `GameAudioFactV1` envelope and
  `GAME_EVENT_AUDIO_FACT` (`audio.fact`). Unlike `effect.request` it implies no
  visual, so a caller can request a recipe sound with a safe fallback.
- `src/hot-reload/modules/movement-system.cpp`: `playActionSound` (hardcoded
  sound strings) replaced by `playActionRecipe`, which resolves the logical
  recipe through the audio-policy owner. Covers landing, dash, down-dash, and
  ground/air jump.
- `src/hot-reload/modules/presentation/effect-composition.cpp`: movement sound
  emissions (dash, down-dash, landing, freeze, footstep) now resolve recipes via
  `hotEmitRecipeSound`; the inline random footstep variant/jitter and the
  hardcoded jump-sound path were removed. Visual composition is unchanged.
- `src/entities/player.cpp`: hurt sound now dispatches `audio.fact`
  (`hurt` recipe) and falls back to the direct call only when the hot handler
  did not accept the fact.
- `src/network/multiplayer-interpolation.cpp`: remote dash, ground jump, air
  jump, and freeze sounds now dispatch `audio.fact` with a cold fallback.
- `src/hot-reload/hot-modules.json`: registered `hot-audio-policy.h` for change
  detection.
- Selftest (`src/network/hot-combat-selftest.cpp`): the stale movement-audio
  tests were corrected to the canonical facts and new `audio.fact` coverage was
  added (recipe reaches `audio.play`; unknown recipe stays unhandled).
- Unrelated unblocking fix: `src/live-code/live-code-selftest.cpp` used
  `MimitaNet::GAME_CAP_SNAPSHOT_CODECS`; the symbol is global, so the qualifier
  was removed. This was a concurrent in-progress networking edit, not part of
  this feature.

## Evidence

Source ownership:

- Hot owner added: `hot.audio-policy` (recipes + selection + command build).
- Cold owners removed for movement: hardcoded sound strings in
  `movement-system.cpp` and inline selection in `effect-composition.cpp`.
- Cold mechanism remaining: `audio.cpp` device/mixer/voice table,
  `playWorldSound` fallback.
- One owner per fact: hot handler sets `handled`; cold fallback runs only when
  unhandled.

Build:

- `python build_game_dll.py` -> `DLL build success` (sources=89).
- `python build_agent.py` -> `BUILD SUCCESS`, exe `mimita-20260923T175931.exe`.

Runtime (headless):

- `--hot-combat-selftest`: 212 ok / 25 fail (Phase 2 exe was 207 ok / 28 fail).
  The three previously stale movement-audio tests now pass and new `audio.fact`
  checks pass; no new failure names. The 25 remaining failures are pre-existing
  and unrelated (animation phase2 and similar).
- `--live-code-selftest`: PASS.
- Live journal (e.g. `live_events_20260923_220139.jsonl`) shows the recipe path
  reaching the bridge: `sound":"entity/player/walk2"`, `volume":0.825`,
  `pitch":1.035`, `category":0` (Movement) - the footstep recipe jitter and
  category were applied, and `entity/player/doublejump` for the air-jump recipe.

Human:

- No live human audible acceptance was performed.

## Remaining (next phases)

1. Phase 4: weapons/projectiles audio recipes. Phase 5: NPC/actor. Phase 6:
   UI/notifications/editor/live-code. Phase 7: music/ambient.
2. Phase 8: logical sound-resource generations with off-thread decode and
   refcounted retire in `PresentationResourceProvider`; wire
   `RELOAD_RESOURCE`/`INVALIDATE_RESOURCE`; `audio.voice_finished`; detailed
   `audio voices`/`resources`.
3. Phase 9: `hitfx.json` control for jump/air-jump/footstep/movement effects.
4. Phase 10: delete obsolete helpers/dead fallbacks after runtime proof.
   Phase 11: audit update + permanent regression coverage.
5. Voice budget/interruption are carried in the command but not yet enforced by
   the mixer; recipe `cooldownMs`/`repeatAllowed` are parsed but not applied.

## Notes

- `Player::updateAudio` (cold-movement path) and `movement-system.cpp`
  (hot-movement path) both emit movement sounds, as before. They are mutually
  exclusive: when hot movement overrides the step, the cold `did*` flags stay
  false, so only one path runs. Live verification should confirm no duplicate
  movement sounds during hot movement.
- Recipe hot-edit and last-valid-on-invalid-JSON behavior are implemented but
  only build-verified here; live edit proof is still required.
