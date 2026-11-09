# NPC/Payload hot policy and actor-world collision lane slice

## Session identity

- Timestamp: 2026-11-08 15:24:39 EST (2026-11-08T20:24:39Z)
- Lane: `npc-collision-hot-20261108`
- Worktree: `C:\mimita-v9-agent-lanes\npc-collision-hot-20261108`
- Branch: `codex/agent-npc-collision-hot-20261108`
- Lane base snapshot: `32a7939e4ed605d7ffc4b5bddf632bc5e9d28c36`
- Build policy: agent did not build. Lane dev-loop built automatically.

## Changed

### Lane baseline

The lane was created from the coordinator's green snapshot after the
coordinator build reached `2026-11-08T19:03:25Z` with
`source_generation == latest_successful_source_generation` and
`latest_build_stale=false`.

The lane snapshot carried the already-written NPC/Payload hot-policy source:
utility goal/action, Payload role, target priority/retention, attack intent,
dash/down-dash policy, human-pusher counting, 25 m cart leash, and the
target/attack diagnostics. No duplicate NPC policy was added in the lane.

### Actor-world collision hot slice

Added a bounded, versioned actor-versus-static-world path to the existing
`chunk-collision` module:

- `src/modules/api/actor-contact-policy-api.h:47`
  - Added `ActorWorldSolveV1`, `MAX_ACTOR_WORLD_CONTACTS`, host-owned contact and
    source-triangle views, and correction/grounded/blocked/generation outputs.
- `src/modules/api/module-abi.h:67`
  - Added `EVENT_COLLISION_ACTOR_WORLD_SOLVE = 406` (404/405 were already owned
    by the actor-pair and final-pose policies).
- `config/modules/modules.json`
  - Registered event `406` under `chunk-collision`.
- `src/world/chunk-collision-module.cpp:22,219`
  - Forward-declared and dispatched `solveActorWorld`.
- `src/physics/movement/actor-contact-policy-module.cpp:120`
  - Added `solveActorWorld`, which reuses the existing bounded contact solver
    and returns correction, weighted normal, max penetration, grounded/blocked,
    and generation.
- `src/hot-reload/module-host.h:30` and `src/hot-reload/module-host.cpp:1355`
  - Added `collisionSolveActorWorld` with ABI/bound validation and catalog-owned
    event dispatch.
- `src/physics/movement/physics-collision.h:204` and
  `src/physics/movement/physics-collision-core.cpp:480`
  - Added `solveActorWorldCorrection`. It builds bounded host-owned input and
    scratch arrays, validates finite/bounded module output, logs sampled
    provenance, and falls back to `solveBatchedCorrection` when the module is
    unavailable or invalid.
- `src/physics/movement/actor-triangle-solver.cpp:966`,
  `src/physics/movement/physics-collision-glb-main.cpp:284`, and
  `src/physics/movement/physics-collision-glb-body.cpp:191`
  - Routed the live actor-world correction calls through the new function.

The host still owns the fixed tick, actor/world/mesh lifetime, cached
broadphase gathering, contact budgets, scratch buffers, final application, and
fallback. Triangle/narrowphase extraction for other owners remains deferred.

### Diagnostics and ledger

- `collision.actor-world` is sampled at most once per second and records actor,
  generation, contact/manifold counts, max penetration, correction,
  grounded/blocked flags, and up to four source triangle IDs.
- Updated the specs-only validation ledger with issue, evidence, cause, ideal
  fix, and status:
  - `docs/specs/performance/performance.md`
  - `docs/architecture/player-npc-systems/npc-movement.md`
  - `docs/specs/networking/networking.md`
  - `docs/specs/networking/contact-authority.md`
  - `docs/specs/npc-behavior/npc-behavior.md`
  - `docs/architecture/collision/collision.md`
  - `docs/specs/hotreload/hotreload.md`
- Appended evidence to:
  - `docs/regressions/performance/server-tick-budget-REG-2026-11-03.md`
  - `docs/regressions/npc-movement/payload-combat-priority-REG-2026-11-08.md`
  - `docs/regressions/collisions/funworld3-cylinder-embedding-REG-2026-11-08.md`
- Recorded the `agent-lane.py check` C++ limitation in
  `docs/specs/dev-loop/agent-lanes-build-authority.md`.

## Validation and evidence

### Static

- `python devscripts/module_ownership.py`: PASS,
  `module_sources=9 exe_sources=579`.
- Catalog now lists `chunk-collision` events `[400,401,402,403,404,405,406]`
  and `npc-behavior` events `[500..508]`.

### Build

- Lane dev-loop build: `2026-11-08T20:22:35.804761Z`
- Build ID: `2026-11-08T20:22:35.804761Z:2bf5d9c0d537cdbfd56d7a910b064dfbe66d02efe811868e9b24c6d4b6bcbc1f`
- `build_source_revision=32a7939e...`, `changed_during_build=false`
- Module generations activated in the same PID:
  - `npc-behavior` generation 1, code hash
    `2003923f801e579e52601d2d1cd4b34683ff9fc0a76aed7a0fd6c1d914b50e49`
  - `chunk-collision` generation 1, code hash
    `369454457bcc434232cb99c843c9b3b5728741fab016abe583c0a1ccb6ed631d`
- No module build failure was recorded.

### Runtime

Run: `20261108_202307`
- Server: PID 19672, journal
  `logs/2026-11-08/20261108_202307/server-01/events.jsonl`
- Client: PID 10756, journal
  `logs/2026-11-08/20261108_202307/client-01/events.jsonl`

Confirmed live:

- `module.activated` for `npc-behavior` generation 1 and `chunk-collision`
  generation 1.
- `collision.actor-world` with `generation=1`, sampled source triangle IDs,
  grounded/blocked flags, and finite correction. Example:
  `source_triangles=[13744,7362,0,0]`, `grounded=true`,
  `max_penetration=0.009478747844696045`.
- `npc.attack-gate` records include target ID and suppression reason. Samples
  were `target_id=0`, `suppression_reason=no_visible_target`; the Payload cart
  role path was not exercised because `payload_objective=false` during the
  stalled run.
- The navigator stall remains: `npc.slow-update` examples show
  `navigator_ms=3058.2` and `3543.8`, with `path_ms` near zero, and
  `performance.server-tick-stall` totals of 3.5–7.9 s.
- The client reproduced the reconnect loop: `network.session.health` cycled
  `Connected -> Reconnecting -> Connected` with `disconnect_policy=timeout`
  after the server stopped answering for ~2.5 s. This is consistent with the
  navigator stall, not with the actor-world or NPC policy code.

## Limits

- No 60 Hz, 240 FPS, hit-authority, or full Payload role acceptance is claimed.
- The actor-world module covers the bounded contact/correction/response slice,
  not the full triangle narrowphase. Dynamic-object, actor-pair, and
  weapon/projectile solver migration remain deferred.
- The cylinder report remains REPORTED / NOT YET REPRODUCED. The new provenance
  is available, but the actual FunWorld3 cylinder geometry and human review are
  still required.
- `collision.actor-world` currently records `actor=0` for the local player
  because `spawnGeneration` was zero in this run. Source triangle provenance is
  still present; a stable actor ID is a follow-up diagnostic improvement.
- The Payload role code was not covered live because the cart objective was not
  active during the stalled run.
- `agent-lane.py check` cannot validate C++ changes by policy; `status` was used
  to observe the lane build. This is recorded in the lane specification.
- Build-generated changes (`config/analytics.json`, `src/pch.h.d`) were not
  authored or claimed by this session.

## Documents and skills

Read:
- `AGENTS.md`
- `docs/ROUTER.md`
- `docs/specs/dev-loop/agent-lanes-build-authority.md`
- `docs/specs/hotreload/hotreload.md`
- `docs/architecture/collision/collision.md`
- `docs/specs/npc-behavior/npc-behavior.md`
- `docs/specs/performance/performance.md`
- `docs/specs/networking/networking.md`
- `docs/specs/networking/contact-authority.md`
- `docs/skills/spec-behavior-review-v1.md`
- `docs/skills/logging-checker-v1.md`
- `docs/skills/efficiency-checker-v1.md`

## Human review still required

- Confirm the 19-NPC navigator stall fix in a later performance pass.
- Confirm the client no longer cycles loading/reconnecting after the server
  holds 60 Hz.
- Confirm Payload push slots, 25 m leash, combat interruption, and no
  shoot-through-cover in a responsive live run.
- Confirm the FunWorld3 cylinder source and behavior on the actual geometry.
- Confirm 240 FPS and hit-claim authority.
