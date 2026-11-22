# Over-budget exe-build compiling wait review (build-wait-reviewer)

Date: 2026-11-22 ~08:31Z
Agent: build-wait-reviewer
Wake: wake-ceb897b3ace1
Issue: issue-727472d32238 (Wait over budget: exe-build at compiling)

## Scope

Read-only evidence review. No source, config, or build-script edits. No compiler,
build.py, module worker, or second dev-loop invoked. No runtime started. Appended
one review, one context update, and one addendum suggestion to the issue, and
recorded this changelog.

## Wait occurrence

- wait_id `exe-build:3:2026-11-22T07:58:48.330Z`, stage `compiling`, reason
  `compiling 0/45 [CXX] src\host\host-snapshot-fallback.cpp`, elapsed 60.472s vs
  60.0s budget, generation 3, source_generation 3.
- Request `2026-11-22T07:58:47.625Z-997bacf1` from session
  `cpp-latency-pressure-profile` (`.dev/agent-coord/events.jsonl` seq 6473-6474,
  07:58:47.853Z).
- The same build completed and published artifact `2026-11-22T08:00:31.376619Z`
  (source_generation 3, sha256 `4fa387f46e02...`) per `.dev/state.json` and
  `.dev/builds/2026-11-22T08-00-31.376619Z/build-identity.json`. Total ~103s, so
  the 60.472s wait was the compile still running, not a stall.
- Follow-up EXE requests `08:10:42.398Z-5e6cca1b`, `08:14:05.864Z-af7a1a5e`, and
  `08:20:18.311Z-9e9bbdaa` all succeeded; the last was a 5.4s no-op. No retry or
  failure is attached to this occurrence.
- Wake `wake-ceb897b3ace1`: queued 07:59:50.265Z (`WAKE_9e188efba3fa40c7b8166707af300d8e`),
  launched via opencode 08:12:27.998Z after a codex attempt. Exactly one wake for
  the wait_id.

## Findings

1. The wait was genuine and benign: a full 45-TU EXE compile took ~103s on current
   source and published successfully. It is not a stuck build.
2. Acceptance passes for the wait record: issue carries wait_id, stage, reason,
   elapsed, request_ids, generation, and evidence; readiness ready=true,
   missing=[]; exactly one reviewer wake was queued.
3. Recurring-friction root cause: `WAIT_ISSUE_SECONDS` in
   `devscripts/dev-loop.py` `check_wait_budget` (~3478-3539) is a single flat 60s
   threshold applied to EXE compiling (45 TUs), game-DLL `game_build` (642 TUs),
   and queue wait, with no per-kind/per-stage allowance and no progress-stall
   requirement. The last hour shows 20+ near-identical waits at 60.1-60.9s while
   the corresponding builds completed, so each legitimate build mints a new issue
   and reviewer wake.
4. Known attribution/journal defects apply and are already owned elsewhere:
   evidence `build_id` is the previous published artifact
   `2026-11-22T02:37:06.972018Z` rather than the in-flight attempt
   (issue-136d4553ff92), and the declared dev-loop journal
   `logs/2026-11-22/20261122_011031/dev-loop/events.jsonl` does not exist
   (issue-0498cbef4ef3). Neither invalidates this review.

## Claims

- Source evidence: none (review only).
- Build evidence: EXE artifact `2026-11-22T08:00:31.376619Z` published from
  source_generation 3; follow-up requests succeeded.
- Runtime evidence: not required for this wait-policy review.
- Human acceptance: not performed.

## Handoff

- Appended review verdict `confirmed-benign` to issue-727472d32238 (now
  in_review) and set related_build/related_run.
- Recommendation only, no code fix claimed: calibrate per-kind/stage budgets from
  measured p95 (EXE compiling above ~103s, game-DLL game_build above the observed
  ~96.5s) or file only after `active_build_progress` is unchanged for N seconds,
  and coalesce same-signature occurrences. Systemic wait-policy ownership remains
  with issue-9a4503f6ed68 / issue-e044a931c54b; in-flight identity with
  issue-136d4553ff92; missing journal with issue-0498cbef4ef3.
