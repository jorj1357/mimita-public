# issue-1722de6ac598: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (reviewed-genuine-heavy-tu-build-with-evidence-gap)

### 2026-11-22T11:00:23.525Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed the over-budget wait against the exact build attempt journal.

WAIT: wait_id=game-dll-build:2026-11-22T10:23:18.609Z, kind=game-dll-build, stage=game_build, elapsed=60.844s, budget=60s.

VERDICT: genuine over-budget build, not a hang. The matching attempt is game-dll-20261122T102318690455Z-54c762 (generation 485, revision 04bceb66c8ff82d890f1d82f92f13da5269fc521b39443f7e9b7d54c94fde192). It completed exit_code=0 at 2026-11-22T10:24:53.922Z after 95.203s and published build/game/generation-485/game-g485.dll.

FIRST DIVERGENCE: At the 60.844s check the build was inside the changed_source_preflight compile of src/actor/player-ai.cpp (38.716s, 10:23:52.268Z -> 10:24:30.997Z). About 30s before that was staging: manifest_snapshot 6.094s, source_snapshot 11.297s, snapshot_prune 9.406s, module slot/worker launch 2.906s (first worker output 10:23:52.185Z). Remaining time: identity-provider compile 1.378s, object cache scan, link 15.750s, publish 0.468s. No stuck stage; the heavy single TU plus staging is why it crossed 60s.

CONTEXT: The build note for this attempt is from cpp-latency-linker-investigation: temporary comment-only one-translation-unit cache and link canary, restore source after capture, on src/actor/player-ai.cpp. A slow one-TU rebuild was the intended workload, so this wait is explainable rather than a defect in the wait detector.

ACCEPTANCE CHECK:
- wait_id/stage/reason/elapsed: present.
- evidence: present.
- reviewer wake queued exactly once: PASS. wake-9724c8cd441d was queued 2026-11-22T10:24:22.941Z; it is the only wake and only issue for this wait_id (no duplicate issue). Note the wake tried codex, failed, then retried opencode and was claimed at 2026-11-22T10:35:44Z, about 11 minutes after the wait - adapter latency, separate from this issue.
- build/run IDs: PARTIAL. evidence.build_id is 2026-11-22T08:00:31.376619Z (last successful build, generation 484), not the in-flight attempt game-dll-20261122T102318690455Z-54c762 (generation 485), and evidence has no run_id. Source: devscripts/dev-loop.py:3543 uses self.latest_build for build_id; the game-build wait dict at devscripts/dev-loop.py:3531-3547 never sets run_id. I set related_build/related_run on this issue to the correct attempt and dev-loop run 20261122_011031.

JOURNAL NOTE: I could not locate a retained dev-loop.wait.over_budget runtime event for this wait in a dev-loop events.jsonl; the authoritative trace used here is .dev/build-timings/game-dll-20261122T102318690455Z-54c762.jsonl. Confidence high on cause, medium on the missing runtime event.

DUPLICATE CHECK: issue-9236b8abc7d8 is a separate wait occurrence (started 2026-11-22T10:26:57.432Z), not a duplicate of this wait_id.

RECOMMENDATION: Record the active attempt id plus run_id in wait evidence instead of the latest successful build. I did not edit source; the suggested recorder fix is attached as a suggestion to issue-136d4553ff92 and needs owner/human approval.

## Context

### 2026-11-22T11:00:54.281Z — build-wait-reviewer

related_build: game-dll-20261122T102318690455Z-54c762
related_run: 20261122_011031
