# Counter-Strike Checkpoint 10 — debug tooling and acceptance harness

Date: 2026-10-02
EST timestamp: 2026-10-02 23:37:46 EST
Branch: `afad20a_rebuild`

## Result

`PASS_WITH_HUMAN_REVIEW`

Source, build, and consolidated pure-rule acceptance are proven. Live
gameplay/visual/human acceptance is NOT performed and remains required. This is
the final plan checkpoint.

## Scope

Checkpoint 10 of `docs/specs/20261002plan.md`: debug/inspection tooling,
consolidated tests, and the acceptance handoff (Stages 16, 17, 18).

## Pre-existing / external edits (not mine)

Same external `config/weapons.json` `beam_thickness` change and runtime-written
user settings flagged since Checkpoint 3. Untouched.

## Files changed

### `src/terminal/debug-commands.cpp`

- `npc_inspect [id]`: team/hp/role preset/behavior preset/target/belief/
  visibility/confidence/distance/goal/action/nav destination/path nodes/
  perception FOV+LOS/reaction/objective context/goal score.
- `npc_brain [id]`: every utility goal score with its terms.
- `team_status`: mode/round/score/roster-lock/team order/team member counts.
- `objective_status`: bomb state/carrier/team/position/site/progress/counters.
- `site_debug` now also accepts `on|off` aliases.
- Includes `gamemode/gamemode.h`, `npc/npc.h`, `<cstdlib>`.

### `src/network/server-gamemode.cpp`

Structured events: `actor.team-assigned` (team change), `actor.preset-applied`
(preset activation), `round.result` and `match.result` (round/match end).

### `src/game/game-cli.cpp`

Added `--counterstrike-acceptance-selftest`, a consolidated runner over the
round+weapons, objective, map-config, grenade, area-effect, team-brain,
perception, utility, npc-grenade, and nav-request selftests.

### `docs/features/gamemodes/counterstrike.md`

Updated the status section to all-ten-checkpoints-implemented, appended Attempt
10, and added a "Handoff — human test instructions" section with the exact
commands and checks for the remaining human acceptance.

## Reasoning

The plan requires a debug/inspection surface and a consolidated acceptance
gate. Commands read the authoritative in-process state and add no new gameplay
paths; events go through the existing `StructuredLogger`; the acceptance runner
reuses the existing pure selftests so there is no duplicate test logic.

## Documents and skills

- Spec: `docs/specs/20261002plan.md` (Stages 16, 17, 18; Checkpoint 10).
- Skill: `docs/skills/spec-behavior-review-v1.md` — no blocker findings.
- Skill: `docs/skills/terminal-command-checker-v1.md` — commands registered with
  usage/description/category and clear failure messages.
- Skill: `docs/skills/logging-checker-v1.md` — events via the single logger;
  no new diagnostic files.

## Validation

Build: `BUILD SUCCESS`, no warnings.

Runtime (`mimita.exe`):

```text
[ACCEPTANCE] round+weapons: PASS
[ACCEPTANCE] objective       : PASS
[ACCEPTANCE] map-config      : PASS
[ACCEPTANCE] grenade         : PASS
[ACCEPTANCE] area-effect     : PASS
[ACCEPTANCE] team-brain      : PASS
[ACCEPTANCE] npc-perception  : PASS
[ACCEPTANCE] npc-utility     : PASS
[ACCEPTANCE] npc-grenade     : PASS
[ACCEPTANCE] npc-nav-request : PASS
[ACCEPTANCE] PASS
```

All twelve individual selftests also PASS. One transient exe-lock link failure
occurred; a retry linked successfully.

## Known limitations / follow-ups

- `npc.perception`/`npc.goal-selected`/`npc.navigation` per-tick events are not
  emitted from the NPC brain path (reserved names; server-side `actor.*`,
  `round.*`, `match.*`, `objective.*`, `area_effect.*` exist).
- Debug commands read the in-process (host) server state.
- The consolidated selftest proves rules/data, not visuals/multiplayer/human
  acceptance.

## Human review still needed

See the "Handoff — human test instructions" section in
`docs/features/gamemodes/counterstrike.md` for the exact commands and the
acceptance checklist (teams, roster, FOV, movement, loadout, presentation,
bomb pickup/plant/defuse/explosion, round/match scoring, grenades, NPC
objective play).
