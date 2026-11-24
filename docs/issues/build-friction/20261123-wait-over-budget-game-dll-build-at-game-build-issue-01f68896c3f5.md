# issue-01f68896c3f5: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Context

### 2026-11-23T20:50:19.982Z — build-wait-reviewer

related_build: game-dll-20261123T200924195125Z-7401c3

## Review (confirmed-true-positive)

### 2026-11-23T20:50:38.474Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Waited 61.659s at stage game_build (budget 60.0s) for wait_id game-dll-build:2026-11-23T20:09:24.135Z. TRUE POSITIVE, not wait-policy noise: the underlying game-DLL build genuinely ran 829.795s.

Verified in the dev-loop journal for run 20261123_192224:
- dev-loop.wait.over_budget at 2026-11-23T20:10:30.622Z: elapsed_seconds 61.659 vs budget_seconds 60.0, created=true, issue=issue-01f68896c3f5.
- dev-loop.wait.reviewer-wake at 20:10:30.622Z: same wait_id/issue, wake-fbc27e1ac98c, adapter codex, queued=true.
- agent-wake events.jsonl: exactly one wake.queued for wake-fbc27e1ac98c / issue-01f68896c3f5 at 20:10:27.734Z. No duplicate wake.

Acceptance check - issue carries wait_id, stage, reason, elapsed, build IDs, evidence:
- wait_id game-dll-build:2026-11-23T20:09:24.135Z; stage game_build; elapsed 61.659s; budget 60.0s.
- build_id 2026-11-23T19:22:15.804284Z; build_attempt_id game-dll-20261123T200924195125Z-7401c3; source_generation 9; generation 571; revision 3d7dedf8...; request 2026-11-23T20:09:24.034Z-087e06e3:mode3:game.
- No run ID applies (build wait, not a test run).
=> Exactly one reviewer wake for this wait occurrence. Criteria met.

Root cause / substantiation, .dev/build-timings/game-dll-20261123T200924195125Z-7401c3.json:
- total_seconds 829.795 (20:09:24.195Z -> 20:23:13.990Z); success=true; 642 TUs, 271 compiled / 372 skipped.
- compile phase 754.586s; compile_sum 7675.49s over jobs=10; changed-source preflight 46.408s.
- Changed paths: 7 C++ files + src/config.h (fanout 270). The config.h edit drove a large fanout recompile; slowest TUs audio.cpp 89.075s, server-gamemode.cpp 85.368s, map-editor.cpp 63.222s.
=> The 60s budget wait reflects a real ~13.8 min build; policy fired correctly.

Host pressure (direct observation, same attempt's resource sampler):
- 613 samples over 829.8s; mean host CPU 99.986%, peak 100.0%; min free RAM 651264 bytes; provider windows_kernel32_host_only.
- INFERENCE: severe host saturation coincided with the slow compile. CAUSAL per-process attribution is NOT proven: per-process CPU/RAM and tracked_process_metrics were unavailable.

Telemetry gap not duplicated: gap_key build-resource-sampling:game-dll-20261123T200924195125Z-7401c3 is already recorded in issue-136d4553ff92 (owner hot-reload-latency-root; native per-process sampler fallback in progress). No new issue created.
Minor data-quality observation (not filed separately): the wait `reason` string embeds a truncated JSON fragment (59499.014,"build_attempt_id":...) rather than human-readable text; it does carry attempt/generation/revision.

Per-occurrence dedup is working as designed: devscripts/build_friction._wait_issue dedupes only on the exact wait_id. This is a new wait_id, so opening one issue per distinct over-budget occurrence is intended, not a duplicate. Aggregate work lives in issue-136d4553ff92 (config.h fanout / build latency) and issue-7d3dcfd7433b / issue-e044a931c54b (measure/reduce waits).

Verdict: confirmed. No wait-policy code fix needed for this occurrence. Do not close as noisy. Linked to the build-latency owner; this is one more data point for the recurring config.h-fanout recompile wait.
