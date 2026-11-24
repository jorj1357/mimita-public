# issue-627ca4b9f577: Wait over budget: exe-build at linking

## Human report (verbatim, immutable)

> exe-build wait at linking exceeded the wait budget


## Context

### 2026-11-24T04:30:05.135Z — build-wait-reviewer

related_build: 2026-11-24T03:30:52.372066Z:3ae36c9314a776a66c842b97356f14e128171e1442d28e49cd18a110b6030db6
related_run: 20261124_022440_01

## Review (reviewed)

### 2026-11-24T04:30:58.684Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Review of over-budget wait exe-build:20:2026-11-24T03:29:41.809Z (issue-627ca4b9f577), run 20261124_022440_01. Read-only: no source edit, no file claim, no fix claimed.

WAIT / ISSUE / WAKE
- dev-loop.wait.over_budget at 2026-11-24T03:30:44.771Z: wait_id exe-build:20:2026-11-24T03:29:41.809Z, kind exe-build, stage linking, reason "linking mimita.exe (object_set_changed)", elapsed 60.25s, budget 60.0s, started_utc 03:29:41.809Z. Issue created once (created=true).
- Exactly one dev-loop.wait.reviewer-wake event for this wait_id; wake-7d93d4ea8cb4 queued once at 03:30:43.177Z. Codex claimed/launched 03:30:44.585/44.598Z then exited; wake.retry to opencode 03:35:41.743Z; opencode claimed/launched 04:02:53.740/53.753Z. Queued-to-launch delay 1930.576s (~32m11s), far beyond the proposed <=5 min dispatch bound. Recurrence appended to issue-84c584957e65.
- Only two journal rows mention this wait_id (over_budget + reviewer-wake), so no duplicate issue and no duplicate wake.

WAIT WAS REAL, BUT THE STAGE LABEL MISATTRIBUTES THE COST
- Build identity: build 2026-11-24T03:30:52.372066Z, attempt exe-build-20261123T222942-3b725c52, request 2026-11-24T03:29:40.150Z-4e3952c7:mode3:cold, EXE success. worker_timing total 71.185s: build_script 61.413s, source_identity_publish 7.472s, post_build_processing 9.093s, artifact validation+publish 0.177+0.675s, unattributed 0.444s. build.py itself printed "Time: 59.52 sec".
- Resource-sampler phase timeline (66 samples at 1s; .dev/build-timings/exe-build-20261123T222942-3b725c52.resource.jsonl): presentation_dll ~03:29:44 to 03:30:06; compiling until 03:30:40.188 (build elapsed 56.809s); linking 03:30:40.188 -> build_complete 03:30:44.464 (build elapsed 59.525s), an ~4.3s link window. The 60.25s threshold check at ~03:30:42.06 therefore landed inside that link tail, so "at linking" is the instantaneous phase at the crossing, not 60s of link time. Pre-link work consumed ~58s of the budget.
- .dev/build-output.log corroborates: "[HOT RELOAD] optional presentation DLL failed (build_game_dll.py exited with code 1, 21.579s); continuing with the EXE build"; "Compiled: 4 / Skipped: 83"; "[LINK] mimita.exe reason=object_set_changed". .dev/presentation-dll-attempt.json stores only "build_game_dll.py exited with code 1" (no compiler text). A separate game-DLL preflight in the same run failed on src/ragdoll/ragdoll-mode.cpp:991 (two-argument glm::angle). Whether the optional presentation DLL failed for the same reason is inferred, not proven, because its child output was not captured.
- Resource pressure (direct observation): mean host CPU 99.955%, peak 100% on 12 logical CPUs; minimum available memory 452,325,376 B of 16,802,918,400 (~2.7% free, 97% used) at 03:30:26.259Z; pagefile 36.5/45.5 GB used. Top processes at ~03:30:41-43Z: RobloxPlayerBeta ~13.6 whole-host %, live mimita.exe client ~12.3%, FL64 ~9.6%, SNAPOS64 ~8.0%, second mimita.exe ~6.4%, pythonw ~5.9%. Contention is observed; it is not proven to be the cause of the 1.4s overshoot.
- Verdict: not a stalled or runaway link. The generic flat 60s wait budget applied to a normal EXE build is the recurring root, and this build merely crossed it in the link tail. Already tracked on issue-136d4553ff92 (flat WAIT_ISSUE_SECONDS applied to every wait kind, plus phase-snapshot attribution).

ACCEPTANCE CHECK FOR THIS OCCURRENCE
- wait_id, stage, reason, elapsed, evidence: present. Issue created exactly once; reviewer wake queued exactly once: met.
- build/run IDs: partially met before this review. evidence.build_id = 2026-11-24T01:44:42.533263Z is the previously published EXE artifact, not the waiting build 2026-11-24T03:30:52.372066Z; request_ids identify the request, but run_id was absent. This review set related_build to the waiting build and related_run to 20261124_022440_01.
- Dispatch bound: not met (~32m11s); recorded on issue-84c584957e65.

GAPS (reported, not duplicated as new issues)
- exe-build-20261123T222942-3b725c52 is absent from brain_check.build_resource_gaps (top 3 were game-dll attempts), so its unavailable_fields are invisible; already raised on issue-136d4553ff92.
- EXE worker_timing has no per-phase split (setup/presentation_dll/compiling/linking); the wait stage is a point-in-time snapshot. Already raised on issue-136d4553ff92.
- Optional presentation-DLL failure text is dropped (exit code only). Already raised on issue-136d4553ff92.

Unknowns: authoritative link duration (sampler resolution ~1s); whether host saturation or the failed optional DLL alone explains the 1.4s overshoot; no uncontended matched baseline for this revision in this window.

No code fix claimed. Owners: issue-136d4553ff92 (stage/budget/observability) and issue-84c584957e65 (wake dispatch).
