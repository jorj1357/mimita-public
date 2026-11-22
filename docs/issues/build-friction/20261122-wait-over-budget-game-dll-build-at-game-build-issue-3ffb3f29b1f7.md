# issue-3ffb3f29b1f7: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-intrinsic-build-cost; journal-gap+budget-policy-followup)

### 2026-11-22T14:44:37.914Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Independent review by build-wait-reviewer (wake-21cd23758a37). Read-only: no source edit, no build/worker/dev-loop invoked, no files leased, no duplicate issue created.

VERDICT: CONFIRMED true positive of the wait hook, but the wait is legitimate intrinsic build cost, not a stall. The fixed 60s budget is below the normal cost of an incremental whole-game DLL rebuild.

REVIEWED OCCURRENCE
- wait_id game-dll-build:2026-11-22T13:44:38.883Z, stage game_build, elapsed 60.383s, budget 60.0s.
- Corresponding attempt: .dev/build-timings/game-dll-20261122T134438974905Z-ada12d.json (started 13:44:38.974Z, matches).
- success=true, status=ok, generation 495, no_op=false, total_seconds=94.681, worker_total_seconds=66.953. DLL build/game/generation-495/game-g495.dll.
- Changed source: src/network/multiplayer-tick.cpp (note 7c553ed781b2, agent codex-networking-stall-fix). That one TU compiled in 46.542s; compile_seconds=49.208. Link 9.593s (lld_linker 9.596s). changed_source_preflight 47.259s. Object-cache scan 3.7s, publish 0.394s.
- The build was making normal progress and finished ~34s after the budget elapsed. It was not stalled or wedged; the detector fired on ordinary compile+link time and the build then succeeded.

ACCEPTANCE CHECK
- Issue carries wait_id, stage=game_build, reason ([GAME BUILD] revision=7d44afce..., sources=642 jobs=10), elapsed, budget, build_id, generation, source_generation=5, request_ids=[auto-5], started_utc: PASS.
- Reviewer wake queued exactly once: PASS. .dev/agent-wake/events.jsonl has one wake.queued for wake-21cd23758a37 / issue-3ffb3f29b1f7 at 13:45:41.604Z; no second queued record for this wait.
- dev-loop.wait.over_budget runtime event: MISSING. The run active at this wait, logs/2026-11-22/20261122_011031, has no dev-loop/ subdirectory, so the wait/wake runtime events were never journaled (run 20261122_134637 only started 13:46:37, after this wait). This is the known defect in issue-6e2d7b91e7d9 / issue-0498cbef4ef3 and blocks the journal half of this issue's proof_expectation. The issue record + wake record are present; the dev-loop wait event is absent.
- Wake dispatch latency: queued 13:45:41.604Z -> codex claimed/launched 13:45:51-52 -> codex exited -> retry to opencode 13:58:41.476Z -> opencode claimed/launched 14:22:49.079Z (~37 min). This is the known late-dispatch defect (issue-84c584957e65), not a wait-detector fault.

WHY IT RECURS (systemic, measured)
Recent whole-game DLL attempts with a real source change (from .dev/build-timings): 94.681s (ada12d, this), 80.443s (567c5b), 118.006s (d10b1b), 137.01s (326bd4), 74.311s (c00839), 68.864s (46b1e5), 61.943s (687e4d), 51.062s (cecbf8). No-op builds are 6-12s. So nearly every source-changing build exceeds WAIT_ISSUE_SECONDS=60 (devscripts/dev-loop.py:109-110) and triggers one build-friction issue, which is exactly the 'noisy wait policy could create duplicate friction issues' risk recorded on this issue. Note: this occurrence's reason field is clean ([GAME BUILD] banner), so the reason-truncation defect seen on sibling occurrences did not affect it.

RECOMMENDATION (not implemented; needs separate owner/claim)
- Tune the game-dll-build budget to measured reality (e.g., per-kind budget ~150s, or exempt a single long-TU compile) so normal 60-140s rebuilds stop manufacturing friction issues; or aggregate recurring identical game-dll-build waits into one issue with occurrences instead of a new issue per wait_id.
- Follow-ups already tracked: budget/policy issue-9a4503f6ed68 (wake-proof-wait, in_review); missing dev-loop journal events issue-6e2d7b91e7d9 / issue-0498cbef4ef3; late wake dispatch issue-84c584957e65.

CONCLUSION: Legitimate long build; no stall to chase. The wait hook behaved correctly for this wait_id. No source change is warranted by this occurrence.
