# TDM team spawns and presentation

- Recorded UTC: 2026-10-24T02:37:07Z (2026-10-23 22:37:07 EDT)
- Branch: 20261011zt
- HEAD: 2103321b
- Scope: continue existing TDM work with separate team spawn groups, JSON-controlled player mesh wireframes, and health bar presentation. Preserve unrelated working-tree edits and untracked files.

## Changes

- `src/network/server-gamemode.cpp`, `assignGamemodeSpawns()` and `gamemodeSpawnPoint()`: when a team declares a spawn group, its authored map spawn candidates must have that exact tag. Previously an untagged generic point could be selected for either team. The resolver uses team-tagged GLB points or first/last map spawn points when available; if team clusters are missing or overlap, it derives separated anchors around the shared map anchor. Authored map candidates coincident with the other team's authored or resolved fallback anchors are skipped.
- `config/gamemodes/tdm.json`: keep the user-requested 30-point goal, use `team_shared`, assign RED/BLUE the `red_spawn`/`blue_spawn` groups and matching tag filters, keep health bars visible, and request mode-owned team wireframes with green/red colors.
- `src/gamemode/gamemode.h`, `GamemodePresentation`: new optional `player_wireframes`, `friendly_wireframes`, and team wireframe color values.
- `src/gamemode/gamemode.cpp`, mode parser: read the new switches and RGB values from `presentation`.
- `src/config/player-visuals-config.h` and `.cpp`: add temporary runtime overrides for wireframe enablement and team colors. The overrides do not write the user's global visuals JSON.
- `src/network/community-match-client.cpp`: apply/clear mode wireframe policy and include the resolved JSON fields in the presentation fingerprint/journal event.
- `src/render/render-player.cpp`: force team wireframes only after authoritative team identity is known; apply the configured friendly/enemy color.
- `docs/specs/graphics/graphics-presentation.md`: document the new mode fields.
- `docs/specs/gamemodes/teamdeathmatch.md`: document separate team spawn groups and colored wireframes.

## Exact edits

`src/network/server-gamemode.cpp:1713-1730,1860-1877`
- Before: `if (e.tag.empty() || group.empty() || e.tag == group)`
- After: `if (group.empty() ? e.tag.empty() : e.tag == group)`; candidates at the opposing team's positions are removed, and missing/overlapping clusters receive distinct team anchors.

`config/gamemodes/tdm.json:6,16,28-29,38-39,54-60`
- Before: `"goal_value": 50` → After: `"goal_value": 30`
- Before: `"spawn_strategy": "shared"` → After: `"spawn_strategy": "team_shared"`
- Before: no team spawn groups → After: RED has `"spawn_group": "red_spawn", "spawn_tag_contains": "red"`; BLUE has `"spawn_group": "blue_spawn", "spawn_tag_contains": "blue"`.
- Before: no TDM healthbar/wireframe policy → After: `"hide_healthbars": false`, `"enemy_healthbars": true`, `"player_wireframes": true`, `"friendly_wireframes": true`, `"friendly_wireframe_color": [0, 255, 0]`, `"enemy_wireframe_color": [255, 0, 0]`.

`src/gamemode/gamemode.h:141,154-156`
- Before: no mode wireframe fields.
- After: `bool hasPlayerWireframes = false; bool playerWireframes = false;`, `bool hasFriendlyWireframes = false; bool friendlyWireframes = false;`, plus mode team wireframe RGB fields.

`src/gamemode/gamemode.cpp:335-336,366-378`
- Before: parser had no mode wireframe keys.
- After: added `readFlag("player_wireframes", ...)`, `readFlag("friendly_wireframes", ...)`, and parsing for the friendly/enemy wireframe RGB arrays.

`src/config/player-visuals-config.h:97-100,105-113,139-144`
- Before: no wireframe mode override API or override storage.
- After: added `setPlayerWireframesEnabled`, override clear/accessors, friendly enable, team color setter/clear/effective color, and their in-memory state.

`src/config/player-visuals-config.cpp:305-328`
- Before: no gamemode wireframe override methods.
- After: added mode enable/clear and team color apply/clear/effective-color methods.

`src/network/community-match-client.cpp:420-440,726-732`
- Before: mode presentation did not apply or fingerprint wireframe fields.
- After: applies/clears wireframe enable/color overrides and records `player_wireframes`, `friendly_wireframes`, and both colors in the presentation fingerprint.

`src/render/render-player.cpp:354-356,452-462`
- Before: wireframe draw required only the global `wireframe->enabled` and was suppressed under a mode presentation override.
- After: known-team actors can be drawn by an explicit mode wireframe policy, using the mode's effective team color.

`docs/specs/graphics/graphics-presentation.md:52-54`
- Before: no mode wireframe keys documented.
- After: added `player_wireframes`, `friendly_wireframes`, and friendly/enemy wireframe color entries.

`docs/specs/gamemodes/teamdeathmatch.md:25-31`
- Before: no TDM team spawn or visual contract.
- After: added separate spawn groups and health bar/team wireframe defaults.

## Instructions and evidence

Read `docs/ROUTER.md`, TDM/gamemode/networking and actor-preset specifications, graphics presentation specification, JSON configuration architecture, runtime scenario workflow, build and task completion procedures, and the spec behavior, documentation, and logging review skills. `docs/specs/gui/gui.md` routed by the router is absent; `docs/specs/gui/guiv2.md` exists. `docs/regressions/regressions-v1.md` is absent; `docs/regressions/README.md` is the available regression index. Existing spawn and client presentation diagnostics were reused; presentation fingerprint includes the new mode settings.

- Spec behavior review: PASS_WITH_HUMAN_REVIEW. JSON drives spawn and presentation policy; live map spawn availability and visual acceptance remain unverified.
- Documentation checker: PASS for focused additions to the TDM and graphics specifications; existing repository TODOs are listed with paths, lines, quotes, and suggested replacements below.
- Logging checker: PASS; no per-frame diagnostic was added, and mode presentation change fields are included in the existing profile fingerprint event.
- `python -m json.tool config/gamemodes/tdm.json`: PASS.
- `git diff --check` on the edited files: PASS (Git reports only line-ending conversion notices).
- Canonical Dev Loop build: PASS, build 2985, source generation 11.
- Executable identity: `C:\mimita-v9\.dev\builds\2985\mimita.exe --versioninfo`; `EVENTS_JSONL_PATH=logs/10-23-2026/20261023_223701/events.jsonl`, run ID `20261023_223701`. Its `versioninfo.executed` event proves identity only.
- Runtime and human acceptance: NOT PERFORMED. The active Dev Loop session remains Payload on build 2978, map `payload1`; it was left running. The new executable was not launched into TDM. TDM spawn distribution/respawns, healthbar visibility, and wireframe appearance still need a fresh TDM run and human visual confirmation.
- Pre-existing/unrelated state was preserved. A `config/playervisuals.json` diff appeared during this work; it changes global outline alpha and was not edited as part of this task. Other user-owned modifications/untracked assets remain untouched.

## Documentation TODO scan

The repository-wide documentation scan found existing TODO notes outside the changed specifications; they were not edited as part of this TDM change:

- `docs/architecture/vision-mentality/vision.md:1`: `2026 10 21 1546 jorj todo explain hmmmmmm` — replace with the intended project vision and its concrete implications for game design.
- `docs/features/TEMPLATE.md:1`: `9 6 2026 1539 -  todo put a template taht is human understandable for how to do feature things` — provide a plain-language feature record template with required fields and one example.
- `docs/architecture/vision-mentality/mapping.md:2`: `jorj todo eplxain map editor conenciton` — explain how the map editor feeds authored map data into runtime consumers.
- `docs/architecture/ecs-entity-etc/ecs.md:1-2`: `9 8 2026 1124 est jorj - todo` / `explain ecs` — add a concise ECS definition and its role in this repository.
- `docs/specs/website/website.md:1`: `2026 10 17 1206 jorj todo database make stotnger` — define database persistence, backup, and recovery expectations.
- `docs/architecture/player-npc-systems/npc-movement.md:1010`: `2026 10 06 1412 jorj todo actor vs actor collision isnt reall working yet so we need to fix that also networking representaiton of hybrid movement mode and ragdoll is choppy so we need to ensure that its smooth as well` — split this into tracked collision and networked presentation issues with reproduction evidence and acceptance criteria.

No TODO note was found in the two edited specifications (`teamdeathmatch.md` and `graphics-presentation.md`).
