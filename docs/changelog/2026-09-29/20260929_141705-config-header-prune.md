# Remove unreferenced config globals

Time: `2026-09-29T14:17:05-04:00` (EST)

## Scope

First staged repository-cleanup pass from `docs/specs/20260929plan.md` Part 5.
This pass removed only inline globals with zero non-document references in the
repository. It did not change collision ownership or gameplay behavior.

## Changed file

`src/config.h`

Removed:

- `DebugConfig::ENABLE_DEBUG_LOGS`
- `DebugConfig::PHYSICS_VERBOSE`
- `DebugConfig::RENDER_VERBOSE`
- `DebugConfig::DEBUG_NPC_DEATH_FREEZE`
- `DebugConfig::DEBUG_COLLISION_DIAGNOSTICS`
- `DebugConfig::DEBUG_AUTH`

Also removed the comment that described the deleted collision-diagnostics flag.

## Reasoning and evidence

Repository-wide source search excluding the defining header returned zero
references for each symbol. Existing collision candidates were not removed:
`doFloorRecovery`, capsule recovery contacts, sweep/slide, body contacts,
weapon capsule setup, and actor-triangle spike all still have live callers.

The working tree contained unrelated pre-existing edits in configuration,
development-loop, gameplay, network, weapon, and documentation files. Those
changes were preserved and not attributed to this pass.

## Validation

`python build_agent.py` — PASS, return code 0; 216 compiled, 281 skipped;
approximately 286 seconds. The build produced `C:\mimita-v9\mimita.exe`.

This is compilation/link evidence only. No runtime or human gameplay
acceptance was required or claimed for this debug-global removal.

## Routed documents and focused review

- `docs/ROUTER.md`
- `docs/specs/20260929plan.md`
- `docs/specs/movement/movement.md`
- `docs/architecture/collision/collision.md`
- `docs/architecture/collision/collision-legacy-inventory-2026-09-29.md`
- `docs/architecture/collision/actor-triangle-deletion-candidates.md`
- `docs/skills/spec-behavior-review-v1.md`
- `docs/skills/efficiency-checker-v1.md`
- `docs/operations/build-and-exe/build-and-exe.md`
- `docs/operations/task-completion/task-completion.md`
- `docs/regressions/README.md`

Result: `PASS_WITH_HUMAN_REVIEW` for the broader cleanup plan; the next
collision deletion still requires caller migration and gameplay proof.
