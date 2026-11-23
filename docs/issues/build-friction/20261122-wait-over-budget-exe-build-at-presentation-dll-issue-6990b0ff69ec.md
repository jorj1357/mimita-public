# issue-6990b0ff69ec: Wait over budget: exe-build at presentation_dll

## Human report (verbatim, immutable)

> exe-build wait at presentation_dll exceeded the wait budget


## Review (confirmed-genuine-failing-exe-build)

### 2026-11-22T19:03:07.084Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Bounded independent review by build-wait-reviewer (wake-d53716cfb2d1), 2026-11-22. Read-only: no source edit, no build/test requested, no fix or closure claimed. No compiler, build.py, worker, or second dev-loop invoked.

CONCLUSION
The wait record is genuine and correctly captured. The 61.454s sample was not a phantom or duplicate: a real EXE build had started 61.454s earlier and was still in its (optional) presentation_dll phase. That same EXE attempt later FAILED at 495s, so this is not a no-op stall. The wait tracker behaved as designed (one issue, one reviewer wake). The avoidable cost inside the sample is the optional presentation-DLL child (92.5s, exit 1, fail-open) still tracked by issue-ae248907f1f1 / issue-e044a931c54b. No code fix is claimed here.

VERIFIED WAIT OCCURRENCE
- wait_id exe-build:4:2026-11-22T17:23:49.758Z; kind exe-build; stage presentation_dll; reason "[BUILD EXCLUSIVITY] acquired the single EXE build"; elapsed 61.454s vs 60.0s budget; started_utc 17:23:49.758Z; generation 4; source_generation 4; build_id 2026-11-22T08:00:31.376619Z; request_ids [].
- Journal logs/2026-11-22/20261122_144609_01/dev-loop/events.jsonl: exactly one dev-loop.wait.over_budget for this wait_id (created=true, wall_time 17:25:00.973Z) and exactly one dev-loop.wait.reviewer-wake (wall_time 17:25:00.971Z). issue occurrences=[] before this review.
- The wait fired 0.279s after the prior game-DLL attempt failed (dev-loop.game-build.failed 17:23:49.479Z); an automatic EXE build then began and the wait sampled during it.

PINNED ATTEMPT EVIDENCE
- The underlying EXE attempt is dev-loop.build.failed wall_time 17:32:12.249Z: build 2026-11-22T08:00:31.376619Z (matches evidence build_id), source_generation 4, duration_seconds 494.796, error "build failed with exit code 1", worker setup 5.133s, build_script 488.242s. So the 61.454s sample was inside a real, long, ultimately-failing EXE build, not a lost/ghost wait.
- .dev/presentation-dll-attempt.json: outcome failed, elapsed_s 92.516, exit_code 1, error "build_game_dll.py exited with code 1", failed_at_epoch ~17:25:38Z. The 61.454s sample elapsed while this optional child was running. build.py:652-812 makes the child fail-open (logs failure, caches it 30 min, continues), so the child did not itself fail the EXE; it did add ~92.5s. The actual EXE exit-1 cause is not in the retained journals (build-output.log was overwritten by the 17:38:44 successful build); classify as UNKNOWN, and as an inference only, the immediately following EXE build re-linked successfully (build-metrics 17:38:44Z, link_reason pending_or_failed_previous_link, link 38.315s).

ATTRIBUTION
- Unlike sibling reviews, evidence build_id here matches the failing attempt's id in dev-loop.build.failed, so it is not the "previous published artifact" defect.
- reason is the last build output line ("[BUILD EXCLUSIVITY] acquired the single EXE build"), which positively proves a build was in progress but is not a stage label. stage presentation_dll + elapsed is whole-EXE-build elapsed, not time-in-phase; this remains the known stage-accounting gap (issue-136d4553ff92).
- request_ids [] is consistent with an automatic build (no explicit request).

WAKE / DISPATCH
- wake-d53716cfb2d1: queued 17:24:55.483Z; codex claimed/launched 17:24:59.887Z/17:25:00.143Z then failed; wake.retry switched to opencode 18:02:46.039Z; opencode claimed/launched 18:02:51.130Z/18:02:51.176Z. Total dispatch delay ~37m55.7s, far beyond the proposed <=5 min bound. Logged as an occurrence on issue-84c584957e65.

ACCEPTANCE
- readiness ready=true, missing=[]; wait_id, stage, reason, elapsed, build/run IDs, and evidence present; exactly one issue and one reviewer wake for this occurrence. Wait mechanics pass. The wait itself is not the defect; the 495s failing EXE build and its 92.5s optional-DLL component are.

RECOMMENDATION (not claimed)
- Route the presentation-DLL component to issue-ae248907f1f1 (failing optional presentation DLL hidden inside EXE work) and the systemic wait/measurement owners issue-e044a931c54b and issue-136d4553ff92. Do not lower/raise the 60s budget here; a per-class budget needs human approval.
- Add a retained, per-attempt EXE build log (or persist the failed attempt's output) so a failed EXE build's first divergence is attributable; the 17:23 attempt's failure reason is currently unrecoverable.

Confidence: high on wait mechanics, the 17:23->17:32 failing EXE attempt, and the 92.5s optional-child cost (direct journal + state file); medium on the EXE exit-1 cause (build output overwritten). No source, build, or runtime behavior changed by this review.
