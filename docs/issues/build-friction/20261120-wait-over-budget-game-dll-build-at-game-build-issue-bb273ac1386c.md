# issue-bb273ac1386c: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-expected-build-latency)

### 2026-11-20T17:25:13.690Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed with dev-loop journal and build-timings. The 60.549s wait at stage game_build maps to build attempt game-dll-20261120T170256138061Z-c77c9e (started 17:02:56.138Z, completed 17:04:44.368Z, total 108.23s, success=false, exit 1, error 'codegen check failed: packet_registry_codegen.py'). The wait was a build genuinely in progress, not a stuck stage. Evidence that the 60s budget is below normal DLL build latency: generation 405 reported total_seconds 2134.073 (source scan/parse ~500-360s, compile 1360.694s, link 136.181s); the follow-on attempt game-dll-20261120T171733871137Z-338310 started 17:17:33.874Z and was still emitting translation_unit.started events at 17:24:15 (elapsed_ms ~401s). Therefore fixed-60s wait issues will fire on every non-cached DLL build. Acceptance check: wait_id, stage, reason, elapsed, and build ID are present in the issue; exactly one reviewer wake was queued for this occurrence (wake-1362ef073454). Defect found: recurrence handling is wrong - 12 near-identical 'Wait over budget: game-dll-build at game_build' issues exist (issue-63dccbd8f8b6, 2008e743c710, 6413477d361d, 87f1017235e5, e84cfa28c7f7, 6bc05eadfb7f, dd54a79216ad, 25b14f436a3e, 40dfaa2a803c, b0ba8698acd7, bb273ac1386c, 9a9807fa0128) instead of occurrences on one canonical issue. Recommendation: suppress/annotate the wait issue while the pinned build attempt is actively producing TU events or derive the budget from rolling build duration, and attach repeat occurrences to a single issue. The c77c9e codegen failure is a separate build defect that needs its own triage. No source edits made; bounded review only.
