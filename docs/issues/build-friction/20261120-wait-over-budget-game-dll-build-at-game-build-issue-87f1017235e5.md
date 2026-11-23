# issue-87f1017235e5: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Scheduled independent review

### 2026-11-23T06:22:44.008Z — cpp-latency-pressure-focus

A bounded independent review was queued as wake wake-c6b3702eb873 after 364 completed work sessions (trigger=sync). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (reviewed-no-fix)

### 2026-11-23T06:28:46.628Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Reviewer: issue-review-sweeper (bounded independent review; wake wake-c6b3702eb873). No source edited, no fix claimed, no lease taken.

SCOPE: first review of this issue; it had no prior review because the original reviewer wake failed (see WAKE CHECK). Reviewed the exact occurrence via .dev/build-timings and .dev/agent-wake because the 2026-11-20 session journals are no longer on disk (see EVIDENCE RETENTION).

OBSERVED FACTS (this exact occurrence)
- Wait record: wait_id game-dll-build:2026-11-20T14:44:45.804Z, stage game_build, elapsed 66.726s vs 60.0s budget, request_ids [auto-1, auto-2], source_generation 2. reason is a raw GAME_STAGE line: manifest_validation started at elapsed_ms 64604.433, build_attempt_id game-dll-20261120T144446297866Z-157ad9.
- In-flight attempt (from .dev/build-timings/game-dll-20261120T144446297866Z-157ad9.json/.jsonl): build.started 14:44:46.298Z, completed 15:18:42.083Z, success=true, generation 402, total_seconds 2035.529 (~33m56s). This was a real long wait, not a 60s boundary crossing.
- Stage breakdown: codegen resident_state 4.0s; packet_registry 7.4s; manifest_snapshot 34.7s (source_count 808); manifest_validation 0.03s; source_snapshot 39.8s; changed_source_preflight 178.4s (3 physics TUs at 150-178s each); object_cache_scan 1133.3s (scan_parse_ms 969106 = ~969s); compile_seconds 426.9s; link 162.5s; publish 3.5s.
- The 60s flag fired at ~66.7s while the build was in manifest_validation. The dominant cost (object_cache_scan) did not even start until 14:50:31Z, ~6 minutes later. So the recorded stage/reason names a cheap, non-causal phase.
- Evidence-field defects visible here: evidence.build_id=2026-11-20T07:47:09.981634Z is a previously published EXE artifact, not the in-flight attempt (real attempt only inside the free-text reason); evidence.generation=401 while the attempt produced generation 402 (generation is assigned at completion, so the payload lags); reason is unstructured progress text.

INFERENCE / UNCERTAINTY
- High confidence: this occurrence's wait was genuine (2035s). The flat 60s budget did not produce a "false positive" here in the sense of a healthy sub-minute build.
- Inference (medium): the 1133s object_cache_scan is an era-wide scan-cost problem, not a one-off: retrained 2026-11-20/21 game-dll timings show object_scan_seconds of 637, 1629, 861, 920, 170, 275, 338, 86s across unrelated attempts; by late 2026-11-22 the same metric is 0.04-3.6s. Cannot tie the 2026-11-20 value to host contention vs a scan regression because the session journal with host/CPU context was pruned.

CONTRARY EVIDENCE (corrects the newer "expected/healthy" verdicts for this occurrence)
- Later reviews of sibling issues (issue-cd97259fdc20, issue-a236165fa6fc) concluded over-budget game-DLL waits are expected because heavy TU compiles take 151-248s. That is correct for the post-2026-11-22 scan-fix era, but it does NOT describe this occurrence: here the build was 34 minutes and dominated by object_cache_scan (1133s), not by changed-TU compile (426s total). Treating all these issues as one flat "60s too tight" story would mislabel this one. The recurring class is real, but its causes shifted over time.
- Acceptance for THIS issue (issue carries wait_id/stage/reason/elapsed/build/run IDs/evidence; reviewer wake queued exactly once) is satisfied on the data/wake-queue side, independent of whether the build was "healthy."

WAKE CHECK (acceptance)
- Exactly one wake.queued for this wait occurrence: wake-3caaaeb65666, .dev/agent-wake/events.jsonl, wall_time 2026-11-20T14:45:53.082Z, agent build-wait-reviewer. No second queued wake exists for this wait_id.
- Crossed & known defect: that single wake FAILED. Events show codex launch 14:45:53.446Z, wake.retry (opencode) 14:54:22.349Z, opencode claimed/launched 14:59:21.6Z (pid 4076), then wake.failed "adapter process exited" 15:27:57.779Z (request wake-3caaaeb65666.json status=failed). No review was ever appended; the issue carried only the 2026-11-23 scheduled-review block until this review. The acceptance wording ("queued exactly once") passes, but the queued wake produced no review, so the issue sat unreviewed ~3 days until the periodic sweep.

EVIDENCE RETENTION (observed)
- logs/2026-11-20 does not exist and logs/11-20-2026 is empty; the dev-loop.wait.over_budget and reviewer-wake journal lines for this occurrence are gone, so the wait reason and wake could only be reconstructed from the issue evidence, .dev/build-timings/game-dll-20261120T144446297866Z-157ad9.*, and .dev/agent-wake/events.jsonl (both retained). Build-timings are the effective cold-review source of truth here.

NEXT DISCRIMINATING CHECK
- Confirm the object_cache_scan era shift is fully closed: sample retained game-dll timings over the last 24h and assert object_scan_seconds stays < ~5s and scan_parse_ms < ~5s on healthy builds. If it reappears >100s, open a separate scan-performance issue with the timeline JSON; if it stays low, this occurrence is explained by the pre-2026-11-22 scan cost plus a coarse wait stage, and the remaining work is the flat-budget/progress-aware-stage policy already tracked under parent issue-e044a931c54b and the recurring-signature proposal on issue-67f2be3d7012.

PROOF / SCOPE
- This review proves the occurrence timeline, the stage mislabel, the evidence-id/generation mismatch, the single queued wake, and its failure. It does not claim any fix, does not claim the systemic latency target, and does not close the issue. The likely correct disposition remains a human decision to merge/close this occurrence into parent issue-e044a931c54b while tracking the latency/scan work elsewhere; I did not perform that merge.
