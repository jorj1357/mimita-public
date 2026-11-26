# issue-ab0d4daf980a: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Context

### 2026-11-26T15:52:51.683Z — build-wait-reviewer

related_build: game-dll-20261126T154118055873Z-1d5285
related_run: 20261126_145549

## Review (confirmed-real-wait-wake-once;record-defects-reason-stale-build-id-generation-no-run-id;underlying-build-failed-compile-resolved)

### 2026-11-26T15:53:23.835Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Verified against the pinned run journal logs/2026-11-26/20261126_145549/dev-loop/events.jsonl (run 20261126_145549).

Wait is REAL, not a false positive. At the 60.613s crossing (15:42:18.5Z) the worker had just completed [GAME_FILE] src/network/community-match-client.cpp (elapsed_ms 57547.0, wall 15:42:15.831Z), with main-systems / vehicle-render / map-editor still compiling. 60.613s is below this attempt's natural wall time.

Waited-on attempt: game-dll-20261126T154118055873Z-1d5285, generation 690, revision 3c43e2ec..., source_generation 12, changed path src/vehicles/vehicle-profile.cpp (dev-loop.game-build.started). It then FAILED at compile at 15:42:31.439Z (total 74.172s). First divergence: src/vehicles/vehicle-system.cpp:1322 cannot convert std::shared_ptr<const VehicleProfile> to const VehicleProfile* in the lambda calling profileBindingHeld(profile, logical) (dev-loop.game-build.failed). So the real friction was a genuine long build that ended in a compile error; the wait policy correctly surfaced it. The edit owner retry game-dll-20261126T154301815009Z-41b473 (rev 8029c2c7, generation 691) published 15:44:10Z, so the compile break is resolved.

Policy acceptance met: exactly one dev-loop.wait.over_budget and exactly one dev-loop.wait.reviewer-wake for this wait_id; issue created 15:42:18.809Z, wake queued (adapter codex, pid 13016). No duplicate issue.

Record-integrity defects (acceptance NOT fully met):
- evidence.reason is a mid-JSON tail starting ':57547.0,' and ending with progress_write_ms 6.898 / record_overhead_ms 7.611 - the last 240 chars of the community-match-client.cpp [GAME_FILE] row, not a readable stage summary. Source: dev-loop.py:3112 _record_game_build_output detail=line.rstrip()[-240:] then :3142 game_build_detail then :3892 reason. Already tracked in issue-6e08d89b55e3; recurrence appended there.
- evidence.build_id=2026-11-26T06:10:41.962780Z is the EXE artifact id (self.latest_build, dev-loop.py:1404), not this game-DLL attempt. evidence.generation=688 is self.game_build_generation, only updated on a successful publish (dev-loop.py:3259), so it is the prior success, not the in-flight generation 690. Only source_generation=12 matches.
- The wait record carried no run_id; related_run was empty. I set related_build/related_run on this issue to the actual attempt/run.

Sibling: a second 60.025s wait at 15:43:01.573Z on the gen-691 retry (issue-28feee119403) is a separate occurrence of the same edit-to-verify cycle, not a duplicate.

Confidence: high (journal events cited; defect source read directly). No source edited; no fix claimed.
