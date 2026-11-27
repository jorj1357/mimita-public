# Build-wait review — issue-6e15970df117 (exe-build at setup)

- **When:** 2026-11-27T17:28:48Z (wake queued), 2026-11-27T18:24Z (review written)
- **Agent:** build-wait-reviewer (wake-ca3c45e6a7a8, opencode)
- **Issue:** issue-6e15970df117 — Wait over budget: exe-build at setup
- **Wake task:** review over-budget exe-build wait at stage setup, elapsed 61.086s
- **Wait / attempt / run:** exe-build:42:2026-11-27T17:27:27.229Z / exe-build-20261127T122727-eaad2173 / 20261127_172726
- **Scope:** read-only review. No source edit, no build/test/runtime started by this agent, no fix or closure claimed.

## Evidence

- `logs/2026-11-27/20261127_172726/dev-loop/events.jsonl`:
  - line 1 (17:27:26.764Z) `dev-loop.control` action=launch mode 45, "AI launch waiting for source validation/build" — the pre-launch EXE rebuild being waited on.
  - line 11 (17:45:37.353Z) `dev-loop.build.completed`: build `2026-11-27T17:45:19.700118Z`, attempt `exe-build-20261127T122727-eaad2173`, duration 1082.086s, `source_generation` 42→46, `source_changed_during_build=true`, setup 0.137s, build_script 1048.061s, post_build 32.129s, artifact publish 8.328s.
  - line 3 (17:29:18.821Z) `dev-loop.wait.issue-failed` is the only wait event for this wait_id: error `agent-wake.py run --once timed out after 30 seconds`. No `dev-loop.wait.over_budget` and no `dev-loop.wait.reviewer-wake`, although the issue was created (17:28:37.692Z) and exactly one wake was queued (wake-ca3c45e6a7a8, 17:28:48.090Z).
- `.dev/build-timings/exe-build-20261127T122727-eaad2173.resource.json`: mean host CPU 99.91%, peak 100%, minimum available memory 25,292,800 B (~24.1 MiB), 694 samples @1s over 1081.851s with the live run active; sampler thread did not stop before timeout, runtime alignment unavailable, cap-render gate failed open after 4s.
- Wake delivery (.dev/agent-wake/events.jsonl + requests/wake-ca3c45e6a7a8.json): one `wake.queued` 17:28:48.090Z, attempts=2; codex claimed/launched 17:30:26.329Z/17:30:26.600Z (pid 27456), `wake.retry` "stale launched request recovered by scheduler" 17:55:34.362Z, opencode claimed/launched 17:57:37.504Z/17:57:37.535Z (pid 20704 = this review). Queue-to-delivered ~29 min.
- Review appended to issue-6e15970df117 (review id 543, verdict `confirmed-expected-long-exe-rebuild; evidence-partial`); `related_build`/`related_run` pinned to the authoritative attempt and run.

## Outcome

- Confirmed real, non-stalled EXE rebuild: the 61.086s trip is a legitimate ~18-minute pre-launch rebuild crossing the flat 60s `WAIT_ISSUE_SECONDS` policy, under ~100% host CPU and ~24 MiB free memory.
- Queue-side acceptance holds (one issue, one `wake.queued`). Journal-side is partial: the `dev-loop.wait.over_budget`/`reviewer-wake` events for this wait are permanently missing.
- First divergence: `build_friction.wake_reviewer` runs `agent-wake.py run --once` inline with timeout=30 and no try/except (`devscripts/build_friction.py:242-249`); `dev-loop.check_wait_budget` wraps record+wake in one try (`devscripts/dev-loop.py:3905-3930`), so `TimeoutExpired` skips the success events and is mislabeled "could not record over-budget wait" after the record+queue already succeeded. `reported_wait_ids` already held the wait_id, so there is no retry.
- Evidence `build_id` is stale `self.latest_build` (08:12:40) and does not name the in-flight attempt; the true identity is `started_utc` + attempt + run (known gap on issue-7d3dcfd7433b / issue-136d4553ff92).

## Follow-up (unowned; no source fix claimed)

- Recurrence appended to the canonical defect issue-9a4503f6ed68 (owner wake-proof-wait), joining its 2026-11-21/23/24/26 occurrences. Suggested smallest fix unchanged: emit `dev-loop.wait.over_budget` immediately after a successful record and make the inline `run --once` best-effort (catch `TimeoutExpired`, as `agent_coord.py:1170-1176` already does).
- Resource-pressure evidence appended to issue-2a5c5da53565 (dev-loop reliability under high CPU/memory).
- Flat wait-budget policy ownership: issue-7d3dcfd7433b (launch-latency-investigation) and issue-e044a931c54b (build-runtime-subagents).
