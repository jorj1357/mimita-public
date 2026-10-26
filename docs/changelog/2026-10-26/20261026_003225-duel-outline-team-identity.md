# Duel enemy outline, trail visibility, and shared weapon set

- Result: source and build PASS; enemy outline submission, trail GPU batch submission, and NPC weapon inventory are evidenced in runtime. Human visual/gameplay acceptance remains pending.
- Started: 2026-10-26T00:32:25Z. Updated: 2026-10-26T00:49:56Z.
- Branch: `20261011zt`.
- HEAD observed: `3f55d889`.
- Latest build: 3363 succeeded; SHA-256 `b93e23bf41620aec503d0073b1958be2e38cc162233df5bea59f10fc9b2384c4`.
- Dev-loop status: running, build 3363, source generation 5, launch mode `duel npc elite`, map `xxity2`; server and client ready with matching build identities.
- Runtime journal: `logs/2026-10-26/20261026_004037_01/events.jsonl`.
- Exact build 3363 `--versioninfo` journal: `logs/10-25-2026/20261025_204718/events.jsonl`.

## Duel enemy outline

The user's report was confirmed in the earlier `20261026_001810` journal. The Duel presentation profile loaded outlines enabled, enemy red, and alpha `1.0`, but the renderer skipped the enemy because both team IDs were unknown (`-1`).

`CommunityMatchClient::active()` excludes Duel. In `src/engine/engine-tick-render.cpp`, that predicate incorrectly guarded local and remote replicated team lookups. Removed those guards for local players, remote players, and remote NPCs. When a replicated team is available it now supplies the outline identity; otherwise the existing actor team remains the fallback.

The later `player_visuals.outline` event (sequence 1417) reports actor 200000 team 0, local team 1, `team_identity_known=true`, `mode_forced=true`, and `drawn=true`, confirming the enemy outline submission. Human visual appearance still needs review in the game window.

## Trail camera visibility

The user reported that trails were visible outside the view but missing inside it. The camera-facing ribbon used a cross-product order that faced away from the camera, and the batch renderer inherited face-culling state. Reversed the ribbon side-vector order, disabled `GL_CULL_FACE` during trail submission, and restored its previous state afterward. The actor event now calls geometry `batched`; a separate bounded event logs only after `glDrawArrays` submits the batch.

The first submission event used the Rendering category, which `config/debuglogger.json` disables. Changed it to the enabled General category. The live journal contains repeated `player_visuals.trail.batch` events from client PID 30292, including 00:43:50.711Z with `draw_submitted=true`, `face_culling=false`, and 72 vertices. This proves the draw submission ran. The current 3363 client has not emitted a trail batch event, and no human visual review has been performed; appearance acceptance remains pending.

## Shared Duel weapon set

Duel already declares `weapon_set_id: 1` as its standard Stable weapon set, but its team entries used generic roles `elite_fighter` and `elite_npc`, which define different role-level loadout policies. Updated both teams to use the existing Duel-specific RED/BLUE player and NPC roles. All four roles share the `duel_standard` preset and omit a weapon-set override, leaving mode set 1 as the shared source.

NPC spawn profile resolution previously left the generic NPC inventory to be filtered when no role/preset weapon set was specified, which could omit weapons from the mode set. `serverResolveActorSpawnProfile` now resolves the active mode weapon set into the NPC profile when no explicit role/preset set exists; the existing NPC loadout owner then applies it. The explicit-team profile event also records the resolved set and weapon IDs.

The live Duel server uses `--weapon-set 1`. Its `npc.grenade-availability` event for actor 1000/team 0 at 00:47:41.118Z lists all ten Stable set 1 weapons: revolver, shotgun, rocket_launcher, spyknife, force_punch, projectile_rifle, hitscan_rifle, big_shotgun, large_machine_gun, and nothing. Config/source and the runtime NPC inventory align. Human gameplay acceptance remains pending.

## Files changed in this session

- `config/gamemodes/duel.json`: Duel player/NPC roles now use the authored RED/BLUE role pairs; the existing red enemy wireframe color remains set to `[255, 1, 1]`.
- `src/engine/engine-tick-render.cpp`: use replicated team identity for Duel outline selection.
- `src/render/render-player.cpp`: reverse trail ribbon orientation, disable and restore face culling around the batch, and emit bounded trail submission diagnostics. Existing actor draw/render diagnostics use the enabled General category.
- `src/network/server-gamemode.cpp`: inherit the active mode set for NPC spawn profiles without an explicit set and include resolved weapon data in the roster-profile event.
- `docs/regressions/2026-10-26/duel-outline-team-identity-REG.md`
- `docs/regressions/2026-10-26/player-trail-camera-visibility-REG.md`
- `docs/regressions/2026-10-26/duel-npc-shared-weapon-set-REG.md`

Pre-existing dirty files were preserved: `config/accounts/default.json`, `config/analytics.json`, `config/audio/music-settings.json`, `config/gamemodes/duel.json`, `config/gamemodes/masterlist/gamemode-masterlist.json`, `config/playervisuals.json`, `config/video-settings.json`, `devscripts/config_selftest.py`, `docs/architecture/json-configuration/json-configuration.md`, `docs/changelog/2026-10-25/20261025_184028-weapon-cycle-outlines-canonical-gamemode-json.md`, and `src/gamemode/gamemode.cpp`. The Duel JSON file already had user changes before this session; those were preserved while updating the requested role/color fields.

## Review and validation

Followed `docs/ROUTER.md` routes for graphics presentation, Duel/gamemode behavior, actor presets, weapons, networking, NPC systems, JSON configuration, runtime validation, build/EXE handling, task completion, logging, and regression records. Focused reviews covered spec behavior, structured logging, and efficiency. `docs/regressions/regressions-v1.md` was absent; used `docs/regressions/README.md`.

- The canonical dev loop published build 3363 successfully; current status is build 3363 with matching client/server IDs. Earlier builds were used while iterating on the changes.
- Exact build 3363 `--versioninfo` was run and identified its events journal.
- The 3363 client/server were launched in Duel NPC Elite using the existing dev loop.
- Duel and role JSONC were parsed; all four role references exist and have no weapon-set override.
- `git diff --check` passed.
- No tests were added or run.

