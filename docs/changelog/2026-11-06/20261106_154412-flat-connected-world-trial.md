# Flat connected-world trial

- Time: 2026-11-06T15:44:12Z (2026-11-06 10:44:12 EST)
- Branch: `20261011zt`; no commit or deployment.
- Result: `PASS_WITH_HUMAN_REVIEW` for the flat three-tile walk; general multiplayer chunk authority and strict per-frame 240 FPS remain open.

## Specification and first divergence

`docs/specs/procedural-infinite-world/procedural-infinite-world.md` requires compatible authored connectors, server-confirmed canonical identities, bounded generation, and fixed 60 Hz gameplay. The live mode-52 journal previously reported `whole_world=false`, `spacing_mode=center`: `InfiniteWorldRuntime::tick` chose the generator profile only from `CommunityMatchClient::mode()`, which was empty before match state arrived. The old `funworld3` dungeon also obstructed a clean open-grid walk. The server loaded only the initial map GLB triangles, so it could not provide generated floor contact beyond the spawn tile.

## Changes

- `src/procedural/infinite-world-runtime.cpp` and `.h`: activate the named profile from the authored zone as well as match state; treat zero as a valid connected-grid origin; cache the loaded mode profile until map/library revision changes; resolve fully compatible horizontal tiles directly from seed/cell and prune resolved cells outside the 25-cell radius. Keep the existing graph resolver for restrictive connections. Report player position, grid choice, pending, and resolved counts in `world-stream.update`.
- `assets/maps/connected_world.glb`, `config/maps/connected_world.json`, `config/worldgen/connected_world.json`, and `devscripts/dev-launch-modes.json`: add a dedicated origin spawn/base floor, three editor-authored 10 × 10 m floor variants with four edge connectors, 10 m X/Y cells with no Z repetition, and launch mode 52 on this map. The original `funworld3` map was restored to its pre-session content.
- `src/map/map-authored-geometry.cpp`, `src/network/server.h`, and `src/network/server-players.cpp`: enable a server fixed-tick flat support plane only when all authored collidable tile parts are equal-height unrotated floors. Log configuration and rate-limited authoritative ground contact. This does not authorize per-cell identity or general chunk geometry.
- `src/worldgen/worldgen-commands.cpp`: correct optional material and tag argument positions; reject duplicate chunk IDs. A timed-out command was retried during authoring and created an empty duplicate `chunk3`; the empty copy was removed from the saved map before reload.
- `docs/specs/procedural-infinite-world/procedural-infinite-world.md`: document the dedicated mode, exact movement/authoring/editor commands, bounded flat-grid behavior, target measurements, and remaining authority/performance limits.

Unrelated `config/analytics.json` runtime edits and the pre-existing `devscripts/blender-ai` submodule state were preserved.

## Validation

- Build: canonical dev loop published debug build `2026-11-06T15:40:23.819103Z:e480592ce44156b8dd95ee6a2363f2b68baf5cde148b5cef5974ca6c08ebb4b8`, source generation 34, then launched that exact build in mode 52.
- Config/static: `gamemode_json_sync.py --check` passed; new map, mode, and launch JSON parsed; focused `git diff --check` passed.
- Runtime: `logs/2026-11-06/20261106_154052/client-01/events.jsonl` recorded start X=0, later X=290.09/cell X=29, `independent_grid=true`, all three chunk IDs installed (57/59/53), and loaded/resolved counts never above 25, with no rejected cells or installs over 8 ms. The player moved through X=250 on the ordinary `agent.move` fixed-tick input path. An earlier run `20261106_152127` returned near X=0 and advanced through cell X=25 to X=270.1.
- Server: `logs/2026-11-06/20261106_153720/server-01/events.jsonl` recorded fixed-tick grounded floor contact at X=257.2 and X=289.9, Z=1.75. Final run `20261106_154052` recorded server contact at X=290.1. Across 14 movement-window server reports in that run, each reported 60–61 ticks, maximum tick 2.721 ms, and zero ticks over 100 ms.
- Editor: `20261106_152127` journal recorded `worldgen.connector.preview` with a compatible neighbor and `map-editor.chunk-preview`; `mapeditor save` persisted `chunk2`, and a full relaunch installed 12 `chunk_a` and 13 `chunk2` cells with 24 connector matches and no rejected cells. A later `mapeditor save`/`reload` persisted `chunk3` with four no-tag connectors and `stonev2`; `20261106_152532` generated all three IDs and logged 257 matches, no missing connectors, while walking past X=280.
- Visual: an agent screen capture of the running game showed a textured open floor and a local humanoid avatar in third-person view. This is not human gameplay acceptance or proof of the expected avatar identity.
- Performance: in final run one-second client windows during travel averaged at least 362.9 FPS, but maximum p95 frame time was 4.55 ms and one frame reached 21.14 ms. The 21.14 ms slice included a 12.01 ms shadow pass; no chunk install crossed the 8 ms diagnostic threshold. Strict 240 FPS on every frame is therefore **not** met.

## Focused review and remaining work

- `docs/skills/spec-behavior-review-v1.md`: high, spec-code disagreement remains for server-confirmed per-cell chunk identity and general generated geometry collision; the flat support plane covers only equal-height floors. No human decision was required for the dedicated flat trial.
- `docs/skills/efficiency-checker-v1.md`: removed per-frame profile file I/O and bounded the resolved cache for mutually compatible flat tiles. Restrictive connector graphs still resolve through an origin path and need a separate long-distance budget/worker design.
- `docs/skills/logging-checker-v1.md`: client updates expose position/cell/budget/cache; server contact logs are limited to one sample per five seconds. Client and server canonical chunk-ID agreement is not yet observable.
- `docs/skills/documentation-checker-v1.md`: new commands match the corrected parser and source; the older informal text at the top of the spec remains historical and needs editorial cleanup separately.
- Remaining implementation/acceptance: server-authoritative per-cell identity and edits, general generated-chunk collision, off-thread generation for complex authored chunks, two-client identity agreement, editor gizmo visual review, and human avatar/gameplay review. The flat trial is not proof of arbitrary 999-player capacity or indefinite travel for restrictive connector graphs.
