# Generic hot effect lifecycle (runtime-unknown effects)

Date: 2026-09-15 17:00 EST (UTC 2026-09-15T21:00:00Z)
Branch: `8292026stash` (worktree, uncommitted)
Result: `PASS_WITH_HUMAN_REVIEW`

## SUBSYSTEM: effects (lifecycle/creation)

- COLD OWNER REMOVED (partially): effect lifetime/growth/fade/expiry is no longer
  necessarily a cold `EffectPartSystem` pool property. A new hot
  `hot.effect-lifecycle` system owns it for generic effect entities.
- HOT OWNER ADDED: `EffectLifetime` (`hot-effect.h`) + `hot.effect-lifecycle`
  (`render.frame`, priority 3). It ages, integrates Transform by Velocity, grows
  scale, fades alpha, and destroys on expiry. Creation reuses the existing
  generic `effect.spawn`/entity primitives.
- COLD MECHANISM REMAINING: particle/quad draw, `EffectPartSystem.render`,
  texture drawing (legitimate).
- STATE AUTHORITY: generic dynamic component `EffectLifetime` on an ordinary
  entity (no enum).
- COMPATIBILITY FALLBACK: cold `EffectPartSystem` composition branches remain for
  unmigrated effect types.
- RUNTIME-NEW PROOF: `hoteffect` command creates a brand-new effect (cube rises,
  grows, fades, expires) with no enum/switch/ABI field.

## Evidence

- `python build_agent.py` -> `Status: SUCCESS`.
- `--hot-combat-selftest` -> PASS incl. "runtime-unknown effect entity created
  (no enum/switch)", "hot effect ages, integrates, and grows", "hot effect
  expires and is destroyed".
- Full suite (9 selftests) -> PASS.

## Classification

- SELFTEST PROVEN: generic effect entity lifecycle; runtime-unknown effect; safe
  expiry/destroy.
- COMPILED INTEGRATION: `hot.effect-lifecycle` system; `EffectLifetime` schema.
- LIVE HOT-EDIT PROVEN: no.

## Bugs deferred

Effect timing/perfection and the remaining client `EffectPartSystem` composition
branches (bullet impact, blood, muzzle flash, decals, camera shake).

## Would a bug still require a cold restart?

For generic/new effects: no (hot). For the remaining client cold composition
branches: yes (cold-restart item #1 partially reduced).

## Update to the primary metric

"BUGS THAT STILL REQUIRE A COLD EXE REBUILD" item 1 (effects) is reduced, not
removed: the generic lifecycle is hot, the client composition branches remain.

## Next cold owner

Wire the client rocket-explosion event (`multiplayer-projectiles.cpp` explode
handling) to a generic hot effect entity so the cold `EffectPartSystem`
composition yields for that path; then migrate blood impact; then audio policy.

## Files changed

`src/hot-reload/hot-effect.h` (new),
`src/hot-reload/modules/presentation/effect-lifecycle.cpp` (new),
`src/hot-reload/hot-modules.json`,
`src/hot-reload/modules/presentation/debug-presentation.cpp`,
`src/network/hot-combat-selftest.cpp`,
`docs/architecture/live-development/hot-cold-audit.md`,
`docs/architecture/live-development/hot-kernel-next-steps.md`.
