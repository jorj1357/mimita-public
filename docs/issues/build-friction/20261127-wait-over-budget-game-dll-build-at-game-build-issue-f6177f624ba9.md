# issue-f6177f624ba9: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Context

### 2026-11-27T02:26:20.126Z — build-wait-reviewer

related_build: game-dll-20261127T014456458183Z-893a11
related_run: 20261127_014331

## Review (confirmed-intrinsic-single-TU-build-cost; identity-defect-recurred; wake-queued-once-dispatch-late)

### 2026-11-27T02:26:54.787Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Occurrence confirmed. Wait game-dll-build:2026-11-27T01:44:56.218Z (stage game_build) fired at elapsed 62.339s / 60.0s budget (dev-loop.wait.over_budget wall 2026-11-27T01:46:16.190Z, journal logs/2026-11-27/20261127_014331/dev-loop/events.jsonl line 63; reviewer-wake line 62). Exactly one issue exists for this wait_id and exactly one wake.queued (wake-08578d19ed29 at 2026-11-27T01:46:04.240Z, .dev/agent-wake/events.jsonl line 3730), so the per-wait duplicate contract holds.

Waited-on build: in-flight attempt game-dll-20261127T014456458183Z-893a11 (dev-loop.game-build.started journal line 58, started 01:44:56.458Z, request 2026-11-27T01:44:56.049Z-3f8f9b01, source_generation 0). It succeeded as generation 751 (dev-loop.game-generation.ready journal line 68, ready 01:47:42.710Z). Not a stall. .dev/build-timings/game-dll-20261127T014456458183Z-893a11.json: total_seconds 166.0; changed_source_preflight 57.703s (src/vehicles/vehicle-system.cpp 57.564s); link 80.297s over 645 inputs / 502,153,396 B of objects; compiled 2, skipped 644. Intrinsic single-TU compile plus a long link, not a wait-policy bug. Resource context (.resource.json): host mean CPU 98.902% / peak 100%, minimum available memory 350,633,984 B (~334 MiB) over 98 samples. This is direct observation; do not infer that resource pressure caused the long link.

Acceptance: wait_id, stage, reason, elapsed, and evidence are present and reason is clean ([GAME BUILD] revision=e2d0bcf4... sources=645 jobs=10 priority=high ...), so the reason-truncation defect (issue-6e08d89b55e3) did NOT recur. One divergence: "build/run IDs" is only partially met. evidence.generation=750 and evidence.build_id=2026-11-27T01:43:27.658897Z are the previously published generation and the last EXE artifact, not the in-flight attempt; no run_id or build_attempt_id is stored. Root: check_wait_budget reads self.latest_build and self.game_build_generation (dev-loop.py:3896-3897); game_build_generation is assigned only on completion (dev-loop.py:3259). Known defect tracked on issue-9a4503f6ed68. Confidence high (journal + code + build-timings agree).

Secondary divergences, already owned elsewhere, not this issue's acceptance: (1) late dispatch - codex claimed/launched 01:46:15.692/15.766Z then exited, wake.retry to opencode 01:55:03.147Z, opencode claimed/launched 02:17:51.271/51.322Z; queue-to-final-launch 1907.08s (~31m47s), attempts=2 (issue-84c584957e65). (2) wake-record misattribution - the dev-loop.wait.reviewer-wake event for this wait embeds wake-c6fb7ac32356 (issue-dea21a9c722d / wait game-dll-build:2026-11-27T01:33:03.139Z), not wake-08578d19ed29, because build_friction.wake_reviewer runs `agent-wake.py run --once`, which drains the oldest pending wake and returns that result (issue-84c584957e65).

No new issue filed, no duplicate, no source fix claimed. related_build/related_run set to the true in-flight attempt/run. Occurrences to be appended to issue-9a4503f6ed68 (identity gap) and issue-84c584957e65 (late dispatch + misattribution). Reviewer: build-wait-reviewer.
