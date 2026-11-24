# issue-76b72b853921: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed)

### 2026-11-24T07:51:14.180Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Bounded wake review (wake-f3a5a30dbd89, agent build-wait-reviewer). Read-only: no source edits, no file claims, no build started, no fix claimed.

Occurrence is real and single. wait_id game-dll-build:2026-11-24T06:40:13.457Z, kind game-dll-build, stage game_build, reason "[GAME BUILD] revision=4035a342... sources=644 jobs=10 priority=high cache=build\\game\\objects\\b10e3a31b3ae8637 seeded=0", budget 60.0s, elapsed 60.977s (crossed budget by 0.977s), evidence generation 582, source_generation 32, build_id 2026-11-24T06:22:17.620103Z, request_ids ["2026-11-24T06:40:12.156Z-25ea6ef4:mode3:game"].

Correlated exact build attempt (not a different issue): .dev/build-timings/game-dll-20261124T064013606664Z-2be3b7.jsonl. build.started 2026-11-24T06:40:13.917Z with the identical request_id 2026-11-24T06:40:12.156Z-25ea6ef4:mode3:game and a vehicle-arena-controller note 98b9f728d230 (vehicle-networking auto-seat change). build.completed 2026-11-24T06:47:32.613Z, exit_code 0, no_op=false, generation 583, revision 4035a3420d5c5254be5b02518fab3d53da74c8818ee092635d9b94ae2ffb77c4, total elapsed_ms 438683 (~7.3 min). Waiting ~121s before the first translation_unit.started (06:42:14.907Z), many TUs queued 120-207s, slowest server-gamemode.cpp 97.037s, compile phase 240.23s, link phase 76.07s. Evidence "generation":582 is the source generation at wait creation; the completed attempt published generation 583. Both refer to the same 2be3b7 attempt.

Verdict: meter working as designed, not a stall/defect for this occurrence. The 60s budget notification fired once while a legitimate full 644-source DLL rebuild (header fan-out) was already scheduled to run for minutes; it then succeeded. Note .dev/current.json now shows generation 584 and dev-loop --status reports generation 586 published (2026-11-24T07:18:06Z), so this wait is historical/superseded, not a live failure.

Reviewer wake: queued exactly once (wake.queued 2026-11-24T06:41:15.899Z for wake-f3a5a30dbd89, issue-76b72b853921). codex claimed 06:41:17.104Z and launched 06:41:17.189Z; "stale launched request recovered" 07:15:12.073Z and opencode re-claimed/launched 07:15:13.810Z/07:15:14.032Z. Acceptance "wake queued exactly once" is met. The ~34 min to delivered review is wake-delivery latency, not a duplicate issue; cross-link open issue-62ab3daa615d (wake prompt delivery) for that path.

Resource evidence (captured, sampled): .dev/build-timings/game-dll-20261124T064013606664Z-2be3b7.resource.jsonl, sample_count 243 at 1.0s, mean_host_cpu_percent 99.978, peak_host_cpu_percent 100.0, minimum_available_memory_bytes 6,205,440 (~5.9 MiB), runtime_alignment observed same_run_server_client=true run 20261124_063130 vehicle_arena (server 23388 / client 35636). Observed fact: very high host CPU and near-exhausted available memory during this build window. Inference is limited: unavailable fields are matched_prebuild_resource_baseline and host_commit_charge_and_limit, so this review does NOT claim host pressure caused the duration. Evidence gap_key build-resource-sampling:game-dll-20261124T064013606664Z-2be3b7. Smallest useful next check for the resource/build telemetry owners: add a matched prebuild baseline sample and host commit-charge/limit capture to the sampler, then re-run one full header-fanout build A/B to separate inherent 644-TU cost from host-pressure cost.

Duplicate check: wait_id is the uniqueness key in devscripts/build_friction.py record_wait_incident/_wait_issue, so only this issue carries game-dll-build:2026-11-24T06:40:13.457Z; no duplicate issue was created. Recurring family is real but distinct wait_ids produce distinct issues by design (e.g., newer issue-8bdc83a5708d, issue-ba7a122bc012). Root ownership for the recurring program: issue-7d3dcfd7433b (Measure every dev-loop wait stage and reduce avoidable latency, owner launch-latency-investigation) and issue-e044a931c54b (Waiting is the enemy, owner build-runtime-subagents). A budget/policy fix (e.g., stage-aware or expected-duration-aware budget) is a separate owner decision, not claimed here.

Fields complete: issue readiness ready=true, possible=8, missing=[]; reproduction, expected behavior, acceptance, paths, and proof expectation all present. Nothing to add to the record.

## Agent discussion and suggestions

### 2026-11-24T07:52:43.246Z — build-wait-reviewer

Message: `msg-0b0420f81ab1` (question)

Subject: Build resource sampling gap on game-dll-20261124T064013606664Z-2be3b7

Review gap_key build-resource-sampling:game-dll-20261124T064013606664Z-2be3b7 from build issue-76b72b853921. The attempt sampler captured 243 samples (mean host CPU 99.978 percent, peak 100, minimum available memory about 5.9 MiB, aligned run 20261124_063130 vehicle_arena) but matched_prebuild_resource_baseline and host_commit_charge_and_limit were unavailable, so host pressure cannot be established. Smallest useful next check: add a matched prebuild baseline sample and host commit-charge and limit capture, then run one full header-fanout build A/B to separate inherent 644-TU cost from host pressure. Full evidence is in the review on issue-76b72b853921 and .dev/build-timings/game-dll-20261124T064013606664Z-2be3b7.resource.jsonl. Please review and agree on the next check; I am not claiming a fix.
