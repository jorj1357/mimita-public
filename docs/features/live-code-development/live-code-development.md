start date: 2026-09-12
last updated: 2026-09-12

# Live-Code Development

## Purpose

Allow MiMITA.exe to stay running while a human or AI edits a small set of C++
files, saves them, and sees validated behavior changes appear in the current
match. The first scope is one local development machine and one running
MiMITA.exe.

This record is both the authoritative feature specification and the live
feature index. It defines intended behavior before implementation expands.

## Core contract

The following statements are the permanent contract of this feature. Code that
contradicts them is wrong unless a human changes this specification first.

```text
Player and NPC are both Actors.
Input source differs; gameplay rules are shared.
World and ActorState survive code reloads.
Hot code receives state and returns decisions/results.
A failed candidate never replaces the active version.
Activation occurs only at a safe simulation boundary.
```

## Persistent state versus replaceable code

Persistent state is owned by the EXE and survives every reload:

- world storage and map geometry;
- player/NPC identity and lifecycle;
- actor state (transform, velocity, momentum, health, inventory, weapon state,
  team, role, emotions, objective state);
- network sockets and authoritative match state;
- replay state;
- rendering resources, OpenGL objects, UI layout state;
- audio and notification systems.

Replaceable code is owned by the hot module and may change on every activation:

- actor decision functions (`chooseActorCommand`, `updateActorEmotion`,
  `chooseActorRole`);
- movement behavior;
- projectile simulation;
- weapon/projectile hit decisions;
- effect and visual presentation behavior;
- UI presentation logic.

Replaceable code receives state and returns decisions or results. It does not
own EXE objects.

## Hot module ownership

- There is exactly one reload system: the `GameAPI` hot-module system. No second
  reload path may be introduced.
- The first architecture ships one coarse-grained replaceable DLL whose
  `GameAPI` contains internally partitioned module tables (actor, movement,
  projectiles, effects, ui). Modules carry their own name and ABI version so
  function-level or per-module DLLs can be introduced later without changing the
  public contract.
- The EXE never overwrites the currently active DLL. Each candidate is staged
  under a unique generation name.
- The EXE continues to own world storage, identity, network sockets,
  authoritative match state, replay state, rendering resources, and audio and
  notification systems.

## Actor / player / NPC unification

Player and NPC are both actors. Human input, NPC AI, replay input, and test
scripts all produce the same `ActorCommand`:

```text
player input -> ActorCommand
NPC AI       -> ActorCommand
replay input -> ActorCommand
test script  -> ActorCommand
```

Gamemodes consume general actor events (actor damaged, actor killed, objective
captured, bomb planted, actor respawn requested) instead of duplicating player
and NPC lifecycles.

The unification is introduced incrementally at the hot boundary: plain-data
actor state and command structs are defined first, the first hot actor functions
consume them, and the server player/NPC twins converge on the shared path over
time. A big-bang rewrite of `ServerPlayer`/`ServerNpc` is explicitly out of
scope for the bootstrap.

## Versioned state envelopes

All data crossing the DLL boundary uses stable plain-data envelopes. No STL
containers, owning pointers, live engine objects, or unstable C++ class layouts
cross the boundary.

```text
state type
state version
byte size
data pointer
simulation tick
code generation
```

Every persistent hot state type declares a version (`ActorState v1`,
`CameraState v1`, `ProjectileState v1`, `UIState v1`). A hot module declares the
state versions it understands.

## State migrations

When a struct changes, live state is migrated before use:

```text
old state
-> migration function
-> temporary new state
-> validation
-> atomic replacement
```

Live state is never replaced in place before the migration succeeds. A migration
must preserve current behavior; for example Euler camera angles to quaternion
must preserve the current view direction and camera position without restarting
the game.

## Fixed-tick activation

Activation happens only at a safe simulation boundary: the top of the fixed
60 Hz tick, before any gameplay, collision, or physics step runs. The game
thread never blocks on compilation or disk I/O from the build worker.

## Code hashes and generation IDs

- Every hot source file has a SHA-256 content hash. Change detection uses hashes,
  not only modification times.
- Each successfully loaded candidate receives a monotonically increasing
  generation ID and records its aggregate code hash.
- The active generation and hash are attached to every journal event.

## Compile failure behavior

Compilation failure leaves the previous generation running. The failed candidate
is never loaded. A failure produces a notification, a journal record with the
compiler error, and no change to gameplay.

## Validation and rollback behavior

- Before activation a candidate passes an API/ABI check and a deterministic
  smoke self-test. Validation failure leaves the previous generation running.
- Multiple saves while a build runs coalesce into the newest candidate.
- Every activated generation can be rolled back. The previous generation's file
  and hash are retained so rollback can be activated at the next safe tick.
- Old code remains loaded until no active call can still be executing it; the
  previous module is unloaded no earlier than one full tick after the swap.

## Deterministic validation

An in-game command/action can run a deterministic check while the game is
active. Each check captures setup, code generations, inputs, random seed, network
profile, expected events, actual events, the first missing or incorrect event,
and a final PASS/FAIL result.

For rocket/projectile checks the traced stages are:

```text
client fire
prediction
packet creation
simulated latency/loss
server receipt
historical lookup
collision decision
damage result
authoritative packet
client reconciliation
visual effect
```

The result answers whether lag compensation is working from evidence rather than
visual guesswork. It also supports old-versus-new replay comparison later.

## Live event recording

The runtime writes an append-only JSONL event journal immediately. Each line
records:

- UTC timestamp with millisecond precision;
- monotonic elapsed timestamp;
- event type;
- simulation tick;
- active code generation and hash;
- file/module;
- relevant actor/projectile/packet IDs;
- result and error information.

Recorded event types include at minimum: file saved; source hash changed;
compile started/finished; compile failure; candidate loaded; validation result;
state migration; code activation; rollback; notification emitted; sound emitted;
test started/finished; packet sent/received; projectile spawned/hit/reconciled;
UI/effect event applied.

The journal is the detailed forensic record. The human-readable changelog
remains required and becomes a summary generated from the live evidence. The
journal writes under the repository time standard
(`docs/architecture/time-and-formatting/time-and-formatting.md`) and flushes
each line so a crash keeps the evidence.

## Future multiplayer READY / switch-tick protocol

Reserved but not implemented in the bootstrap. When multiple peers run replaceable
code, a peer advertises its code hash and generation, peers reach a READY state,
and all peers switch at a shared switch tick. The server stays authoritative for
shared gameplay results. Matching code hashes improve agreement but do not remove
server authority. Public-server arbitrary native code is out of scope until
sandboxing, signing, and incident response exist.

## Current status

solved as of 2026-09-12T10:05:00Z | implemented (Phase 0-4 foundation) | not implemented (migrations, multi-module activation, projectile/network tests, multiplayer protocol)

Implemented foundation:

- this specification and route;
- centralized UTC/monotonic time utility;
- append-only JSONL live event journal;
- hot-reload lifecycle notifications and success sound;
- `GameAPI v3` with versioned envelopes, module tables, and a self-test hook;
- non-blocking background compile pipeline with hashed change detection,
  generation-stamped candidates, ABI/self-test validation, safe-tick activation,
  and one-tick deferred unload;
- generation registry with rollback support at the system level;
- `hotreload status` terminal command;
- hot `actor` module (`chooseActorCommand`, `updateActorEmotion`,
  `chooseActorRole`) driving NPC emotion, role, and movement speed through the
  shared Actor model, with EXE-owned state;
- hot `presentation` module for damage-number formatting and rocket trail
  parameters;
- deterministic smoke validation that asserts invariants and determinism rather
  than pinning tuned constants (a `* 1.1` effect edit now activates instead of
  being rejected);
- `--live-code-selftest` headless verification of time, hashing, journal, and
  all active modules.

Not yet implemented:

- player-side hot actor decisions (only the NPC path is wired);
- state migration execution;
- atomic multi-module activation and dependency-graph invalidation;
- deterministic rocket lag-compensation test and end-to-end network evidence;
- the multiplayer READY/switch-tick protocol.

## Decisions

- One coarse replaceable DLL with internal module tables for the bootstrap;
  per-module split is deferred.
- The runtime owns the JSONL journal as a single writer. The build worker writes
  its raw result to `build/hotreload/<generation>/build-result.json`; the runtime
  ingests it and records the compile events.
- The success sound reuses an existing asset through a `soundPath` alias so no
  new binary asset is required.
- Actor unification is incremental at the hot boundary.
- The background build worker invokes `python build_game_dll.py` through the
  process PATH. Local development assumes python is available on PATH; resolving
  the interpreter explicitly is future work.
- Source-change detection uses SHA-256 content hashes. Watching uses a throttled
  periodic hash poll (every 15 frames) rather than filesystem notifications.
- The deterministic candidate self-test asserts invariants and determinism, not
  the currently tuned numeric output. Pinning tuned constants would reject
  intended behavior changes (observed with a `* 1.1` effect edit).

## Ownership

- Primary code owner: `src/hot-reload/hot-reload-system.*`
- Module contract owner: `src/hot-reload/game-api.h`
- Build worker owner: `build_game_dll.py`, `src/hot-reload/hot-modules.json`
- Live evidence owner: `src/live-code/live-journal.*`, `src/utils/time-format.*`
- Notification/sound owner: `src/live-code/live-code-events.*`, `src/audio/audio.*`
- Runtime activation boundary: `engineTickSetup` in
  `src/engine/engine-tick-setup.cpp`
- Network owner, if applicable: not yet (projectile/network phases)

## Related authoritative documents

- Specification: this file
- Architecture: `docs/architecture/time-and-formatting/time-and-formatting.md`,
  `docs/architecture/player-npc-systems/player-npc-systems.md`
- Workflow: `docs/operations/build-and-exe/build-and-exe.md`
- Focused review skill: `docs/skills/spec-behavior-review-v1.md`,
  `docs/skills/logging-checker-v1.md`
- Regression record: `docs/regressions/regressions-v1.md`

## Relevant files

- `src/hot-reload/game-api.h`
- `src/hot-reload/game-modules.h`
- `src/hot-reload/hot-reload-system.h`
- `src/hot-reload/hot-reload-system.cpp`
- `src/hot-reload/hot-modules.json`
- `src/hot-reload/modules/actor-behavior.cpp`
- `src/hot-reload/modules/presentation.cpp`
- `src/live-code/live-journal.h`
- `src/live-code/live-journal.cpp`
- `src/live-code/live-code-events.h`
- `src/live-code/live-code-events.cpp`
- `src/live-code/live-actor.h`
- `src/live-code/live-actor.cpp`
- `src/live-code/live-presentation.h`
- `src/live-code/live-presentation.cpp`
- `src/live-code/live-modules.h`
- `src/live-code/live-modules.cpp`
- `src/live-code/live-code-selftest.h`
- `src/live-code/live-code-selftest.cpp`
- `src/utils/time-format.h`
- `src/utils/time-format.cpp`
- `build_game_dll.py`
- `docs/gold/2026-09-12-live-code-hot-reload-reference.md`

## Tests and evidence

- Automated tests: `mimita.exe --live-code-selftest` (headless; time, hashing,
  journal, GameAPI load/ABI/self-test, and all active modules).
- Runtime commands: `hotreload status`, `hotreload rollback`.
- Logs: `logs/features/live-code/yyyy-mm-dd/live_events_yyyymmdd_hhmmss.jsonl`
  and the build worker's `build/hotreload/<generation>/build-result.json`.
- Reference: `docs/gold/2026-09-12-live-code-hot-reload-reference.md`.
- Human playtest: required for notification, damage-number, and rocket-trail
  visual acceptance.

## Acceptance criteria

- The game thread performs no blocking compile or candidate load on any tick.
- The active DLL is never overwritten; each candidate is a unique generation.
- A failed compile or a failed self-test leaves the previous generation active.
- Activation occurs at the top of a fixed tick before gameplay runs.
- The previous generation can be reactivated by rollback at the next safe tick.
- A JSONL journal line exists for each pipeline stage with UTC millisecond time
  and the active generation.
- The build produces `Status: SUCCESS` with `python build_agent.py`.

## Unresolved future work

- Actor command hot functions and shared server actor lifecycle.
- Damage-number/UI presentation commands over the boundary.
- State migration execution and version negotiation.
- Function-level recompilation and multi-module atomic activation.
- Deterministic rocket lag-compensation and projectile reconciliation tests.
- Multiplayer code-hash READY/switch-tick coordination.

## Changelog and regression links

- `docs/changelog/2026-09-12/20260912_095451-live-code-bootstrap.md` (phases 0-2)
- `docs/changelog/2026-09-12/20260912_101238-live-code-actor-presentation.md`
  (phases 3-4 and the smoke-test fix)
