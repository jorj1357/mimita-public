# issue-5f49047dae49: Culling, LOD, and work-avoidance backlog

## Human report (verbatim, immutable)

> suggestions like , ai shouls suggest more ideas like that efficiency and performance and fps increasses, make it so that ai can sugegst these thigns and raise issues and explan new workflows or ideas and put them in shared ai brain


## Task linked

### 2026-11-20T05:41:47.381Z — runtime-60hz

Task: culling-visibility (step 0)

## Task linked

### 2026-11-20T05:41:49.498Z — runtime-60hz

Task: offscreen-simulation-lod (step 0)

## Task linked

### 2026-11-20T05:41:51.145Z — runtime-60hz

Task: ui-dirty-budget (step 0)

## Task linked

### 2026-11-20T05:41:52.693Z — runtime-60hz

Task: network-relevance (step 0)

## Task linked

### 2026-11-20T05:41:54.338Z — runtime-60hz

Task: performance-auto-proposals (step 0)

## Context

### 2026-11-20T06:25:32.428Z — work-metrics

reproduction: Run the 67-NPC scenario and inspect frustum/LOD culling, UI atomics, network rate limits, and ragdoll/particle work.
expected_behavior: Each stage uses a bounded owner and avoids work outside the visible/useful set while preserving 60 Hz gameplay.
acceptance: Backlog has owner, measurement, and proof for each item; implemented items reduce work without visual regressions.
paths: ['docs/specs/performance/culling-and-work-avoidance.md', 'src/engine/engine-tick-render.cpp', 'src/config/collision-lod-config.cpp']
evidence: {'spec': 'docs/specs/performance/culling-and-work-avoidance.md', 'runtime': 'logs/2026-11-20/20261120_040807'}

## Scheduled independent review

### 2026-11-22T08:25:02.117Z — fun-cs-npc-objective

A bounded independent review was queued as wake wake-8de1f02ae72d after 266 completed work sessions (trigger=work_end). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (evidence-gap)

### 2026-11-22T08:37:47.342Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Reviewer: issue-review-sweeper

Scope: one bounded independent read-only review of issue brief/readiness, proof status, linked artifacts, spec, and the newest relevant journals. No source edit, build, run start, close, or fix is claimed by this review.

OBSERVED
- Readiness: ready=false, score 6/8, state=new, owner=unassigned. issue readiness reports missing proof_expectation and risk; observe additionally flags missing summary. All 5 tasks (culling-visibility, offscreen-simulation-lod, ui-dirty-budget, network-relevance, performance-auto-proposals) are state=open, owner=unassigned, and have empty acceptance, method, evidence, and proof fields; every task latest_proof is null, so there are no task_proof records for this issue.
- Acceptance in the issue is Backlog has owner, measurement, and proof for each item, and the linked spec (docs/specs/performance/culling-and-work-avoidance.md) requires each entry to become a task with an owner, expected metric gain, evidence, risk, and a pinned-run acceptance check. None of the 5 tasks currently carries those fields, so the issue own acceptance is not met as recorded.
- The only cited runtime evidence path, logs/2026-11-20/20261120_040807, is absent. logs/2026-11-20 does not exist; a recursive search of logs/ finds no *040807* directory. The legacy-format logs/11-20-2026 directory exists but contains 0 entries (last write 2026-11-21T16:52:51 local). The issue therefore cannot be reproduced from its own evidence citation.
- The spec and linked source paths exist: docs/specs/performance/culling-and-work-avoidance.md (17 backlog candidates), src/engine/engine-tick-render.cpp, and src/config/collision-lod-config.cpp. git log --since=2026-11-20 for these three paths returns no commits, and git status for them is clean at HEAD 9fd637d595a308483a9f82c88349a5915c82a0ca (2026-11-21 22:38:28 -0500). There is no repository evidence that a backlog item was implemented under this issue.
- Inherited, explicitly cross-issue evidence: the 2026-11-22T07:33Z evidence review of issue-549680e53b26 reports current-generation gen-470 FCS run 20261122_065419_fcs-route-proof-final (client-01 and server-01 exist) at 98.03 percent of frames over the 4.1667 ms budget, median window p50 7.65 ms, max frame 5,500.36 ms, zero clean windows; neighboring FCS runs are also 96-97 percent over budget. That run is FCS with npcs_active=0, not this issue 67-NPC scenario.
- The 67-NPC baseline is already recorded as failing in issue-8bfc1ca56222 triage: pinned run 20261120_031127 produced 67 NPCs but server loop was 11-13 Hz with 100-570 ms stalls and client was 11-17 FPS.

INFERENCE / UNCERTAINTY
- Because the issue is a planning backlog, absence of runtime behavior proof is not by itself a defect. The concrete defect is that the tasks are not actionable or verifiable: no owner, expected metric, risk, proof expectation, or pinned-run acceptance exists for any of them, so a cold agent cannot pick one up and prove it.
- The dead evidence path is plausibly retention or date-directory migration: devscripts/dev_loop/game_build.py _prune_logs deletes dated logs/ directories after 3 kept dates, and _looks_like_date only recognizes YYYY-MM-DD, while an empty legacy MM-DD-YYYY directory remains. This does not prove deletion of this specific run; the run may never have been written at the cited path. Either way the citation is currently unreproducible.
- The inherited gen-470 FCS data indicates the broader frame-budget problem persists, but it is not this issue 67-NPC workload and cannot be used as acceptance for any backlog item.

CONTRARY EVIDENCE
- The spec is complete and internally consistent, and the 5 tasks map to named spec candidates, so the issue is not missing scope. No observed evidence contradicts the backlog being open; there is also no observed evidence that any item is fixed.

NEXT DISCRIMINATING CHECK
1. Re-pin the actual 67-NPC scenario on the newest published generation and attach exact build ID, game generation, run ID, and journals; capture at least a 60 s steady window with performance.fps and performance.frame-budget. If the historical run cannot be recovered, correct or remove the dead evidence path rather than carrying it forward.
2. Fill proof_expectation and risk on the issue, and make the first task (culling-visibility) actionable: owner, expected metric delta, evidence gate, pinned-run check, and risk. Apply the same to the other four tasks before claiming the issue accepts.
3. Reuse the issue-549 instrumentation and issue-8bfc 67-NPC baseline to avoid a duplicate harness; report the first over-budget owner from that pinned run. A build or spec-only change is not proof.

Confidence: high that readiness is false, all tasks are unactionable/unproven, and the cited runtime path is absent; medium that retention/migration explains the missing path; low/unknown on current 67-NPC behavior for this specific backlog.
