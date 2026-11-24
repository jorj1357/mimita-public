# issue-4fe0ec224d19: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-genuine-wait-single-queue)

### 2026-11-24T06:50:49.697Z — build-wait-reviewer

Reviewer: build-wait-reviewer

CLASSIFICATION: Genuine over-budget game-DLL build wait. Not a false positive and not a duplicate.

ISSUE COMPLETENESS (acceptance part 1): PASS.
issue readiness => ready=true, missing=[]. Evidence carries wait_id (game-dll-build:2026-11-24T06:22:29.727Z), stage (game_build), reason, elapsed_seconds (61.095), budget_seconds (60.0), build_id (2026-11-24T06:22:17.620103Z), request_ids (2026-11-24T06:21:07.582Z-fce07e1f:mode3:game), and started_utc (2026-11-24T06:22:29.727Z).

WAIT IS REAL, NOT A STALLED DETECTOR (direct evidence):
- Pinned build attempt .dev/build-timings/game-dll-20261124T062229815697Z-8af3a7.json: total 411.16s, compile 384.144s, 170 compiled / 475 skipped, compile_max 70.587s (src/network/server-gamemode.cpp), changed_source_preflight 32.679s, link 8.0s. The 61.095s wait at stage game_build is a real mid-build window, not an idle queue.
- Cause of length is a mixed header-fanout build. Build note 09bf1381964e (vehicle-arena-controller, Vehicle arena phase A) changed src/network/schema/packet-vehicle.h (fanout 160) and src/network/server.h (fanout 61); 31 cache_invalidations cite those two headers.
- Resource summary .resource.json for the same attempt: mean_host_cpu_percent 99.779, peak 100.0, duration 411.154s => host was CPU-saturated during compile, consistent with build pressure, not a detector defect.

REVIEWER WAKE (acceptance part 2): PASS for queued exactly once.
- .dev/agent-wake/events.jsonl has exactly one wake.queued for wake-8818e3e6900b (issue issue-4fe0ec224d19) at 2026-11-24T06:23:32.425Z.
- Observation (not a failure): the first launch at 06:23:34.656Z used adapter codex, was recovered as stale at 06:32:27.705Z (wake.retry), and relaunched via opencode at 06:32:29.960Z (this activation). One queue, two launches; adapter-staleness is a separate reliability follow-up, not a duplicate queue.

DUPLICATE CHECK: PASS. Exactly one build-friction issue exists for this wait_id; occurrences list is empty; build_friction._wait_issue would append rather than create.

DATA-QUALITY NOTE (acceptance reason is present but unusable): evidence.reason is the last 240 chars of a GAME_* JSON line, so it begins mid-number (9956.409,{...}) and is not parseable. Source: dev-loop.py _record_game_build_output truncates detail = line.rstrip()[-240:] (dev-loop.py:3112) and check_wait_budget stores reason = game_build_detail (dev-loop.py:3892). This does not invalidate the wait, but the reason is not human-actionable.

UNDERLYING SLOWNESS is already owned elsewhere: this is the same edit-to-live C++ latency target tracked by issue-136d4553ff92 (target <=10s). No per-wait code change is warranted for this occurrence; the wait is a symptom, not a new defect. Several sibling game_build wait issues from 2026-11-23/24 already exist, so do not create more.

NEXT STEP: leave open as corroborating evidence for the latency effort; no source edit here (reviewer is not authorized to edit). Confidence: high (pinned build artifact + single wake.queued + unique wait_id).

## Agent suggestion

### 2026-11-24T06:51:08.671Z — build-wait-reviewer

Data-quality / small-fix suggestion for this wait record (not a duplicate issue): the evidence.reason field is unusable because it stores the last 240 chars of a GAME_* JSON line instead of an actionable reason. Observed here as reason = '9956.409,{...build_attempt_id...}' beginning mid-number.

Cause: dev-loop.py _record_game_build_output sets game_build_detail = line.rstrip()[-240:] (dev-loop.py:3112), and check_wait_budget copies it into wait.reason (dev-loop.py:3892). For GAME_STAGE/GAME_FILE JSON lines this truncates from the front and drops the stage/phase prefix the reviewer needs.

Smallest safe fix (owner to claim, not done here): in _record_game_build_output, when a [GAME_*] prefix matched, set game_build_detail to the parsed event_name plus a short stable summary (e.g. name + stage/path) rather than the raw tail; keep the raw line only for the runtime event fields. Metric target: 100% of wait issues for game-dll-build carry a reason that parses or contains a named stage/path; verify on the next over-budget game_build wait. Paths: devscripts/dev-loop.py. This is independent of the genuine-slowness finding (owned by issue-136d4553ff92). Leave unowned for review.
