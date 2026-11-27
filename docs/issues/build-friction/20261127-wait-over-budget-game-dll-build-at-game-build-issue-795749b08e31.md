# issue-795749b08e31: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (true-positive-progressing-build)

### 2026-11-27T17:48:10.620Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewer: build-wait-reviewer (wake-b7ec3635830e), 2026-11-27T17:3xZ. Read-only: no source edit, no build/test started, no proof claimed.

VERDICT: true positive, not a stall. The 60s WAIT_ISSUE_SECONDS budget crossed while a real, progressing whole-game DLL compile was running.

OBSERVED FACTS (attempt game-dll-20261127T165414319868Z-c53bb1, generation 798, request build-request-035048d826a6, run 20261127_165709):
- Build started 16:54:14.319Z; wait recorded at 16:54:14.303Z elapsed 61.129s (budget 60.0). Build completed 17:01:29.035Z, success=true, exit_code=0, no_op=false, published generation 798 (build\game\generation-798\game-g798.dll); total 435.208s.
- Phase split: manifest/codegen prep before compile, compile 402.929s (92.6% of total), link 20.275s, coherence 0.814s, publish 0.615s, object-cache scan 1.843s.
- 23 TUs compiled / 624 skipped / 646 source_count, jobs=10. Slowest: src/vehicles/vehicle-system.cpp 254.916s, server-gamemode.cpp 214.116s, render-player.cpp 198.325s, server-physical-contact-actors.cpp 195.987s, server-melee-actions.cpp 189.132s. Max per-TU queue_wait 186.851s, mean 69.852s (worker pool starved/contended).
- Trigger: changed header src/entities/player-avatar-animations.h invalidated 20 dependents plus 3 missing/invalid dep files (23 compiled). Postmortem flags a header changed without a recorded fanout.
- Resource sampler: 348 samples @1s, mean host CPU 97.735%, peak 100.0%, minimum available memory 38,838,272 B (~37 MiB). runtime_alignment: run 20261127_165709 live server vehicle_freeplay pid 22280 and client pid 5736 (client 186.6 fps window), so a concurrent live pair shared the host with the build. resource.json lists matched_prebuild_resource_baseline, host_commit_charge_and_limit and client_mode_independent_confirmation as unavailable (unknown, not zero), so host pressure is a correlate, not a proven cause.

ACCEPTANCE CHECK:
- wait_id/stage/elapsed/build+request IDs/evidence are present.
- Reviewer wake queued exactly once: .dev/agent-wake/requests/wake-b7ec3635830e.json and one .dev/agent-wake/events.jsonl wake.queued row for issue-795749b08e31 (16:55:20.646Z). Meets "exactly once per wait occurrence".
- reason is NOT readable: evidence.reason = "10213.216,\"build_attempt_id\":\"game-dll-20261127T165414319868Z-c53bb1\",...\"record_overhead_ms\":4.882}" - the last 240 chars of the module-identity-provider translation_unit.completed row (.dev/build-timings/game-dll-20261127T165414319868Z-c53bb1.jsonl:765). Tracked as existing defect issue-6e08d89b55e3; appended an occurrence there, no duplicate issue filed.
- Identity is stale: evidence.build_id=2026-11-27T08:12:40.178185Z (prior EXE artifact) and generation=null, while the in-flight attempt was generation 798. The correct attempt/gen survive only inside the reason tail. Tracked under issue-7d3dcfd7433b (wait-stage measurement / identity); appended there.

INFERENCE / UNCERTAINTY:
- High confidence the wait was legitimate: at the 61.129s crossing only ~7 TUs had completed; the build ran another ~6.2 minutes and published generation 798.
- The flat 60s budget is below the natural wall time of an ordinary header-fanout DLL rebuild on this 12-logical-CPU host under a live game; this confirms the recurring class already debated on issue-136d4553ff92 (60s flat budget; one issue per wait_id).
- Host CPU/memory pressure plausibly lengthened compile/queue, but without a matched pre-build baseline this is not established as the dominant cause.

CONTRARY EVIDENCE:
- The build was not wedged: TUs completed throughout and link/publish succeeded.
- Only one wake for this wait_id; no duplicate-issue storm from this occurrence.

NEXT CHECKS (owner-sized, unclaimed here):
1. Keep the 60s budget decision in issue-136d4553ff92; if raised, gate on progress (TU completions) rather than wall clock alone.
2. The reason-field fix (readable [GAME_STAGE]/[GAME_ERROR] summary) stays owned by issue-6e08d89b55e3.
3. Put the active game-build attempt id + target generation + run_id into the wait record (currently self.latest_build) - issue-7d3dcfd7433b.
No behavior/build fix is in scope for this review; this occurrence is a functional over-budget record, not a regression.
