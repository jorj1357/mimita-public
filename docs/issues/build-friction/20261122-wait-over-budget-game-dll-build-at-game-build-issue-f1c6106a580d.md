# issue-f1c6106a580d: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-intrinsic-build-cost)

### 2026-11-22T13:19:39.168Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Independent review of over-budget wait occurrence `game-dll-build:2026-11-22T12:12:28.366Z`.

FINDING: CONFIRMED — legitimate intrinsic build cost; the wait hook worked as designed. No wait-policy code change is required for this occurrence.

Corresponding build: `.dev/build-timings/game-dll-20261122T121228440102Z-3aba91.json` (generation 490)
- build.started 2026-11-22T12:12:28.440Z; build.completed 2026-11-22T12:13:49.062Z; elapsed_ms=80609; exit_code=0; success=true.
- Only changed source: `src/effects/zombie-tower-effects.cpp` (build note fe3166518ec1: "Add a harmless source comment to measure one-translation-unit compile/link/publish after digest-cache optimization").
- Dominant phases: changed_source_preflight 30.515 s (single-TU compile 30.209 s), link 10.998 s, object_cache_scan 3.010 s, publish 0.737 s.
- The build genuinely exceeded the 60 s budget (reported elapsed 61.008 s) and completed successfully; it was not a stalled or wedged build.

Issue record: complete at review time (readiness ready 8/8, missing=[]); carries wait_id, stage=game_build, reason, elapsed_seconds=61.008, generation=490/build id, and evidence.

Reviewer wake (acceptance "queued exactly once"): SATISFIED. Exactly one `wake.queued` event for `wake-26c4ad5a6f09` / issue-f1c6106a580d at 2026-11-22T12:13:32.440Z (`.dev/agent-wake/events.jsonl`).

EVIDENCE GAPS / FIRST DIVERGENCE:
1. Runtime wait event is NOT journaled. The active run 20261122_011031 has no `dev-loop/` directory and no `dev-loop.wait.over_budget` event; `logs/2026-11-22/20261122_011031/` contains only run.json, server-01/, diagnostics/, and host-resource CSVs. The wait event exists only in the brain issue and wake queue. This is the existing defect tracked by issue-6e2d7b91e7d9 / issue-0498cbef4ef3 ("Root dev-loop drops runtime journal events: wait and watcher evidence never written"). The declared proof_expectation ("Dev-loop wait event plus the issue record and reviewer wake record") is therefore only partially satisfied: issue record + wake record present, dev-loop wait event absent.
2. Reviewer wake dispatch took ~39 min. wake.queued 12:13:32.440Z → primary adapter `codex` launched 12:13:34.154Z → codex exited ~12:14:52Z with model_not_found ("The 'gpt-6-luna' model is not supported when using Codex with a ChatGPT account") → wake.retry 12:34:35.056Z switched to fallback `opencode` → opencode claim/launch only at 12:52:39.040Z. The retried request was drained by the NEXT over-budget wait's `run --once` (build_friction.wake_reviewer), because no persistent agent-wake daemon is running. The wait hook itself is correct; the delay is a wake-adapter/dispatch defect, filed separately as a follow-up issue.

CONCLUSION: This over-budget wait is a true positive caused by a ~30 s single-TU compile plus ~11 s link/publish for a deliberately edited latency-test TU; the 60 s budget fired correctly. Recommend leaving the wait policy unchanged for this occurrence and not treating it as a regression. Recurrence of the same wait_id would be appended as an occurrence, not duplicated.

## Agent suggestion

### 2026-11-22T13:21:22.396Z — build-wait-reviewer

Wake-dispatch defect split out as issue-84c584957e65: the codex primary wake adapter fails with model_not_found (gpt-6-luna unsupported on ChatGPT account) and no wake daemon drains the opencode fallback, so this wait review started ~39 min after the issue. The wait itself was confirmed as a legitimate intrinsic build cost.

## Agent suggestion

### 2026-11-22T13:24:26.364Z — build-wait-reviewer

Follow-up split out as issue-84c584957e65: codex primary wake adapter fails model_not_found (gpt-6-luna unsupported on ChatGPT account) and no wake daemon drains the opencode fallback, delaying this review ~39 min. The wait itself was confirmed as legitimate intrinsic build cost.
