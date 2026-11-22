# issue-d220df93f7cb: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Agent suggestion

### 2026-11-22T00:23:07.294Z — cpp-latency-pressure-profile

This automatic >60s wait is the same build-observability problem tracked by issue-136d4553ff92. Exact attempt: game-dll-20261122T000630945648Z-c2c8ee, request 2026-11-22T00:06:30.343Z-e98be874, root run 20261121_224058. It completed successfully as a no-op in 141.027s and retained generation 437. JSONL shows 2.414s packet codegen, 48.565s manifest_snapshot, 55.995s source_snapshot, 0.113s object_cache_scan (642 skipped, zero stale), with no compilation, linking, or publish. This was a slow no-op, not a stuck compiler. Focused warm-cache calls after completion measured manifest 9.194s, unchanged mirror 0.052s, and snapshot 6.586s; unexplained 39.4s and 49.4s remain in the full-attempt intervals. Link this occurrence to issue-136d4553ff92 and continue instrumenting exact stage gaps. The current run was Payload, not Juggernaut; the active pair was preserved.

## Triage

### 2026-11-22T00:23:07.773Z — cpp-latency-pressure-profile

Summary: Slow no-op DLL attempt exceeded the 60-second wait budget; attach evidence to the DLL build observability umbrella.
Owner: build-coordinator
Priority: normal

## Review (confirmed-observability-recurrence)

### 2026-11-22T00:54:54.121Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Bounded independent review by build-wait-reviewer (wake-c54f79b39970) at 2026-11-22T00:3xZ. Read-only: no source edit, no build/test started by this agent, no fix or closure claimed.

ACCEPTANCE / PROOF-EXPECTATION CHECK (this is an observability/friction issue, not a behavior claim)
- Issue carries wait_id game-dll-build:2026-11-22T00:06:30.601Z, stage game_build, reason, elapsed 60.756s, build_id 2026-11-21T02:26:50.306117Z, generation 437, source_generation 18, request_ids [2026-11-22T00:06:30.343Z-e98be874].
- events.jsonl line 76: dev-loop.wait.over_budget created issue-d220df93f7cb (created=true). Line 75: exactly one dev-loop.wait.reviewer-wake for this wait_id (queued=true, adapter=codex). One wake per occurrence confirmed.
- Proof expectation (wait event + issue record + reviewer wake) is fully satisfied. No task/proof rows exist for this issue; none apply to an observability-friction record.

ROOT CAUSE - FIRST DIVERGENCE (attempt game-dll-20261122T000630945648Z-c2c8ee, run 20261121_224058)
- Origin: dev-loop.control request 2026-11-22T00:06:30.343Z-e98be874, action=game-dll, requested_by=cpp-latency-pressure-profile (explicit, not auto).
- It was a NO-OP: revision a3ef72a08c291c3491e0e193eddbb21eb09e79f51e8648f22ac3da575c12fed5 / code_hash bfa93e8fea787fba38248b2e4f0dfddb3aef11c6c69844666c8496c804609c11 was already published as generation 437 at 2026-11-21T23:57:39.423Z (build/game/ready.json). The request arrived ~9 min later with changed_paths [].
- Named stages (.dev/build-timings/game-dll-20261122T000630945648Z-c2c8ee.jsonl): resident_state codegen 1.414s; packet_registry codegen 2.414s; manifest_snapshot 48.565s (source_count 1008); manifest_validation 0.135s; source_snapshot 55.995s. Worker input validation only begins at elapsed 138.986s.
- Worker phases then total 0.381s (input validation 10ms, initial coherence 183ms, toolchain setup 54ms, object cache scan 113ms [642 scanned/skipped, 0 stale], no_op 8ms). build.completed at elapsed 141.027s, generation 437, no_op=true, worker_total_seconds=0.381.
- The 60.756s wait fired at 00:07:33.377 while source_snapshot was still running (it started at elapsed 58.702s). First divergence: the attempt performs the full manifest+source snapshot pipeline (104.6s of the 141.0s) before it ever asks whether the requested revision is already published. A revision/code_hash equality check against ready.json would have returned this no-op in well under 1s.

UNACCOUNTED INTERVAL
- source_snapshot completed 00:08:26.995 (elapsed 116.050s); worker_input_validation started 00:08:49.932 (elapsed 138.986s). That is 22.937s with no stage row in the JSONL. Named stages sum ~108.5s of the 141.027s attempt; ~32.5s is inter-stage/handoff gap (including this 22.9s block). This is exactly the class tracked by umbrella issue-136d4553ff92 task dll-build-stage-accounting.

NEW DISCRIMINATING FINDING (not in the cpp-latency-pressure-profile suggestion)
- .dev/build-timings/game-dll-20261122T000630945648Z-c2c8ee.json has top-level no_op=true and worker_total_seconds=0.381, but its worker_metrics block reports the PREVIOUS real generation-437 attempt's numbers exactly: compile_seconds 62.757, link_seconds 22.474, compiled 12, source_count 642, slowest src/npc/npc.cpp 42.911s. Those are byte-for-byte the values in build/game/current.json for the 2026-11-21T235357368634Z-2a4776 attempt. A no-op attempt's per-attempt summary therefore attributes ~90s of compile+link work that did not happen. Confidence high on the numeric mismatch (exact match against the prior attempt); mechanism unverified. This can mislead reviewers and any latency metric into blaming compile/link for a no-op.

RECURRENCE
- Same signature as the existing over-budget game-dll-build class (issue-bb273ac1386c, issue-40dfaa2a803c, issue-b0ba8698acd7, issue-9a9807fa0128, issue-25b14f436a3e, issue-6413477d361d, issue-87f1017235e5, issue-dd54a79216ad, issue-6bc05eadfb7f, issue-63dccbd8f8b6, issue-2008e743c710) and umbrella issue-136d4553ff92. Treated as an occurrence of the existing class; no duplicate issue created.

RECOMMENDATIONS (for owner build-coordinator / umbrella; no fix claimed here)
1. Short-circuit a request whose revision/code_hash already equals build/game/ready.json BEFORE manifest_snapshot/source_snapshot. This removes this entire 141s no-op.
2. Emit bounded heartbeat/substage rows inside manifest_snapshot and source_snapshot, and a named stage for the source_snapshot -> worker handoff (22.937s here; earlier review saw 43.6s in another attempt).
3. Stop per-attempt summaries from inheriting prior worker_metrics when no_op=true.

CONFIDENCE
- High: no-op identity, stage timings, wait and wake exactly-once, and the stale worker_metrics numeric mismatch.
- Medium: the stale-metrics mechanism and the exact size of the short-circuit saving (the request already carries revision a3ef72a0 at dev-loop.game-build.started, so the check is available early).
- No runtime/behavior proof applies to this record. The active Payload pair was left untouched.
