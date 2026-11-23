# Issue-8e48cd0bc786 Independent Review

# Task

- Task ID: bounded independent review of issue-8e48cd0bc786 (wake-604df6e18ff9)
- Session ID: session-efb3dfd7562c / work-c1b3302b9d43
- Owner: issue-review-sweeper
- Focus: Rocket launcher explosion marks float away from world surfaces
- Summary: Reviewed issue readiness, proof state, committed/compiled gate source, the current generation-513 build, and the latest rocket-bearing journals. Appended one evidence review (verdict `inconclusive - gate committed and compiled; no hitfx-traced runtime proof`, review id 211) and one measured process proposal to issue-b396205aaecf. No source, build, run, close, or fix was performed.
- Status: REVIEW_RECORDED_OPEN
- Date, time, timezone: 2026-11-22T20:34:00Z

# Pre-existing changes

- The shared checkout already contained extensive modifications from other agents. This session edited no source and reverted nothing.
- Files created by this session: this changelog and temporary body files outside the repository. The issue review and suggestion were appended through `agent-coord.py`.

# Requested behavior

Complete one bounded independent review of issue-8e48cd0bc786 without editing source, building, closing, or claiming a fix; record observed facts, inference, uncertainty, contrary evidence, and the next discriminating check, plus a measured process proposal when supported.

# Findings

- Readiness is complete (`missing=[]`, 1 active task `rocket-impact-placement-proof`); issue remains `in_review`.
- Proof `task-proof-9d05ae9ca4ff` is still contradictory: `status=runtime_proven`, `result=unproven`, empty `verified_utc`/`build_id`, `game_generation=0`, and cites `logs/2026-11-21/20261121_000922/client-01/events.jsonl` which does not exist (that run dir holds only `run.json`, 4727 B). Root cause is tooling: `proof_add()` defaults `status='runtime_proven'` at creation (`devscripts/agent_coord.py:1430-1451`) and `proof_verify()` is optional (`:1453-1496`).
- The world-impact gate is now committed, not working-tree-only: `git blame` attributes `src/network/server-projectiles.cpp:740` and `src/network/multiplayer-projectiles.cpp:2416-2420` to `d89884f6` (2026-11-21 16:47:59 -0500); the rocket source files are clean against HEAD.
- Current build `generation=513` (`build/current.json`, `game-g513.dll`, built 2026-11-22T19:49Z) and its snapshot `.dev/build-snapshots/game-dll-20261122T194850321333Z-5c98ea/source` contain the gate at server:740 and multiplayer:1232/1261/1267,2416-2420.
- Source mapping (`server-projectiles.cpp:1955-2001`) sets reserved[0]=1 only for `"world"`; `"player"`, `"entity"`, and `"lifetime"` suppress the mark.
- Run `20261122_174937` (gen 507, mode 44 juggernaut horde, dust2cyberiav4) is real rocket evidence: 389 rocket projectiles, 328 `PROJECTILE_POLICY_EVENT` (202 WorldImpact, 65 PlayerImpact, 61 LifetimeExpired), 271 `world.destructible-explosion`. But it never enabled `hitfx_trace`, so its 0 `impact_decal.spawn` / 0 `impact_decal.render_submitted` are expected and prove nothing.
- Contrary evidence: `src/pobjects/persistent-physics.cpp:403 doExplosion` remains an unconditional sibling mark path (default `spawnMark=true`, +Z normal); offline `src/combat/weapon-rocket-launcher.cpp:158-171` marks actor impacts while the network path suppresses them.

# Specification alignment

- Followed the routed issue-review and proof rules: readiness and proof inspected, facts separated from inference, no behavior or fix claim made for the unproven change, all prior issue text preserved, and no gate/build authority invoked.

# Exact implementation changes

- None. Append-only issue review and suggestion only.

# Validation

- Build status: no build requested; no compiler, worker, build script, or dev-loop invoked.
- Runtime evidence: read-only inspection of `build/current.json`, the generation-513 snapshot, `logs/2026-11-22/20261122_174937/{server,client}-01/events.jsonl`, `logs/2026-11-21/20261121_000922/`, the proof store, and the gate source.
- Review artifact: review id 211 on issue-8e48cd0bc786.
- Process proposal: appended to issue-b396205aaecf (baseline 37 `runtime_proven` rows with empty `verified_utc`; 31 with a cited journal missing on disk).

# Measured evidence

- Rocket run 20261122_174937: 389 rocket `npc.projectile-created`; 328 rocket policy events (202 WorldImpact, 65 PlayerImpact, 61 LifetimeExpired); 271 `world.destructible-explosion`; 130 client `explosion.shockwave.spawn`; 0 `impact_decal.spawn`; 0 `impact_decal.render_submitted`.
- Proof integrity: 65 `status=runtime_proven` rows, 37 with empty `verified_utc`, 31 of 39 with a named journal whose file is gone.

# Regression review

- Regression entry appended: no.
- Why: no behavior change was made; this was a read-only review with append-only issue updates.

# Human acceptance

- Visual review: not performed by this session.
- Gameplay review: not performed; the rocket-impact behavior remains unproven at runtime.
- Still unverified: a hitfx-traced pinned run showing `impact_decal.spawn` + `impact_decal.render_submitted` for a world hit and no mark for actor/open-air cases, plus multi-angle visuals.

# Agent handoff context

- Owner: rocket-impact-investigation (issue and task); build-runtime-subagents (proof-integrity proposal on issue-b396205aaecf).
- Next exact step: pin one post-gate run (>=507; current 513) with `hitfx_trace 1`, ideally a mode-44 horde run like 174937 or a mode-43 rocket duel, and require the correlated spawn/render events in review id 211.
- Do not redo: this readiness/proof check, the gate-commit blame, the generation-513 snapshot check, or the run-174937 event counts.

# How to reverse

- No source or build to reverse. Remove this changelog only through normal review if created in error; preserve the append-only issue review and suggestion.
