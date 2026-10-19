# Melee knockback, percent, shield, feedback, controls, and crash fixes

- Status: `PASS_WITH_HUMAN_REVIEW`
- Timestamp: 2026-10-19T22:35:11Z (2026-10-19 18:35:11 EDT, America/New_York)
- Branch: `20261011zt`
- HEAD at completion: `26562692` (`some animations ssnsnsnsnsss melee things`)
- Build: 2657 (`C:\mimita-v9\.dev\builds\2657\mimita.exe`)
- Verification executable journal: `logs/10-19-2026/20261019_183157/events.jsonl` (`--versioninfo`) and runtime journals listed below.

## Pre-existing and concurrent work

The working tree already contained animation, weapon, audio, analytics, and
animation-exporter edits before this task. Another concurrent session committed
the task changes together with unrelated Zombie Tower, notification, scenario,
and documentation work as `26562692`. This session did not create that commit.
Untracked/new Melee runtime scenarios created here are
`devscripts/scenarios/melee_npc_death_v1.json` and
`devscripts/scenarios/melee_shield_v1.json`. The pre-existing Melee regression
record `docs/regressions/2026-10-19/melee-npc-death-stale-dependency-REG.md`
was extended with runtime evidence. Two new regression records were added under
`docs/regressions/2026-10-19/`.

## Documents and skills read

- `AGENTS.md`, `docs/ROUTER.md`
- `docs/specs/gamemodes/supersmashbrosmelee.md` (authoritative behavior)
- `docs/specs/movement/movement.md`, `docs/specs/weapons/weapons.md`,
  `docs/specs/networking/networking.md`, `docs/specs/gui/guiv2.md`
- `docs/architecture/json-configuration/json-configuration.md`,
  `docs/architecture/collision/collision.md`
- `docs/workflows/runtime-scenario-validation.md`
- `docs/operations/build-and-exe/build-and-exe.md`,
  `docs/operations/task-completion/task-completion.md`
- `docs/skills/spec-behavior-review-v1.md`,
  `docs/skills/logging-checker-v1.md`
- `docs/regressions/regressions-v1.md` and the Melee regression records.

## Findings and confirmed owners

1. **Knockback versus WASD:** `movement-smash.json` used `mimita`/`Override`.
   `applyPreCollisionBasicMovement()` folded and cleared external impulse;
   `applyBasicExternalImpulseControl()` cleared it on input; `applyBasicWalk()`
   overwrote horizontal velocity. Fixed with a new `Compose` mode.
2. **Percent and knockback:** `server-melee-actions.cpp` multiplied percent and
   knockback by `1 + targetPercent/100` and multiplied percent by contact force.
   Live journal `20261019_205156` recorded percent additions of 3905, 4899, 5696,
   and 11976. Fixed with a flat percent rule and the shared curve.
3. **NPC-to-player results:** the server path ran, but the local player never
   read its own snapshot percent/shield, and confirmed feedback carried no
   impulse. Fixed through local snapshot reconciliation and one accepted impulse
   application per reliable event.
4. **NPC death crash:** crash report
   `%LOCALAPPDATA%\MiMITA\crashes\crash-2026-10-19_16-52-41-15192.txt` reported
   `INT_DIVIDE_BY_ZERO` at `mimita.exe+0x7ae85a`, mapped to
   `serverProceduralWorldOwnsNpc` -> `encounterNpcIds.find`. The object
   `procedural_procedural-world.o` was older than `server-gamemode.h` because
   its zero-byte `.d` file was trusted as having no dependencies. Fixed in
   `build.py` and by deleting the zero-byte `.d` files.
5. **Hit feedback:** confirmed Melee used `spawnDamageNumber = false` and the
   damage-number renderer printed `20`, not `+20%`. Feedback now uses
   `percentDisplay`/`percentAmount` and carries the accepted impulse.
6. **Shield:** durability/decay/break existed, but the multiplier was not
   clamped, shield state was dropped from snapshots, and no cyan ellipsoid was
   rendered. Added clamped multipliers, snapshot fraction, visual tuning, and
   renderer.
7. **Controls:** hold-jump state was not stored in `Player::JumpState` or copied
   by `movement-conversion.cpp`, so local `holdJumpActive` reset every tick.
   Left Ctrl was not bound to movement. Added state persistence, a `crouch`
   binding, airborne fast fall, and `stable_ground_grace_seconds`.
8. **Animation:** the selector already used `stableOnGround`; the server was
   overwriting it with raw `onGround`. Added stable-state preservation and a
   bounded `anim.melee.state_flap` diagnostic. Distinct knee/elbow/up-B/side-B/
   down-B clips remain an authoring decision.

## Exact source and configuration changes

- `build.py` — zero-byte `.d` dependency files now rebuild and are removed.
- `config/melee.json` — added `shield.min_damage_multiplier`,
  `shield.max_damage_multiplier`, `shield.visual` (`center_offset_m`, `size_m`,
  colors, alpha endpoints), `force.min_impulse`, `reference_base_impulse`,
  `reference_seconds`, and the 0/100/200 percent `knockback_curve`.
- `config/movement/movement-smash.json` — `movement_mode: "smash"`,
  `stable_ground_grace_seconds: 0.15`, fast-fall settings, and
  `hold_jump_max_multiplier: 1.5` (from 22.6).
- `src/combat/melee-rules-config.{h,cpp}` — new shared validated/hot-reloaded
  rules owner, curve interpolation, shield multiplier, and shield color/alpha.
- `src/network/server-melee-actions.cpp` — shared `computeMeleeContact()` result
  for player/NPC attacker and target pairings; flat percent; pre-hit curve
  knockback; physical distance-to-impulse conversion; shield clamp/break;
  impulse-bearing feedback; shield-state samples; natural-break presentation.
- `src/physics/movement/movement-step.{h,cpp}` — distance-to-impulse conversion,
  `Compose` behavior, fast fall, stable grounded walk.
- `src/physics/movement/movement-types.h`, `src/config/movement-config.cpp` —
  `Compose` mode, `crouchHeld`, fast-fall and stable-grace tuning, `smash` parse.
- `src/physics/movement/movement-conversion.cpp` — hold-jump state copied both
  ways; server stable ground preserved.
- `src/network/server-players.cpp`, `src/network/movement-validation.cpp` —
  server stable ground and shield snapshot fraction.
- `src/network/packets.h`, `snapshot-chunks.cpp`, `server-packets.cpp`,
  `multiplayer-*.cpp/.h`, `server.h` — protocol 42; `meleeShield` snapshot byte;
  shield/crouch flags; local meter and impulse fields.
- `src/engine/engine-tick-net.cpp` — applies accepted local Melee impulse once,
  including natural shield break; writes `melee.client.impulse_applied`.
- `src/engine/engine-tick-render.cpp` — cyan shield ellipsoid from
  `shield.visual`, interpolated by remaining fraction.
- `src/effects/hit-effects.{h,cpp}`, `effect-part.h`,
  `effect-part-particles.cpp` — `+N%` world-space damage number.
- `src/entities/player.h`, `src/input/*`, `src/engine/engine-tick-combat.cpp`,
  `src/main-systems.cpp` — crouch binding, Ctrl fast fall, hold-jump fields,
  `agent.shield` runtime hook.
- `src/network/server-packet-chat.cpp`, `src/terminal/npc-commands.cpp` — bounded
  `npc_void` runtime test hook for the NPC death scenario.
- `src/entities/player-animation.cpp` — stable grounded server state and bounded
  `anim.melee.state_flap` evidence.

## Validation evidence

### Build and executable identity

- Repeated canonical builds through
  `python devscripts/dev-loop.py --request build --wait-for-build`.
- Final successful build: **2657**.
- `mimita.exe --versioninfo`:
  - `EXECUTABLE_PATH=C:\mimita-v9\.dev\builds\2657\mimita.exe`
  - `EVENTS_JSONL_PATH=logs/10-19-2026/20261019_183157/events.jsonl`
  - `RUN_ID=20261019_183157`

### Runtime journals

- `logs/2026-10-19/20261019_215612/events.jsonl`
  - `melee.hit` for player-to-NPC: `percent_added` 20.0 and 3.0 while
    `percent_before` rose 0 -> 20 -> 23; `curve_distance_m` rose 0.5 -> 2.4.
- `logs/2026-10-19/20261019_222536/events.jsonl`
  - NPC-to-player `melee.hit`: `percent_added` 3.0 and 20.0,
    `target_is_npc: false`, followed by `melee.client.impulse_applied`.
- `logs/2026-10-19/20261019_223206/events.jsonl`
  - `melee.shield_state` shows durability 48.83 -> 1.17 and alpha
    0.490 -> 0.111; natural `melee.shield_break`; local
    `melee.client.impulse_applied` with `[0,0,5]`; later NPC hits applied
    once with impulses about 0.999 and 2.89; `melee.stock_lost` for actor 1000
    with 3 stocks remaining.
  - `melee_shield_v1`: passed.
  - `melee_npc_death_v1`: passed.
- `logs/2026-10-19/20261019_222536/events.jsonl` (`melee_fight_v1`): passed,
  including NPC-to-player contact and local impulse application.
- No new crash report was written after the final runtime runs; the newest
  crash dump remains `crash-2026-10-19_17-52-33-24168.dmp`.
- Final dev-loop status after the NPC death scenario: server alive, client
  connected, build 2657.

### Limits

- Runtime NPC death evidence forced the lethal state through the `npc_void`
  test hook. The normal stock-loss path ran and the server survived; not every
  respawn-loop/map combination was exercised.
- No live player-to-NPC hit was captured after the final NPC-decay correction;
  the earlier run proves the shared result path and the source uses the active
  mode's `smash` decay for NPC targets.
- Animations: state-flap count was 0 in the final runs. Distinct knee/elbow/up-B/
  side-B/down-B clips remain missing and need a pose/timing authoring decision.
- Human acceptance is still required for knockback feel, WASD steering while
  launched, jump height, crouch stability, shield readability, and hit feedback
  readability.
