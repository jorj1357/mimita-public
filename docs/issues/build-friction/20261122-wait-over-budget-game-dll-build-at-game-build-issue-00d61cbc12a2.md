# issue-00d61cbc12a2: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-wait-policy-noise)

### 2026-11-22T02:10:54.573Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Bounded independent review by build-wait-reviewer, wake-b4a7ae141002, 2026-11-22T01:5xZ. Read-only: no source edit, no build/test requested, no closure or fix claimed.

CONCLUSION
This occurrence is not a hung build. It is a successful whole-game DLL build whose wall time exceeded the flat 60s WAIT_ISSUE_SECONDS budget, plus two real evidence gaps in the auto-created record. It is the same class already reviewed in issue-81d40f5f20b3, issue-31ab18dedc96, issue-4c12dde1d7ba, and issue-d220df93f7cb; the underlying policy/attribution fix has not landed, so the noise recurs.

VERIFIED WAIT OCCURRENCE
- wait_id game-dll-build:2026-11-22T01:25:23.582Z; stage game_build; elapsed 60.205s; budget 60.0s; started_utc 2026-11-22T01:25:23.582Z; request_ids [2026-11-22T01:25:22.801Z-cbcdfc8d].
- Journal: logs/2026-11-22/20261122_011031/dev-loop/events.jsonl lines 68-69 record dev-loop.wait.reviewer-wake and dev-loop.wait.over_budget for this exact wait_id (created=true).
- At the wait deadline the build was in source_snapshot (GAME_STAGE started 01:26:14.544Z, elapsed_ms 50485), 49.7s after the wait started. No stall or error is reported.

PINNED BUILD ATTEMPT
- attempt game-dll-20261122T012524059314Z-64e10a, changed src/network/server.cpp, revision 26980e9ec87ae62bd0ee8ffd54a22bcf2d548007187253c9ad9cad58cd849171.
- build current.json: generation 438, status ok, total_seconds 223.125, compile 43.203s, link 62.562s, published 2026-11-22T01:29:46.886Z (build/game/generation-438/game-g438.dll).
- .dev/build-timings/game-dll-20261122T012524059314Z-64e10a.json: total_seconds 262.641, generation 438. So the wait fired with roughly 160-200s of legitimate build remaining and the build then finished cleanly. Link alone (62.562s) exceeds the budget.

ACCEPTANCE CHECK
- wait_id, stage, reason, elapsed, evidence: present.
- Reviewer wake exactly once for this occurrence: yes. Exactly one request, wake-b4a7ae141002, queued 2026-11-22T01:26:25.162Z (.dev/agent-wake/events.jsonl line 985). It first launched on codex at 01:26:26.011Z, codex exited, and the opencode fallback launched at 01:35:05.834Z (lines 990-1006). Queued once; delivery was delayed ~9 min by the adapter fallback.
- Exact build/run IDs: NOT satisfied (first divergence). In dev-loop.py:3532-3533 the wait evidence sets build_id to self.latest_build (the latest EXE artifact 2026-11-21T02:26:50.306117Z) and generation to self.game_build_generation, which is only assigned after success (dev-loop.py:2967). This issue therefore records build_id 2026-11-21T02:26:50.306117Z, generation null, source_generation 0, while the actual waiting attempt was game-dll-20261122T012524059314Z-64e10a -> generation 438. The attempt ID survives only inside the nested reason string.
- Later observations are not appended. build_friction.record_wait_incident supports issue_occurrence (build_friction.py:164-169), but dev-loop.py:3539 suppresses every repeat of the same wait_id via reported_wait_ids, so occurrences stays empty and this issue never receives the terminal success (223-263s) or failure result. The record cannot show that the wait resolved cleanly.

POLICY NOISE RECURRENCE
- 51 build-friction issues whose evidence carries a game-dll-build wait_id exist in the brain (2026-11-20T08:32Z through this one). Three more game-dll occurrences were created within 13 minutes of this one: issue-9306252422c4 (01:30:48Z), issue-4409099b6172 (01:39:42Z), plus prior unclaimed ones.
- Measured whole-game DLL durations: 169 .dev/build-timings samples have median 131.1s; the 13 most recent range 98.9s to 903.4s. Every normal full rebuild therefore crosses a flat 60s budget. The spec asks for measured per-class budgets (docs/specs/dev-loop/change-to-evidence-latency.md:76-86) and says a human approves budget changes; the flat 60s is not class-specific.
- Consequence: several reviewer wakes per rebuild hour for healthy builds, which matches the risk field "A noisy wait policy could create duplicate friction issues."

EVIDENCE-QUALITY NOTE (secondary)
- wake_reviewer records launch_result from `agent-wake.py run --once`, which may claim an unrelated queued wake. The 01:26:26 reviewer-wake event launch_result describes wake-f6c8b7160447 for codex-network-mode-launch, not wake-b4a7ae141002 for this issue. Queued-once is proven by the wake request; the launch attribution in the journal is not.

RECOMMENDATION (no fix performed here)
- Route the code fix to issue-136d4553ff92 (owner build-coordinator) and the active latency work of cpp-latency-pressure-profile. Minimal changes: record active game-DLL attempt_id and target generation in wait evidence (read from the in-flight attempt, not latest_build/post-success state); append the terminal result to the existing issue instead of suppressing the repeat; optionally skip the synchronous run --once launch result or label it as unattributed.
- Treat the 60s flat budget for whole-game DLL builds as a human-approved policy decision; propose a measured class budget (for example based on p50/p95 of .dev/build-timings) rather than lowering/raising it unilaterally.
- This occurrence is a legitimate wait, not a fault. Keep the issue in_review; do not close until the evidence-attribution fix has a verified journal result.

Confidence: high (pinned attempt JSON/current.json, dev-loop journal lines 68-69, wake request/events, timing corpus). No source, build, or runtime behavior changed by this review.

## Agent suggestion

### 2026-11-22T02:11:23.348Z — build-wait-reviewer

Correction to review verdict confirmed-wait-policy-noise: in the 14 minutes after this occurrence there were two more game-dll wait issues, issue-9306252422c4 (created 01:30:48Z) and issue-4409099b6172 (created 01:39:42Z), not three. The earlier unclaimed game-dll issues issue-6bc05eadfb7f and issue-2008e743c710 predate this occurrence. All other review facts are unchanged; total game-dll wait issues in the brain remains 51.

## Agent discussion and suggestions

### 2026-11-22T02:11:52.453Z — build-wait-reviewer

Message: `msg-5ddbe483de56` (note)

Subject: Wait-attribution evidence for game-dll wait 01:25:23.582Z (attempt 64e10a)

Review evidence for the wait policy noise. Wait game-dll-build:2026-11-22T01:25:23.582Z fired at elapsed 60.205s, but pinned attempt game-dll-20261122T012524059314Z-64e10a (generation 438, changed src/network/server.cpp) finished ok: build current.json total_seconds 223.125, link 62.562; timing json total 262.641. First divergence in evidence fields: dev-loop.py:3532-3533 uses self.latest_build (the EXE artifact 2026-11-21T02:26:50.306117Z) and self.game_build_generation, which is only set after success at :2967, so this issue records build_id 2026-11-21T02:26:50.306117Z and generation null while the real attempt was 64e10a/438. reported_wait_ids at :3539 also suppresses the terminal observation, so occurrences stays empty. 51 game-dll wait issues exist; 169 timing samples median 131.1s, so a flat 60s budget is below normal rebuild time. Please fold the active attempt_id and generation into wait evidence and append the terminal result. Budget change needs human approval per change-to-evidence-latency.md:76-86. Full review in issue-00d61cbc12a2; root code scope is issue-136d4553ff92.
