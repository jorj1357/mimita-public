# issue-e044a931c54b: Waiting is the enemy: remove avoidable build and test waits

## Human report (verbatim, immutable)

> ensure waiting is marked as an issue like a principle to get wait times down as close to 0 sec as possible


## Assignment

### 2026-11-20T03:17:28.752Z — build-runtime-subagents

Owner: build-runtime-subagents

## Agent suggestion

### 2026-11-20T06:04:03.845Z — root-build-wait-efficiency

Add the user's specific >60s trigger: when an agent estimates or observes a build/test wait over one minute, log duration/ETA and blocker, create or link a build-friction issue, and investigate the current queue/worker phase immediately. Reuse this issue for the same root cause and append evidence rather than duplicate tasks.

## User quote

### 2026-11-20T06:04:04.653Z — root-build-wait-efficiency

Via root-build-wait-efficiency; supports unspecified:

> every time an agent has this , like, talking about build and how it coudl take N minutes, if its like more than a minute, that is a issue, and should ahve a issue and begin invesitgating furhter why, like this should  be there like  waiting issue time issue need to get  these times tracked and super aware

## User quote

### 2026-11-20T07:21:38.218Z — melee-arcade-fix

Via melee-arcade-fix; supports unspecified:

> This task observed root dev-loop whole-game DLL build attempt game-dll-20261120T071159488678Z-9a31ba: object_cache_scan took 169.689s across 642 inputs (source_count 642; 16 compiled, 626 skipped), compile 105.533s, link 22.102s, total 331.372s. It delayed the first pinned runtime validation attempt; build timing summary is .dev/build-timings/game-dll-20261120T071159488678Z-9a31ba.jsonl and generation 398 was published.

## Agent suggestion

### 2026-11-20T20:11:48.585Z — build-wait-reviewer

Measured exe-build wait evidence from issue-00b133ae7e13 review: a 1-TU incremental EXE build took 130.017s/171.983s compile while a 12-worker game-DLL build ran, versus 19.328s when idle (build/build-metrics.jsonl, 19:45:36 and 19:55:14 vs 07:47:06). Host has 12 logical CPUs and both builds use jobs=12, so total compile workers are ~2x oversubscribed. In addition, both published artifacts reported source_changed_during_build=true and .dev/state.json showed latest_build_stale=true immediately, triggering another build chain. Candidate fix: cross-build worker arbitration (cap EXE+game-DLL+module compile workers to logical CPUs minus headroom or serialize EXE vs game-DLL compile) plus source-stability debounce before publish. Expected: return 1-TU builds toward the 19s baseline, under the 60s budget.
