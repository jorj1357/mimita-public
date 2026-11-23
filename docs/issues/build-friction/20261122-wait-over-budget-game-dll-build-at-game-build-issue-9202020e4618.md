# issue-9202020e4618: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Context

### 2026-11-22T19:35:00.278Z — build-wait-reviewer

related_build: game-dll-20261122T183449310183Z-383c31
related_run: 20261122_174002

## Review (reviewed-with-findings)

### 2026-11-22T19:36:21.792Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Bounded review of wait occurrence game-dll-build:2026-11-22T18:34:48.257Z (elapsed 60.98s, budget 60.0s).

VERDICT: Acceptance is met for this occurrence with the same attribution defect seen on prior occurrences. The wait was productive single-TU compile work, not a stall; the build completed successfully. The recurring over-budget policy continues to open one issue per slow build.

EVIDENCE (direct observation, logs/2026-11-22/20261122_174002/dev-loop/events.jsonl)
1. dev-loop.wait.over_budget (line 148) at 2026-11-22T18:35:56.631Z: wait_id game-dll-build:2026-11-22T18:34:48.257Z, kind game-dll-build, stage game_build, elapsed_seconds 60.98, budget_seconds 60.0, source_generation 11, request_ids [auto-11], build_id 2026-11-22T17:39:49.763492Z, generation 511. created=true.
2. The actual attempt was game-dll-20261122T183449310183Z-383c31 (.dev/build-timings/game-dll-20261122T183449310183Z-383c31.json): started 18:34:49.310Z, completed 18:38:02.839Z, SUCCESS, generation 512, revision a8f03688..., total_seconds 193.503, worker total 186.598. changed_paths=[src/gamemode/gamemode.cpp] (build note 7995138acaeb, agent horde-behavior-evidence).
3. First divergence: the wait fired during changed_source_preflight, which measured 119.786s because one translation unit, src/gamemode/gamemode.cpp, compiled in 119.638s (events.jsonl line 150: submitted 18:35:13.683Z, completed 18:37:13.327Z). The dev-loop.game-build.result (line 152) shows status ok, compiled 2, skipped 641, compile_seconds 126.255, link_seconds 32.363, total_seconds 186.598. The build was advancing the whole time.
4. The gcc -ftime-report captured on that TU (events.jsonl line 150) explains the cost: phase parsing 59.54s (52%), opt and generate 41.00s (36%), preprocessing 25.86s (23%), template instantiation 18.59s (16%), callgraph functions expansion 22.75s. This is genuine compile expense, not a hung worker.
5. Reviewer wake was queued exactly once for this wait_id: wake-b93fbea362ba queued 2026-11-22T18:35:52.946Z (.dev/agent-wake/events.jsonl line 1842). No duplicate issue or wake was created for this occurrence (dev-loop.wait.over_budget created=true, line 148).

ATTRIBUTION DEFECT (does not block this occurrence)
evidence.build_id 2026-11-22T17:39:49.763492Z and evidence.generation 511 are the previously published EXE artifact and prior generation, not the in-flight attempt 383c31/generation 512. This matches issue-136d4553ff92 and the prior reviews (issue-d668de6bfc5e, issue-31ab18dedc96). related_build is now corrected to game-dll-20261122T183449310183Z-383c31 and related_run to 20261122_174002; the raw evidence field is append-only and cannot be retroactively fixed.

LATE DISPATCH (already owned elsewhere)
The codex primary adapter claimed at 18:35:55.968Z and launched pid 9620 at 18:35:56.224Z, then stalled; the scheduler retried to opencode at 18:56:11.918Z and this review launched at 19:07:49.218Z, about 32 minutes after issue creation. This matches the existing late-dispatch issue-84c584957e65; not re-filed here.

RECURRENCE / DUPLICATE NOISE (policy decision, no code change made)
This is another instance of the same 60s flat-budget behavior documented in issue-136d4553ff92 and the prior build-wait reviews: per-wait_id uniqueness in devscripts/build_friction.py:143-202 plus a 60s budget (devscripts/dev-loop.py:108-109) below the cost of one slow TU (here ~120s) means nearly every changed-source game-DLL build files another issue and wakes another reviewer. Suggested direction, unchanged: condition the budget on stage progress / expected per-TU cost, or coalesce by kind+stage into one canonical issue with occurrences. Ownership and budget policy belong to build-friction; left unowned.

NO CODE FIX CLAIMED. No repository source or config edited; coordination records only.
