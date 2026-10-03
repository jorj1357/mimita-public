# Counter-Strike fix pass #2: spectator team, actorState, selftest alignment, bomb pulse

Date: 2026-10-03
EST timestamp: 2026-10-03 15:17:06 EST
Branch: `afad20a-rebuild`

## Result

`PASS_WITH_HUMAN_REVIEW`

Source, build, and pure-rule evidence are proven. The bomb pulse sphere is a new
config-driven client render; live gameplay/visual acceptance is NOT performed and
remains required.

## Scope

Continuation of the Counter-Strike playtest-fix work on `afad20a-rebuild`. This
session completed the remaining fix-pass-#2 items:

- `CommunityMatchClient::actorState(actorId)` (referenced by the TAB leaderboard
  render but never declared/defined in the prior pass).
- Spectator as a real team; stale `team_list` alias removed.
- Countdown freeze alignment and TAB leaderboard JSON columns (already in the
  working tree from the interrupted pass; reconciled here).
- Selftest alignment for the new 3-team model (CT, T, Spectator).
- New config-driven objective pulse sphere visual.

This is the single changelog for this session.

## Regression fixed this session

Adding the built-in `Spectator` team to `config/gamemodes/counterstrike.json`
(3 ordered teams) broke two selftests that asserted exactly 2 teams:

- `--cs-round-selftest`: `FAIL: expected 2 ordered teams`
- `--gamemode-selftest`: `FAIL` (teamsOk size() == 2)

Both now assert the 3-team model: playing teams at indices 0/1 with capacity 5,
and the Spectator team at index 2 with capacity 0. The runtime already treats
only teams 0/1 as playing teams (`roundRosterNpcCounts`, `buildObjectiveRoster`,
`chooseFallbackTeam` all iterate `team < 2`), so no gameplay change was needed.

## Files changed

### Match client identity
- `src/network/community-match-client.h`: declared `uint8_t actorState(uint32_t)`.
- `src/network/community-match-client.cpp`: defined `actorState`; `localActorState`
  now delegates to it (single lookup owner). TAB render uses `actorState`.

### Selftest alignment (3-team model)
- `src/network/server-gamemode.cpp`: `serverCounterStrikeRoundSelfTest` now expects
  3 ordered teams and asserts Spectator index 2 capacity 0.
- `src/game/game-cli.cpp`: `--gamemode-selftest` `teamsOk` checks CT/T/Spectator;
  added an objective-visual parse assertion.

### Bomb pulse sphere (new, config-driven)
- `config/gamemodes/counterstrike.json`: bomb objective gained a `visual` block
  (`pulse`, `radius`, `pulse_amplitude`, `period_seconds`, `color`).
- `src/gamemode/gamemode.h`: added `GamemodeObjectiveVisual`; embedded in
  `GamemodeObjectiveDefinition`.
- `src/gamemode/gamemode.cpp`: parses the objective `visual` block.
- `src/engine/engine-tick-render.cpp`: added `renderObjectivePulse` and wired it
  into the effects pass. It reads the replicated objective position/state from
  `CommunityMatchClient` and the visual policy from the active gamemode JSON, then
  draws a depth-tested filled sphere via `DebugVis::drawFilledSphere` (flushed by
  `DebugVis::flushTris`). Renders only while the bomb is Carried/Dropped/Planted;
  Planted shifts to a hot warning tint. Cosmetic only; never decides state.

## Validation

Build: `BUILD SUCCESS` (`python build.py build-only`).

Runtime:

```text
[ACCEPTANCE] round+weapons: PASS
[ACCEPTANCE] spawn-tags   : PASS
[ACCEPTANCE] objective    : PASS
[ACCEPTANCE] map-config   : PASS
[ACCEPTANCE] grenade      : PASS
[ACCEPTANCE] area-effect  : PASS
[ACCEPTANCE] team-brain   : PASS
[ACCEPTANCE] npc-perception: PASS
[ACCEPTANCE] npc-utility  : PASS
[ACCEPTANCE] npc-grenade  : PASS
[ACCEPTANCE] npc-nav-request: PASS
[ACCEPTANCE] PASS
```

All 13 individual selftests PASS, including:

```text
[GAMEMODE SELFTEST] objective_visual enabled=1 radius=0.35 amp=0.15 period=1.50
[GAMEMODE SELFTEST] PASS
```

## Evidence separation

- Source: this file + the diffs above.
- Build: `BUILD SUCCESS`; `mimita.exe` relinked.
- Runtime (rules/data): acceptance + 13 individual selftests PASS.
- Runtime (visual): objective-visual parse proven; the actual pulse sphere was
  NOT observed by a human.
- Human acceptance: pending.

## Known limitations / follow-ups

- Per-team human avatar is still preset-driven; NPC avatars are per-team.
- `config/npc-difficulty.json` still declares `targetMode: "player"` /
  `damageOtherNpcs: false`; safe due to the team gate but intent should be cleaned.
- Bomb sites in `config/maps/dust2cyberiav4.json` remain unauthored; verify with
  `site_debug`.
- The pulse sphere period uses client wall-clock (`MimitaNet::nowMs`) for phase;
  cosmetic only.
- Pre-existing unrelated working-tree changes were NOT touched and are NOT claimed
  here: `config/weapons.json` `beam_thickness` (2.0 -> 0.01), weapon-box debug
  visuals, collision/tunneling work, and runtime-written user settings.

## Human review still needed

- Confirm the bomb pulse sphere appears at the bomb when carried/dropped/planted
  and shifts tint when planted; tune via the JSON `visual` block.
- Confirm TAB leaderboard team tags + ALIVE/DEAD/SPECT state render.
- Confirm death -> freecam until round end, then revive at the team spawn.
