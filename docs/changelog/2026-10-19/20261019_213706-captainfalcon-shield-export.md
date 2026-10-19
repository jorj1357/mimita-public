# Captain Falcon shield animation export and wiring

- Local time: 2026-10-19 17:37:06 EDT (UTC-04:00)
- UTC: 2026-10-19T21:37:06Z
- Branch/starting commit: `20261011zt` / `35c9b12af4ac2258b24e0116d156d40eedf99cbb`
- Request: export the supplied Blender `shield20261019` action with the MiMITA exporter and use it while shielding, following the Melee spec.

## Changes made

- `config/animations/library/melee/captainfalcon/shield.json` is new. Blender 5.0 ran `tools/mimita_animation_exporter.py` on `C:\important\quiet\blender-projects\mimita sizing 6.blend`, root `plrOrigin.039`, retaining the six current part Actions (`head.028Action.009`, `leftArm.029Action.009`, `leftLeg.024Action.009`, `rightArm.029Action.009`, `rightLeg.024Action.009`, `torso.026Action.009`). It exports 60 ticks, 3 authored poses, all six canonical parts, and marker ticks 0/20/40/60. The `.blend` was not saved. There was no prior output file.
- `assets/avatars/captainfalcon/animations.json:369` adds the shield clip to `include`. Before, the include array ended with `config/animations/library/locomotion/captainfalcon/run.json`; after, it also contains `config/animations/library/melee/captainfalcon/shield.json`.
- `config/weapons.json:1937-1943` changes the shield action from three legacy one-tick `cf_shield` phase references to `"animation_clip_id": "shield"`. Before: `"phases": { "windup": {"ticks": 1, "clip_id": "cf_shield"}, "active": {"ticks": 1, "clip_id": "cf_shield"}, "recovery": {"ticks": 1, "clip_id": "cf_shield"} }`. After: `"animation_clip_id": "shield"`. Other shield damage, knockback, and audio values remain as they were.
- `src/combat/melee-client.cpp:389-400` now samples the unified clip at its parsed active-start tick while Q is held; the timeline markers supply that tick (20 for this export). Before, it sampled the legacy clip at a fixed one-tick phase pose. Existing legacy phase behavior remains as a fallback for actions without `animation_clip_id`.

## Preserved workspace state

The repository already contained many unrelated edits before this task, including prior Captain Falcon clip/config work, movement and combat changes, exporter changes, documentation edits, deletions, and untracked files. They were preserved. `config/weapons.json`, `src/combat/melee-client.cpp`, and the avatar profile also had pre-existing edits; this task changed only the shield block/branch/include described above. No unrelated changes are attributed to this task.

## Documents and reviews

- Read `docs/ROUTER.md`, `docs/specs/gamemodes/supersmashbrosmelee.md` (shield is hold-based; shield presentation is a separate visual requirement), `docs/specs/animations/animations.md`, `docs/specs/animations/blender-animation-authoring.md`, `docs/architecture/json-configuration/json-configuration.md`, `docs/architecture/time-and-formatting/time-and-formatting.md`, `docs/operations/asset-management/asset-management.md`, `docs/operations/build-and-exe/build-and-exe.md`, `docs/operations/task-completion/task-completion.md`, `docs/workflows/runtime-scenario-validation.md`, `docs/skills/spec-behavior-review-v1.md`, and `docs/skills/asset-checker-v1.md`.
- Export/profile/config consistency validation: PASS. JSON parsed; clip duration and four markers are valid; all frames include the six required parts; profile includes the clip; exactly one weapon action references it.
- `docs/skills/spec-behavior-review-v1.md`: PASS_WITH_HUMAN_REVIEW. Hold-based shield timing uses the exported active pose; visible presentation still needs in-game review.
- `docs/skills/asset-checker-v1.md`: PASS. Export is in the shared animation library and included in the Captain Falcon profile.

## Build and runtime evidence

- Canonical command `python devscripts/dev-loop.py --request build --wait-for-build` returned success for build `2625`. The dev-loop status afterward identified `.dev/builds/2625/mimita.exe`, source generation 37, and `latest_build_stale=False` at that observation.
- Running `.dev/builds/2625/mimita.exe --versioninfo` reported `EVENTS_JSONL_PATH=logs/10-19-2026/20261019_173555/events.jsonl` and executable path `.dev/builds/2625/mimita.exe`.
- The existing `npc-navigation` dev-loop profile is configured for Melee with one NPC. A fresh run was launched through the dev-loop; its journal is `logs/2026-10-19/20261019_213626/events.jsonl`. It records `weapon.melee.moveset_loaded` for 14 actions and `anim.set.loaded` with `valid=true`, `clip_count=56`, and the new `shield.json` include path. This proves config/profile loading, not that the shield input path played.
- The Q-held action and visible pose were not exercised or visually inspected in-game. Human visual acceptance remains pending. The Melee shield sphere/effect requirement is separate and is not implemented by exporting this body animation.
- The dev-loop-launched run was left open; no existing game process was terminated.

## Status

Export and wiring are complete; build and asset/config loading are confirmed. Shield-input runtime playback and human visual review remain unverified. No regression is declared.
- After build `2625` and runtime launch, concurrent workspace source edits advanced the dev-loop source generation from 37 to 42. The latest status is `build_failed` for generation 42, with output tail ending `[SKIP] src\world\world-gltf-loader.cpp` and `BUILD FAILED`; the status does not identify the failure cause. This later result is not evidence against or in favor of build 2625, which was current for generation 37 when checked. Preserve this as an unresolved workspace build state rather than attributing it to the shield change.
- Follow-up build request for the then-current source generation 45 also failed with exit code 1. The dev-loop retained build 2626 and marked it stale. Its status output did not expose a compiler diagnostic, so the failure remains unassigned; build 2625 still proves the shield source as it stood at generation 37 compiled. The live run remained active and was not restarted.
