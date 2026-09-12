# Live-code phases 3-4: shared actor + presentation modules, and the smoke-test fix

- EST timestamp: 2026-09-12 06:12:38 EDT (UTC 2026-09-12T10:12:38Z)
- Branch: `8292026stash`
- Commits: none (work left uncommitted for human review)
- Result: `PASS_WITH_HUMAN_REVIEW`

## Task

Continue the bootstrap: implement Phase 3 (shared Actor model with hot actor
decisions) and Phase 4 (hot visual/UI presentation), add a gold reference, and
fix the self-test flaw found during live use.

## The live failure and the fix

The human edited `src/effects/effect-part.cpp` from
`effect.position[0] += effect.velocity[0] * dt;` to `... * dt * 1.1f;`, saved,
and the candidate was rejected; reverting restored activation. Live game and
networking stayed up.

Root cause: `gameSelfTest` pinned the exact output (`position[0] == 0.1`), so any
intended behavior change failed validation.

Fix: `src/effects/effect-part.cpp` `gameSelfTest` now asserts determinism
(identical inputs -> identical outputs), finiteness, forward motion in a sane
bound, alive below max lifetime, and sticky immobility. Re-verified by applying
`* 1.1`, rebuilding the DLL, running `--live-code-selftest` -> PASS, then
reverting and rebuilding.

## Exact files and changes

- `src/hot-reload/game-api.h`: added `ActorStateV1`, `ActorCommandV1`,
  `ActorEventV1`, actor function typedefs, `GameActorModuleV1`;
  `DamageNumberStyleV1`, `RocketTrailStyleV1`, presentation typedefs,
  `GamePresentationModuleV1`; actor/presentation state versions and event ids.
- `src/hot-reload/game-modules.h` (new): internal module entry-point
  declarations.
- `src/hot-reload/modules/actor-behavior.cpp` (new): hot `chooseActorCommand`,
  `updateActorEmotion`, `chooseActorRole`; publishes the `actor` module.
- `src/hot-reload/modules/presentation.cpp` (new): hot `formatDamageNumber` and
  `rocketTrail`; publishes the `presentation` module.
- `src/effects/effect-part.cpp`: `gameSelfTest` rewritten to invariants;
  `MimitaGetEffectModule()` added; `GetGameAPI` publishes effect + actor +
  presentation modules from `game-modules.h`.
- `src/hot-reload/hot-modules.json`: added the actor and presentation sources
  and `game-modules.h` to change detection.
- `src/live-code/live-modules.h/.cpp` (new): ABI-checked module lookup by name.
- `src/live-code/live-actor.h/.cpp` (new): EXE bridge building `GameEnvelope`s
  and calling the actor module.
- `src/live-code/live-presentation.h/.cpp` (new): EXE bridge for damage-number
  and rocket-trail styles.
- `src/npc/npc.h`: added `Npc::liveRoleId`.
- `src/npc/npc.cpp`: added `applyLiveActorBehavior` (`:411`), called at `:1179`;
  projects the NPC into `ActorStateV1`, runs emotion/role/command, scales the
  shared movement input, and writes emotion/role back.
- `src/effects/effect-part-particles.cpp`: `spawnDamage` (`:28`) builds a base
  damage style, lets the hot presentation module override it, then spawns.
- `src/combat/weapon-rocket-launcher.cpp`: after reading smoke config, lets the
  hot presentation module override trail params (`:318`).
- `src/live-code/live-code-selftest.cpp`: now also verifies actor module
  presence, `chooseCommand`, `chooseRole`, damage formatting, and rocket trail.
- `src/main-systems.cpp`: `registerLiveCodeCommands()`.
- Docs: `docs/gold/2026-09-12-live-code-hot-reload-reference.md` (new),
  `docs/features/live-code-development/live-code-development.md` (status).

## Reasoning

- The actor module only returns plain data; the EXE owns identity, health,
  velocity, emotion write-back, and physics. This satisfies "hot code receives
  state and returns decisions" without transferring engine objects.
- The NPC path was wired first because it is deterministic and server-side;
  player/remote-actor decoration can reuse the same `ActorStateV1` mapping later.
- Presentation overrides are optional: when the module is absent or returns
  false, the existing config-driven behavior is unchanged.

## Validation and evidence

- `python build_game_dll.py` -> success, 3 sources (effect, actor, presentation).
- `python build_agent.py` -> `Status: SUCCESS`.
- `mimita.exe --live-code-selftest` -> `PASS` with actor and presentation checks.
- `* 1.1` experiment: DLL built, self-test `PASS`, then reverted.
- Journal under `logs/features/live-code/2026-09-12/`.

## Focused skills

- `docs/skills/spec-behavior-review-v1.md`: PASS. The actor/presentation contracts
  match the feature specification; no spec-code disagreement.
- `docs/skills/logging-checker-v1.md`: PASS. Hot decisions are observable through
  the existing journal and notification events; no new per-frame spam was added.

## Human review still required

- Visual acceptance of damage-number crit formatting and the enlarged rocket
  trail in a live match.
- Live acceptance that editing `actor-behavior.cpp` visibly changes NPC behavior
  while the fight continues.
- Notification/success-sound acceptance.

## Pre-existing edits preserved

Unrelated working-tree changes (combat, config, gamemode, network, npc, render,
and others) were not reverted or claimed.
