# issue-e6c253b2fa9f: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Context

### 2026-11-25T01:39:21.656Z — build-wait-reviewer

related_build: game-dll-20261125T010553542744Z-42909c
related_run: 20261124_221014

## Review (acceptance-met-identity-gap)

### 2026-11-25T01:41:13.041Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Bounded independent review by build-wait-reviewer (wake-532aa904a20c). Read-only: no source edited, no build/run started, no fix or proof claimed.

OBSERVED (run 20261124_221014, journal logs/2026-11-24/20261124_221014/dev-loop/events.jsonl):
- dev-loop.wait.over_budget at wall_time 2026-11-25T01:06:56.444Z, wait_id game-dll-build:2026-11-25T01:05:53.086Z, created=true, issue issue-e6c253b2fa9f. Exactly one reviewer wake for this wait_id: dev-loop.wait.reviewer-wake -> wake-532aa904a20c (agent-wake events: wake.queued 01:06:54.887Z, codex adapter launched pid 1812 at 01:06:56.251Z; that adapter exited, wake.retry 01:15:09.511Z fell back to opencode, claimed and launched 01:35:01Z). The one-issue/one-wake invariant held exactly for this wait_id.

WAITED-ON ATTEMPT: game-dll-20261125T010553542744Z-42909c (source_generation 39, request 2026-11-25T01:05:52.478Z-24d34272:mode3:game, jobs=10, 644 sources, full rebuild seeded=0). Unlike earlier waits this night, this build FAILED: dev-loop.game-build.failed at 01:11:13.621Z, failed_phase=worker_timeout, error "worker timed out after 300s", duration_seconds 319.19 (.dev/build-timings/game-dll-20261125T010553542744Z-42909c.json). The 60s over-budget wait was a leading symptom of a build heading for the 300s worker timeout, not a false alarm.

IDENTITY GAP (against this issue acceptance): wait evidence carries build_id 2026-11-24T19:27:02.742428Z and generation 627, both last-published values, not the in-flight attempt. Same defect as issue-9a4503f6ed68 (dev-loop.py:3896-3897 uses self.latest_build; game_build_generation is assigned only on completion at :3259 while started_utc is set at build start). related_build and related_run were empty and were set by this review to the attempt and run above.

AFTERMATH (direct, same run): 01:14:06 EXE build failed exit 1; 01:14:09 attempt game-dll-20261125T011406552810Z-170580 (src_gen 53) link failed in 3.464s (273.9s); 01:21:35 attempt b7c31e link failed 3.293s; 01:23:23 bb7709 codegen check failed (packet_registry_codegen.py); 01:26:02 9473ce disk_low 2033 MB free need 2048; 01:28:49 ccdc3f disk_low 1437 MB free; 01:29:27 attempt 51b848 (src_gen 59) succeeded at 23.1s and published generation 632. First divergence after the wait is the 300s worker timeout under resource exhaustion; the tail is a link/codegen/disk failure chain until resources recovered.

RECURRING: four distinct wait_ids fired the same night -> four issues (issue-0567fb769b04 60.363s, issue-e3eb52067a38 60.501s, issue-e6c253b2fa9f 60.317s, issue-a36ab17255e9 61.037s). Per-wait_id uniqueness mints one issue and one wake per attempt; aggregation belongs to issue-9a4503f6ed68 (occurrence appended there).

RESOURCE CONTEXT (direct observation, one build window): .resource.json for attempt 42909c: 264 samples @1s over 319.139s, 12 logical CPUs, host total mem 16,802,918,400 B, mean host CPU 99.465%, peak 100.0%, minimum available memory 14,106,624 B (~13.5 MiB), provider windows_kernel32_host_and_process, no sampler errors. runtime_alignment unavailable (fresh_server_and_client_journal_evidence_not_matched) so live mode/workload is unknown; no top competing process captured. Inference only: near-saturated CPU and near-exhausted free memory plausibly lengthened the 644-source rebuild past 300s; this sample does not prove causation. Resource occurrence recorded on issue-2a5c5da53565; disk_low tail recorded on issue-916633ab3313.

CONTRARY EVIDENCE: the wait policy itself worked exactly as specified: one issue, one wake, full wait evidence, no duplicate for this wait_id. The failure is the waited-on build, not the wait detector.

NEXT DISCRIMINATING CHECK (owner decision, not done here): (1) carry in-flight build_attempt_id/generation/run_id in wait evidence and populate related_build/related_run at creation; (2) stage-aware or stall-progress game_build budget (>= observed p95) and coalesce per (kind,stage); (3) capture a matched pre-build resource baseline plus top competing processes to separate build pressure from pre-existing host load. All three are already tracked in issue-9a4503f6ed68 / issue-2a5c5da53565 / issue-ca5eb16d9e21; no new issue created and no separate fix claimed by this review.
