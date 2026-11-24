# game-dll-build wait review (issue-816c05804889)

Reviewed wait `game-dll-build:2026-11-24T07:53:14.620Z` (62.252 seconds at the budget check, stage `game_build`) against attempt `game-dll-20261124T075315478413Z-04a3b7` (generation 587), request `2026-11-24T07:53:01.604Z-c3e4ca1d:mode3:game`, journal `logs/2026-11-24/20261124_075114/dev-loop/events.jsonl`.

The build was real and succeeded, not stalled: `dev-loop.wait.reviewer-wake` and `dev-loop.wait.over_budget` are both present at `2026-11-24T07:54:24.968/.969Z` with `created=true`; generation 587 published at `07:56:57.014Z` with `total_seconds` 221.684. Dominant costs were one translation unit, `src/vehicles/vehicle-system.cpp` at 101.057 seconds, and a 53.644-second link over 491,604,041 bytes / 644 inputs, after about 51.6 seconds of manifest/snapshot/codegen stages. The flat 60-second budget is below routine whole-game DLL duration, so the wait is expected rather than anomalous.

Exactly one reviewer wake (`wake-30dce0a84a51`) was queued (`07:54:20.143Z`) and the reviewer-wake event embeds that correct wake id. Gaps against the acceptance criteria:

- `evidence.build_id` is `2026-11-24T07:48:35.684015Z`, the launched EXE artifact, not the in-flight game-DLL attempt; `generation` is null and there is no `build_attempt_id`/`run_id`. Identity defect owned by issue-136d4553ff92 / issue-9a4503f6ed68.
- `elapsed_seconds` 62.252 is the budget-crossing sample, not the total wait (~221.7 seconds start-to-publish).
- Dispatch was late: codex claimed/launched `07:54:24.597/.892Z` but delivered nothing; retry `08:32:49.965Z`; opencode claimed/launched `09:20:38.674/.761Z`, 5178.6 seconds after queue (~86.3 minutes).

Resource sample (attempt scope): 176 samples at 1 second, mean host CPU 99.994%, peak 100%, minimum available memory 17,457,152 B (~16.6 MiB) of ~15.65 GiB; runtime alignment unavailable, so build-phase only.

Review verdict `true-positive-expected-build-duration; identity-and-latency-gaps-tracked` appended to issue-816c05804889. Dispatch recurrence appended to issue-84c584957e65; resource evidence appended to issue-2a5c5da53565. No source edit, build, runtime change, process control, or runtime proof; the issue remains `in_review` and unowned.
