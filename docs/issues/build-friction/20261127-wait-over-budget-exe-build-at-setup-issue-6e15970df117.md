# issue-6e15970df117: Wait over budget: exe-build at setup

## Human report (verbatim, immutable)

> exe-build wait at setup exceeded the wait budget


## Review (confirmed-expected-long-exe-rebuild; evidence-partial (missing dev-loop.wait.over_budget event))

### 2026-11-27T18:23:41.246Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Bounded wake review (wake-ca3c45e6a7a8, agent build-wait-reviewer), 2026-11-27T18:2xZ. Read-only: no source edited, no file claimed, no build/run started, no fix claimed, no issue closed.

OCCURRENCE (confirmed; not a stall)
- Readiness was already complete before this review: ready=true, score 8/8, missing=[]; state=new, owner empty, 0 tasks, no prior reviews, no duplicate for wait_id exe-build:42:2026-11-27T17:27:27.229Z.
- Wait evidence: kind=exe-build, stage=setup, reason="preparing build", elapsed 61.086s vs 60.0s budget, generation 42, source_generation 43, started_utc 2026-11-27T17:27:27.229Z, build_id 2026-11-27T08:12:40.178185Z (lagging; see Identity).
- Correlated journal logs/2026-11-27/20261127_172726/dev-loop/events.jsonl:
  - line 1 (17:27:26.764Z) dev-loop.control action=launch mode 45 "AI launch waiting for source validation/build" - the pre-launch EXE rebuild the wait is waiting on.
  - line 11 (17:45:37.353Z) dev-loop.build.completed build=2026-11-27T17:45:19.700118Z, attempt=exe-build-20261127T122727-eaad2173, duration 1082.086s, source_generation 42->46, source_changed_during_build=true, setup 0.137s, build_script 1048.061s, post_build 32.129s, artifact_publish 8.328s.
  So the 61.086s trip is a legitimate ~18-minute EXE rebuild crossing the flat 60s WAIT_ISSUE_SECONDS policy, not a wedged setup phase.
- Resource context (.dev/build-timings/exe-build-20261127T122727-eaad2173.resource.json): mean_host_cpu_percent 99.91, peak 100.0, minimum_available_memory_bytes 25,292,800 (~24.1 MiB), 694 samples @1s over 1081.851s with the live run active. The host was CPU-saturated and near memory exhaustion for the whole build; this is the dominant cause of both the 18-minute duration and the slow wake launch below. Secondary: sampler thread did not stop before timeout; runtime_alignment unavailable; cap_render_gate failed open after 4s.

EVIDENCE GAP (known defect, recurrence)
- The only journal event for this wait_id is line 3 (17:29:18.821Z) dev-loop.wait.issue-failed, reason "could not record over-budget wait", error "agent-wake.py run --once timed out after 30 seconds". There is NO dev-loop.wait.over_budget and NO dev-loop.wait.reviewer-wake for this wait, even though both succeeded: issue created 17:28:37.692Z and exactly one wake queued (wake-ca3c45e6a7a8, wake.queued 17:28:48.090Z).
- First divergence: build_friction.wake_reviewer runs "agent-wake.py run --once" inline with timeout=30 and no try/except (devscripts/build_friction.py:242-249). dev-loop.check_wait_budget wraps record_wait_incident + wake_reviewer in one try (devscripts/dev-loop.py:3905-3930); TimeoutExpired escapes, skips over_budget/reviewer-wake, and is logged as "could not record over-budget wait" although the record and queue already succeeded. reported_wait_ids already holds the wait_id, so there is no retry.
- Same defect already tracked on issue-9a4503f6ed68 (owner wake-proof-wait) with earlier 2026-11-21/23/24/26 occurrences; appending this 2026-11-27 occurrence there instead of creating a duplicate. Suggested fix remains: emit over_budget right after a successful record and make the inline run --once best-effort/non-fatal (catch TimeoutExpired), as agent_coord.py:1170-1176 already does. Not claimed here.

IDENTITY
- Evidence build_id is self.latest_build (the prior 08:12:40 build) and does not name the in-flight attempt; the waiting build is identified by started_utc + attempt exe-build-20261127T122727-eaad2173 / run 20261127_172726. Known gap already tracked on issue-7d3dcfd7433b and issue-136d4553ff92.

WAKE DELIVERY
- Exactly one wake.queued for wake-ca3c45e6a7a8 (17:28:48.090Z); request attempts=2. codex claimed/launched 17:30:26.329Z/17:30:26.600Z (pid 27456), then wake.retry "stale launched request recovered by scheduler" 17:55:34.362Z, then opencode claimed/launched 17:57:37.504Z/17:57:37.535Z (pid 20704 = this review). Net queue-to-delivered ~29 minutes; the first adapter did not deliver. This is the known late/mis-dispatched wake path (issue-84c584957e65 / issue-6907072bb155), not a duplicate queue.

DUPLICATE CHECK
- Only this issue carries wait_id exe-build:42:2026-11-27T17:27:27.229Z. Sibling exe-build issues carry distinct wait_ids: issue-551c405dc80c (exe-build:17:2026-11-27T00:42:18.238Z, compiling), issue-054ea1bcda9b (exe-build:0:2026-11-21T01:38:16.191Z, starting). No duplicate created; no recurrence occurrence appended (same wait_id did not recur).

ACCEPTANCE STATUS
- "The issue carries wait_id, stage, reason, elapsed, build/run IDs, and evidence" = PARTIAL: wait_id/stage/reason/elapsed present; in-flight build/run IDs absent (only stale latest_build).
- "a reviewer wake is queued exactly once for this wait occurrence" = PASS: one wake.queued for this wait_id.
- "Dev-loop wait event plus the issue record and reviewer wake record" = PARTIAL: issue + wake records exist, but dev-loop.wait.over_budget and dev-loop.wait.reviewer-wake journal events are missing (known build_friction.py:242-249 inline-timeout defect).

ROOT-CAUSE OWNERSHIP
- Flat wait budget / wait policy: issue-7d3dcfd7433b (launch-latency-investigation) and issue-e044a931c54b (build-runtime-subagents).
- Missing over_budget/reviewer-wake evidence: issue-9a4503f6ed68 (wake-proof-wait).
- High CPU/memory dev-loop reliability: issue-2a5c5da53565 (resource evidence appended there).
- No source fix claimed in this bounded review.

CONFIDENCE: High that the wait is a legitimate long EXE rebuild under extreme resource pressure and is not a wedge. High that the missing over_budget/reviewer-wake events are the known build_friction.wake_reviewer inline-timeout defect. Unknown: the exact span of the 60s "setup" label (worker setup was 0.137s, so the label reflects the daemon phase before compile progress, not worker setup).

## Context

### 2026-11-27T18:24:07.742Z — build-wait-reviewer

related_build: 2026-11-27T17:45:19.700118Z (attempt exe-build-20261127T122727-eaad2173)
related_run: 20261127_172726
