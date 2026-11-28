// 2026-11-28 08:31 UTC
/* purpose
* record one bounded independent issue-review activation
* append evidence/counterevidence/next-check review to issue-8e48cd0bc786
* append one measured process proposal to issue-dad84e4e7c50
* no source, build, close, run, or fix claim by this session
*/

# Independent bounded review of issue-8e48cd0bc786

- Task ID: wake-246c95495aac
- Work session: work-6ffa618396a8
- Agent: issue-review-sweeper
- Focus: one read-only review of the floating Rocket Launcher explosion-mark issue: readiness, proof status, current build identity, and the latest surviving rocket journals
- Summary: Readiness still complete (`missing=[]`, ready=true, 1 active task, state `in_review`, no fix proven). Proof record `task-proof-9d05ae9ca4ff` is still self-contradictory (`status=runtime_proven` with `result=unproven`, empty `verified_utc`/`build_id`, `game_generation=0`, deleted journal). The world-only gate is present in the current generation-816 snapshot. New surviving recent rocket bases exist on gen 808/814 (many explode-world/player/lifetime terminals), and run `20261128_061051` contains 16 surviving local `rocket.impact-mark.resolved` world-triangle hits whose `impact_position == mark_position`. Across all `logs/2026-11-28` there are still zero `impact_decal.spawn`, `impact_decal.render_submitted`, and `hitfx_trace` lines; tracing defaults off. Actor-hit and open-air paths remain unproven. Appended review id 568 and one process proposal to `issue-dad84e4e7c50`. No source edit, build request, run, close, or fix claim.
- Status: complete (review only)
- Date, time, timezone: 2026-11-28T08:31Z, ISO 8601

# Pre-existing changes

- The shared checkout contains extensive pre-existing modifications from other agents (config files, `src/vehicles/*`, `src/network/*`, `docs/issues/*`, and many changelogs). This session edited no source or configuration, reverted nothing, and touched only the issue records through `agent-coord.py`, coordination state, and this changelog. No compiler, `build.py`, module worker, second dev-loop, or game runtime was invoked.

# Requested behavior

Run one bounded independent review of `issue-8e48cd0bc786`: inspect readiness/proof/linked artifacts/latest journals; append one evidence-grounded review with observed facts, inference/uncertainty, contrary evidence, and the next discriminating check; then a short development-process review with one specific, measurable proposal to the best existing improvement issue (or state none). Do not edit source, build, close, or claim a fix.

# Specification alignment

- AGENTS.md gates 0b/0c/0d, 2, 3, 4: coordination begin/observe, observations separated from inference, no compiler/build/second dev-loop in a review wake.
- `docs/specs/dev-loop/agent-proof-and-verification.md`: a build is not behavior proof; acceptance requires a pinned non-zero-generation journal.
- `docs/specs/dev-loop/agent-wake.md`: a woken agent is a bounded activation that checks proof state before ending.

# Exact implementation changes

None. No source, specification, or configuration change.

# Changed files and reverse steps

| File | Before | After | Why | Reverse |
|---|---|---|---|---|
| `docs/issues/bug/20261121-rocket-launcher-explosion-marks-float-away-from--issue-8e48cd0bc786.md` | reviews 57/211/301/404/459/507 | review 568 (inconclusive) appended | durable independent review evidence | remove only appended review text |
| `docs/issues/improvement/20261124-require-runtime-event-proof-for-every-gameplay-c-issue-dad84e4e7c50.md` | existing review/suggestion history | one proof-record-consistency process proposal appended | measured repeated process friction; no duplicate issue | remove only appended suggestion |
| `.dev/agent-coord/brain.db` | no review id 568 | review id 568 + suggestion | coordination record | restore from backup if withdrawn |
| `docs/changelog/2026-11-28/20261128_0831-independent-issue-review-issue-8e48cd0bc786.md` | absent | added | one final session changelog | delete file |

# Validation

- `python devscripts/agent-coord.py begin`, `sync`, `observe`, `issue brief`, `proof status`, `activity set`, `issue review`, `issue suggest`, and `end`.
- Read-only artifact checks: `build/game/current.json`, `.dev/build-snapshots/game-dll-20261128T081455389961Z-9fcc1f/source/src/network/{server,multiplayer}-projectiles.cpp`, `src/effects/hit-effects.cpp`, `src/effects/effect-part.cpp`, `src/effects/effect-part-render.cpp`, `src/effects/hitfx-commands.cpp`, `config/impact_decals.json`, and the 2026-11-28 and recent client/server journals via `rg`. No build or run requested.

# Measured evidence

- Readiness: ready=true, score 8/8, missing=[], state in_review, 1 active task `rocket-impact-placement-proof`.
- Proof `task-proof-9d05ae9ca4ff`: status=runtime_proven, result=unproven, verified_utc="", build_id="", game_generation=0, run_id=20261121_000922; cited journal no longer exists. Not evidence.
- Current build: generation 816, status ok, code_hash `6b13b5de7473f0efee423da1a1ac8624b5c651ef28b9282954a381fd16daba04`, generated 2026-11-28T08:15:20Z, dll `game-g816.dll`.
- Gate in gen-816 snapshot: `server-projectiles.cpp:747` world-only `reserved[0]`; `multiplayer-projectiles.cpp:1297/1326/1332` `reserved[0] != 0`; `hasWorldImpactAnchor` at 2502-2506.
- Surviving recent rocket bases:
  - `20261128_062046` (gen 808, payload, mimita-pipes): 176 explode-world, 28 explode-player, 3 explode-lifetime, 301 npc.projectile-created.
  - `20261128_054617_01` (gen 814, vehicle arena, slaughterhouse1): 821 explode-world, 106 explode-player, 73 explode-lifetime.
  - `20261128_061051` (gen 814, vehicle arena): client-01 16 `rocket.impact-mark.resolved`, all `impact_surface=world_triangle`, `mark_applied=true`, `mark_surface=static_world_triangle`, `nearest_world_surface_distance_m=-1.0`, `impact_position == mark_position`.
- Across all `logs/2026-11-28`: 0 `impact_decal.spawn`, 0 `impact_decal.render_submitted`, 0 `hitfx_trace`. Tracer default off (`hit-effects.cpp:34`).
- `config/impact_decals.json`: enabled=true, explosionMarks.enabled=true.

# Runtime evidence separation

- Source: current checkout and gen-816 snapshot inspection.
- Build evidence: none claimed; the cited runs use existing published gen 808/814 artifacts.
- Runtime evidence: recent rocket terminals and 16 local world-triangle mark resolutions are real, but they do not include spawn/render submission and never enabled hitfx tracing. Actor-hit and open-air mark behavior is unproven. No fix is proven.
- Human acceptance: none claimed.

# Regression review

- Regression entry appended: no. This is one bounded review plus one process proposal; no behavior change.
