# Collision, melee, and FCS trace handoff

**EST timestamp:** 2026-11-17 21:27 EST  
**Branch:** `checkpoint/one-checkout-condensation-20261111`  
**Commits:** none created.

## Result

Created `docs/handoffs/collision-melee-fcs-trace-handoff.md` with the source
ownership trace, current live journal findings, evidence gaps, and an ordered
diagnostic implementation proposal. This is a source/documentation handoff;
no gameplay or diagnostic code was changed.

## Findings

- World contact details already exist at
  `src/physics/movement/actor-triangle-solver.cpp`, including point, response
  normal, surface normal, penetration, triangle/entity, and velocity before and
  after. Detailed contact events require Collision Trace logging. The regular
  `collision.actor-world` event in `physics-collision-core.cpp` is aggregate.
- Client melee claim contains target, contact ID, point, direction, blade
  endpoints, and force. Server acceptance also contains contact ID and damage,
  but omits the validated point/direction. `server.damage.applied` does not
  consistently include the melee correlation ID, leaving a journal join gap.
- FCS host objective context, NPC utility selection, goal selection, navigation
  planning, and progress diagnostics exist. Their key context is split across
  events; the chosen formation slot and utility score are not consistently
  joined to a failed path.
- Read-only scan of run `20261118_020227` found 566
  `collision.actor-world`, 1,106 `collision.bounce.applied`, 272
  `npc.nav-plan-failed`, 210 `npc.movement-progress-failed`, 731
  `npc.objective-context`, and 782 `npc.utility-decision` events. Latest utility
  decisions were largely T `MoveToObjective` and CT `DefendSite`; this does not
  prove the CTs were at authored spawns. Many T navigation failures were
  `blocked` or `initial`; the carrier had a failed plan to the objective area.
  A current human snag/melee reproduction was not present in this run.

## Files and line anchors

- Added `docs/handoffs/collision-melee-fcs-trace-handoff.md` (purpose and scope;
  source trace; live evidence; proposed implementation and acceptance).
- No source/config files changed in this session. Existing workspace changes
  were preserved, including earlier user-requested edits to
  `config/gamemodes/funcounterstrike.json` and `config/healthbar.json`.

## Instructions and focused review

- Followed `AGENTS.md`, `docs/ROUTER.md`, and
  `docs/specs/dev-loop/agent-lanes-build-authority.md`: shared checkout only,
  no lane/build request, no build wait.
- Read collision/movement, melee/contact authority, FCS/NPC behavior,
  structured logging, scenario recording/runtime validation, time-format,
  task-completion, and regression routing documents.
- `docs/skills/spec-behavior-review-v1.md`: PASS for source ownership trace;
  no spec change or implementation was made. Runtime/human acceptance remains
  needed for each reported behavior.
- `docs/skills/documentation-checker-v1.md`: PASS for handoff purpose, audience,
  scope boundary, and routed references.

## Validation and remaining work

- Inspected the source owners and counted selected event names in the exact
  server journal path documented in the handoff. No tests were added or run.
- Build and runtime code validation: not applicable; source did not change.
- Human review: needed to accept the handoff's proposed diagnostic scope before
  code changes. Follow the five ordered steps in the handoff, then exercise a
  human-recorded corner snag and a live FCS objective round through the root
  dev-loop and inspect that run's journal.
