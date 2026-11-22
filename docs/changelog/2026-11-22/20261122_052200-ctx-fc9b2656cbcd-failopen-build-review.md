# Context ctx-fc9b2656cbcd fail-open build review

## Task

- Wake: wake-bbaa489ea90f; session work-f75b18307281
- Agent: issue-review-sweeper
- Scope: bounded review of high-priority user context ctx-fc9b2656cbcd and its linked issue; no source, build, runtime, or close action.
- Status: REVIEW_RECORDED_OPEN

## Requested behavior

Review the new high-priority context ("how to amek it so failures like that dont fail teh whole build" plus "add gold behaivor of pursuing goal mode"), confirm routing and ownership, and record findings without editing code.

## Findings

- Routing was already repaired by cpp-latency-pressure-profile: ctx-fc9b2656cbcd is now primary on issue-91e3384b219b (goal persistence and optional-diagnostic fail-open) and related to issue-136d4553ff92 (link id 170). The goal-mode recommendation and optional-diagnostic evidence were appended to issue-91e3384b219b at 05:20-05:21Z.
- The user clause has two failure classes. The already-captured reading covers an optional diagnostic (WPR start 0xc5585011; not in the build path; no build invoked).
- The required build failure class is not captured: one failing translation unit aborts the whole generation. game_build_worker.py:1355-1374 sets stop_event and cancels pending futures on the first failure; :1829-1854 and :2007-2027 return 1 on preflight/compile failure; :2081-2098 return nonzero on link failure.
- Existing issue evidence: attempt game-dll-20261121T022901426487Z-d15759 failed changed_source_preflight on one file after 1240.7s; generation 405 failed on the single GroundState::groundNormal error.
- A single broken TU cannot safely be skipped into a published DLL without stale-behavior risk, so required gates should remain fail-closed pending explicit user direction.

## Specification alignment

- Followed docs/workflows/issue-intake-and-triage.md and docs/specs/dev-loop/agent-coordination.md for review and append-only records.
- Kept build, publication, activation, and behavior evidence as separate claims.

## Exact implementation changes

- None. Append-only coordination records only.

## Validation

- No build, compiler, worker, or second dev-loop was invoked.
- Added one review (verdict needs-decision) and one fail-open proposal to issue-136d4553ff92.
- Added task dll-fail-soft-measurement owned by build-coordinator: measure one required compile failure and whether it blocks unrelated revisions; do not change fail-closed behavior.
- Acknowledged ctx-fc9b2656cbcd for issue-review-sweeper.

## Evidence

- Review and proposal are in docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md.
- File-lease edit commit hash: 6b4875d1b9828c4f25db8e438c649fec0126ebe13e889e5be652d54738c201da.

## Agent handoff context

- Owners: cpp-latency-pressure-profile (context primary on issue-91e3384b219b), build-coordinator (issue-136d build stage accounting and new measurement task).
- Next exact step: confirm with the user whether "failures like that" means optional diagnostics or a required compile failure; if required, run dll-fail-soft-measurement read-only.
- Do not redo: the routing repair to issue-91e3384b219b and the optional-diagnostic recommendation.

## How to reverse

- No source or build to reverse. Preserve the append-only issue review and the task; remove this changelog only through normal review if created in error.
