# issue-e87167e20d7d: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-true-positive-wake-record-defect)

### 2026-11-26T23:37:35.823Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Independent bounded review by build-wait-reviewer (activation wake-c7a184e091be), read-only: no source edit, no build/test request, no runtime/behavior claim, no issue close.

VERDICT: confirmed true positive. A real whole-game DLL compile genuinely exceeded the 60 s wait budget (it was not wedged), and exactly one build-friction issue and one reviewer wake were created for this wait occurrence. One NEW defect found in the wait-incident recording path: the inline wake-launch timed out at 30 s, so the dev-loop logged a misleading dev-loop.wait.issue-failed and dropped the dev-loop.wait.over_budget and dev-loop.wait.reviewer-wake journal events.

WAIT UNDER REVIEW (pinned)
- wait_id: game-dll-build:2026-11-26T22:56:31.735Z
- kind/stage: game-dll-build / game_build
- elapsed at record: 62.64 s vs budget 60.0 s
- started_utc: 2026-11-26T22:56:31.735Z
- source_generation: 18
- request_ids: [2026-11-26T22:56:31.473Z-e7e4fffa:mode3:game]
- wait record build_id: 2026-11-26T22:52:33.481041Z (previous EXE artifact), generation: null, no run_id (known identity defect, see below)
- in-flight attempt (from reason tail and build artifacts): game-dll-20261126T225631910089Z-cf9679, generation 733, revision 6978f811ff6f2b9d0b420f0a601b1e91e872212ca67efcdc76ffdd8f893183bc, dev-loop run 20261126_225234

BUILD OUTCOME (direct artifacts)
- .dev/build-timings/game-dll-20261126T225631910089Z-cf9679.json: started 22:56:31.910Z, completed 23:00:06.138Z, total_seconds 214.235, success true, no_op false, dll build\game\generation-733\game-g733.dll.
- phase_durations_ms: changed_source_preflight 14188 (6.6%), compile 179500 (83.8%), link 5906 (2.8%), object_cache_scan 2234, publish 562, coherence_validation 969. compile dominates.
- compiled 75 TUs, skipped 570, source_count 644, jobs 10. slowest TUs: map-editor.cpp 44.628 s, ragdoll-mode.cpp 42.155 s, engine-tick-net.cpp 40.190 s.
- batch sealed 10 changed compiler inputs (dev-loop.batch.sealed 22:56:31.938Z) including headers renderer.h, player-avatar-animations.h, post-fx.h; cache_invalidations list renderer.h / player-avatar-animations.h / post-fx.h "dependency newer than object" (~27 TUs) plus 2 "missing or invalid dependency file" and 1 "object file is empty". This is a header-fanout rebuild, not a 2-file incremental build. Directly changed: src/entities/player-animation.cpp, src/render/postfx-commands.cpp.

RESOURCE PRESSURE (observed vs inferred; per resource-evidence handoff)
- Sample window 2026-11-26T22:56:31Z to 23:00:06Z, 161 samples at 1 s; host logical CPUs 12.
- Observed: mean host CPU 98.415%, peak 100.0% (whole machine); minimum available memory 24,162,304 B (~23.0 MiB); 14 of 161 samples had < 200 MB available.
- Observed top competing processes at the CPU-peak sample (elapsed 35.7 s): MsMpEng.exe (Windows Defender) 65.4% of one logical CPU = 5.45% whole host; System 62.5% = 5.21%; ChatGPT.exe 58.5% = 4.88%; plus python.exe.
- Reported runtime context: mode vehicle_arena, map arena2, NPCs 0 (mode source: dev-loop state; unverified against journal).
- Inferred (not causal proof): the host was near-saturated on both CPU and free memory during the compile, which plausibly lengthened per-TU compile time. There is no matched no-build baseline (unavailable_fields: matched_prebuild_resource_baseline, host_commit_charge_and_limit, journal_aligned_live_mode_workload_and_frame_tick_metrics), so resource pressure is a contributing co-factor, not a quantified cause. Dominant cause is the ~75-TU header-fanout compile.
- Artifacts: .dev/build-timings/game-dll-20261126T225631910089Z-cf9679.resource.json and .resource.jsonl.

WAIT RECORD / WAKE / DEDUPE (pinned)
- logs/2026-11-26/20261126_225234/dev-loop/events.jsonl line 129: dev-loop.wait.issue-failed, wait_id game-dll-build:2026-11-26T22:56:31.735Z, error "agent-wake.py run --once timed out after 30 seconds", reason "could not record over-budget wait", wall 22:58:12.872Z.
- There is NO dev-loop.wait.over_budget and NO dev-loop.wait.reviewer-wake event for this wait in that journal (contrast the prior reviewed occurrence issue-99494df70b20, which had both).
- The issue WAS recorded: coord.issue.opened issue-e87167e20d7d 22:57:35.047Z (.dev/agent-coord/events.jsonl seq 20598).
- Exactly one wake was queued: .dev/agent-wake/events.jsonl wake.queued wake-c7a184e091be 22:57:42.545Z; one issue references this wait_id; no duplicate build-friction issue exists for it.

DEFECT (new, first divergence; reported only, no code edited)
- build_friction.wake_reviewer (devscripts/build_friction.py:244-246) runs agent-wake.py run --once with a hard timeout=30 and no try/except; subprocess.run raises TimeoutExpired, so wake_reviewer raises after the issue was already created and the wake queued.
- dev-loop.check_wait_budget (devscripts/dev-loop.py:3905-3930) wraps record_wait_incident AND wake_reviewer in one try; the raise skips dev-loop.wait.reviewer-wake and dev-loop.wait.over_budget and lands in the except, emitting dev-loop.wait.issue-failed "could not record over-budget wait" (false: the issue was recorded).
- Because reported_wait_ids already contains the wait_id (added before wake_reviewer), the loop does not retry, so this wait's over_budget/reviewer-wake evidence is permanently lost. The event stall at 22:58:12 coincided with the still-running 100%-CPU compile, so the 30 s inline drain was too tight under the same pressure.
- Impact: the acceptance evidence "reviewer wake record" (dev-loop.wait.reviewer-wake) is missing for this occurrence, and the misleading issue-failed event could prompt a duplicate manual issue. The wake itself still worked (below).
- Suggested smallest fix (for the owning agent/human, not edited here): treat the inline run --once launch as best-effort inside wake_reviewer (catch TimeoutExpired, return queued/launch_error) and/or emit dev-loop.wait.over_budget and dev-loop.wait.reviewer-wake independently of inline-launch success, with an accurate launch-status field. Cross-linked to issue-9a4503f6ed68 (owner wake-proof-wait).

DELIVERY LATENCY (cross-link)
- Wake queued 22:57:42.545Z, first claimed/launched by codex 23:05:01.7Z (~7.3 min), codex exited, wake.retry to opencode 23:06:41.6Z, opencode claimed/launched 23:29:26.4Z (~31.7 min after queue); this review is that opencode launch. Same reviewer-wake latency class as issue-84c584957e65; not re-filed here.

ACCEPTANCE
- Issue carries wait_id, stage, reason, elapsed, build/run IDs, evidence: PARTIAL. wait_id/stage/elapsed/source_generation/request_ids present; readiness ready=true missing=[]. reason is a truncated mid-JSON tail (dev-loop.py:3112 [-240:]) and build_id/generation/no run_id describe the previous EXE artifact, not attempt cf9679 (known identity/reason defect, issue-9a4503f6ed68 occurrences; issue-6e08d89b55e3). related_build/related_run for this issue are set to the in-flight attempt/run below.
- Reviewer wake queued exactly once for this wait occurrence: PASS (single wake-c7a184e091be; single referencing issue). The dev-loop journal event for it is missing due to the DEFECT above.

ROOT-CAUSE ATTRIBUTION (observed vs inferred)
- Observed: compile phase 179.5 s of 214.2 s (83.8%) from a ~75-TU header-fanout rebuild after renderer.h / player-avatar-animations.h / post-fx.h edits. The wait is a true positive, not a stall.
- Inferred: a flat 60 s budget cannot separate a normal header-fanout rebuild from a wedge; ~90 of 100 open build-friction issues are game-dll-build ~60-62 s, so the documented "noisy wait policy" risk has materialized (policy, cross-linked below).
- Unknown: exact CPU/memory contribution (no matched baseline).

CROSS-LINK / OWNERSHIP
- Policy/recurrence umbrella: issue-7d3dcfd7433b (Measure every dev-loop wait stage and reduce avoidable latency; owner launch-latency-investigation) and issue-e044a931c54b (Waiting is the enemy; owner build-runtime-subagents).
- Wait-hook identity/reason plus this wake-launch defect: issue-9a4503f6ed68 (owner wake-proof-wait). Reviewer-wake latency: issue-84c584957e65.
- No code fix claimed here; this reviewer does not own devscripts/dev-loop.py or devscripts/build_friction.py. Leave this issue open as a true-positive occurrence.

EVIDENCE PATHS
- logs/2026-11-26/20261126_225234/dev-loop/events.jsonl (lines 84, 114, 129, 175)
- .dev/build-timings/game-dll-20261126T225631910089Z-cf9679.json / .jsonl / .resource.json / .resource.jsonl
- .dev/agent-wake/events.jsonl (wake-c7a184e091be)
- .dev/agent-coord/events.jsonl (issue-e87167e20d7d seq 20598)

## Context

### 2026-11-26T23:37:41.402Z — build-wait-reviewer

related_build: game-dll-20261126T225631910089Z-cf9679
related_run: 20261126_225234
