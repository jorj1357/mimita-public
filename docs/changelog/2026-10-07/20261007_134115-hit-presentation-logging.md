# Hit presentation diagnostics

- Status: `PASS_WITH_HUMAN_REVIEW`
- Timestamp: `2026-10-07 13:41:15 -04:00` (America/New_York)
- Branch: `afad20a-rebuild`
- Scope: Add bounded structured diagnostics for remote tracers, confirmed
  local-victim damage, and tracer/blood render-distance decisions.

## Pre-existing work preserved

The worktree already contained unrelated edits in configuration, NPC/server
code, weapon/UI code, planning documents, and earlier changelogs. In
particular, `config/debuglogger.json` already had the `rendering` category
disabled and `file_output` false; this session did not change that policy.

## Source changes

### `src/engine/engine-tick-net.cpp`

The remote-shot tracer call now retains the returned `EffectPart*` and writes
`presentation.tracer_spawn` when the Rendering structured category allows
Important records. It records shooter, target, local-target status, weapon,
spawn acceptance, camera distances to the tracer origin and hit, beam length,
and packet effect flags. The old behavior spawned the tracer and discarded the
result without recording whether the spawn policy rejected it.

### `src/network/multiplayer-projectiles.cpp`

The confirmed local-victim branch now writes
`presentation.local_victim_damage` before calling the existing victim hit
effect path. It records attacker, victim, damage, weapon, victim-effect gate,
server hit position and normal, camera distance, and camera position. The
existing blood/effect behavior is unchanged.

### `src/effects/effect-part-render.cpp`

The renderer now aggregates once-per-second Rendering diagnostics for tracer
visibility, tracer culls caused by the generic 40 m policy, other generic
effect culls, blood particle/decal visibility and culls, and the active blood
render/fade distances. No render threshold or effect behavior was changed.

## Documents and focused reviews

- `docs/ROUTER.md`
- `docs/specs/weapons/weapons.md`
- `docs/specs/networking/networking.md`
- `docs/specs/debug-logging/debug-logging.md`
- `docs/workflows/runtime-scenario-validation.md`
- `docs/operations/build-and-exe/build-and-exe.md`
- `docs/operations/task-completion/task-completion.md`
- `docs/skills/spec-behavior-review-v1.md`
- `docs/skills/logging-checker-v1.md`
- `docs/regressions/regressions-v1.md`

## Validation

- `git diff --check`: passed for the three session-touched source files.
- Canonical build: `SUCCESS`; affected objects were forced/recompiled after
  the first incremental run incorrectly reported `Nothing changed`.
- `mimita.exe --versioninfo`: passed.
- Exact journal path: `logs/10-07-2026/20261007_134108/events.jsonl`.
- Connected Counter-Strike gameplay run: not performed.
- Human visual acceptance: not performed.

## Follow-up required

Enable the pre-existing `rendering` StructuredLogger category at `important`
with file output for the next live run, reproduce nearby and distant NPC
shots, and inspect the new event IDs. The diagnostics are intended to decide
whether the next patch must change event routing, the generic tracer cull, the
blood distance configuration, or the first-person victim presentation path.

