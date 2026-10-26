# Impact decal shader-state fix and live Juggernaut trace

- Time: 2026-10-26T21:03:17Z (2026-10-26 17:03:17 EDT)
- Branch: `20261011zt`; HEAD at final review: `0d7138cdf5e603fde85d16242a6a2a02a432ed63`
- Result: `PASS_WITH_HUMAN_REVIEW` for the separate bullet-hole art/size question; the reported shard defect was accepted by the player in Juggernaut.

## Scope and first incorrect step

The user reported bullet holes, world cracks, and blood as stretched shards, asked for detailed client effect events, and identified Juggernaut as the reproduction mode. The relevant specification in `docs/specs/visuals/blood-bulletholes-viscera.md` requires the source PNG and alpha shape to remain intact without stretched or fragmented marks. Source inspection found that `CosmeticSystem::renderCosmetics()` enabled `uCosmeticTextureEnabled` for avatar cosmetics but did not clear it. `shaders/basic.frag:204-219` transforms texture coordinates using cosmetic offset/scale/rotation before its textured-effect branch. Later impact quads therefore sampled their PNGs using the last avatar cosmetic's UV transform. The `abusiveboy` avatar has extreme image scales, which amplified the shared-shader state leak; its JSON image scaling is not evidence of stretched body geometry.

The live A/B isolated the effect owner: with build 3376 in Juggernaut, `decals 0` removed the shards according to the player. The fix in build 3378 restored decals, and the player reported that they no longer looked like shards. The remaining size/art question is distinct: the current bullet-hole scale 7.0 produces a 2.8-world-unit stamp and may warrant separate visual tuning.

## Changes

- `src/avatar/cosmetic-system.cpp:135-150,275-276`: resolve shared cosmetic uniform locations once per actor and clear `uCosmeticTextureEnabled` after cosmetic rendering. Previously it could remain enabled for later draw calls; now the cosmetic renderer relinquishes that shader state.
- `src/effects/effect-part-render.cpp:77-80`: set `uCosmeticTextureEnabled=0` and `uTexBreatheEnabled=0` immediately before sampling impact textures, preserving the stamp's complete 0..1 UV range. `src/combat/weapon-viewmodel.cpp:548-549` likewise establishes its own texture shader state.
- `src/effects/effect-part-render.cpp:627-726`: `decals 0` hides already-spawned surface stamps as well as stopping new ones, so the live A/B command accurately isolates decals.
- `src/effects/effect-part.h:91-100`, `src/effects/effect-part.cpp:393-432`, `src/effects/effect-part-blood.cpp:357-370`, `src/effects/effect-part-render.cpp:683-706`, and `src/effects/hitfx-commands.cpp:195-207`: when `hitfx_trace 1` is on, emit centralized `impact_decal.spawn`, first-visible `impact_decal.render_submitted`, `impact_blood_spray.spawn`, and trace-toggle events. Records include cause, position, actor IDs where known, texture, scale and world quad size, UV range, rotation, stretch mode, alpha, lifetime, and a matching decal ID. The rendering category was enabled at Important level in `config/debuglogger.json`. These diagnostic edits were incorporated into shared-worktree commit `0d7138cd` during this session; the shader-state fix remains an uncommitted local change.
- `docs/specs/open-gameplay-issues.md:1007`: recorded the confirmed shader cause, A/B outcome, build 3378 acceptance, trace path, and separate remaining art/size review.

The worktree contained unrelated configuration and documentation edits at start and changed concurrently. Those were preserved. The earlier source-only investigation has its own separate record, `docs/changelog/2026-10-26/20261026_164451-impact-decal-image-investigation.md`.

## Evidence

- Build: the canonical dev loop published builds 3373 (diagnostics), 3376 (decals switch), and 3378 (shader fix). Each completed successfully. Exact build identity was checked with each published `mimita.exe --versioninfo`; build 3378 reported `EVENTS_JSONL_PATH=logs/10-26-2026/20261026_170028/events.jsonl`. A first launch after some builds did not reach logger readiness; retry through the same dev loop succeeded. No manual stale executable was substituted.
- Runtime: Juggernaut build 3378 journal `logs/2026-10-26/20261026_210103/events.jsonl` recorded matched spawn/draw IDs for bullet holes and world cracks, loaded textures, 0..1 UV, equal quad axes, and alpha. For example, `decal-91` is a 2.0 x 2.0 world-crack stamp, and `decal-92` is a 2.8 x 2.8 bullet-hole stamp. The toggle event confirmed trace was turned off afterward. Earlier live journals also recorded blood spray/surface effects. This proves the effect path and diagnostics ran; journal values alone cannot judge appearance.
- Human visual check: the player said **“No, shards disappeared”** with `decals 0` in Juggernaut, then **“no now it does decals but they dont look like shards”** with decals on in build 3378. This accepts the shard fix in the observed scene, without deciding whether bullet-hole size/art matches the intended reference.
- Source check: `git diff --check` passed for the edited source and tracker. No synthetic tests were added.

## Routed review

Read `AGENTS.md`, `docs/ROUTER.md`, the visuals, avatar, debug-logging, JSON configuration, terminal-command, runtime-scenario, build/EXE, time/formatting, and task-completion documents, and the focused `spec-behavior-review-v1.md`, `logging-checker-v1.md`, `asset-checker-v1.md`, and `documentation-checker-v1.md` checklists. The router-selected `docs/regressions/regressions-v1.md` and `docs/doc-review-09-03-2026.md` were absent. No previously working version was established, so no regression record was added. `docs/specs/visuals/blood-bulletholes-viscera.md:2` contains `jorj todo explain`; the spec owner should expand it into concrete visual acceptance examples for bullet holes, blood, and cracks.
