# issue-84ec2bf27598: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-build-wait)

### 2026-11-20T20:07:53.719Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed by build-wait-reviewer in a bounded wake activation (wake-a46207801856, coordination session-29c24d704749). Read-only; no source edits, no builds started.

Issue completeness: readiness ready=true, missing=[], all required fields present, including wait_id, stage, reason, elapsed, build/run IDs, and evidence. No clarification needed.

Reproduction confirmed from the recorded build attempt, not a stuck queue:
- Build attempt game-dll-20261120T190454164010Z-f76868 started 2026-11-20T19:04:54.164Z, completed 2026-11-20T19:11:10.113Z, success=true, generation 410, source_generation 31, revision 28fa397..., request_ids auto-30/auto-31.
- Total 375.934 s vs the 60 s wait budget, so the auto-friction issue and reviewer wake are correct.
- Dominant measured cost is the object/dependency scan: object_scan_seconds 215.0, scan_parse_ms 207622.354, scan_stat_ms 199.648. Then changed_source_preflight 38.379 s, compile 68.254 s (12 units), link 32.395 s, publish 20.329 s, dependency_check_ms 6892.509.
- Evidence: .dev/build-timings/game-dll-20261120T190454164010Z-f76868.json and .jsonl (phase/timeline, build.completed at 19:11:10.111Z).

Recurrence: this is the third recorded over-budget game_build wait on 2026-11-20 after issue-2008e743c710 and issue-63dccbd8f8b6 (08:46/08:47). Per instructions, add occurrences here rather than create another duplicate. The recurring cause is long full/partial game-DLL builds whose scan phase alone exceeds the 60 s budget, not a stalled dev-loop.

Actionable recommendation (no fix claimed):
1. Reduce or cache the dependency/object scan (scan_parse_ms ~208 s is ~55% of the build) so a one-file change does not trigger the full scan cost.
2. Have the wait reporter distinguish "actively progressing long build" from "no progress" and attach the current phase/percent, so reviewer wake payloads lead with the dominant phase.
3. Keep the 60 s budget; the issue is valid.

Wake-delivery note: the reviewer wake prompt file .dev/agent-wake/logs/2026-11-20T19-05-55.137Z-build-wait-reviewer.prompt.txt contains the full request, but the opencode session received only its first line ("You were woken by MiMITA agent-wake. This is a bounded test activation."). This is the known defect issue-62ab3daa615d; the requested work here was recovered by reading the prompt file. It does not invalidate this wait occurrence.

Verdict: valid, complete, recurring build-friction. No duplicate created. No behavior proof claimed; this is evidence review only.

## Scheduled independent review

### 2026-11-24T00:59:43.878Z — hot-reload-latency-root

A bounded independent review was queued as wake wake-4faab5b3c977 after 482 completed work sessions (trigger=sync). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (confirmed-valid-original-with-stale-identity; recurrence-carried-by-per-occurrence-siblings)

### 2026-11-24T01:09:35.715Z — issue-review-sweeper

Reviewer: issue-review-sweeper

﻿INDEPENDENT REVIEW — issue-review-sweeper (wake-4faab5b3c977), read-only, no source edit, no build, no close, no fix claimed.

OBSERVED FACTS
- Readiness: ready=true, missing=[], state=in_review, one prior review (#17 build-wait-reviewer, verdict confirmed-build-wait). No missing reproduction/acceptance/paths/proof field.
- Original occurrence (wait_id game-dll-build:2026-11-20T19:04:54.123Z): evidence carries wait_id, stage=game_build, reason, elapsed_seconds=60.066, budget_seconds=60.0, request_ids auto-30/auto-31, source_generation 31, build_id=2026-11-20T17:17:26.879159Z, generation=409.
- Independent timings read (.dev/build-timings/game-dll-20261120T190454164010Z-f76868.json): the attempt that actually served this wait started 2026-11-20T19:04:54.164Z, success=true, generation 410, revision 28fa397..., request_ids auto-30/auto-31, total_seconds 375.934; object_scan_seconds 215.0, scan_parse_ms 207622.354, compile 68.254s (12 TUs), link 32.395s.
  => This issue's own evidence records the PREVIOUS published build (generation 409 / 2026-11-20T17:17:26.879159Z), not the in-flight attempt (generation 410 / game-dll-20261120T190454164010Z-f76868). The acceptance 'carries ... build/run IDs' is met only in the weak sense; the true in-flight build/run ID is absent. This is a same-signature instance of the stale-identity defect already filed on issue-9a4503f6ed68 (root: dev-loop.py:3878-3879 check_wait_budget uses self.latest_build / self.game_build_generation, assigned only at completion). Review #17 did not flag it.
- Wake: .dev/agent-wake/events.jsonl has exactly one build-wait-reviewer wake for this issue, wake-a46207801856 (plus my distinct manual sweep wake-4faab5b3c977). So 'a reviewer wake is queued exactly once for this wait occurrence' holds, and it delivered (review #17). Wake acceptance met for the original occurrence.
- Current-HEAD recurrence: wait game-dll-build:2026-11-24T00:39:01.308Z minted sibling issue-ecdc697b2415 (created 2026-11-24T00:40:02.293Z, state=new, reviews=[]). Its evidence carries build_id=2026-11-23T19:22:15.804284Z and generation=null (previous published/unknown). The in-flight attempt (.dev/build-timings/game-dll-20261124T003901444604Z-3c8d4e.json) is generation 576, started 00:39:01.444Z, success at 00:54:50.688Z, total_seconds 949.243 (worker 934.336; changed_source_preflight 108.585s; compile phase 814.569s over 305 TUs; link 5.088s; object_scan 3.53s). Its evidence.reason is again a truncated mid-JSON tail ('...progress_write_ms':4.73,'record_overhead_ms':5.507}), reproducing issue-6e08d89b55e3.
- Late dispatch on that fresh sibling: wake-c4a5d204a3c9 queued 00:40:03.383Z; codex claimed/launched 00:40:05.920/00:40:05.950Z then exited; wake.retry to opencode 00:57:57.135Z; opencode claimed/launched 00:59:44.201/00:59:44.217Z => ~19m41s delay; issue-ecdc697b2415 still had no review at sweep time. Fresh occurrence of issue-84c584957e65 (owned there; not duplicated here).
- Backlog at sweep: 145 open 'Wait over budget: game-dll-build at game_build' issues (128 in_review, 16 new, 1 assigned), each keyed to a distinct wait_id.

INFERENCE AND UNCERTAINTY
- build_friction.record_wait_incident keys uniqueness on wait_id and deliberately creates a new issue per wait (devscripts/build_friction.py:143-169; _wait_issue at 131-140). Review #17's advice to 'add occurrences here rather than create another duplicate' is contrary to current design: the sibling issues are intended, not accidental duplicates. This issue is the earliest confirmed occurrence, not the recurring tracking home.
- High confidence on stale identity for BOTH the original and the fresh occurrence (evidence dict compared directly against the timings JSON). High confidence the original wait was real and over budget (375.934s >> 60s). Medium confidence on the cost attribution below (correlation, not a controlled experiment).
- I did not inspect the wake prompt payload for wake-a46207801856 beyond its queue record and the resulting review.

CONTRARY EVIDENCE
- Review #17 attributed ~55% of the 2026-11-20 cost to the object/dependency scan (object_scan 215s, scan_parse 207.6s). That was a cold-cache build. In the fresh gen576 warm-cache build the scan fell to 3.53s while compile dominated 814.6s/923.1s (305 TUs). So 'reduce the scan' is not a sufficient recurring fix; the recurring driver alternates between cold scan and header-fanout-driven recompilation. The gen576 fan-out (player.h 233, movement-types.h 270, input-frame.h 175, packet-combat/common 160) correlates with the 305 recompiled TUs and is owned by header-fanout issue-196c3045b4cb.
- Apart from stale identity, the original occurrence is fully compliant: complete fields, genuine over-budget build, exactly one reviewer wake, wake delivered, review appended.

NEXT DISCRIMINATING CHECK
On the next over-budget game-dll-build wait, confirm (a) evidence.build_id/generation equal the in-flight attempt (game-dll-<started>/generation N+1) rather than the prior published build or null, and (b) evidence.reason is a readable stage summary, not a mid-JSON tail. If either is still wrong, neither the identity fix (issue-9a4503f6ed68) nor the reason fix (issue-6e08d89b55e3) has landed. Record separately whether the wait fired on a build that ultimately succeeded (flat-budget false positive) or genuinely stalled.

VERDICT: confirmed-valid-original-occurrence; issue-creation and single-delivered-wake acceptance met for the original occurrence, caveated by stale in-flight build/run identity in its own evidence; recurring phenomenon now carried by per-occurrence siblings (145 open). No new issue, no close, no fix claimed. Proof of future behavior remains unverified until the next-check evidence exists.
