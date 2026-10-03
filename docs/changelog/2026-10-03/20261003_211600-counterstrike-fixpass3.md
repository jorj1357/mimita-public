# Counter-Strike fix pass #3: spawns, freeze, patrol, map, body parts, win text, cursor, outlines

Date: 2026-10-03
EST timestamp: 2026-10-03 17:16:00 ET
Branch: `afad20a-rebuild`

## Result

`PASS_WITH_HUMAN_REVIEW`

Source, build, and pure-rule evidence are proven. No live gameplay or visual
acceptance was performed; the 10-item human checklist in
`docs/specs/20261003-counterstrike-fixpass3-handoff.md` remains required.

## Scope

Continuation of the Counter-Strike playtest-fix work. The previous session
root-caused 11 human-reported bugs, implemented some fixes, and left the handoff
`docs/specs/20261003-counterstrike-fixpass3-handoff.md`. This session finished the
remaining items:

1. Remove `allowed_body_parts` (issue 6).
2. Replace remaining `dust2cyberiav3` map defaults with `dust2cyberiav4` and fix
   the stale header comment (issue 4).
3. Room joins prefer the requested map; dev-loop restarts the server when the
   launch-mode map changes (issue 4).
4. Send the per-round winner team (issue 8).
5. Exclude `PauseMenu::isOpen()` from the forced cursor (issue 10b).
6. Gamemode player-outline override (issue 11).
7. Optional `UtilityGoalKind::Patrol` diagnostics polish.

Issue 1 (intermission) was explicitly skipped by the human; issue 7 (round win
rules) required no code change. This is the single changelog for this session.

## Pre-existing edits (NOT mine, NOT touched or claimed)

The working tree already contained unrelated uncommitted edits from earlier
sessions: weapon `beam_thickness`, weapon-box debug visuals, collision/tunneling
work, `config/weapons.json`, and runtime-written user settings. In particular,
`config/actor-presets/counter_strike.json` already had revolver tuning
(`fire_delay` 0.3, `reload_time` 1.5, `recoil` 30.0) before this session. Those
values were preserved.

## Files changed

### Body parts (issue 6)

`config/actor-presets/counter_strike.json` — removed the revolver damage policy:

```jsonc
// old
      "hitscan": { "enabled": true, "beam_thickness": 0.0, "world_thickness": 0.0, "range": 1000.0 },
      "damage_policy": {
        "allowed_body_parts": ["head"]
      }
// new
      "hitscan": { "enabled": true, "beam_thickness": 0.0, "world_thickness": 0.0, "range": 1000.0 }
```

`weapon-execution.cpp` already returns 0 for disallowed parts only when the set
is non-empty (`if (!def.allowedBodyParts.empty() && ...)`), so no code change was
needed. The rifle `headshot_multiplier: 4.0` still applies to head hits.

### Map identity (issue 4)

- `src/main-systems.cpp` (`defaultMapPath`): `assets/maps/dust2cyberiav3.glb` →
  `assets/maps/dust2cyberiav4.glb`.
- `src/engine/engine-tick-state.cpp` (`defaultMapPath`): same v3 → v4.
- `src/engine/engine-tick-state.cpp` (connect/room-join map pick):

```cpp
// old
std::string connectMap = defaultMapPath;
if (!mci.directAddress.empty() && !mci.mapName.empty())
    connectMap = "assets/maps/" + mci.mapName + ".glb";
// new
std::string connectMap = defaultMapPath;
// Prefer the map the launch/join requested even without a direct address
// (dev-loop room joins), instead of the hardcoded fallback, until the server
// Welcome arrives.
if (!mci.mapName.empty())
    connectMap = "assets/maps/" + mci.mapName + ".glb";
```

- `src/gamemode/map-config.cpp` (`mapConfigSelfTest`): `reg.load("dust2cyberiav3")`
  → `reg.load("dust2cyberiav4")` so the selftest loads the active CS map.
- `config/maps/dust2cyberiav4.json` header comment: `dust2cyberiav3` → `dust2cyberiav4`.
- `devscripts/dev-loop.py`: added `server_map_name()` (reads the running server's
  `--map`) and `stop_server()` (terminates only the external server), and
  `launch_latest()` now restarts the server when the launch-mode map differs
  instead of reusing the old map for the whole session. `python -m py_compile`
  passes.

### Per-round winner (issue 8)

`src/network/server-gamemode.cpp` (`buildDuelStatePacket`):

```cpp
// old
pkt.winnerTeam = d.winnerTeam;
// new
// Result screens: a normal round sends the round winner; only the match-over
// screen uses the overall match winner. d.winnerTeam stays -1 until then.
pkt.winnerTeam = d.matchOver ? d.winnerTeam : d.roundWinnerTeam;
```

The client (`engine-tick-ui-overlays.cpp`) already chooses "win the round" vs
"win the match" from `match.matchOver()` and resolves the team name, so no client
change was needed.

### Cursor (issue 10b)

`src/engine/engine-tick-state.cpp` — both forced-cursor sites now exclude the
pause menu:

```cpp
// old
gameState == GAME_PLAYING && !Terminal::instance().isOpen() && !isChatOpen() && !duelMatchOver && MouseLock::locked()
// new
gameState == GAME_PLAYING && !Terminal::instance().isOpen() && !isChatOpen() && !PauseMenu::isOpen() && !duelMatchOver && MouseLock::locked()
```

`PauseMenu::toggle` already sets `GLFW_CURSOR_NORMAL` on open and disables the
keyboard, so the ESC menu shows a cursor and the `L` handler (which already
requires `!PauseMenu::isOpen()`) is not re-locked by a state/phase change while
the menu is open.

### Player outlines (issue 11)

- `src/gamemode/gamemode.h`: `GamemodePresentation` gained
  `bool hasPlayerOutlines = false; bool playerOutlines = true;`.
- `src/gamemode/gamemode.cpp`: parses
  `readFlag("player_outlines", next.presentation.hasPlayerOutlines, next.presentation.playerOutlines);`.
- `src/config/player-visuals-config.h/.cpp`: added
  `setPlayerOutlinesEnabled`, `clearPlayerOutlinesOverride`,
  `hasPlayerOutlinesOverride`, `playerOutlinesEnabled`, and `effectiveMode()`.
  The override is in-memory only and never writes `config/playervisuals.json`;
  configured modes are preserved.
- `src/render/render-player.cpp`: resolves the effective mode through
  `PlayerVisualsConfig::instance().effectiveMode(...)` instead of reading the raw
  `selfMode`/`enemyMode`/`teammateMode`.
- `src/network/community-match-client.cpp`: includes `config/player-visuals-config.h`,
  applies/clears the override in `onState` from
  `modeConfig.presentation.hasPlayerOutlines`, and clears it in `reset()`.
- `config/gamemodes/counterstrike.json`: `presentation` gained
  `"player_outlines": false`.

### Patrol diagnostics polish

- `src/npc/npc-utility.h`: added `UtilityGoalKind::Patrol`.
- `src/npc/npc-utility.cpp`: `utilityGoalName` returns `"Patrol"`;
  `scoreUtilityGoal` scores Patrol only when no hostile is known/visible;
  `actionForGoal` maps Patrol to `Reposition`; `selectUtilityGoal` reclassifies
  the target-less `KillTarget` fallback as Patrol when there is also no objective
  context. `makeNavGoal` `switch` has a `default`, so Patrol falls through to the
  same legacy state mapping the target-less fallback already used — navigation
  behavior is unchanged. The utility selftest still passes (Survive at low health
  is not reclassified).

### Selftest alignment (pre-existing mismatch)

`src/game/game-cli.cpp` `--actor-preset-selftest`:

```cpp
// old
revolver->hasFireDelay && revolver->fireDelay == 0.8f &&
revolver->hasReloadTime && revolver->reloadTime == 2.2f &&
// new
revolver->hasFireDelay && revolver->fireDelay == 0.3f &&
revolver->hasReloadTime && revolver->reloadTime == 1.5f &&
```

This was already failing before this session because the working-tree actor
preset was retuned while the test at HEAD still expected the old values. The
assertion now matches the current preset.

## Documents and skills

- `AGENTS.md`, `docs/ROUTER.md`.
- `docs/specs/20261003-counterstrike-fixpass3-handoff.md` (the requirements).
- `docs/specs/gamemodes/gamemodes.md` (general gamemode runtime).
- `docs/operations/task-completion/task-completion.md`.
- `docs/architecture/time-and-formatting/time-and-formatting.md`.
- `docs/features/gamemodes/counterstrike.md` (Attempt 13 appended).
- `docs/skills/spec-behavior-review-v1.md` — Result: `PASS`. No spec-code
  disagreement that required a code change. Unresolved warning: the handoff
  leaves `freeze_seconds` as unused config and lists `config/gamemode-good-maps.json`
  containing both `dust2cyberiav3` and `dust2cyberiav4`; neither was changed.
- `docs/regressions/README.md` (no new regression file; the actor-preset selftest
  mismatch is a stale test, not a new behavior regression).

## Validation

Build (after deleting the four affected objects to defeat the mtime-skip quirk):

```text
[CXX ] src\network\community-match-client.cpp
[CXX ] src\network\server-gamemode.cpp
[CXX ] src\npc\npc-utility.cpp
[CXX ] src\render\render-player.cpp
[CXX ] src\game\game-cli.cpp
 BUILD SUCCESS
```

Runtime (rules/data) — `--counterstrike-acceptance-selftest` PASS (all 11
groups) plus all 13 individual selftests PASS, including:

```text
--cs-round-selftest                 PASS
--gamemode-selftest                 PASS
--spawn-tag-selftest                PASS
--objective-selftest                PASS
--map-config-selftest               PASS
--actor-preset-selftest             PASS
--grenade-selftest                  PASS
--area-effect-selftest              PASS
--team-brain-selftest               PASS
--npc-perception-selftest           PASS
--npc-utility-selftest              PASS
--grenade-reasoning-selftest        PASS
--npc-nav-request-selftest          PASS
TOTAL FAILURES: 0
```

## Evidence separation

- Source: this file + the diffs above.
- Build: `BUILD SUCCESS`; `mimita.exe` relinked at 2026-10-03 17:13:58.
- Runtime (rules/data): acceptance + 13 individual selftests PASS.
- Runtime (visual/gameplay): NOT performed. Map selection, cursor, outlines,
  win text, patrol movement, spawn positions, and freeze gating were not
  observed live.
- Human acceptance: pending.

## Known limitations / follow-ups

- `config/gamemode-good-maps.json` still lists `dust2cyberiav3` next to
  `dust2cyberiav4`, so automatic rotation can still pick v3.
- Patrol is compile/test-verified only; runtime wall-hugging or stuck loops are
  unobserved.
- `freeze_seconds` remains parsed but unused; the human wants NPCs to act at GO.
- Intermission duration (issue 1) was explicitly skipped by the human.
- No commit was made; nothing was pushed or deployed.

## Human review still needed

Use the checklist in
`docs/specs/20261003-counterstrike-fixpass3-handoff.md` (section 5):

1. Map is `dust2cyberiav4`, not v3.
2. No actor spawns at the opposite team spawn, including after deaths.
3. NPCs stand still during 3-2-1 and act at GO.
4. NPCs patrol forward, re-route on walls, do not circle or retrace.
5. Cross-team NPCs fight; same-team never fires.
6. Body and limb shots deal damage.
7. Round outcomes (timeout → CT, explode → T, defuse → CT, wipe → opposing).
8. "win the round" shows the winning team name.
9. ESC opens a menu with a visible cursor; `L` unlocks while spectating.
10. No white player outline in Counter-Strike.
