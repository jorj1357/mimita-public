# issue-1d1b9c62ac08 Independent Issue Review

# Task

- Task ID: wake wake-24255a218e9b, issue issue-1d1b9c62ac08 (Payload Arcade NPCs fall through the world)
- Owner: payload-arcade-collision-fix (task payload-arcade-npc-grounding, open)
- Focus: bounded independent review of issue brief/readiness, proof status, linked artifacts, latest relevant journals, plus one development-process review
- Summary: Read-only review. Appended one issue review (id 517, verdict `unverified`), one process proposal to issue-91e3384b219b, and one issue-linked question to payload-arcade-collision-fix. No source edit, no build, no compiler, no second dev-loop, no fix claimed, nothing closed.
- Status: REVIEW_RECORDED_OPEN
- Date, time, timezone: 2026-11-27T04:16Z

# Pre-existing changes

- The shared checkout already contained many other agents' uncommitted edits (config, devscripts, src, docs). This session edited no source and reverted nothing.
- Files created by this session: this changelog plus append-only records written through `agent-coord.py issue review / issue suggest / ask` into docs/issues markdown and the coordination DB.

# Requested behavior

- Complete one bounded independent review of issue-1d1b9c62ac08 starting from issue brief/readiness, proof status, linked artifacts, and the latest relevant journals; append observed facts, inference/uncertainty, contrary evidence, and one next discriminating check. Then do a short process review and, if evidence supports it, append one measurable proposal to the best existing improvement issue. Do not edit source, build, close, or claim a fix.

# Findings

- Readiness: ready=true, score 8/8, missing=[], state in_review; one open task payload-arcade-npc-grounding (owner payload-arcade-collision-fix). `proof list --issue` returns []; no accepted runtime proof exists.
- Published generation is now 757: build/game/generation-757/game-g757.dll, code hash d4d314f45fd1eeda2e0223524067f1f1ae6ad82b95b773caae1c3edf34c37ec5, published 2026-11-27T03:21:25.722Z from .dev/build-snapshots/game-dll-20261127T032050910894Z-474ca1/source.
- Collision inputs are unchanged across every 2026-11-27 snapshot hashed and the current worktree: server-npcs.cpp 2e37709f8e52..., physics-collision-glb-main.cpp 0abedb32ac21..., physics-collision-glb-sweep-slide.cpp 6b5fd3cec1d3..., server-infinite-collision.cpp 7088255dd484.... Gen 748, 753, 755, and published 757 all run the same candidate collision code.
- The exact reproduction is still unrun: rg for funworld3|payload over every retained run.json returns 0 files.
- New large-world evidence: logs/2026-11-27/20261127_011757 (gen 748, launch mode 31 "tdm npc elite", map atdm, 51,879-triangle world, ~24.5 min) has 28 `npc.below-map.respawned` across 8 NPC ids (200001-200008), t=1041.77-1470.87 s, map_floor_z=57.10, `from` z as low as 26.5 while actors stand near z≈237-284. Per-actor trace z-min 26.5-92.0. This is a >150 m descent below the whole collision mesh, caught only by the below-map guard (src/network/server-npcs.cpp:2971-3002), on the same source as published gen 757.
- Payload and atdm both set world_streaming.enabled=false, so both use the static collision-mesh path; funworld3 has chunks + infinite_zone and payload's mesh is large (issue evidence 31270 tris), making the 51,879-triangle atdm result a closer structural analogue than the 5.5k-triangle dust2 runs.
- Smaller-map counter-runs on the same code: gen 755 run 20261127_021425 (dust2, --npcs 5) has 1,160 fall-samples (855 candidate_count=0, 1,055 world_contact=false), 0 below-map, trace z≈2351-2368; gen 753 run 20261127_020457 has 1 below-map.

# Specification alignment

- Followed AGENTS.md gates 0b/0c/4: coordination begin/sync/observe, build evidence kept separate from runtime proof, no source/build change, no duplicate issue created.
- Followed docs/specs/dev-loop/agent-self-improvement.md and docs/workflows/issue-intake-and-triage.md: one evidence-grounded review plus one measurable process proposal to an existing issue.

# Exact implementation changes

- None to source or build. Append-only issue review (id 517), process suggestion on issue-91e3384b219b, one issue-linked ask (msg-d83b3e7d7ee5), and this changelog.

# Validation

- Commands used: `agent-coord.py begin/sync/observe/proof list/issue show/issue review/issue suggest/ask`, read-only PowerShell journal scans under `logs/2026-11-27/**/events.jsonl`, SHA-256 hashing of `.dev/build-snapshots/**/source` and `src/**` collision files, and reads of `build/game/current.json`, `devscripts/dev-launch-modes.json`, `config/gamemodes/payload.json`, and `config/maps/funworld3.json`.
- Build status: no build requested; no compiler, worker, or dev-loop build request invoked.

# Measured evidence

- gen 748 atdm run: 28 below-map respawns / 8 actors; z-min 26.5 vs boundsMin 57.1; trace t=1004-1477 s.
- gen 755 dust2 run: 34 spawns, 5,477-triangle summary, 1,160 fall-samples (855 cc=0, 1,055 world_contact=false), 0 below-map.
- All 2026-11-27 collision source hashes match current: server-npcs 2e37709f, glb-main 0abedb32, sweep-slide 6b5fd3, infinite-collision 7088255d.
- Published gen 757 code hash d4d314f4...; no NPC run on gen 757 exists yet.

# Regression review

- Regression entry appended: no. No behavior or code change was made.

# Human acceptance

- Visual/gameplay review: not performed (read-only review).
- Still unverified: exact funworld3/payload Arcade outcome on gen 757; whether atdm below-map is a broadphase/gather miss or unauthored floor at the descent x,y.

# Agent handoff context

- Next check: one isolated run pinned to gen 757 using existing launch mode 57 (`--start-test-run --test-profile npc-navigation --test-launch-mode 57 --test-game-generation 757`), attach run identity, npc.spawn.resolved, npc.collision-world.summary, npc.trace, and classify any below-map. Ask sent to payload-arcade-collision-fix to confirm owner/command.
- Process proposal on issue-91e3384b219b: allow the scheduled sweeper one bounded isolated run when the next check names an existing launch mode; target 1 pinned journal per review, <1 build cycle to evidence, 0 repeated repro recommendations. Cross-links issue-df9cc9acbb0d.
- Do not redo: this review's evidence collection. Do not close as fixed without a pinned gen-757 funworld3 journal or a reasoned proof exemption.
