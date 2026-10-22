# Player boost trail and contrast experiment

- Time: 2026-10-22T01:29:12Z (America/New_York display date 2026-10-21)
- Branch: 20261011zt
- HEAD: 9ec73c3ae724037da33d032d54843cbf1186f97a
- Result: PASS_WITH_HUMAN_REVIEW

## Scope and pre-existing work

Added a first configurable speed-trail and texture-brightness/tint experiment to the active player-visuals path. Enemy outlines default off. The trail is client-rendered, follows recent high-speed motion, respects world depth, and does not create replay effect events. Existing live Duel client/server processes stayed on build 2743 and were not restarted.

At the start, unrelated worktree changes included config/gamemodes/sandbox.json, config/video-settings.json, and an untracked docs/changelog/2026-10-22/ folder. During this session, config/accounts/default.json, config/healthbar.json, and src/engine/engine-tick-ui-game-hud.cpp also appeared modified. They were left untouched.

## Exact edits

1. config/playervisuals.json:10-11,22-24,31-32
   - Before: enemy mode was "outline" (original line 19); there were no trail or contrast entries.
   - After: enemy mode is "none"; contrast lifts texture brightness to 1.2, and an orange trail is enabled at speed 35, sampled every 0.035s, with 0.22s lifetime, 12 samples, 3px width, and 0.65 alpha. Teammate trail/contrast defaults are off. Comments document the editable fields.
2. src/config/player-visuals-config.h:51-68,77-78
   - Before: no PlayerTrailSettings or PlayerContrastSettings structures.
   - After: added bounded trail fields (minSpeed, sample interval, lifetime, sample count, width, alpha, color) and contrast fields (brightness, tint, tintStrength) for self/enemy/teammate settings.
3. src/config/player-visuals-config.cpp:52-96,125-134
   - Before: the parser did not read trail or contrast objects.
   - After: parses and validates those complete objects into the candidate config; invalid ranges or malformed colors reject the candidate so existing atomic last-valid reload behavior remains in place.
4. src/entities/player.h:716-724,753-757
   - Before: const glm::vec4& outlineColor = glm::vec4(1.0f)) const; and bool hideHead = false) const;
   - After: both render entry points accept an optional neutral playerTint, preserving existing callers and allowing local per-player texture tuning.
5. src/entities/player-render.cpp:28-38,109-117,206,436,470-481,503,510
   - Before: body rendering always set uTint to white.
   - After: the selected tint multiplier is applied to physical, fallback-mesh, and capsule body rendering, then reset to white after each path.
6. src/render/render-player.cpp:36-165,323-367
   - Before: no player movement history, boost trail draw, or contrast selection existed.
   - After: keeps at most 128 actor histories with at most 64 samples each, samples only when configured speed is met, drops large position discontinuities, fades and draws world-space line segments with depth testing, and restores modified OpenGL state. The render path selects independent enemy/teammate settings and passes configured brightness/tint to the avatar renderer.

## Documents and focused review

Read docs/ROUTER.md, docs/specs/visuals/visuals.md, docs/architecture/json-configuration/json-configuration.md, docs/skills/spec-behavior-review-v1.md, docs/skills/documentation-checker-v1.md, docs/workflows/runtime-scenario-validation.md, docs/operations/build-and-exe/build-and-exe.md, docs/operations/task-completion/task-completion.md, docs/architecture/time-and-formatting/time-and-formatting.md, docs/architecture/live-development/live-development.md, docs/specs/dev-loop/dev-loop.md, and docs/regressions/2026-10-21/dev-loop-status-write-race-REG.md.

- Specification review: docs/specs/visuals/visuals.md says V1 initially implements outline and describes trails as future capability. The user's direct request authorized this first trail/contrast experiment. brightness is a texture multiplier with optional tint; it is not a full contrast curve. Visual feel and exact values remain open to user review.
- Router's docs/regressions/regressions-v1.md is absent in this checkout; the relevant current dev-loop recovery regression was read instead.
- The visuals spec line 1 TODO is: 2026-09-07T18:40:16Z jorj - todo explain this  more like. Suggested expansion: describe the player's visibility problem, art-direction constraints, candidate visual layers, active config owner, and which choices remain undecided. The spec was not edited.
- Documentation checker: no documents were changed, so no document rewrite was needed.

## Validation and evidence

- config/playervisuals.json parsed as valid JSON.
- git diff --check found no whitespace errors; Git printed line-ending conversion notices for the edited C++ files.
- Canonical dev-loop build request succeeded: build 2748, source generation 7, published_current. The dev-loop status reported the latest build current and successful.
- .dev/builds/2748/mimita.exe --versioninfo printed the executable identity and EVENTS_JSONL_PATH=logs/10-21-2026/20261021_212903/events.jsonl.
- No gameplay scenario was run. The active client/server remain on build 2743; no new client was launched to avoid changing the user's live session. Build and version-info do not prove the trail or tint is visible or feels right.
- Human review still needed for trail location/readability, brightness against dark/light/noisy backgrounds, and tuning minSpeed, lifetime, alpha, width, and tint. Saving the JSON while the new executable is running should hot-reload these values through the existing owner.
- No tests were added or run.



## Verbatim edit excerpts

- config/playervisuals.json: original enemy mode line:
  - Before: "mode": "outline",
  - After: "mode": "none",
  - Added: "trail": { "enabled": true, "minSpeed": 35.0, "sampleIntervalSeconds": 0.035, "lifetimeSeconds": 0.22, "maxSamples": 12, "width": 3.0, "alpha": 0.65, "color": [255, 100, 55] },

- src/config/player-visuals-config.h:51-68
  - Before: no trail or contrast settings structures existed.
  - After:
    struct PlayerTrailSettings {
        bool enabled = false;
        float minSpeed = 35.0f;
        float sampleIntervalSeconds = 0.035f;
        float lifetimeSeconds = 0.22f;
        int maxSamples = 12;
        float alpha = 0.65f;
        float width = 3.0f;
        glm::vec3 color{255.0f, 80.0f, 40.0f};
    };
    struct PlayerContrastSettings {
        bool enabled = false;
        float brightness = 1.0f;
        glm::vec3 tint{255.0f};
        float tintStrength = 0.0f;
    };

- src/config/player-visuals-config.cpp:123-134
  - Before: PlayerWireframeSettings* wire = &out.selfWireframe; followed only by enemy/teammate capsule and wireframe selection.
  - After: PlayerContrastSettings* contrast = &out.selfContrast; and PlayerTrailSettings* trail = nullptr; followed by per-category selection and calls to readContrast and readTrail.

- src/entities/player.h:724,757
  - Before: const glm::vec4& outlineColor = glm::vec4(1.0f)) const; and bool hideHead = false) const;
  - After: const glm::vec4& outlineColor = glm::vec4(1.0f), const glm::vec3& playerTint = glm::vec3(1.0f)) const; and bool hideHead = false, const glm::vec3& playerTint = glm::vec3(1.0f)) const;

- src/entities/player-render.cpp:37-38,206
  - Before: renderCurrentPose(shader, view, proj, flash, hideHead); and glUniform3f(uTintLoc, 1.0f, 1.0f, 1.0f);
  - After: renderCurrentPose(shader, view, proj, flash, hideHead, false, 0.0f, glm::vec4(1.0f), playerTint); and glUniform3f(uTintLoc, playerTint.r, playerTint.g, playerTint.b);

- src/render/render-player.cpp:46,145,367
  - Before: no renderPlayerTrail helper or call existed; the base player call ended with hideHead.
  - After: void renderPlayerTrail(const Player& player, const Camera& cam, uint32_t entityId, const PlayerTrailSettings& settings); draws the sampled path with glDrawArrays(GL_LINES, 0, (GLsizei)vertices.size()); and calls player.render(gRenderer->shaderProgram, view, proj, hideHead, playerTint);.


