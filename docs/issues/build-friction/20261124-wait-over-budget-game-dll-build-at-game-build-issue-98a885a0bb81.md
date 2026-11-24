# issue-98a885a0bb81: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (reviewed-proof-complete-followups-routed)

### 2026-11-24T22:01:14.199Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Bounded build-wait review by build-wait-reviewer (wake-a61c8e5d2cf2, session-969642282120). Read-only: no source edit, no build/test requested, no fix or close claimed.

VERDICT: the wait-occurrence proof is complete; two follow-ups are routed (wait-evidence fields and late reviewer wake).

OBSERVED EVIDENCE (direct observation)
- Wait: wait_id game-dll-build:2026-11-24T20:55:04.680Z, kind game-dll-build, stage game_build, elapsed 61.186s, budget 60.0s, started 2026-11-24T20:55:04.680Z.
- Dev-loop journal (authoritative): logs/2026-11-24/20261124_205541_01/dev-loop/events.jsonl line 13 `dev-loop.wait.over_budget` at 2026-11-24T20:56:34.144Z; line 12 `dev-loop.wait.reviewer-wake` at 2026-11-24T20:56:34.129Z (issue issue-98a885a0bb81, wake wake-a61c8e5d2cf2). Exactly one of each exists for this wait_id (2 total matching events across logs/2026-11-24).
- Issue record: issue-98a885a0bb81 opened 2026-11-24T20:56:06.063Z by dev-loop-wait with wait_id, stage, reason, elapsed, request_ids, and budget.
- Wake record: .dev/agent-wake/requests/wake-a61c8e5d2cf2.json created 2026-11-24T20:56:07.426Z (one request; attempts=2 = primary codex + fallback opencode). .dev/agent-wake/events.jsonl shows one wake.queued at 20:56:07.432Z.
=> proof_expectation ("dev-loop wait event plus the issue record and reviewer wake record") is met 3/3 for this occurrence.

ACTUAL BUILD (why the wait exceeded budget; the wait is a symptom, not a false alarm)
- Attempt game-dll-20261124T205504793823Z-0c153b (generation 620, revision c75a32f3648a7054da36750dc0bbf92e23f59c945244d7e82ad2c290474b341b), per .dev/build-timings/game-dll-20261124T205504793823Z-0c153b.json: success=true, total_seconds 285.82, changed_source_preflight 202.55s driven by one translation unit (src/main-systems.cpp 202.42s), link 34.28s.
- Same attempt .resource.json: mean host CPU 99.977%, peak 100.0%, minimum available memory 32,190,464 B (~30.7 MiB), 144 samples over 285.8s.
- Same-TU contrast: attempt game-dll-20261124T201013477657Z-77e222 compiled in 8.35s with 4,234 MB free; this attempt took 202s with ~30.7 MiB free. This matches the 149.6s / 166 MiB gen618 single-TU pattern already reported by hot-reload-latency-root (thread-0e4585258fe7, msg-199a0d906a19). Inference (not yet proven): the 60s budget was crossed because the build genuinely ran 285s under host memory pressure; classification is resource-pressure / slow single-TU compile, not a wait-policy false positive.

EVIDENCE-QUALITY GAPS (routed, not fixed here)
1. Wait evidence omits run_id (20261124_205541_01), the game-DLL attempt id (game-dll-20261124T205504793823Z-0c153b), and the journal path. Reported build_id is the EXE artifact id (2026-11-24T19:27:02.742428Z); reported generation 618 is the last *published* generation, not the in-flight build generation 620, because dev-loop.py updates game_build_generation only on completion (devscripts/dev-loop.py:3259). The acceptance says "build/run IDs", but a reviewer cannot reconstruct this attempt from the issue record alone. Routed to issue-9a4503f6ed68 (owner of the wait-hook acceptance).
2. The reviewer wake was queued once but reached the reviewer late: created 20:56:07.426Z; codex claimed/launched 20:56:33-34Z; wake.retry 21:13:39.208Z; opencode claimed/launched 21:14:31Z (~18m24s). This is the same root cause tracked by issue-84c584957e65; occurrence appended there.
3. Board duplicate appearance: many open "Wait over budget: game-dll-build at game_build" issues share one title. devscripts/build_friction.py _wait_issue dedupes by unique wait_id (one issue per occurrence, by design), so this is expected volume rather than duplicate-record noise. Note for triage: do not merge distinct occurrences.

NO source edit, build, or runtime test was performed by this reviewer. Build success here is compile/link evidence only; no behavior claim.
