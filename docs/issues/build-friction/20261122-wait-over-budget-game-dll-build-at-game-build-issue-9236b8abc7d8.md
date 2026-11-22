# issue-9236b8abc7d8: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Context

### 2026-11-22T10:49:38.853Z — build-wait-reviewer

related_build: game-dll-20261122T102657546775Z-8fc61c

## Review (pass_with_findings)

### 2026-11-22T10:50:48.230Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed build/test wait issue-9236b8abc7d8 (game-dll-build at game_build, wait_id game-dll-build:2026-11-22T10:26:57.432Z).

Verdict: the wait was legitimate, not a stall. No code fix is required for this occurrence.

Evidence checked:
- .dev/build-timings/game-dll-20261122T102657546775Z-8fc61c.json: success=true, generation=486, total_seconds=84.109, started 2026-11-22T10:26:57.546Z, completed 2026-11-22T10:28:21.697Z. Phase breakdown shows real work: changed_source_preflight=28.235s, compile=1.453s (dominated by src/actor/player-ai.cpp 27.743s inside the preflight window), link=15.86s. Only 2 TUs compiled and 641 skipped from the shared object cache, so this was a one-source incremental build, not a full rebuild.
- .dev/build-timings/game-dll-20261122T102657546775Z-8fc61c.jsonl: src/actor/player-ai.cpp translation_unit completed at elapsed_ms=61453 (~10:27:59.4). The 60s threshold was crossed at 10:27:58.17 while player-ai.cpp was still compiling, then link and publish completed at 10:28:21.690.
- .dev/build/current.json + ready.json: generation 486 published, status ok/ready, code_hash ea2262...3058, revision ab29681...4e3f.
- .dev/agent-wake/events.jsonl: exactly one wake.queued event for issue-9236b8abc7d8 (wake-3d326719a879, WAKE_9bf75c4b5eb940499bf529e6f547af35) at 2026-11-22T10:28:01.185Z.
- Build notes: note f52b8fa00946 (cpp-latency-linker-investigation, created 2026-11-22T10:26:53.850Z) - Restore source after temporary comment-only hot-build canary - on src/actor/player-ai.cpp. This is why the build was triggered; the rebuild is expected build work from a deliberate canary restore, not a queue or worker hang.

Acceptance result: satisfied. One issue per wait_id, reviewer wake queued exactly once, and all required wait fields present (wait_id, stage, reason, elapsed, budget, request/build IDs, evidence).

Diagnostic note (positive): unlike issue-741e8b4fb8fb, this issue reason field is a readable stage summary ([GAME BUILD] revision=... sources=642 jobs=10 priority=high cache=... seeded=0) rather than a mid-JSON tail, so the diagnostic-quality defect reported earlier did not recur here.

Journal note: this occurrence has no persisted dev-loop journal entry for the wait and no matching dev-loop events.jsonl, so the wait event itself is only captured in the issue evidence; the build-timings summary/jsonl are the authoritative build record. Reconciling dev-loop wait persistence remains part of the separately reported diagnostic work; not fixed here.

Duplicate-volume note: each over-budget wait is keyed by wait_id, so this is a distinct occurrence by design, not a duplicate of the neighboring game-dll-build wait issues. The underlying fixed-60s-budget-vs-real-compile/link-variance friction is tracked by existing build-friction work (e.g. issue-0322fa45b0e8 and the build-runtime-subagents queue issue); no new issue was created for this occurrence.
