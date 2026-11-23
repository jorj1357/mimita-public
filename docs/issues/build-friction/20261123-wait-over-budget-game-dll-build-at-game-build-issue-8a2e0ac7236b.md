# issue-8a2e0ac7236b: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed)

### 2026-11-23T16:39:58.180Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed by build-wait-reviewer from the dev-loop journal and current build state. No source edited; no build/test started.

VERDICT: confirmed over-budget wait (legitimate), but caused by an in-progress compile FAILURE on a heavy TU, not a stuck queue.

EXACT TIMELINE (run 20261123_144429, logs/2026-11-23/20261123_144429/dev-loop/events.jsonl):
- 15:57:37.033 batch sealed, request 2026-11-23T15:57:37.033Z-f2552bcf (source_generation 3); changed game paths: src/network/server-packet-chat.cpp, src/terminal/debug-commands.cpp, src/devtools/terminal.cpp (build notes from mode3-latency-goal: adminadmin dev-unlock).
- 15:57:37.550 wait game-dll-build:2026-11-23T15:57:37.550Z begins; attempt game-dll-20261123T155737827366Z-ad165e (rev 6af8618d, generation 562).
- 15:58:41.050 dev-loop.wait.over_budget: elapsed 60.923s, budget 60.0s -> issue-8a2e0ac7236b created:true; exactly one reviewer wake queued (wake-ada3b5f1f2dc).
- 15:59:12.623 file failed; 15:59:25.756 dev-loop.game-build.error; 15:59:25.988 result status=failed; phase=changed_source_preflight; path=src/terminal/debug-commands.cpp; error "changed-source preflight failed: src/terminal/debug-commands.cpp in 85.397s".
- Compile error text: src/terminal/debug-commands.cpp:583 "no matching function for call to Terminal::registerCommand(<brace-enclosed initializer list>)" inside the new adminadmin registration.

ROOT CAUSE: The single changed TU src/terminal/debug-commands.cpp preflight/compile alone took 85.397s and then failed on a real C++ overload error in the work-in-progress adminadmin command. The wait crossed 60s before the failure surfaced. Not an avoidable queue stall, not a stale binary.

BUDGET CALIBRATION (actionable): WAIT_ISSUE_SECONDS defaults to a flat 60s (dev-loop.py:114-115). The clean rebuild of the same TU in the next attempt (generation 563, attempt game-dll-20261123T160037032706Z-485a05) took compile 36.524s + link 21.293s, worker_total 60.977s, total 69.123s (build/current.json). So even a CORRECT single-file edit to debug-commands.cpp exceeds the 60s budget on the clean path, and any failing preflight (85.4s here) is guaranteed to. The flat budget does not scale with measured changed-path cost.

OBSERVABILITY DEFECT: the over_budget wait record reported generation=561 while the actual in-flight attempt was generation 562 (the 15:57 wait here and the 16:00 wait both reported 561 while the real builds were 562/563). dev-loop.py:3165-3169 sets game_build_status/started_utc/source_generation when the worker starts but not game_build_generation; that field is only assigned on completion (dev-loop.py:3259) yet is read for the wait record at dev-loop.py:3879. During a build it therefore always reports the previous completed generation.

PROOF EXPECTATION: satisfied. One dev-loop.wait.over_budget event, one issue record, and one reviewer wake exist for wait_id game-dll-build:2026-11-23T15:57:37.550Z.

RECOMMENDATION: no duplicate issue (occurrences already auto-filed). Route the flat-budget and stale-generation findings to issue-136d4553ff92 (owner build-coordinator, build stage accounting/end-to-end speed) and issue-e044a931c54b (avoidable waits); these are shared-authority changes and were not made here.
