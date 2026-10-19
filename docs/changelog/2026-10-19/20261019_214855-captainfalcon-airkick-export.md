# Captain Falcon airborne B kick export

- Local time: 2026-10-19 17:48:55 EDT (UTC-04:00)
- UTC: 2026-10-19T21:48:55Z
- Branch/starting commit: `20261011zt` / `35c9b12af4ac2258b24e0116d156d40eedf99cbb`
- Request: export the supplied `knee20261019` Blender action and use it for the airborne right-click move, with the hitbox attached to the right leg.

## Move identification

The Melee spec calls airborne right-click `b_air_forward_dash` (`docs/specs/gamemodes/supersmashbrosmelee.md:374`). The existing move-set has that exact action ID with `button: secondary` and `actor_state: airborne` (`config/weapons.json:1771-1773`). This is the no-modifier airborne B move, not the separate jump-modified up-B or crouch-modified down-B action.

## Changes made

- `config/animations/library/melee/captainfalcon/b_air_forward_dash.json` is new. Blender 5.0 ran the MiMITA exporter on `C:\important\quiet\blender-projects\mimita sizing 6.blend`, root `plrOrigin.041`, keeping the six currently assigned body Actions. It exports 60 ticks, four authored poses, all six canonical parts, and source/playback markers 0/20/40/60. The `.blend` was not saved. There was no prior output file.
- `assets/avatars/captainfalcon/animations.json:370` adds the exported file to the profile include list. Before, the list ended with `shield.json`; after, it also includes `b_air_forward_dash.json`.
- `config/weapons.json:1771-1778` changes the existing airborne secondary action's clip from `kick_a` to `b_air_forward_dash`. Before: `"animation_clip_id": "kick_a"`. After: `"animation_clip_id": "b_air_forward_dash"`. The existing hitbox remains `"attached_part": "rightLeg"` with animated-limb attachment; no hitbox size, offset, phase window, damage, knockback, or input values were changed.

## Preserved workspace state

Many unrelated changes were already present in the checkout, including Melee config/code changes, other Captain Falcon clips, documentation changes, deletions, and untracked files. They were preserved. This task changed only the new exported clip, its profile include, and the clip reference for `b_air_forward_dash`.

## Documents and focused review

- Read `docs/ROUTER.md`, `docs/specs/gamemodes/supersmashbrosmelee.md`, `docs/specs/animations/animations.md`, `docs/specs/animations/blender-animation-authoring.md`, `docs/architecture/json-configuration/json-configuration.md`, `docs/architecture/time-and-formatting/time-and-formatting.md`, `docs/operations/asset-management/asset-management.md`, `docs/workflows/runtime-scenario-validation.md`, `docs/skills/spec-behavior-review-v1.md`, and `docs/skills/asset-checker-v1.md`.
- Spec-behavior review: PASS_WITH_HUMAN_REVIEW. The chosen action exactly matches the spec's airborne secondary move; the clip supplies animation timing. In-game pose/contact acceptance remains open.
- Asset review: PASS. The exported shared clip is included in the Captain Falcon profile.
- JSON consistency check: PASS. Clip duration is 60, four markers are ordered at 0/20/40/60, all keyframes contain the six required parts, the profile includes the file, and the secondary-airborne action references it. The `dash_foot` hitbox is present once and remains attached to `rightLeg`.

## Live runtime evidence and limitation

- The existing dev-loop Melee run remained open; it was not restarted or terminated. Its journal is `logs/2026-10-19/20261019_213626/events.jsonl`.
- The client avatar profile hot-reloaded at revision 2 with `clip_count=57` and the new clip path. The weapon move-set hot-reloaded at revision 5. The live NPC action path emitted `melee.action.start` for `b_air_forward_dash`.
- The server then emitted `melee.hitbox.pose_unavailable` for `b_air_forward_dash`, part `rightLeg`. The server had loaded its avatar profile before this export and had no corresponding `anim.set.reloaded` event; only the client profile reloaded. `AvatarAnimationRegistry::pollReload` is called from `src/sim/simulate-tick.cpp`, while server hitbox sampling reads the cached registry in `src/network/server-melee-actions.cpp`. This explains why that already-running server could not sample the newly added profile clip; it is not evidence that the exported right-leg binding is malformed. I did not restart the existing server under the runtime-safety instruction.
- No human pressed right-click while airborne in the game. Player action playback, confirmed contact, and visual acceptance remain unverified. The running server's cached clip set needs a fresh supported runtime lifecycle before its server-side contact path can use this newly included clip.
- No build was required for these JSON-only changes. A separate dev-loop build attempt against newer concurrent workspace sources had failed and marked the latest successful build stale; this task made no C++ edits and does not claim to resolve that workspace build state.

## Status

The export, profile include, action binding, and existing right-leg hitbox association are in place. Client profile reload and server action selection are observed. Server pose sampling and human gameplay/visual acceptance remain incomplete in the currently running session. No regression is declared.
