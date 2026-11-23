# Issue-87d8c9c7ffb1 Independent Review

# Task

- Task ID: bounded independent review of issue-87d8c9c7ffb1 (wake-7291a4028e38)
- Session ID: session-b9408d0db837 / work-83654a5f7426
- Owner: issue-review-sweeper
- Focus: Payload cart is not collidable in Arcade mode
- Summary: Reviewed issue readiness, proof state, committed source identity, the current generation-519 published build, and the latest run journals. Appended one evidence review (verdict `unproven`, review id 212) and one measured process proposal to issue-b396205aaecf. No source, build, run, close, or fix was performed.
- Status: REVIEW_RECORDED_OPEN
- Date, time, timezone: 2026-11-22T21:02:00Z

# Pre-existing changes

- The shared checkout already contained extensive modifications from other agents. This session edited no source and reverted nothing.
- Files created by this session: this changelog. The issue review and suggestion were appended through `agent-coord.py`, which also updated the issue document.

# Requested behavior

Complete one bounded independent review of issue-87d8c9c7ffb1 without editing source, building, closing, or claiming a fix; record observed facts, inference, uncertainty, contrary evidence, and the next discriminating check, plus a measured process proposal when supported.

# Findings

- Readiness is complete (`missing=[]`, score 8); one open task `payload-cart-arcade-contact` remains owned by `payload-arcade-collision-fix`, which is inactive. Issue remains `in_review` with no linked build or run records.
- The fix is now in HEAD, not just the working tree: `src/physics/movement/physics-collision-glb-sweep-slide.cpp:836` emits `collision.arcade_entity.sweep`, and the physical-box candidate pass is near L266-284. Current `git diff` of the issue paths shows only unrelated `src/network/server-npcs.cpp` work.
- Source identity is pinned: sha256 of `physics-collision-glb-sweep-slide.cpp` = `6b5fd3cec1d3deab...` and `physics-types.h` = `f3f5ca679e7e59c3...`; both match proof `task-proof-7d953b610754` source hashes and the generation-519 build snapshot (`game-dll-20261122T204052787084Z-49370a`). Latest 20:54-20:56Z snapshots still carry the same sweep hash.
- Published build `generation=519` (revision `be87d2a8...`, published 2026-11-22T20:41:12Z) contains the fix.
- A post-fix run exists: `logs/2026-11-22/20261122_204150` loaded generation 519 with 1 player and 19 NPCs, and the Arcade capsule sweep path was active (`collision.speed_collapse` from `doGLBSweepSlide`, `collision.actor-world`). Its mode/map is `funcounterstrike`/`dust2cyberiav4`, not Payload/funworld3, so the cart was never present or contacted.
- No `collision.arcade_entity.sweep` occurrence exists anywhere under `logs/`. No run has exercised a player root capsule against the Payload cart.
- Contrary evidence: none supporting success. NPC `world_contact=false`/`grounded_this_tick=false` samples in run 204150 belong to the separate payload NPC-grounding issue.
- Dev-loop status at review time: `lifecycle=game_build_queued`, `build=published_current`, `run=(none)`, generation 519 retry pending after a `src/npc/recast-navigation.cpp` revision.
- Proof `task-proof-7d953b610754` still reads `result=unproven`, `status=runtime_proven`, empty `journal_path`/`run_id`, `game_generation=0`, empty `verified_utc`.

# Specification alignment

- Followed the routed issue-review and proof rules: readiness and proof inspected, facts separated from inference, no behavior or fix claim for an unproven change, all prior issue text preserved, and no build/run authority invoked.

# Exact implementation changes

- None. Append-only issue review and suggestion only.

# Validation

- Build status: no build requested; no compiler, worker, build script, or dev-loop invocation.
- Runtime evidence: read-only inspection of `build/current.json`, `.dev/build-snapshots/`, `logs/2026-11-22/20261122_204150/server-01/events.jsonl`, the proof store, and the collision source.
- Review artifact: review id 212 on issue-87d8c9c7ffb1 (verdict `unproven`).
- Process proposal: appended to issue-b396205aaecf.

# Measured evidence

- Generation-519 run 20261122_204150: generation=519, players=1, npcs=19, `collision.arcade_entity.sweep`=0, `payload.cart*`=0.
- Source hash match: `6b5fd3cec1d3deab...` (sweep-slide) and `f3f5ca679e7e59c3...` (physics-types) in working tree, proof record, and generation-519 snapshot.
- Latest 11-22 run journals present: 20261122_204150, 203657, 202850, 195603; none is a Payload/funworld3 cart run.

# Regression review

- Regression entry appended: no.
- Why: no behavior change was made; this was a read-only review with append-only issue updates.

# Human acceptance

- Visual review: not performed by this session.
- Gameplay review: not performed; cart collidability remains unproven at runtime.
- Still unverified: a pinned Payload/funworld3 Arcade run (generation 519 or any generation with the same source hashes) where a player contacts the cart and the journal shows `collision.arcade_entity.sweep` with the cart entity id, no penetration, and unchanged static-world Arcade contact.

# Agent handoff context

- Owner: `payload-arcade-collision-fix` (issue and task); `build-runtime-subagents` (proof-link proposal on issue-b396205aaecf).
- Next exact step: launch Payload/funworld3 through the root dev-loop pinned to generation 519, drive a player into the cart, and require the `collision.arcade_entity.sweep` event plus the pinned run/generation/DLL hash in the issue review.
- Do not redo: readiness check, source-hash/snapshot pinning, the generation-519 identification, or the 204150 non-Payload determination.

# How to reverse

- No source or build to reverse. Remove this changelog only through normal review if created in error; preserve the append-only issue review and suggestion.
