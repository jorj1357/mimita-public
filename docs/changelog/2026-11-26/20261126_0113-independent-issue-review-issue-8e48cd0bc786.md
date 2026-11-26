// 2026-11-26 01:13 UTC
/* purpose
* record one bounded independent issue-review activation
* append evidence/counterevidence/next-check review to issue-8e48cd0bc786
* append one non-duplicate rocket-proof instrumentation proposal to issue-b396205aaecf
* no source, build, close, or fix claim by this session
*/

# Task

- Wake ID: wake-5a57be69dc90 (bounded independent review)
- Session ID: work-c5c627d489d2
- Owner: issue-review-sweeper
- Focus: independent review of issue-8e48cd0bc786 (rocket launcher explosion marks float away from world surfaces)
- Summary: Re-verified readiness (ready=true, 8/8, missing=[], state in_review,
  1 active task, no fix proven). Confirmed the world-only gate is present in
  the current generation-662 snapshot (server-projectiles.cpp:741;
  multiplayer-projectiles.cpp:1303/1332/1338). Found two surviving
  generation-627 rocket-launcher runs (20261125_175329 and 20261125_184820,
  fun counter strike) with 2,581 explode-world, 517 explode-player and 39
  explode-lifetime projectile terminals plus 419 client rocket shockwaves,
  decals config enabled, and still zero impact_decal.spawn/render_submitted
  because hitfx tracing was never enabled. Found the first surviving
  rocket.impact-mark.resolved event (run 20261124_205956) for a direct world
  hit with mark_applied=true and mark_position equal to impact_position; the
  local actor-impact path that can anchor to the nearest world triangle within
  8 m remains unexercised. Appended review 459, one process proposal on
  issue-b396205aaecf, and this changelog.
- Status: bounded review complete; no code changes
- Date, time, timezone: 2026-11-26T01:13Z, ISO 8601
- Branch: (left as checked out by the shared dev-loop)
- Base commit: none recorded
- Final commit: none

# Pre-existing changes

- `git status --porcelain` showed many pre-existing modified/untracked files
  from other agents and the rocket-league work. This session attributed,
  reverted, or claimed none of them.
- Files added/modified by this session are listed below only.

# Requested behavior

- The wake prompt asked issue-review-sweeper to complete one bounded
  independent review of issue-8e48cd0bc786 starting with issue brief/readiness,
  proof status, linked artifacts, and the latest relevant journals; append one
  evidence-grounded review with observed facts, inference/uncertainty,
  contrary evidence, and the next discriminating check; then append at most
  one evidence-backed process improvement or state none found. No source edit,
  build, close, or fix claim.

# Specification alignment

- `AGENTS.md` gate 0c and `docs/workflows/issue-intake-and-triage.md`:
  append-only issue evidence; no duplicate issue; no fix claim without proof.
- `docs/specs/dev-loop/agent-proof-and-verification.md`: a build is not
  behavior proof; require a pinned non-zero-generation journal.
- `docs/specs/dev-loop/agent-wake.md`: a woken agent is a bounded activation
  that checks proof state before ending.

# Exact implementation changes

- No source or configuration changes.
- Added coordination/repository records only: one issue review (id 459) on
  issue-8e48cd0bc786, one process suggestion on issue-b396205aaecf, and this
  final changelog.

# Changed files and reverse steps

| File | Before | After | Why | Reverse |
|---|---|---|---|---|
| `docs/issues/bug/20261121-rocket-launcher-explosion-marks-float-away-from--issue-8e48cd0bc786.md` | reviews 57/211/301/404 | review 459 (inconclusive) appended | durable independent review evidence | remove appended review |
| `docs/issues/improvement/20261120-runtime-registry-and-build-to-proof-links-issue-b396205aaecf.md` | existing proof-link proposals | one rocket trace-toggle/proof-validation proposal appended | measured repeated process gap, no duplicate issue | remove appended suggestion |
| `docs/changelog/2026-11-26/20261126_0113-independent-issue-review-issue-8e48cd0bc786.md` | absent | added | one final session changelog | delete file |

# Validation

- `python devscripts/agent-coord.py begin`, `sync`, `observe`,
  `issue readiness`, `proof status`, `activity set`, `issue review`,
  `issue suggest`, and `end` completed with agent issue-review-sweeper.
- Read-only artifact checks: `build/game/current.json`, `build/game/ready.json`,
  `.dev/build-snapshots/game-dll-20261126T005556909723Z-fa8154/source`,
  `.dev/build-timings/game-dll-20261125T012133976103Z-b7c31e.json`,
  `src/combat/weapon-rocket-launcher.cpp`, `src/effects/hit-effects.cpp`,
  `src/effects/effect-part.cpp`, `src/effects/effect-part-render.cpp`,
  `src/effects/hitfx-commands.cpp`, `config/impact_decals.json`, and the
  pinned client journals. No dev-loop build or run was requested by this
  session.

# Measured evidence

- Readiness: ready=true, score 8/8, missing=[], state in_review, 1 active task.
- Proof record `task-proof-9d05ae9ca4ff`: status=runtime_proven,
  result=unproven, game_generation=0, build_id="", verified_utc="",
  run_id=20261121_000922, journal `logs/2026-11-21/20261121_000922/client-01/
  events.jsonl`; `Test-Path logs/2026-11-21` is False. Not evidence.
- Current build: generation 662, status ok, code_hash
  `24d73b811b3d4a8a3bb5e36ded1034730932592456aaf5fc0565c199521182c4`,
  revision `e4b902e4...`, generated 2026-11-26T00:59:11Z.
- Surviving gen-627 rocket bases (artifact build_id
  `2026-11-25T01:21:28.987918Z:5dda0879...`, mapped by build-timings to 627):
  - `20261125_175329`: 578 explode-world, 117 explode-player,
    6 explode-lifetime, 147 continue; 71 client rocket shockwaves; 2
    impact_decals.config_read; 0 decal spawn/render; 0 hitfx.
  - `20261125_184820`: 2003 explode-world, 400 explode-player,
    33 explode-lifetime, 949 continue; 348 client rocket shockwaves; 2
    config reads; 0 decal spawn/render; 0 hitfx.
- First surviving `rocket.impact-mark.resolved`:
  `logs/2026-11-24/20261124_205956/client-01/events-0005.jsonl:8138`,
  impact_surface=world_triangle, mark_surface=static_world_triangle,
  mark_applied=true, world_triangle_index=9140, impact_position ==
  mark_position == [183.832,-298.669,735.635].
- `config/impact_decals.json`: enabled true, explosionMarks.enabled true;
  loaded fingerprint `6989:b53d80c391645c31` in both new runs.
- No rocket impact run exists on generations 628-662; newest gen-662 runs are
  vehicle-arena (Rocket League) free play with zero rocket policy events.

# Runtime evidence separation

- Source: current checkout inspection and coordination/issue records.
- Build evidence: none claimed; the cited runs use an existing published
  generation-627 artifact, and no build was requested by this review.
- Runtime evidence: the two gen-627 journals prove more than 3,000 rocket
  terminals reached client FX without decal events while tracing was off;
  they do not prove mark placement either way. The one mark-resolution event
  proves a direct world hit anchored exactly for the local path. No fix is
  proven and the issue remains open.
- Human acceptance: none claimed.

# Regression review

- Regression entry appended: no. This is one bounded review of a known
  user-visible bug plus one process proposal; it introduces no behavior
  change.
