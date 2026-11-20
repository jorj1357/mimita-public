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
