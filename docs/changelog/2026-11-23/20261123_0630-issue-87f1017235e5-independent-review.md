# issue-87f1017235e5 Independent Build-Wait Review

# Task

- Task ID: bounded independent review of issue-87f1017235e5 (wake-c6b3702eb873)
- Owner: issue-review-sweeper
- Focus: game-dll-build wait at game_build, elapsed 66.726s, wait_id game-dll-build:2026-11-20T14:44:45.804Z
- Summary: Reconstructed the exact occurrence from `.dev/build-timings/game-dll-20261120T144446297866Z-157ad9.*` and `.dev/agent-wake/events.jsonl` (the 2026-11-20 session journals are pruned), appended review verdict `reviewed-no-fix` to issue-87f1017235e5, and appended one process proposal to build-friction auto-intake owner issue-67f2be3d7012. No source, build, runtime, or fix was performed.
- Status: REVIEW_RECORDED_OPEN
- Date, time, timezone: 2026-11-23T06:30:00Z

# Pre-existing changes

- The shared checkout already contained other agents' uncommitted edits and changelogs (e.g. `20261123_062432-mode-3-batch-build-spec.md`). This session edited no source and reverted nothing.
- Files created by this session: this changelog. Files appended through `agent_coord.py`: the issue review and the process suggestion.

# Requested behavior

Complete one bounded independent review of issue-87f1017235e5: issue brief/readiness, proof status, linked artifacts, latest journals; append one evidence-grounded review with observed facts, inference/uncertainty, contrary evidence, and the next discriminating check. Do not edit source, build, close, or claim a fix. Then append one specific process-improvement proposal if the evidence supports it.

# Findings

- Readiness: ready=true, missing=[], score 8. The issue carries wait_id, stage, reason, elapsed, budget, request_ids, generation, and evidence. State was `new` with no prior review.
- Acceptance on the data/wake-queue side holds: exactly one `wake.queued` exists for this wait occurrence (wake-3caaaeb65666, `.dev/agent-wake/events.jsonl`, 2026-11-20T14:45:53.082Z). No second queued wake for the wait_id.
- The occurrence was a genuine long wait, not a 60s-boundary false positive: attempt `game-dll-20261120T144446297866Z-157ad9` ran 2035.529s (~33m56s), success=true, produced generation 402. The 60s flag fired at ~66.7s while the build was in `manifest_validation` (0.03s stage), and the dominant cost `object_cache_scan` (1133.3s; scan_parse_ms 969106) did not start until ~6 min later. Stage/reason therefore names a cheap, non-causal phase.
- Era-wide scan cost, not one-off: retrained 2026-11-20/21 game-dll timings show `object_scan_seconds` 637/1629/861/920/170/275/338/86s; by late 2026-11-22 it is 0.04-3.6s.
- Payload defects recurred: `evidence.build_id` is a previously published EXE artifact (not the in-flight attempt; the attempt id appears only inside the free-text reason), and `evidence.generation=401` lags the produced 402 (generation assigned at completion).
- The original single reviewer wake FAILED: codex adapter exited -> wake.retry (opencode) -> launched pid 4076 -> `wake.failed "adapter process exited"` at 2026-11-20T15:27:57Z. No review was appended, so the issue sat unreviewed until this 2026-11-23 sweep first reviewed it (~3 days).
- Cold-review evidence fragility: `logs/2026-11-20` is absent and `logs/11-20-2026` is empty; the dev-loop `wait.over_budget`/`reviewer-wake` journal lines are gone. Review had to rely on retained `.dev/build-timings` and `.dev/agent-wake/events.jsonl`.

# Specification alignment

- Followed AGENTS.md gates 0b/0c/4: read coordination state and proof status, kept build evidence separate from behavior proof, made no source or build change, created no duplicate issue.
- Preserved all prior issue text; both appends are append-only.
- Followed gate 0d by recording the stage/identity defects and reviewer-wake failure against the existing owners instead of opening a new issue.

# Exact implementation changes

- None. Append-only issue review and changelog only.

# Validation

- Build status: no build requested; no compiler, worker, dev-loop, or second loop invoked.
- Evidence read: `issue observe/show/readiness`, `.dev/build-timings/game-dll-20261120T144446297866Z-157ad9.json/.jsonl`, `.dev/agent-wake/events.jsonl` and `requests/wake-3caaaeb65666.json`, plus neighboring game-dll timings for the scan baseline.
- Review artifact: verdict `reviewed-no-fix` on issue-87f1017235e5 (state now `in_review`). Process proposal appended to issue-67f2be3d7012.

# Measured evidence

- Wait elapsed 66.726s at budget 60.0s; attempt total 2035.529s; object_cache_scan 1133.302s (scan_parse_ms 969106.423); compile_seconds 426.857; link_seconds 162.472; changed_source_preflight 178.391s.
- Wake lifecycle: queued 14:45:53.082Z, retry 14:54:22.349Z, relaunched 14:59:21.6Z, failed 15:27:57.779Z; request status=failed.

# Regression review

- Regression entry appended: no. No behavior or code change was made.

# Human acceptance

- Visual review: not performed. Gameplay review: not applicable.
- Still unverified / open: whether owners will make the wait budget progress-aware, store the in-flight attempt id/generation, fix the `reason`, coalesce recurring waits, and make reviewer-wake failure recoverable. Disposition to merge this occurrence into parent issue-e044a931c54b remains a human decision.

# Agent handoff context

- Wait-policy/identity/noise owner: parent issue-e044a931c54b; build-friction intake process: issue-67f2be3d7012; systemic latency: issue-136d4553ff92.
- Next exact step: verify object_cache_scan stays low over the last 24h of retained timings; if it reappears >100s, open a separate scan-performance issue with the timeline JSON.
- Do not redo: this occurrence's timeline reconstruction or the scan-baseline sampling.
- Do not close as a build defect without owner/human review.

# How to reverse

- No source or build to reverse. Remove this changelog only through normal review if created in error; preserve the append-only issue review and suggestion.
