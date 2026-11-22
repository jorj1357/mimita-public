# issue-ce4c245a8a70: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed)

### 2026-11-22T04:43:36.990Z — build-wait-reviewer

Reviewer: build-wait-reviewer

REVIEW (build-wait-reviewer) — confirmed over-budget wait, policy noise confirmed, reason-field defect found.

VERIFIED THIS OCCURRENCE (journal logs/2026-11-22/20261122_011031/dev-loop/events.jsonl):
- dev-loop.wait.over_budget event_id DEV_RUNTIME_8af536cf09514692b65918e20d11a0a6 at 2026-11-22T04:25:52.749Z; wait_id game-dll-build:2026-11-22T04:24:51.316Z; kind game-dll-build; stage game_build; elapsed 60.214s; budget 60.0s; created=true -> issue-ce4c245a8a70.
- dev-loop.wait.reviewer-wake event_id DEV_RUNTIME_de5b274cf9374041801c405295707db4 queued=true (wake-d8333c8c5780). Acceptance 'one visible issue + one reviewer wake' is met for queueing.

FIRST DIVERGENCE — budget is below normal build cost, so every build files an issue:
- Generation 455 build record (.dev state current.json): total_seconds 67.796 = changed_source_preflight 28.173 + compile 29.217 + link 14.824 + object_scan 4.079. Changed sources: only src/main-systems.cpp.
- devscripts/dev-loop.py:109 WAIT_ISSUE_SECONDS defaults to 60s. A legitimate single-file incremental build therefore exceeds the budget by construction, producing one 'Wait over budget: game-dll-build at game_build' issue per build (dozens already exist, many in_review). This matches the issue's own risk note ('noisy wait policy could create duplicate friction issues').

SECOND FINDING — 'reason' is not a human-readable reason:
- For this wait the recorded reason is a 240-char tail of a GAME_STAGE JSON line: ':57590.996,build_attempt_id:game-dll-20261122T042451390437Z-634baf,...'.
- Cause: dev-loop.py:2830 sets detail = line.rstrip()[-240:] and :2860 assigns game_build_detail = detail; check_wait_budget (dev-loop.py:3528) uses game_build_detail as the wait reason. When the last output line is a [GAME_STAGE] JSON record, the reason is a truncated JSON fragment. Other occurrences show the same (partial JSON or a '[GAME BUILD] revision=...' header). Acceptance requires the issue to carry 'reason'; it carries a machine fragment.

THIRD FINDING — one queued wake launched twice:
- .dev/agent-wake/events.jsonl for wake-d8333c8c5780: wake.queued once (04:25:52.201Z), then wake.launched codex pid 26064 (04:25:52.726Z), wake.retry adapter=opencode (04:30:11.230Z), wake.claimed/launch opencode pid 10364 (04:37:04Z). Queue-once holds, but the same wake started two reviewer sessions (codex then opencode). This duplicates reviewer work for a single wait occurrence.

RECOMMENDATIONS (no code edited or claimed here; needs a separate owner):
1. Make the budget stage-aware or raise it above measured normal incremental cost (>=90-120s for game-dll-build) so routine single-file builds do not trip it; keep 60s only for genuinely stalled stages.
2. Derive a readable reason from the parsed event payload (stage + state + elapsed_ms) instead of the raw last-240-chars tail.
3. Ensure a retried wake reuses the same session/does not start a second concurrent reviewer for the same wake_id.

Confidence: high for all three findings (direct journal + state + code references). No runtime behavior proof claimed; this is process/evidence review only.

## Review (confirmed)

### 2026-11-22T04:46:17.866Z — build-wait-reviewer

Reviewer: build-wait-reviewer

SUPPLEMENT — exact pinned attempt data (game-dll-20261122T042451390437Z-634baf, .dev/build-timings/...634baf.json):
- success=true, no_op=false, generation 454, source_generation 5, started 2026-11-22T04:24:51.390Z, completed 04:26:12.809Z, total_seconds 81.407, worker_total_seconds 64.128.
- changed_paths: src/gui/ui-system.cpp, ui-system-buttons.cpp, ui-text-input.cpp, ui-system-internal.h (header change -> 26 TUs compiled, 617 skipped, 639 dependency checks).
- phase_durations_ms: changed_source_preflight 10785.963, object_cache_scan 2184.191, compile 40555.912 (max single TU 17313.198), link 9589.146, coherence_validation 301.007, publish 550.858.
- The trip at 04:25:52.749Z (60.214s) occurred during the compile tail (last file event online-menu.cpp 04:25:53.649Z), about 20s before completion. Not wedged; forward progress throughout. This reinforces the miscalibration finding rather than a stuck build. The truncated reason ':57590.996' is a compile-stage GAME_STAGE elapsed_ms tail, confirming the reason-attribution defect.
- Correcting scope of the general argument: generation 455 (later, src/main-systems.cpp only) was also 67.796s total, so both header-fanout and single-file builds exceed the flat 60s budget.
