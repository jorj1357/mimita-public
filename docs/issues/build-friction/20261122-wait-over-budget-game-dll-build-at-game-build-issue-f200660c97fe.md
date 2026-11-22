# issue-f200660c97fe: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-with-evidence-gaps)

### 2026-11-22T13:51:52.843Z — build-wait-reviewer

Reviewer: build-wait-reviewer

test

## Review (confirmed-with-evidence-gaps)

### 2026-11-22T13:52:52.835Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewer: build-wait-reviewer

Independent review of issue-f200660c97fe (wake-637b83a47c6a). This body supersedes an accidental pre-review comment whose body was only test; that comment contained no findings.

VERDICT: confirmed wait, real build friction; issue evidence has gaps. No code edited, no files claimed, no duplicate issue created.

WAIT AND BUILD CORRELATION (proven from build timings)
- wait_id: game-dll-build:2026-11-22T13:00:15.598Z; stage game_build; elapsed 60.735s; budget 60.0s; request_ids auto-1 and 2026-11-22T13:00:31.139Z-6b8ed743.
- Matching attempt: game-dll-20261122T130015674891Z-687e4d, started 13:00:15.674Z, completed 13:01:17.636Z, success=true, generation 492, total_seconds 61.943, changed path src/effects/zombie-tower-effects.cpp.
- At the 60.735s wait mark (~13:01:16.333Z) the attempt was in final coherence_validation/publish, not idle or stalled; it completed ~1.3s later.
- Phase breakdown (ms): changed_source_preflight 24891.038 (includes the single-TU compile), link 9024.737, object_cache_scan 2856.286, compile 1665.600, coherence_validation 1476.106, publish 607.208, toolchain_setup 408.431, vendor_compile 33.336.
- Slowest TU: src/effects/zombie-tower-effects.cpp = 24.540s. Link input = 480,395,963 bytes / 641 objects.
- Recurrence: prior attempt game-dll-20261122T125133309168Z-46b1e5 took 68.864s. A normal one-TU incremental whole-game DLL build is therefore routinely above the fixed 60s WAIT_ISSUE_SECONDS budget (devscripts/dev-loop.py:109-110), so these waits are a policy/threshold class, not a one-off reporter glitch.

WAIT POLICY BEHAVIOR (proven)
- One issue per exact wait_id: devscripts/build_friction.py:131-169 dedups by evidence.wait_id. rg over docs/issues finds this wait_id only in the prior review of issue-bbaee7b0d2ee; no second issue exists for it.
- One reviewer wake queued for this occurrence: .dev/agent-wake/events.jsonl line 1532 has exactly one wake.queued for wake-637b83a47c6a at 13:01:19.515Z (adapter codex), then fallback/retry claims at 13:08:02 and 13:35:18 for the same wake id. Retries reuse the same queued wake and do not create a duplicate issue.

EVIDENCE GAPS / DEFECTS (reported, not fixed)
1. reason is malformed. Stored reason begins 438.765,build_attempt_id:..., a raw truncated worker-output tail instead of a readable stage reason. Owner: dev-loop.py:2833 sets game_build_detail = line.rstrip()[-240:]; dev-loop.py:3539 uses it as reason.
2. generation is stale. Evidence says 491, but the matching attempt built generation 492. dev-loop.py:2976 assigns game_build_generation only after result parsing, while dev-loop.py:3544 reads it during the build.
3. build_id is not the in-progress attempt. Evidence records self.latest_build = 2026-11-22T08:00:31.376619Z (prior published build), not build_attempt_id game-dll-20261122T130015674891Z-687e4d. The attempt id is only buried inside the malformed reason fragment.
4. no run_id in the wait evidence (actual runtime run 20261122_011031). Acceptance asks for build/run IDs.
5. Dev-loop wait event is missing. State points events_path at logs/2026-11-22/20261122_011031/dev-loop/events.jsonl, but that directory does not exist (run dir contains only server-01, diagnostics, CSVs, run.json). record_runtime_event swallows OSError at dev-loop.py:2295-2300, so the proof-expectation wait event cannot be read from the journal; only the issue record and agent-wake record exist.

INTERPRETATION AND NEXT
- The friction is real and rooted in whole-game build cost for a heavy single-TU edit plus source preflight, snapshot, and link. This overlaps the build-coordinator work in issue-136d4553ff92. Any budget-policy change is a human decision.
- Recommended follow-ups (report only): make the wait reason stable and human-readable; record the in-progress build_attempt_id, generation, and run_id; ensure the dev-loop events directory exists or surface the write failure; consider a stage- or kind-specific budget for whole-game DLL builds.
- Files to touch if authorized: devscripts/dev-loop.py (game_build_detail/reason, wait evidence fields, runtime journal parent), devscripts/build_friction.py (evidence shape). src/effects/zombie-tower-effects.cpp is the build-cost owner but needs no change for the reporter.

CONFIDENCE: high on the 61.943s attempt and the single-issue/single-wake behavior; high on defects 1-4 from source and stored evidence; high on defect 5 (events_path is configured but logs/2026-11-22/20261122_011031/dev-loop does not exist).

## Context

### 2026-11-22T13:53:53.623Z — build-wait-reviewer

related_build: game-dll-20261122T130015674891Z-687e4d
related_run: 20261122_011031
