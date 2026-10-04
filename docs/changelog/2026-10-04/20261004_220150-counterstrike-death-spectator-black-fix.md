# Counter-Strike death spectator no longer blacks out the world

Date: 2026-10-04
EST timestamp: 2026-10-04 18:01:50 ET
UTC timestamp: 2026-10-04T22:01:50Z
Branch: `afad20a-rebuild`

## Result

`PASS_WITH_HUMAN_REVIEW`

Source and build evidence are proven (2 translation units recompiled,
`mimita.exe` relinked, `BUILD SUCCESS`). No live gameplay acceptance was
performed. The human must die in Counter-Strike, confirm the world stays
visible in spectator/freecam, and that other actors keep moving.

## Scope

Human report: when dying in Counter-Strike and entering the death
spectator/freecam, "the screen go[es] black, I can see other actors but they
won't move... it works [fails] every time." Requested: on every Counter-Strike
death, enter the spectator team with the world visible.

This is the single changelog for this session.

## Root cause

The fullscreen black spawn-flash quad in `src/engine/engine-tick-render.cpp:458`
is drawn inside the actors block (after the world pass, before actor draws), so
it hides the world but leaves the HUD and other actors visible — exactly the
reported symptom.

It is gated only by `player.spawnFlashTimer > 0.0f`. That timer had exactly one
decrement owner: `src/sim/simulate-tick.cpp` (line ~192), one unit per fixed
tick. `simulateTick` is skipped whenever the gameplay freecam is active
(`src/engine/engine-tick-replay.cpp:336`).

The Counter-Strike death spectator forces the gameplay freecam on
(`src/engine/engine-tick-camera.cpp:298`, the round-based death spectator block
added since `8ac195e7`). The local host's `DeathSystem::update` also respawns
its own player (it only gates on `DuelQueue::inDuel()`, not the gamemode
one-life rule), which sets `spawnFlashTimer = 10.0f`
(`src/combat/death-system.cpp:305`). With the freecam active the timer could
never decrement, so the world stayed black behind the flash quad for the whole
spectating period. The world itself always rendered; it was simply covered.

## Files changed

### Spawn-flash decay moved to a guaranteed per-frame owner

`src/engine/engine-tick-state.cpp` — added near the top of `engineTickState`
(which runs every render frame, before the `GAME_PLAYING` block, regardless of
freecam or death):

```cpp
if (player.spawnFlashTimer > 0.0f)
    player.spawnFlashTimer = std::max(0.0f, player.spawnFlashTimer - dt * 60.0f);
```

`src/sim/simulate-tick.cpp` — removed the duplicate decrement:

```cpp
// removed
if (sim.player->spawnFlashTimer > 0.0f)
    sim.player->spawnFlashTimer = std::max(0.0f, sim.player->spawnFlashTimer - 1.0f);
```

The `dt * 60.0f` rate preserves the previous one-per-fixed-tick duration (10
units ~= 0.17 s) while being frame-rate independent and impossible to wedge.
One owner remains.

### Counter-Strike spectator behavior

No change was needed: `config/gamemodes/counterstrike.json` already has
`respawn_seconds: 0` and a `spec` (Spectator) team, and
`src/network/server-gamemode.cpp` `updateActorStates` already moves a dead,
non-respawning actor to the Spectator team during objective rounds. The client
camera already forces spectator freecam from the replicated actor state. The
only defect was the stuck flash covering the world.

## Validation

```text
[CXX ] src\engine\engine-tick-state.cpp
[CXX ] src\sim\simulate-tick.cpp
[LINK] mimita.exe
 BUILD SUCCESS
Compiled: 2
Skipped : 518
```

## Evidence separation

- Source: this file and the diff.
- Build: `BUILD SUCCESS`; `mimita.exe` relinked 2026-10-04 18:01 local.
- Runtime: NOT performed.
- Human acceptance: pending (die in Counter-Strike, confirm world visible and
  actors moving in spectator).

## Documents read

- `AGENTS.md`, `docs/ROUTER.md`.
- `docs/specs/movement/movement.md` section 15 (death/respawn contract).
- `docs/specs/20261003plan.md` (pass 5: "wehn i die, im just its just black i
  tihnk freecam mode broke"; desired ragdoll→spectator death camera).
- `docs/features/gamemodes/counterstrike.md`.
- `docs/operations/build-and-exe/build-and-exe.md`.

## Known limitations / follow-ups

- A local host's `DeathSystem::update` still locally respawns a networked
  Counter-Strike one-life player because it does not consult the gamemode
  respawn rule (only `DuelQueue`). This is the reason the flash is armed at
  death. It is a separate lifecycle defect; the scroll back is server-controlled
  so gameplay is unaffected. Left unchanged to keep this fix minimal.
- The load-triggered `map_spawn_debug.txt` write and the stale
  `DEATH_POSITION` variable were not touched.
- No commit was made; nothing was pushed or deployed.
