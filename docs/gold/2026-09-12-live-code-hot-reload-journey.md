# From kill-and-rebuild to live C++ gameplay: the hot-code journey

Date recorded: 2026-09-12
Status: GOLD REFERENCE / FIRST WORKING ARTIFACT
Scope: how MiMITA moved from "close the game, rebuild the exe, relaunch" to
"edit a C++ gameplay file, save, and watch the running match change".

## The first working artifact

Observed by the developer in a running match:

```text
Edit: C:\mimita-priv-v8\src\hot-reload\modules\rocket-behavior.cpp
Save.
A new immutable generation compiles in the background.
At the next safe simulation tick the running game activates it.
Rocket damage vs an NPC changes — no exe rebuild, no restart, no reconnect.
```

That is the milestone. Editing C++ that determines authoritative gameplay
behavior now changes the live match. The architecture below is what made it
possible and what must keep being generalized.

## Where we started

The repository already had a replaceable DLL, but it was effect-only and the
development loop was:

```text
close MiMITA.exe
edit C++ gameplay
rebuild the whole exe
relaunch
reconnect
play until the behavior shows up
repeat
```

Symptoms that defined the work:

- `src/hot-reload/game-api.h` v2 had a single `updateEffects` function.
- `src/hot-reload/hot-reload-system.cpp` called
  `std::system("python build_game_dll.py")` on the game thread (blocking) and
  used mtimes, not hashes.
- No generation IDs, no code hashes, no rollback, no live event journal.
- `build_agent.py` ran `taskkill /f /im mimita.exe` before every build, so any
  agent build killed the running game.
- The authoritative server path (`server-projectiles.cpp`,
  `server-damage.cpp`) computed and clamped damage entirely in cold code
  (`rocketDirectDamage: 150`, `std::clamp(damage, 1, 500)`), so editing the hot
  module could not affect real server damage.

## The milestones (repo-grounded)

1. **Specification and evidence.** `docs/features/live-code-development/`
   defined persistent state vs replaceable code, fixed-tick activation, code
   hashes and generation IDs, compile-failure retention, and rollback.
   `src/utils/time-format.*` and `src/live-code/live-journal.*` added a UTC
   millisecond, append-only JSONL journal. `src/live-code/live-code-events.*`
   added notifications and success/failure sounds.

2. **Real hot-module contract.** `GameAPI` v3 gained `GameEnvelope`, named
   module tables, and a deterministic `selfTest`. The loader became a background
   worker with hashed change detection, generation-stamped candidates, ABI +
   self-test validation, activation at the top of the fixed tick, and
   two-generation retention for rollback.

3. **First hot gameplay code.** `src/hot-reload/modules/actor-behavior.cpp`
   (`chooseActorCommand`, `updateActorEmotion`, `chooseActorRole`) and
   `presentation.cpp` (damage numbers, rocket trail) proved that behavior, not
   just effects, could be replaced live.

4. **The over-strict test lesson.** The first effect self-test pinned
   `position[0] == 0.1`, so a legitimate `* 1.1` edit was rejected. Fixed by
   asserting invariants and determinism instead of tuned constants. This is the
   rule now: never pin a tuned value.

5. **Entity/component migration (partial).** `src/ecs/` added a stable
   `EntityId` and sparse component storage; players, NPCs, and rockets got
   entities; `--entity-slice-selftest` proved identity survives a DLL reload.

6. **The live-development invariant.** `build_agent.py`'s kill was removed; it
   now refuses to relink while the game runs and points at
   `devscripts/live-build.py`. Immutable generation files and a cold/live split
   were documented in
   `docs/architecture/live-development/live-development.md`.

7. **Generic authoritative gameplay boundary.** `GameEventV1` +
   `DamagePolicyV1` + `GameGameplayModuleV1::onEvent` replaced the feature-specific
   `explosionParameters` bridge. `serverResolveDamagePolicy` dispatches the event
   and applies the result; the silent 500 clamp was replaced by an explicit
   dev-unlimited safety limit. Rocket, grenade, hitscan, melee, and contact
   damage all route through the same policy.

8. **The server lifecycle gap.** The dedicated server (`--server`) returns from
   `handleGameCLI` before `gameInit`, so it never started the hot loader or the
   journal. `runServer` now initializes them, polls at each fixed step, and
   announces its generation.

9. **Repeated-activation and observability round.** Client and server shared
   `build/hotreload/genN/` and one `build-result.json`, so they could read each
   other's outcome; and a failed build was never retried. Fixed with per-PID
   build directories, bounded-backoff retry, and process/PID/session/generation/
   hash on every journal event and notification. `PACKET_CODE_GENERATION` seeds
   the future multiplayer READY/switch-tick protocol.

## Why it works (the mental model)

```text
THE ENTITY STAYS
THE BEHAVIOR CHANGES
THE UNIVERSE DOES NOT RESTART
```

- The EXE owns persistent state and mechanisms: entity ids, component storage,
  event dispatch, tick scheduling, network authority, physics queries, the hot
  loader, and explicit safety limits.
- Hot code owns gameplay policy: damage, projectile motion, explosion falloff,
  actor decisions, gamemode rules.
- A save produces an immutable generation; validation and activation happen at
  a safe fixed tick; a failure keeps the previous generation; the world,
  session, and entity ids survive.

The generic event boundary is the key: the kernel emits
`GAME_EVENT_DAMAGE_POLICY` with base values, the hot behavior returns
`outDamage`, and the kernel applies it. `999999` is not a special case — it is a
generic authoritative result.

## Thought process that guided the work

1. **Preserve behavior first, then move it.** Every slice kept JSON as the base
   and the working networking/authority path intact.
2. **One owner per concept.** Extend the existing `GameAPI`/loader rather than
   add a second reload system; reuse existing ids (`playerId`, `Npc::id`,
   `ServerProjectile.id`, `fireSerial`) instead of inventing new ones.
3. **Make the failure visible.** Journal + notifications + explicit
   `HOT_RELOAD_BOUNDARY_VIOLATION` so "it didn't work" becomes "here is the
   exact step and generation that diverged".
4. **Tests assert invariants, not tuned values.** The `* 1.1` rejection is the
   canonical lesson.
5. **The running process is sacred.** Live build never writes `mimita.exe`;
   cold build refuses while the game runs.
6. **Independent processes need independent state.** The client and server each
   own their watcher, active generation, and evidence; they share source content
   and, later, a switch tick.

## What in the repo helped

- `GameAPI` already existed, so the contract could be extended instead of
  replaced.
- The fixed 60 Hz tick gave a clean, safe activation boundary.
- Stable IDs and server authority already existed, so the event path could
  reuse them.
- The existing effect DLL, notification system, audio, and replay were reused.
- `build_game_dll.py` already compiled a single hot source, so generalizing it to
  a manifest was small.
- The gold and router documents gave a place to write down the invariant and the
  evidence rules.

## What could have made it easier

- **Design for the general case from the start.** The first hot ABI was
  effect-specific (`updateEffects`) and then parameter-specific
  (`explosionParameters`), which forced two later rewrites. If the first
  contract had been "the kernel emits events, hot code returns results", the
  rocket work would have been the first instance instead of a retrofit. This is
  the main lesson.
- **Do not put cold call sites behind hot behavior.** Every new hot capability
  needed a new EXE call site and therefore a cold build. Keeping a small stable
  set of coarse hook points in the EXE from the beginning would have avoided
  repeated bootstrap builds.
- **Process identity and per-process build paths should have existed on day
  one.** The client/server sharing `build/hotreload/genN/` was a subtle,
  expensive bug.
- **Retry semantics should have been explicit.** "Observe the hash, mark it done
  before compiling" silently stalled on any transient failure.
- **The dedicated-server lifecycle was easy to miss** because `--server`
  bypassed `gameInit`; a checklist item "every process that simulates must init
  the hot system" would have caught it sooner.

## Resolving the "general vs get it working" tension

You do not have to choose. The practical rule that emerged:

```text
Get it working behind a SEAM, then generalize the SEAM.
```

- First slice: make the smallest thing work end to end (one function, one
  event), but put it behind a boundary that is already generic in shape
  (`GameEnvelope`, event type id, plain-data payload).
- Then migrate the next behavior through the same seam instead of adding a new
  bridge.
- Only the seam itself is allowed to be "one-off" once; behavior never is.

Said another way: it is fine to build the first bridge narrow, as long as the
bridge is a doorway and not a dead end. The rocket event path is now a doorway
that grenades, hitscan, melee, and contact already walk through.

## How to expand

See `docs/architecture/live-development/hot-kernel-next-steps.md`. The order:

1. Per-entity `BehaviorBinding` + content-hashed behavior tables.
2. Kernel event queue with nested `emitEvent`.
3. Component capabilities (`readComponent`/`writeComponent`/`findEntities`/
   `spawnEntity`/`destroyEntity`).
4. Full explosion ownership in the behavior.
5. Deterministic ordering and per-entity RNG streams.
6. Converge the ECS store and the server structs onto one source of truth.
7. Schema-driven events/components; then a bytecode/IR sandbox so even new
   mechanisms are hot.

## Evidence and status

- Working artifact: user-observed live change of rocket damage vs an NPC after
  editing `src/hot-reload/modules/rocket-behavior.cpp`.
- Automated: `--live-code-selftest`, `--hot-authoritative-selftest`,
  `--entity-slice-selftest`, and `devscripts/test-live-build-invariant.py`.
- Open: one bootstrap cold build for the latest observability/retry/protocol
  round, then the full running-server proof (server journal
  `hot_damage_policy_result process=server generation=<current> code_hash=<current>
  base_damage=... out_damage=... result=hot`).

## Related

- `docs/architecture/live-development/live-development.md`
- `docs/architecture/live-development/hot-kernel.md`
- `docs/architecture/live-development/hot-kernel-next-steps.md`
- `docs/gold/2026-09-12-live-jsonl-ai-observability.md`
- `docs/features/live-code-development/live-code-development.md`
