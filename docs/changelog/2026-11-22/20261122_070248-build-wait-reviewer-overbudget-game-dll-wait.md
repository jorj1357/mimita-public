# Over-budget game-DLL wait review (build-wait-reviewer)

Date: 2026-11-22 ~07:02Z
Agent: build-wait-reviewer
Context: wake-2aaad8bf8bbb
Issue: issue-00e2e9243beb (Wait over budget: game-dll-build at game_build)

## Scope

Read-only evidence review. No source, config, or build-script edits were made. No
compiler, build.py, module worker, or second dev-loop was invoked. No new runtime
was started.

## Wait occurrence

- wait_id `game-dll-build:2026-11-22T06:21:19.547Z`, stage `game_build`,
  elapsed 60.036s, budget 60s.
- Actual attempt `.dev/build-timings/game-dll-20261122T062119640904Z-8db74c.json`,
  generation 465, changed `src/npc/npc.cpp`, success, total 121.59s, completed
  06:23:21.246Z.
- Cost: changed-source preflight 69.18s (single TU `src/npc/npc.cpp` compile
  68.873s), link 20.477s, object-cache scan 4.937s.
- Wake `.dev/agent-wake/requests/wake-2aaad8bf8bbb.json`: queued 06:22:21.098Z,
  codex launch 06:22:22.273Z, retry 06:30:49.997Z, opencode launch 06:35:14.013Z
  (~173s queue-to-effective-launch). Exactly one wake for this wait.

## Findings

1. Wait evidence has the wrong identity. It records build_id
   `2026-11-22T02:37:06.972018Z` (latest EXE build) and generation 464, while the
   building attempt was `game-dll-20261122T062119640904Z-8db74c` / generation 465.
   `devscripts/dev-loop.py:3525-3535` uses `self.game_build_generation`, assigned
   only on completion at line 2967.
2. The dev-loop wait journal event cannot be verified. `.dev/state.json`
   `events_path` for run 20261122_011031 does not exist; `record_runtime_event`
   (`devscripts/dev-loop.py:2292-2297`) only prints OSError. Issue and wake records
   exist; journal evidence does not.
3. The 60s budget is below normal game-DLL build duration. 34 of 44 attempts on
   2026-11-22 exceeded 60s (median 122.9s), producing 94 open identical wait
   issues. Recommend a budget above normal duration and/or aggregation per
   kind+stage with occurrence counts.

## Claims

- Source evidence: none (review-only).
- Build evidence: attempt game-dll-20261122T062119640904Z-8db74c, generation 465.
- Runtime evidence: none required for this wait-policy issue.
- Human acceptance: not performed.

## Handoff

- Appended review verdict `findings` to issue-00e2e9243beb.
- Appended a suggestion with these evidence pointers to owner issue
  issue-9a4503f6ed68 (wake-proof-wait).
- No code fix claimed; owner should apply the identity, journal-directory, and
  budget fixes.
