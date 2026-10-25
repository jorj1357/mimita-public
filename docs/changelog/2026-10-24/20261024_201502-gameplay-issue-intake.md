# TDM run issue intake and evidence

- **Time:** 2026-10-24T20:15:02Z (2026-10-24 16:15:02 EDT)
- **Branch / HEAD:** `20261011zt` / `0472121b`
- **Result:** PASS_WITH_HUMAN_REVIEW

## Scope and findings

Recorded the TDM run identity mismatch, reported NPC-death disconnect/crash,
NPC aggression request, and position-correction report in the open gameplay
issue inbox. No gameplay source or configuration was changed.

`config/gamemodes/tdm.json` already specifies capacity 5 for RED and BLUE,
goal 30, time limit 180 seconds, intermission 10 seconds, countdown 3 seconds,
results 5 seconds, separate `red_spawn` / `blue_spawn` groups, visible health
bars, and green / red player wireframes. The requested TDM behavior is already
present in that config. The reported dev-launch profile 1 (`current`) is
configured in `devscripts/dev-launch-modes.json` to pass `--mode sandbox`,
`--gamemode sandbox`, and `--no-npcs`; the journal's final server arguments
include these after `--mode melee`, and the server starts as Sandbox. The
status showed runtime build 3091 while 3093 was latest. This run therefore did
not exercise TDM or its NPC roster/presentation.

Source inspection found `src/network/server-gamemode.cpp` already increments
the TDM team score for a confirmed opposing-team kill and emits
`tdm.team-score.updated`; the shared team spawn-group selection uses each
team's configured `spawn_group`. These source paths were inspected, not changed
or runtime-validated in this Sandbox session.

The supplied journal records a client `death.kill.enter` for `NPC Dev` with
`actor_type=player`, `killer=void_death`, and Z=-500.08 at
2026-10-24T19:28:05.557Z. The launch command uses `NPC Dev` as the host player
and disables NPCs. The server continues sending snapshots through
19:28:29.006Z. Dev-loop status later reports PID 4808 alive and no client PID;
at 20:08:00.883Z the journal records a successful supervised stop. The evidence
does not establish an NPC kill or server crash.

For position correction, the journal records the dev-loop delivering
`teleport 73.639,20.781,355.618` at 19:25:51.299Z. The server correction reason
is `blocking-geometry`, with a 50.05 m correction; the client reports 48.72 m
error and then about 0.011 m on subsequent corrections. This is evidence for a
blocked teleport, not the separately reported ordinary-movement case.

## Changed files and exact content

### `docs/specs/open-gameplay-issues.md`

Before: no 2026-10-24 intake existed at the start of the edit. Added the new
intake at lines 1-57. Exact addition:

```markdown
## New intake — 2026-10-24, 20:15 UTC

Player report from the run presented as TDM mode 1 on `funworld3`:

- **TDM config and run identity:** `config/gamemodes/tdm.json` currently
  specifies five actors per team, a 30-point goal, a 180-second limit,
  10-second intermission, three-second countdown, five-second results,
  separate `red_spawn` / `blue_spawn` groups, visible health bars, and green /
  red player wireframes. This matches the requested defaults in the file.
  The supplied run does not prove those settings ran: dev launch profile 1
  (`current`) is configured in `devscripts/dev-launch-modes.json` with
  `--mode sandbox`, `--gamemode sandbox`, and `--no-npcs`. The journal also
  shows the final server arguments include those values and starts with
  `mode=sandbox`; the earlier `--mode melee` argument is superseded. Current
  dev-loop status identifies runtime build 3091 while build 3093 is latest.
  Select or configure a launch profile that uses `--gamemode tdm` and does not
  disable NPCs, then confirm the server journal says `mode=tdm` and records
  the intended executable before
  evaluating TDM phases, roster, scoring, spawns, or presentation.
- **NPC death followed by apparent server failure:** The player reports that
  killing an NPC is followed by a “last packet X seconds ago” indication and
  what looks like a server crash. In
  `logs/2026-10-24/20261024_192228/events.jsonl`, the client records
  `death.kill.enter` for `NPC Dev` at `2026-10-24T19:28:05.557Z`, with
  `actor_type=player`, `killer=void_death`, and Z = -500.08; another death
  event follows 50 ms later with an unknown killer. The launch arguments name
  the host player `NPC Dev` and include `--no-npcs`, matching the death record's
  `actor_type=player`; this does not show an NPC being killed. The server continues
  sending snapshots through `19:28:29.006Z` and logs another event at
  `19:28:30.530Z`. Dev-loop status later showed server PID 4808 alive and no
  client PID; at `20:08:00.883Z` its journal records a successful supervised
  server stop (`stopped=true`). The evidence does not show a server crash after
  the reported death. It does show the intended TDM run was logged as Sandbox,
  and the client death record is not an authoritative NPC kill record. Next,
  reproduce on the intended build and confirmed TDM mode, then correlate the
  actor's server-side death, score/respawn events, last server tick/snapshot,
  client disconnect, and process exit/stop result. Keep crash, server stall,
  and client/network disconnect as separate outcomes.
- **NPC aggression:** The player wants NPCs to engage more aggressively. This
  run has no TDM NPC behavior evidence (`npc_count=0` in the relevant server
  snapshot summaries), so it cannot identify a TDM profile or prove a tuning
  failure. First confirm the TDM role-to-behavior-profile selection and record
  target acquisition, first legal shot, accepted shots, weapon changes, and
  pursuit. Then tune the selected profile in JSON while preserving shared
  weapon cooldowns and legal attack rules.
- **Position correction around map geometry:** At
  `2026-10-24T19:25:51.299Z`, the dev-loop delivered the client command
  `teleport 73.639,20.781,355.618`. The server logged
  `movement.server-position-corrected` with reason `blocking-geometry` and a
  50.05 m correction; the client applied the authoritative position with a
  48.72 m error, then subsequent corrections were about 0.011 m. This run
  shows a correction after an explicit teleport into blocking geometry. It
  does not establish a correction during ordinary movement far from geometry.
  Reproduce the ordinary movement case separately and compare client
  prediction, server-accepted position, collision contacts, and correction
  reason.
```

Added the new `NPC death appears to stop the server or disconnect the client`
No prior text existed for this new intake section; no existing issue-table rows
were changed.

## Validation and limits

- `git diff --check -- docs/specs/open-gameplay-issues.md`: PASS.
- No tests or builds run; this session made documentation-only changes.
- No new runtime scenario run. Existing journal and dev-loop status are
  evidence only for the run and process state recorded above.
- TDM runtime phase, score, spawn separation, health bars, and wireframe colors
  still need a run that actually starts `mode=tdm` on the intended build, then
  human visual/play acceptance.
- Focused docs reviewed: `docs/skills/spec-behavior-review-v1.md`,
  `docs/skills/documentation-checker-v1.md`,
  `docs/specs/gamemodes/teamdeathmatch.md`,
  `docs/specs/gamemodes/gamemodes.md`,
  `docs/specs/npc-behavior/npc-behavior.md`,
  `docs/architecture/player-npc-systems/npc-movement.md`,
  `docs/specs/networking/networking.md`,
  `docs/workflows/runtime-scenario-validation.md`, and
  `docs/regressions/README.md`.
- `docs/ROUTER.md` references `docs/doc-review-09-03-2026.md`, which is absent
  from this checkout; the documentation checker and task-completion guide were
  reviewed instead.
- Pre-existing edits were left untouched: `config/accounts/default.json`,
  `config/analytics.json`, `config/audio/music-settings.json`,
  `config/hitfx.json`, `config/playervisuals.json`,
  `devscripts/dev-launch-modes.json`, `devscripts/dev-loop.py`,
  `docs/architecture/collision/collision.md`,
  `src/engine/engine-tick-net.cpp`, `src/engine/engine-tick-replay.cpp`,
  `src/engine/engine-tick-state.cpp`, `src/engine/engine-tick.cpp`,
  `src/engine/engine-tick.h`, `src/gui/gui-main.cpp`, `src/gui/gui-main.h`,
  `src/input/input-poll.cpp`, `src/input/input-poll.h`,
  `src/main-systems.cpp`, `src/main.cpp`,
  `src/network/multiplayer-packets.cpp`, `src/network/multiplayer-tick.cpp`,
  `src/network/net_mode.cpp`, `src/network/net_mode.h`,
  `src/network/server-browser.cpp`, `src/network/server-packets.cpp`,
  `src/network/server.cpp`, `src/physics/movement/actor-triangle-solver.cpp`,
  `src/physics/movement/physics-collision-mesh.cpp`,
  `src/physics/movement/physics-collision.h`, and the three already-present
  changelog files in `docs/changelog/2026-10-24/`.
