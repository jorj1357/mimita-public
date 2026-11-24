# issue-c4b82c2ed632: Wait over budget: exe-build at compiling

## Human report (verbatim, immutable)

> exe-build wait at compiling exceeded the wait budget


## Review (reviewed)

### 2026-11-24T02:53:08.853Z — build-wait-reviewer

Reviewer: build-wait-reviewer

test-review-probe

## Review (reviewed)

### 2026-11-24T02:54:14.543Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Review of over-budget wait exe-build:49:2026-11-24T01:41:57.628Z (issue-c4b82c2ed632), run 20261124_005533. Supersedes the earlier short probe review by this same agent. WAIT/ISSUE/WAKE: over_budget 01:43:10.945Z created issue-c4b82c2ed632 once (wait_id exe-build:49:2026-11-24T01:41:57.628Z, stage compiling, elapsed 61.661s, budget 60s); reviewer-wake 01:43:10.855Z queued wake-70987c3bb01d once (codex); wake.queued 01:43:04.135Z, codex claimed/launched 01:43:10.131/01:43:10.487Z then exited, retry to opencode 01:46:08.868Z, opencode claimed/launched 02:27:47.135/02:27:47.306Z. One wake, no duplicate launch, but reviewer reached the wait about 44m43s late (recorded as recurrence on issue-84c584957e65). WAIT WAS REAL: dev-loop.build.completed 01:44:49.921Z, build 2026-11-24T01:44:42.533263Z, generation 49, duration 166.920s, build_script 152.578s, source_identity_publish 10.852s, post_build_processing 13.123s, artifact validation/publish 0.338/1.409s, status ok. RESOURCE: exe-build-20261123T204157-f923e86c captured 117 samples over 166.905s; host mean CPU 99.995%, peak 100%, min available memory 228171776 bytes of 16802918400 (~1.4%, 89% used), pagefile pressure; competing RobloxPlayerBeta ~17.7%, FL64 ~10.4%, SNAPOS64 ~8.1%, OpenCode ~3.4%, Code ~2.5%, live mimita client ~16.8-25.2% whole-host. Host was saturated (direct observation); contention as cause is inferred, not proven. GAPS: unavailable_fields matched_prebuild_resource_baseline, host_commit_charge_and_limit, journal_aligned_live_mode_workload_and_frame_tick_metrics; runtime_alignment unavailable; brain_check.build_resource_gaps lists only game-dll attempts, not this exe-build attempt; over-budget reason field held a progress line '[SKIP] src host host-session-identity.cpp' rather than a diagnosis. DISPOSITION: acceptance met for this occurrence (issue carries wait_id, stage, reason, elapsed, build/run IDs, evidence; one issue and one reviewer wake). Dispatch-latency acceptance NOT met (~44m43s), recorded under issue-84c584957e65. No code fix claimed. Owner unassigned. Next: align exe-build resource sampling to a matched live workload and surface exe-build attempts in build_resource_gaps; reduce compile cost/contention (related issue-136d4553ff92 and cpp-latency agents). Unknown: whether reducing contention alone brings this build under 60s; no isolated uncontended baseline for this revision in this window.
