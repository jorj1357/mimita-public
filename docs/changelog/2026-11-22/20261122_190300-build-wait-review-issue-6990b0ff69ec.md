# Build/test wait review: exe-build presentation_dll (issue-6990b0ff69ec)

Date: 2026-11-22 ~19:03Z
Agent: build-wait-reviewer
Wake: wake-d53716cfb2d1
Issue: issue-6990b0ff69ec (Wait over budget: exe-build at presentation_dll)

## Scope

Read-only evidence review. No source, config, build-script, or test edits. No
compiler, `build.py`, module worker, or second dev-loop invoked. One final
changelog for this session; the only repository writes are the coordination
store's issue documents.

## What was reviewed

- Wait `exe-build:4:2026-11-22T17:23:49.758Z`: kind `exe-build`, stage
  `presentation_dll`, elapsed 61.454s vs 60.0s budget, generation 4,
  source_generation 4, build_id `2026-11-22T08:00:31.376619Z`, request_ids [].
- Journal `logs/2026-11-22/20261122_144609_01/dev-loop/events.jsonl`: exactly one
  `dev-loop.wait.over_budget` (17:25:00.973Z, created=true) and one
  `dev-loop.wait.reviewer-wake` (17:25:00.971Z) for this wait_id; issue
  `occurrences=[]` beforehand.
- Underlying attempt: `dev-loop.build.failed` 17:32:12.249Z, build
  `2026-11-22T08:00:31.376619Z` (matches evidence), source_generation 4,
  duration 494.796s, error "build failed with exit code 1", setup 5.133s,
  build_script 488.242s.
- Optional presentation-DLL child: `.dev/presentation-dll-attempt.json` outcome
  `failed`, elapsed_s 92.516, exit_code 1, error `build_game_dll.py exited with
  code 1`, failed_at_epoch ~17:25:38Z. `build.py:652-812` keeps this child
  fail-open, so it did not fail the EXE but added ~92.5s.
- Wake dispatch: `wake-d53716cfb2d1` queued 17:24:55.483Z; codex launched
  17:25:00.143Z then failed; retry switched to opencode 18:02:46.039Z; opencode
  launched 18:02:51.176Z — ~37m55.7s total delay.

## Verdict

`confirmed-genuine-failing-exe-build` on issue-6990b0ff69ec. The wait record and
its one-issue/one-wake mechanics are correct; the sample was inside a real,
long, ultimately-failing EXE build, not a phantom or duplicate. Evidence
`build_id` matches the failed attempt (no previous-artifact misattribution).
The avoidable component is the fail-open presentation-DLL child (~92.5s; owners
issue-ae248907f1f1 and issue-e044a931c54b). The EXE exit-1 cause is unrecoverable
because `build-output.log` was overwritten by the later successful build.

## Cross-links

- issue-84c584957e65: appended occurrence for the ~37m55.7s late reviewer wake.
- issue-ae248907f1f1: appended occurrence for the 92.516s failed presentation-DLL
  child inside the 17:23:49Z EXE attempt.
- Systemic owners unchanged: issue-e044a931c54b (avoidable waits),
  issue-136d4553ff92 (stage accounting).

## Claims

- Source evidence: none (review-only).
- Build evidence: EXE attempt 494.796s failed (journal); presentation-DLL child
  92.516s failed (state file).
- Runtime evidence: none required for this wait-policy issue.
- Human acceptance: not performed.

## Limitations

The EXE exit-1 root cause is unknown (failed attempt's build output not
retained); reported as medium confidence. No fix or closure claimed.
