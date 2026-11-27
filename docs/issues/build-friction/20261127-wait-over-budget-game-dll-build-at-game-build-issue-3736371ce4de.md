# issue-3736371ce4de: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (pass)

### 2026-11-27T16:02:50.055Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Bounded independent review by build-wait-reviewer (wake-8af078e5e1a7), read-only. No source edit, no build/test started by this agent, no fix or closure claimed.

OCCURRENCE
- wait_id game-dll-build:2026-11-27T07:55:43.159Z; kind game-dll-build; stage game_build; budget 60.0s; elapsed 60.533s.
- build_attempt_id game-dll-20261127T075543204684Z-cefc30; generation 787; source_generation 24; request build-request-caea29b4489f.
- The evidence build_id 2026-11-27T07:45:59.076100Z is the EXE artifact/build identity (.dev/builds/2026-11-27T07-45-59.076100Z/build-identity.json), not the DLL attempt. The DLL attempt identity is the game-dll-... id above. The two ids are not interchangeable.
- Primary artifacts: .dev/build-timings/game-dll-20261127T075543204684Z-cefc30.json (summary+postmortem) and .resource.json / .resource.jsonl.

ACCEPTANCE / PROOF CHECK
- Issue carries wait_id, stage, reason, elapsed, build_id, generation, source_generation, request_ids, and evidence: yes.
- Reviewer wake queued exactly once: yes, wake-8af078e5e1a7. .dev/agent-wake/events.jsonl has one wake.queued at 07:56:44.337Z; exactly one request file exists for this issue.
- Dev-loop wait event: NOT FOUND. No retained logs/**/dev-loop/events.jsonl contains dev-loop.wait.over_budget, dev-loop.wait.reviewer-wake, the wait_id, or the issue id. The dev-loop runtime_context run at the wait was 20261127_074605 (resource sample); that dir retains only run.json and no dev-loop/ subdir. This removes the journal half of the stated proof expectation. Whether the append failed (known family issue-6e2d7b91e7d9) or the file was pruned is not distinguishable from current artifacts; inference favors the dropped-write family because the run dir exists while its dev-loop subdir does not. The issue-6e2d7b91e7d9 owner should confirm.
- Reviewer activation, not just queue: LATE. codex claimed/launched 07:56:45.675Z/45.791Z (pid 23976) and delivered no review; wake.retry "stale launched request recovered by scheduler" at 15:53:48.099Z switched to opencode; opencode claimed/launched 15:53:49.013Z/49.034Z (pid 13200, this session). Queue-to-actual-review is ~7h57m, far beyond the <=5 min target. The exactly-once queue contract held; the launch happened twice and only the fallback delivered. Recurrence of issue-84c584957e65.

WAIT / BUILD ANALYSIS (first divergence)
- Not a hang. Build started 07:55:43.204Z and completed 07:57:30.509Z, total 107.305s, success=true. The 60s budget fired ~52s into the compile phase.
- Phases: compile 93.905s, link 4.396s, object_scan 1.751s, publish 0.365s, total 107.305s.
- Work: 40 TUs compiled, 607 skipped. Changed headers in the build note: src/network/community-match-client.h, src/vehicles/vehicle-hud.h, src/vehicles/vehicle-system.h, plus 4 changed sources. Current dependency fanout: community-match-client.h 25 sources, vehicle-system.h 20 sources. The postmortem records header_fanout=null, so fanout was not declared in the build note (gate 6; postmortem suggestion).
- Slowest TUs: map-editor.cpp 37.438s, main-systems.cpp 31.890s, multiplayer-tick.cpp 30.096s, engine-tick-net.cpp 29.054s, engine-tick-camera.cpp 28.537s.
- The flat 60s budget is below the natural wall time of this header-fanout build. The wait is a true positive, not evidence of a stall.

RESOURCE EVIDENCE
- 12 logical CPUs; jobs=10; per-TU queue_wait max 73.624s, mean 31.580s.
- Host CPU peak 100.0%, mean 92.319%; minimum available memory 41.8 MB (43,851,776 bytes) of 16,802,918,400 bytes. 100 samples at 1s.
- Compile-phase top processes (resource.jsonl): build orchestrator pythonw 5.835% whole host; opencode.exe x3; GoogleDriveFS.exe; Code.exe; msedgewebview2.exe. Compiler children were not individually attributed (top_processes empty in early samples), so the compiler share is unknown. Host CPU/queue numbers are direct observations; the split between build TUs and the other listed processes is an inference.

REASON-FIELD DEFECT
- evidence.reason is a mid-JSON tail (":58155.9,\"build_attempt_id\":\"game-dll-20261127T075543204684Z-cefc30\",..."), not a readable stage summary. Recurrence of issue-6e08d89b55e3.

RECOMMENDATIONS (owners; no fix claimed here)
1. issue-84c584957e65: a launched-but-stale wake must fall back promptly and deliver once. Latest data point: 7h57m; codex launched then no review, opencode delivered only when this session was started.
2. issue-6e2d7b91e7d9: guarantee the dev-loop events_path directory exists before writes and record wait events even with no run journal; otherwise this issue's proof expectation cannot be met.
3. issue-6e08d89b55e3: store a short human-readable stage summary in the wait reason instead of the last 240 chars of worker output.
4. build-coordinator / issue-d220df93f7cb: a flat 60s budget is below a normal changed-header build here (107s); consider a progress-aware or fanout-scaled budget, and require --header-fanout in build notes (postmortem header_fanout=null).

RECURRENCE / DEDUPE
- Same signature as the existing over-budget game-dll-build class; one issue per wait_id is working. No duplicate issue created. Cross-links: issue-136d4553ff92 (stage accounting), issue-6e2d7b91e7d9 (missing wait events), issue-6e08d89b55e3 (reason field), issue-84c584957e65 (late dispatch), issue-d220df93f7cb (owner build-coordinator).

CONFIDENCE / LIMITS
- High: build identity/timing, compile-vs-link split, header fanout, exactly-once queue, ~7h57m dispatch delay, reason-tail defect.
- Medium: dropped-write vs retention for the missing dev-loop event; compiler-level CPU attribution.
- No runtime/behavior proof applies to this build-friction record. The waited-on build succeeded (generation 787) and was later superseded by generation 794; no game run was started or touched by this review.
