# issue-ecdc697b2415: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-build-friction-recurrence; no-fix-claimed; evidence-complete)

### 2026-11-24T01:05:55.121Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewer: build-wait-reviewer - bounded wake wake-c4a5d204a3c9, coordination session session-084a743f908d / work-dfdaa6014c85. Read-only: no source edit, no build request, no compiler, no second dev-loop.

Completeness: issue readiness ready=true, missing=[], score=8. Required fields (wait_id, stage, reason, elapsed, build_id, started_utc, evidence) are present; no clarification needed.

Reproduction confirmed from the dev-loop journal - a legitimate long build, not a stalled queue:
- Wait event: logs/2026-11-24/20261124_001546/dev-loop/events.jsonl, event dev-loop.wait.over_budget at 2026-11-24T00:40:06.119Z, run_id 20261124_001546, wait_id game-dll-build:2026-11-24T00:39:01.308Z, elapsed_seconds 60.787 vs budget_seconds 60.0, created=true -> issue-ecdc697b2415.
- Reviewer wake queued exactly once: dev-loop.wait.reviewer-wake at 2026-11-24T00:40:06.118Z (wake-c4a5d204a3c9, adapter codex, queued=true). Matches the acceptance of one reviewer wake for this occurrence.
- Underlying build: attempt game-dll-20261124T003901444604Z-3c8d4e, request 2026-11-24T00:39:00.895Z-27738571:mode3:game (Mode 3 batch), started 2026-11-24T00:39:01.444Z, completed 2026-11-24T00:54:50.688Z, success=true, generation 576, source_generation 40, revision e33b0a4a..., total_seconds 949.243. Evidence: .dev/build-timings/game-dll-20261124T003901444604Z-3c8d4e.json/.jsonl plus per-file dev-loop.game-build.file events showing continuous TU progress.

Dominant measured cost (this was a large mixed header+source batch, not a hang):
- compile phase 814.6s recompiling 305 TUs (compile_seconds 923.1, jobs 10), preceded by changed_source_preflight 108.6s.
- Trigger: 21 changed .cpp + 13 changed headers with high fan-out - src/physics/movement/movement-types.h 270, src/entities/player.h 233, src/input/input-frame.h 175, src/network/schema/packet-combat.h / packet-common.h 160, src/network/movement-validation.h 151, src/input/input-state.h 146.
- Slowest TUs: src/network/server-gamemode.cpp 100.7s, src/audio/audio.cpp 99.9s, src/mapeditor/map-editor.cpp 73.9s, src/avatar/avatar.cpp 73.9s.

Resource observation (labelled): the resource sidecar captured mean host CPU 99.995% (peak 100.0%) across 670 samples, 10 jobs. minimum_available_memory_bytes=229376 (~224 KB). The CPU saturation is a direct observation; the 224 KB free-memory figure is implausibly low as true free RAM and is treated as unknown/low-confidence pending validation. runtime_alignment.status=unavailable (no matched prebuild baseline, no journal-aligned live workload), so per-worker attribution during this build is not available here. Cross-link: CPU/build-resource ownership sits under issue-136d4553ff92 and the live gen576 resource work.

Recurrence / duplicate policy: this is the same failure class as issue-84ec2bf27598 (2026-11-20, with prior occurrences issue-2008e743c710 and issue-63dccbd8f8b6). devscripts/build_friction.py record_wait_incident() keys uniqueness on the exact wait_id (_wait_issue), so every new over-budget wait opens a new issue instead of appending an occurrence. That contradicts the reviewer-prompt instruction to add the occurrence instead of creating a duplicate issue. This is a reporter/dedupe policy gap, not a defect in this occurrence. No duplicate issue was created by this review.

Actionable recommendation (no fix claimed here):
1. In the wait reporter, distinguish an actively progressing long build from no progress and include current phase / percent / compiled-TU count in the wake payload; keep the 60s budget. Already recommended on issue-84ec2bf27598; still open.
2. Reduce header fan-out and the 108.6s changed_source_preflight for movement-types.h / player.h / input-frame.h; track under issue-136d4553ff92 (build latency).
3. Decide whether recurring per-wait issues should roll up into one umbrella build-friction issue; current behavior produced this issue in addition to issue-84ec2bf27598 for the same condition.

Verdict: valid build-friction occurrence for a legitimate long Mode 3 batch build. The wait is real, the record is complete, the reviewer wake fired once, and the build succeeded. No code fix or behavior proof claimed here. Cross-links: issue-136d4553ff92 (build latency), issue-84ec2bf27598 (prior same-class occurrence).
