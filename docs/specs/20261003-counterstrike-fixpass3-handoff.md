# Counter-Strike fix pass #3 — HANDOFF

Date: 2026-10-03
Branch: `afad20a-rebuild`
Status: **IN PROGRESS** — build currently passes; several fixes unimplemented.
Author of this handoff: previous agent session (interrupted mid-task).

Read `AGENTS.md` and `docs/ROUTER.md` first. This document does not replace them.

---

## 1. Mission

Fix a batch of human playtest bugs in the `counterstrike` gamemode on
`afad20a-rebuild`. The human reported 11 issues; the agent root-caused all of
them and began implementing. This handoff records exactly what is done, what is
not, and how to finish safely.

Human decisions made before implementation (do not re-litigate):

- Sequence: fix all issues, then one build + one selftest run, then one changelog.
- Intermission: "ignore that just leave it" (no change required).
- NPC patrol: make it work with **no map knowledge** — walk forward until a wall,
  then pick a new forward route, avoiding recently visited positions (snapshot
  roughly once per second of position, avoid repeating). No bomb sites or enemy
  location knowledge needed.
- Patrol direction source: option (c) — straight forward until wall, then
  re-route. Not enemy-spawn-directed, not map-center-directed.
- Body parts: remove `allowed_body_parts` entirely (all parts allowed).
- Player outlines: gamemode JSON field + in-memory override (same pattern as
  ragdoll/blood overrides).

---

## 2. Root causes (evidence-backed)

| # | Issue | Root cause (file:line at time of investigation) |
|---|---|---|
| 1 | Intermission too long | `counterstrike.json` `rounds.intermission_seconds:30`; applied `server-gamemode.cpp` ~594/650. Not hot-reloaded for CS (reload path gates on `npcWaves`). **Human said ignore.** |
| 2/9 | Team spawns collapse to CT spawn | Kill handler overwrote victim anchor with `gamemodeSpawnPoint(d)` (team −1 ⇒ `d.spawnA`=CT) at `server-gamemode.cpp` ~3130/3138. Respawn consumed it. |
| 3 | NPCs attack before GO | `beginObjectiveRound` armed `wakeupTimer` on `npcSystem.all()` before fresh roster bodies were adopted; adopted bodies got only `spawnActionDelayTicks=1` (`server-npcs.cpp` ~230). `freeze_seconds` was dead config. `simulateSharedNpcs` had no freeze gate. |
| 4 | v3 map loaded via devloop | Client room-join fallback hardcoded `dust2cyberiav3.glb` and honored `--map` only for `--connect` (`engine-tick-state.cpp` ~84/239; `main-systems.cpp` ~347). Dev-loop reused an old server without comparing map (`dev-loop.py` ~637). |
| 5/10 | NPCs circle in spawn | Bomb sites have no `position` ⇒ `objectiveKnown=false` ⇒ objective goals score 0 ⇒ `KillTarget`(0.25) ⇒ no nav goal ⇒ state machine `Idle`/`RandomWalk`. `Advance` existed but was target-gated. |
| 6 | Body-part restriction | `counter_strike.json` `damage_policy.allowed_body_parts:["head"]` ⇒ `weapon-execution.cpp` ~134-136 returns 0 for non-head hits. |
| 7 | Round win rules | **Already correct** (`checkObjectiveRoundEnd`, `serverObjectiveTick`). No change expected; verify only. |
| 8 | "win the round" shows no team | Packet sent `d.winnerTeam` (only set at match-over) but per-round winner is `d.roundWinnerTeam`. Client `winnerTeam()` stale ⇒ `teamName()` empty. |
| 10b | ESC/L cursor | `engine-tick-state.cpp` ~344-347 forced cursor DISABLED from `MouseLock::locked()` and excluded Terminal/chat but **not `PauseMenu::isOpen()`**. Forced-spectator freecam re-locked. |
| 11 | White outline box | `config/playervisuals.json` enemy `mode:"outline"`; no gamemode override existed. |

---

## 3. Work completed (DO NOT REDO)

All of the following are in the working tree and **compile** (`BUILD SUCCESS`,
`mimita.exe` linked at 2026-10-03 16:46). They have **NOT** been runtime-tested
yet and there is **NO** changelog yet.

### A. Spawn fix (issues 2/9) — DONE
`src/network/server-gamemode.cpp`:
- Kill handler (~line 3127+): victim player anchor now uses
  `gamemodeSpawnPoint(d, victimTeam)` where `victimTeam` comes from `d.matchTeams`;
  same for the NPC branch.
- `reloadGamemodeMap` NPC loop: `gamemodeSpawnPoint(d, nTeam)`; player loop:
  `gamemodeSpawnPoint(d, pTeam)`.
- `rotateToNextGamemodeMap` NPC loop: `gamemodeSpawnPoint(d, nTeam)`.
- `serverRespawnAllActors` player loop: `gamemodeSpawnPoint(d, pTeam)`.

### B. Freeze/GO gating (issue 3) — DONE
- `src/network/server-npcs.cpp` `finalizeServerNpcSpawn` (~228): if
  `d.objectiveRounds && d.roundCountdownFreeze && !d.warmup`, wakeup is set to
  the remaining countdown time (not `spawnActionDelayTicks`). This catches NPCs
  adopted after `beginObjectiveRound` ran.
- `src/network/server-npcs.cpp` `simulateSharedNpcs` (~786): added
  `freezeRoundCountdown` gate that skips target selection during the freeze.
- `src/network/server-gamemode.cpp` `beginObjectiveRound` (~1487): existing
  bodies held for `d.countdownSeconds` (note: `freezeSeconds` deliberately NOT
  added — human wants NPCs to start at GO, and `freeze_seconds` remains unused
  dead config; consider removing it later).

### C. NPC patrol (issues 5/10) — DONE (compile only)
- `src/npc/npc-state-machine.h`: added `NpcState::Patrol` and patrol fields to
  `NpcStateMachine` (`patrolDir`, `patrolRepathTimer`, `patrolRecent` ring
  `PATROL_RECENT_MAX=8`, `patrolRecentCount/Head`, `patrolSnapshotTimer`).
- `src/npc/npc-state-machine.cpp`:
  - `npcStateName`, `stateMinTime` (0.5), `stateMaxTime` (3.0*base), and
    `scoreState` (returns 0; chosen directly) handle `Patrol`.
  - No-target branch in `pickNextState` now returns `Patrol` instead of
    `Idle`/`RandomWalk`; the stale `(0,0,0)` last-known guard now requires
    `npc.targetMemory.hasMemory` before chasing.
- `src/npc/npc-states.cpp`: `NpcState::Patrol` case steers along `patrolDir`,
  snapshots position once/second into the ring.
- `src/npc/npc.cpp`:
  - New static helper `updatePatrolHeading(npc, world, nearCandidates, dt)`:
    samples 16 headings, rejects wall-blocked ones via
    `NpcNavigation::obstacleInDirection`, scores by recency novelty + mild
    continuity, refreshes every 2–4 s or when the heading becomes blocked.
  - Called in `updateOneNpc` right after `nearCandidates` is gathered, only when
    `currentState == Patrol && !hasTarget`.
  - `makeNavGoal` adds a `Patrol` case producing `ReachPosition` 12 m forward
    along `patrolDir` so the navigator routes around walls.
  - Added `<limits>` include.

**Not done for patrol:** no `UtilityGoalKind::Patrol` was added. The utility
brain still reports `KillTarget` when no target, so `npc_brain` diagnostics will
look inconsistent with the actual `PATROL` state. Optional polish; the executor
(state machine) is what drives behavior.

### D. Map (issue 4) — PARTIALLY DONE
- `src/engine/engine-tick-state.cpp` ~84: hardcoded client fallback changed from
  `dust2cyberiav3.glb` to `dust2cyberiav4.glb`. **This edit was just applied.**
- **STILL TODO:** the fallback still only honors `mci.mapName` when
  `directAddress` is non-empty (line ~240), so devloop `--room` joins still use
  the hardcoded default until the server Welcome/JoinAccept map arrives. Decide
  whether to also prefer `mci.mapName` for room joins.
- **STILL TODO:** `src/main-systems.cpp` ~347 still hardcodes
  `dust2cyberiav3.glb` — change to v4 for consistency.
- **STILL TODO:** `devscripts/dev-loop.py` ~637 server reuse does not compare the
  resolved `map_name`; an old v3 server is reused. Add a map comparison so the
  server restarts when the launch-mode map differs.
- **STILL TODO:** `config/maps/dust2cyberiav4.json` line 1 comment says
  "dust2cyberiav3" — fix.

### E. Body parts (issue 6) — NOT DONE
`config/actor-presets/counter_strike.json`: still has
`"damage_policy": { "allowed_body_parts": ["head"] }` at lines 46-48. Remove the
whole `damage_policy` block. Headshot multiplier still applies to head hits.

### F. Win text (issue 8) — NOT DONE
- `src/network/server-gamemode.cpp` ~778 sends `pkt.winnerTeam = d.winnerTeam`.
  Per-round winner is `d.roundWinnerTeam` (set in `endObjectiveRound`).
- Client builds `winner + " win the round"` in
  `engine-tick-ui-overlays.cpp` ~775-779. Once the winner is replicated this will
  work, but verify `match.teamName()` resolves on the client.
- Suggested minimal fix: during `DUEL_PHASE_RESULTS`, send `d.roundWinnerTeam`
  (and for match over send `d.winnerTeam`).

### G. Cursor (issue 10b) — NOT DONE
- `src/engine/engine-tick-state.cpp` ~344 and ~376: add `&& !PauseMenu::isOpen()`
  to the DISABLED condition so the ESC menu shows a cursor.
- Ensure the forced-spectator freecam does not re-lock when the user pressed L.
- Confirm `InputCommandSystem::isKeyboardEnabled()` is true while dead/spectating
  so the L handler in `engine-tick.cpp` ~249 can run.

### H. Outlines (issue 11) — NOT DONE
- Add a gamemode presentation flag (e.g. `player_outlines` / `hide_player_outlines`)
  to `GamemodePresentation` in `src/gamemode/gamemode.h` and parse it in
  `gamemode.cpp` (mirror the existing `presentation` flags ~252).
- Apply as an in-memory override in `CommunityMatchClient` (mirror
  `RagdollDeathConfig::setRuntimeEnabled` / `HealthbarConfig::setModeVisibilityOverride`)
  that forces `PlayerVisualsConfig` enemy/self/teammate mode to `none` while the
  mode is active; restore on `reset()`. Owner file: `src/config/player-visuals-config.*`.
- Add `player_outlines: false` to `counterstrike.json` presentation block.

### I. Intermission (issue 1) — explicitly SKIPPED by human.

### J. Round win rules (issue 7) — verify only, no code change expected.

---

## 4. Build / test instructions

Build (incremental build has a quirk — see below):

```powershell
python build.py build-only
```

- The incremental builder compares file mtimes. This filesystem sometimes does
  not advance mtime on edit, so a changed file can be SKIPPED. If a change is
  not compiled, force it:
  `(Get-Item <path>).LastWriteTime = Get-Date` before building.
- "Nothing changed." means all objects were current — NOT necessarily success of
  new work. Confirm with `git diff` and the object timestamp.

Run the acceptance + individual selftests:

```powershell
.\mimita.exe --counterstrike-acceptance-selftest
.\mimita.exe --cs-round-selftest
.\mimita.exe --gamemode-selftest
.\mimita.exe --spawn-tag-selftest
.\mimita.exe --objective-selftest
.\mimita.exe --map-config-selftest
.\mimita.exe --actor-preset-selftest
.\mimita.exe --grenade-selftest
.\mimita.exe --area-effect-selftest
.\mimita.exe --team-brain-selftest
.\mimita.exe --npc-perception-selftest
.\mimita.exe --npc-utility-selftest
.\mimita.exe --grenade-reasoning-selftest
.\mimita.exe --npc-nav-request-selftest
```

Last known green run (before patrol/spawn/freeze edits): all 11 acceptance
groups + 13 individual PASS. **Re-run after this batch.** Note the 3-team model
(CT, T, Spectator) means `--cs-round-selftest` and `--gamemode-selftest` assert
3 ordered teams; do not reintroduce a 2-team assertion.

---

## 5. Runtime acceptance checklist (human)

Launch counterstrike via the devloop (mode 5) or in-game GUI on
`dust2cyberiav4`:

1. Map is v4, not v3 (check `changemap`/GUI and the loaded map).
2. No actor of either team ever spawns at the opposite team's spawn, including
   after deaths/kills (issue 2/9).
3. NPCs stand still during 3-2-1 and only act at GO (issue 3).
4. NPCs **patrol forward continuously**, re-route on walls, and do not circle in
   spawn or retrace the same ground (issues 5/10).
5. When NPCs meet a hostile, they fight (cross-team), and same-team never fires.
6. Body shots and limb shots deal damage (revolver no longer head-only)
   (issue 6).
7. Round outcomes: no plant by timeout → CT; explode → T; defuse → CT;
   team wipe → opposing team (issue 7).
8. "win the round" shows the winning team name (issue 8).
9. ESC opens a menu WITH a visible cursor; L unlocks the mouse while spectating
   (issue 10b).
10. No white outline around players in counterstrike (issue 11).

---

## 6. Remaining task list (finish in this order)

1. `config/actor-presets/counter_strike.json` — remove `damage_policy`
   `allowed_body_parts`.
2. `src/main-systems.cpp` — change fallback map v3 → v4.
3. `config/maps/dust2cyberiav4.json` — fix header comment.
4. `devscripts/dev-loop.py` — restart server when launch-mode map differs.
5. `src/network/server-gamemode.cpp` — send per-round winner team.
6. `src/engine/engine-tick-state.cpp` — exclude `PauseMenu::isOpen()` from the
   forced cursor; fix freecam L unlock.
7. Gamemode outline override: `gamemode.h/.cpp`, `player-visuals-config.*`,
   `CommunityMatchClient`, `counterstrike.json`.
8. (Optional polish) Add `UtilityGoalKind::Patrol` so `npc_brain` diagnostics
   match the `PATROL` state.
9. Build; fix mtime-skips; run full selftest suite.
10. Write ONE changelog under `docs/changelog/2026-10-03/` and append an Attempt
    entry to `docs/features/gamemodes/counterstrike.md`.
11. Report source/build/runtime/human evidence separately. Do not claim visual
    or multiplayer acceptance without performing it.

---

## 7. Warnings / gotchas

- **Do NOT commit** unless the human explicitly asks.
- The working tree already contains UNRELATED pre-existing edits (weapon
  `beam_thickness`, weapon-box debug visuals, collision/tunneling work,
  `config/weapons.json`, runtime-written user settings). Preserve them; do not
  claim them; do not revert them.
- `config/weapons.json` must remain unchanged by CS work; overrides are in-memory
  via the actor preset.
- `config/gamemodes/counterstrike.json` already has the 3-team model (CT, T,
  Spectator) and a bomb `visual` pulse block from the prior session.
- VSync stays off; gameplay runs at fixed 60 Hz; do not add per-frame gameplay.
- One changelog per repository-touching session.
- `freeze_seconds` in `counterstrike.json` is currently parsed but unused; do
  not wire it in unless the human asks (they want NPCs to act at GO).
- The patrol code was written but never runtime-observed; expect to debug
  wall-hugging, stuck loops, or overly-long straight runs. Tuning knobs live in
  `updatePatrolHeading` (probe distance, 16 samples, 2–4 s refresh) and the state
  min/max times.
