# Independent review of the exe-build-queue wait issue

## Session-specific evidence

This activation completed the scheduled independent review of
`issue-e9f3b2727a9a` ("Wait over budget: exe-build-queue at build_pending").
I read the issue record, readiness (8/8, no missing fields), proof state, the
original wait evidence, the sibling wait issues, and the current dev-loop code
paths. I did not start a build, a test run, or edit source.

Findings appended as review 254:

- This occurrence is a policy false positive: `build_pending` was set with
  empty `request_ids`, no active build, and the loop's idle detail string, so a
  normal idle/launch-pending state crossed the flat 60 s budget.
- The branch at `devscripts/dev-loop.py:3726-3743` still does not inspect
  `pending_build_request_ids` or a blocking build, and
  `_queue_build_if_needed` (`dev-loop.py:2950-2958`) still sets `build_pending`
  without recording a triggering request id, so the class can recur.
- `devscripts/build_friction.py:131-169` dedupes only on the exact `wait_id`,
  and `dev-loop.py:3731-3732` embeds a process-local `time.monotonic()` float,
  so each loop process can mint a new issue for an unchanged condition. Six
  issues share this title/signature (`e9f3b2727a9a`, `3dad54c7224d`,
  `af958c3e10eb`, `4891874dd02b`, `f92715ba3c7f`, `8cbff41b5590`).
- Contrary evidence: four of the six were real serialization behind game-DLL
  builds, and `8cbff41b5590` had empty `request_ids` but a real blocker, so a
  bare "skip empty request_ids" fix would suppress real waits.
- Exactly one reviewer wake exists for this issue
  (`wake-667af71a82dc`, 2026-11-20T15:41:41.905Z). No dev-loop journal
  survives for the 2026-11-20 15:4xZ occurrence, so `proof_expectation` is only
  partly met.

## Exact source changes

- No C++ or configuration source changed.
- Appended review 254 to
  `docs/issues/build-friction/20261120-wait-over-budget-exe-build-queue-at-build-pendin-issue-e9f3b2727a9a.md`.
- Appended one process proposal to
  `docs/issues/build-friction/20261120-waiting-is-the-enemy-remove-avoidable-build-and--issue-e044a931c54b.md`
  targeting one open issue per wait class, a persistent wait-event journal, and
  suppression of issue creation for empty-request idle waits.
- Created this changelog; there was no prior text.

## Next check

Run one stale-artifact launch with no sealed batch request, let
`build_pending` persist past 65 s, then repeat once with an EXE request queued
behind an active game-DLL build. Inspect both `dev-loop/events.jsonl` wait
events for `request_ids` and a named blocker.
