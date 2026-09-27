# Phases 4-7: weapon, NPC, UI, and world audio routed through hot recipes

Date: 2026-09-23T22:27:12Z
Status: one-shot audio migrated for weapons/NPC/UI/world; build + headless selftests pass; no live human acceptance

## Change

Infrastructure:

- `src/hot-reload/game-api.h`: appended `char sound[64]` to `GameAudioFactV1`
  (audio.fact.v2). An explicit logical sound overrides the recipe's sound list
  while the recipe still owns policy (category/volume/pitch/falloff).
- `src/hot-reload/hot-audio-policy.h` / `audio-policy.cpp`: `HotAudioOverrideV1`
  gained an optional explicit `sound`; `hotEmitRecipeSound` now accepts an
  explicit sound even when no recipe matches (generic impact defaults).
- `src/live-code/live-behavior.h/.cpp`: added `LiveBehavior::emitAudioFact(...)`
  (float[3] and glm::vec3 overloads). One-line cold callsite:
  `if (!emitAudioFact(recipe, sound, pos, owner, spatial, ...)) <cold fallback>;`.
- `config/audio-recipes.json`: added `weapon.fire/reload/dryfire/equip/melee/
  impact/explosion`, `projectile.inair/impact`, `hitmarker`, `npc.spawn`,
  `npc.action`, `actor.death`, `actor.respawn`, `ui.click/hover/chat`,
  `live.success/failure`, `editor.action`, `music.change`, `ambient.effect`,
  `pobject.spawn`, `bomb.effect`.

Phase 4 (weapons/projectiles): `weapon-audio.cpp`, `weapon-fire-damage.cpp`,
`weapon-godball-movement.cpp`, `weapon-hafs.cpp`, `weapon-rocket-launcher.cpp`,
`weapon-system-equip.cpp`, `weapon-swordsword.cpp`, `weapon-system.cpp`,
`weapon-spyknife.cpp`, `weapon-quick-hit.cpp`, `hitmarker-audio.cpp`,
`multiplayer-shots.cpp`, `engine-tick-net.cpp`, `engine-tick-camera.cpp`.
`effect-composition.cpp` weapon-fire and actor-sound handlers now emit recipes.

Phase 5 (NPC/actor): `npc.cpp`, `npc-spawn.cpp`, `death-system.cpp`.

Phase 6 (UI/notifications/editor/live-code): `ui-system.cpp`,
`ui-system-buttons.cpp`, `chat-bubble.cpp`, `live-code-events.cpp`,
`dev-overlay-commands.cpp`, and the hot `ui-actions.cpp` (now calls
`hotEmitRecipeSound` instead of building `GameAudioCommandV1` directly).

Phase 7 (music/ambient/world): `persistent-physics.cpp`, `gamemode-manager.cpp`.

## Evidence

Source ownership:

- Hot owner: `hot.audio-policy` now resolves weapon/NPC/UI/world recipes.
- Cold fallback: every migrated call keeps the original cold call inside
  `if (!emitAudioFact(...))`, so exactly one owner plays.
- Cold mechanism remaining: device/mixer/voice table; music streaming
  (`music-manager.cpp` uses its own miniaudio streaming engine).

Build:

- `python build_game_dll.py` -> DLL built (90 sources).
- `python build_agent.py` -> `BUILD SUCCESS`, exe `mimita-20260923T182608.exe`.

Runtime (headless):

- `--hot-combat-selftest`: 212 ok / 25 fail (identical to the Phase 3 exe); no
  new failure names.
- `--live-code-selftest`: PASS.
- Live journal `live_events_20260923_222655.jsonl` shows the weapon-fire recipe
  reaching the bridge: `sound":"rocketlauncher/rocketlaunchershoot"`,
  `volume":0.894`, `pitch":0.992`, `category":2` (Weapons) - explicit weapon
  sound plus recipe jitter.

Human:

- No live human audible acceptance was performed.

## Remaining (next phases)

1. Phase 8: logical sound-resource generations with off-thread decode and
   refcounted retire in `PresentationResourceProvider`; wire
   `RELOAD_RESOURCE`/`INVALIDATE_RESOURCE`; `audio.voice_finished`; detailed
   `audio voices`/`resources`.
2. Phase 9: `hitfx.json` control for jump/air-jump/footstep/movement effects.
3. Phase 10: delete obsolete helpers/dead fallbacks after runtime proof.
   Phase 11: audit update + permanent regression coverage.
4. Looping/owner-stopped voices are NOT yet hot: `spyknife`/`quick-hit`
   `stopOwner` voices, `npc-spawn` `stopOwner`, and music streaming. They need
   the hot-side slot API (`audio.set_voice`/`stop_voice`) plus a looping recipe.
5. Voice budget/interruption and recipe `cooldownMs`/`repeatAllowed` are parsed
   but not enforced by the mixer.
6. Live recipe hot-edit and per-category live acceptance still required.

## Notes

- Two behavior deltas from leaving owner-stopped voices cold: consecutive
  `spyknife`/`quick-hit` swings now overlap (recipe `overlap: allow`) instead of
  cutting the previous voice; their `stopOwner` calls become no-ops for the
  migrated sound. Acceptable for now; revisit with the slot API.
- Replay playback in `engine-tick-camera.cpp` now takes the recipe's
  category/maxDistance for arbitrary recorded sounds; volume/pitch are passed
  through exactly.
- This session ran while a concurrent networking/snapshot-codec task was editing
  and building the same tree; the build lock serialized builds and no
  networking files were touched by this work.
