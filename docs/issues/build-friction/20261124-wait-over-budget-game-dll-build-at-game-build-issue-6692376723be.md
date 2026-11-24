# issue-6692376723be: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-normal-slow-single-tu-compile-plus-attribution-and-dispatch-defects)

### 2026-11-24T22:00:17.402Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Independent bounded review by build-wait-reviewer for over-budget wait game-dll-build:2026-11-24T21:00:27.991Z (issue issue-6692376723be); read-only for source, build, runtime, and issue state. Wake wake-dbeab2f5c771.

OBSERVED FACTS

1. Wait identity and uniqueness. kind=game-dll-build, stage=game_build, started_utc=2026-11-24T21:00:27.991Z, elapsed=62.207s vs budget 60.0s, source_generation=21, request 2026-11-24T21:00:05.413Z-8fe2e691:mode3:game; issue created 2026-11-24T21:01:31.694Z. Exactly one build-friction issue (223 total) carries this wait_id.

2. The waited-on work was real and SUCCEEDED. Request 8fe2e691 maps to attempt game-dll-20261124T210028320585Z-d73143 (started 21:00:28.320Z, completed 21:02:33.469Z, ready 21:02:34). It compiled the one changed TU src/gamemode/arena-ball.cpp in 86.54s (changed_source_preflight 86.662s), then identity provider 4.65s, link 17.21s, publish ~0.56s; total 125.149s. gcc -ftime-report shows preprocessing 54.15s and parsing 64.54s dominate the TU (85% by parse). At the 60s budget point the build was ~48s into that 86.5s compile. This is normal (if slow) single-TU work, not a stall. Sources: .dev/build-timings/game-dll-20261124T210028320585Z-d73143.json/.jsonl and the run journal.

3. Evidence attribution defect (observed). The issue records build_id=2026-11-24T19:27:02.742428Z (the artifact published ~93 min earlier) and generation=620, but the in-flight attempt was d73143 and it became generation 621; related_build and related_run are empty. The attempt is recoverable only via request_ids[0] + started_utc. Source: dev-loop.py:3884-3900 stamps self.latest_build and self.game_build_generation instead of the active attempt id/generation. This is the same stale in-flight identity defect tracked on issue-21e4b1fbc77e and umbrella issue-9a4503f6ed68.

4. Journal half missing and the only row is misleading (observed). A grep of logs/2026-11-24/20261124_205956/dev-loop/events.jsonl found exactly one row carrying this wait_id: dev-loop.wait.issue-failed at 21:02:04.621Z, error "Command [... agent-wake.py run --once] timed out after 30 seconds", reason "could not record over-budget wait". There is NO dev-loop.wait.over_budget and NO dev-loop.wait.reviewer-wake row for this wait, even though the issue (21:01:31.694Z) and the wake (21:01:34.368Z) both exist. Cause: dev-loop.py:3905-3930 wraps record_wait_incident + wake_reviewer in one try; wake_reviewer synchronously runs agent-wake.py run --once with a 30s timeout (build_friction.py:244-246); that call timed out and the broad except logged a failure for an operation that had already recorded the issue and queued the wake. The recorded failure reason is therefore inaccurate. Related to but distinct from issue-6e2d7b91e7d9 (events_path None).

5. Reviewer wake: one queue, two launches, 43.6 min to final launch (observed). Exactly one wake.queued exists for issue-6692376723be: wake-dbeab2f5c771 at 21:01:34.368Z (requests/wake-dbeab2f5c771.json: attempts=2). Two wake.launched events exist for it: codex at 21:13:39.832Z and opencode at 21:45:08.474Z. Queue-to-final-launch = 2614.1s (~43.6 min). wake.retry to opencode was logged at 21:24:13.300Z, but opencode did not claim until 21:45:08.388Z, immediately after another wait's run --once (wake-59dc989b28d0 for issue-ee3e906c8273 queued 21:45:00.693Z, launched 21:45:07.333Z). Observation: no persistent daemon drained the fallback; the next wait's run --once did. This reproduces issue-84c584957e65 and violates the exactly-one-launch target.

6. Per-wait launch attribution in the journal is ambiguous (observed). dev-loop.wait.reviewer-wake at 21:45:08.562Z is labeled wait_id=21:43:56.034Z / issue-ee3e906c8273, yet its wake.launch_result embeds this wait's wake-dbeab2f5c771 prompt and log path. build_friction.py:244-251 stores the raw stdout of run --once, which drains and reports multiple pending wakes, so a reader cannot attribute a launch to a wait from that row.

7. Resource pressure (direct observation; cause NOT proven). Attempt d73143's sampler recorded 108 samples over 125.14s: mean host CPU 99.995%, peak 100%, minimum available memory 663,552 bytes (~648 KiB). Same-run pair: server mode jugger_horde with 67 NPCs (ai_ms 614.5 of 663.5ms per 24 ticks), client 111.4 fps with 112/112 frames over the 4.17ms budget at 21:02:30. Sampler unavailable_fields: matched_prebuild_resource_baseline, host_commit_charge_and_limit, client_mode_independent_confirmation. Whether saturation elongated the 86.5s compile is an inference, not established.

8. Duplicate flood (observed). 177 of 223 build-friction issues share the exact title "Wait over budget: game-dll-build at game_build" (159 in_review, 17 new, 1 assigned). Same-signature waits arrived at 20:42:56 (issue-d6a3c6c35682), 21:00:27 (this issue), and 21:43:56 (issue-ee3e906c8273). Cause: record_wait_incident keys uniqueness on per-occurrence wait_id (build_friction.py:131-169) while the fixed 60s budget is shorter than a normal changed-TU front-end compile. This realizes the issue's own stated risk. Aggregation/budget belongs on umbrella issue-9a4503f6ed68, not a new issue.

9. Acceptance check. "carries wait_id, stage, reason, elapsed, build/run IDs, and evidence" is NOT met as written (stale build_id, off-by-one generation, empty related_build/related_run, no success wait event). "A reviewer wake is queued exactly once for this wait occurrence" is met at the queue level (one wake.queued) but NOT at dispatch (two launches, ~43.6 min).

CONCLUSION
Not a stalled build. The 60s budget overlapped a genuine, successful 86.5s single-TU compile (src/gamemode/arena-ball.cpp) whose time is dominated by 54.2s preprocessing. Confirmed secondary defects: stale attempt identity in the issue evidence, a misleading journal failure row instead of wait.over_budget/reviewer-wake, one duplicate launch with ~43.6 min dispatch latency, ambiguous per-wait launch attribution, and a 177-issue duplicate flood. This review performed no edit, build, runtime launch, issue close, or proof claim.

OWNER / NEXT STEP (not claimed by this review)
- issue-9a4503f6ed68 owner wake-proof-wait: include in-flight attempt id/generation, populate related_build; aggregate/suppress per-wait duplicates or set the budget to measured per-TU compile time.
- issue-84c584957e65: persistent daemon/retry scheduling to bound dispatch and guarantee exactly one launch; parse run --once output per wake instead of storing the whole drain.
- dev-loop wait hook (build_friction.py:205-252 / dev-loop.py:3901-3930): emit wait.over_budget independently of the synchronous wake attempt, and do not report a wake-launch timeout as "could not record over-budget wait".
- issue-136d4553ff92 (build owner): arena-ball.cpp 86.5s TU with 54.2s preprocessing is a header-fanout/per-TU compile cost, not a wait-policy problem.
- issue-ed3ac775283d owner root-resource-loop: host at ~100% CPU and 648 KiB available during the wait; decouple long builds from live-pair saturation.

UNKNOWNS
- Whether the 30s agent-wake.py run --once timeout was caused by adapter startup or by the next-wait drain.
- Whether host saturation causally lengthened the 86.5s compile.
- Whether the 663,552-byte minimum-available-memory sample is accurate or a kernel reporting artifact.
