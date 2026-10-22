# Player visuals live reload and enemy point light

- Time: 2026-10-22T01:53:19Z (America/New_York: 2026-10-21 21:53:19 EDT)
- Branch: `20261011zt`
- HEAD: `9ec73c3ae724037da33d032d54843cbf1186f97a`
- Result: `PASS_WITH_HUMAN_REVIEW`

## Finding

The active JSON had `enemy.contrast.brightness=12.2` (allowed range 0..4), `tintStrength=120` (0..1), and `trail.width=212` (0.01..4 after this change). The loader validates the complete candidate atomically, so any one invalid value rejected every field in that save and preserved the old visual state. Also, `PlayerVisualsConfig::pollReload()` was inside the global `config/hotreload.json` gate, and the renderer required both a selected `mode` and a layer's `enabled` flag. A mode of `none` therefore hid an explicitly enabled layer.

## Changes

1. `config/playervisuals.json:2-29`
   - Before: enemy contrast was `{"enabled": false, "brightness": 12.2, "tint": [255, 255, 255], "tintStrength": 120.0}` and trail width was `212.0`; no point-light settings existed.
   - After: all values are within accepted ranges; enemy trail, contrast, and an orange point light are enabled, while outline/capsule/wireframe remain disabled. The current saved trail interval is `0.1` seconds and was preserved. Global `renderOrder` now includes the trail; obsolete `mode` keys and unconsumed per-layer order numbers were removed.
   - Exact excerpts:
     - Before: `"trail": { "enabled": true, "minSpeed": 0.01, "sampleIntervalSeconds": 0.1, "lifetimeSeconds": 0.22, "maxSamples": 3, "width": 212.0, "alpha": 0.65, "color": [255, 22, 55] }`
     - After: `"trail": { "enabled": true, "minSpeed": 5.0, "sampleIntervalSeconds": 0.1, "lifetimeSeconds": 0.4, "maxSamples": 16, "width": 0.55, "alpha": 0.9, "color": [255, 50, 45] }`
     - Added: `"pointLight": { "enabled": true, "intensity": 2.0, "radius": 12.0, "heightOffset": 1.0, "color": [255, 110, 45] }`
2. `src/config/player-visuals-config.h:59-83,119`
   - Before: no point-light settings or record of the last attempted file timestamp existed; data stored per-category `mode` fields.
   - After: added validated enemy/teammate point-light settings and a last-attempted timestamp; removed unused mode state so each layer's `enabled` field is its direct switch.
   - Before: no `PlayerPointLightSettings` type existed.
   - After: `struct PlayerPointLightSettings { bool enabled = false; float intensity = 1.5f; float radius = 8.0f; float heightOffset = 1.0f; glm::vec3 color{255.0f, 110.0f, 45.0f}; };`
3. `src/config/player-visuals-config.cpp:27-55,125-244,247-283`
   - Before: the parser supported trail/contrast but silently ignored unknown keys, did not parse point lights, and retried the same invalid file every poll.
   - After: every supported key is checked, the point-light object is validated, the whole candidate remains atomic, invalid saves report the path/error to `events.jsonl`, and only a new file timestamp triggers another failed-save attempt. Successful loads write `player_visuals.config.applied` with effective feature switches and render order.
   - Before: `if (wt == std::filesystem::file_time_type{} || wt == mLastWrite) return false;`
   - After: `if (wt == std::filesystem::file_time_type{} || wt == mLastAttemptedWrite) return false;`
4. `src/engine/engine-tick-setup.cpp:185-189`
   - Before: `PlayerVisualsConfig::instance().pollReload();` ran only inside the global `shouldPoll` block.
   - After: a separate ten-frame counter polls this presentation file even if the global config gate is disabled.
5. `src/render/dynamic-light.h:16-42` and `src/render/dynamic-light.cpp:24-89`
   - Before: dynamic lights only had short lifetimes and no actor owner/update path.
   - After: the existing fixed light pool can own and refresh one persistent light per rendered network actor, update its replicated position/color/intensity/radius, remove it when disabled/dead, and expire it if the actor disappears.
6. `src/render/render-player.cpp:36-174,370-527`
   - Before: outline/capsule/wireframe were gated by one mutually exclusive `mode`; the trail used GL line primitives, whose width can be clamped by the graphics driver.
   - After: explicit layer `enabled` switches work independently, configured `renderOrder` dispatches the actual layer calls, the trail is a camera-facing world-space triangle ribbon, and a selected enemy/teammate point light follows `player.pos` (the client-replicated actor position) through the existing dynamic-light manager. It adds no marker; the existing unshadowed lighting can brighten nearby wall surfaces.
   - Before: `if (mode == "outline" && outlineEnabled && ...)`
   - After: `if (outlineAllowed && outlineEnabled && ...)`
   - Before: `glDrawArrays(GL_LINES, 0, (GLsizei)vertices.size());`
   - After: `glDrawArrays(GL_TRIANGLES, 0, (GLsizei)vertices.size());`

## Documents and review

- Read `docs/ROUTER.md`, `docs/specs/visuals/visuals.md`, `docs/architecture/json-configuration/json-configuration.md`, `docs/specs/hotreload/hotreload.md`, `docs/workflows/runtime-scenario-validation.md`, `docs/specs/debug-logging/debug-logging.md`, `docs/specs/performance/performance.md`, `docs/operations/build-and-exe/build-and-exe.md`, `docs/operations/task-completion/task-completion.md`, `docs/architecture/time-and-formatting/time-and-formatting.md`, `docs/architecture/live-development/live-development.md`, and `docs/skills/spec-behavior-review-v1.md`, `docs/skills/logging-checker-v1.md`, `docs/skills/efficiency-checker-v1.md`.
- Specification review: `docs/specs/visuals/visuals.md` requires independent layers, hot reload on existing actors, and atomic retention of the last valid config. The user's request authorizes trying trail/contrast/point-light layers beyond the original outline-first V1. Through-wall rendering remains off.
- The router names `docs/regressions/regressions-v1.md`, which is absent at that path; `docs/regressions/README.md` and the relevant dev-loop status-write regression were read. No current player-visual regression record exists.
- The visuals spec has the line 1 note `2026-09-07T18:40:16Z jorj - todo explain this  more like`. Suggested expansion: explain the intended player-readability art direction, how trails/contrast/local lights fit it, and which FOV/scale/readability choices still need human decision. The spec was not edited.
- Logging review: one applied/rejected event per changed file timestamp, with the canonical JSONL logger; no per-frame reload logging.
- Efficiency review: fixed-size light pool scans at most 32 slots per rendered remote actor; trail histories remain capped at 128 actors and 64 samples. No gameplay collision, physics, or network path was changed.

## Validation and evidence

- `config/playervisuals.json` parsed with PowerShell `ConvertFrom-Json`; `git diff --check` passed (Git emitted existing line-ending conversion notices).
- Canonical dev-loop builds 2751 through 2754 succeeded during iteration; final status was `published_current` for build 2754.
- `.dev/builds/2754/mimita.exe --versioninfo` reported `EXECUTABLE_PATH=C:\mimita-v9\.dev\builds\2754\mimita.exe`, working directory `C:\mimita-v9`, and `EVENTS_JSONL_PATH=logs/10-21-2026/20261021_215238/events.jsonl`.
- Runtime limitation: the existing client remains alive on build 2748 and the dev-loop status reports its server is not alive. It was left open. The version-info command was not a gameplay/config scenario; no `player_visuals.config.applied` event from build 2754 or visual output was observed. Build 2754 must be selected/launched before testing the new point light and independent-layer behavior. Human review still needs to confirm the trail/light readability at the user's midrange, 100 FOV, and high movement speed.
- No tests were added or run, following the repo instruction not to introduce synthetic visual tests as the first response.

## Pre-existing and concurrent work

At task start, these edited files were already present and were preserved: `config/accounts/default.json`, `config/analytics.json`, `config/audio/music-settings.json`, `config/gamemodes/npc_waves.json`, `config/gamemodes/sandbox.json`, `config/healthbar.json`, `config/npc-difficulty.json`, `src/engine/engine-tick-ui-game-hud.cpp`, `src/network/multiplayer-reconcile.cpp`, `src/network/multiplayer-tick.cpp`, `src/network/server-damage.cpp`, `src/network/server-gamemode.cpp`, `src/network/server-players.cpp`, and the earlier edits in `config/playervisuals.json`, `src/config/player-visuals-config.cpp`, `src/config/player-visuals-config.h`, and `src/render/render-player.cpp`. The current pre-existing changelog folder was preserved. Additional edits appeared later in `config/behavior-profiles.json`, `config/tips.json`, `docs/specs/open-gameplay-issues.md`, `src/entities/player-render.cpp`, and `src/entities/player.h`; they were not changed by this work.
