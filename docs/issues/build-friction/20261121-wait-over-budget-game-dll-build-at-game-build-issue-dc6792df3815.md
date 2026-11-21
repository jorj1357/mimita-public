# issue-dc6792df3815: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-build-wait)

### 2026-11-21T17:06:39.526Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed by build-wait-reviewer in a bounded wake activation (wake-d96e69d7481f, coordination session session-77349442f307). Read-only: no source edits, no builds, no leases.

Issue completeness: readiness ready=true, missing=[], all required fields present (wait_id, stage, reason, elapsed, build/run IDs, reproduction, expected behavior, acceptance, paths, risk, proof expectation, evidence). No clarification needed.

Occurrence is VALID (not a false positive):
- Wait game-dll-build:2026-11-21T16:25:06.256Z started 16:25:06.256Z, budget 60s, elapsed 60.858s.
- Trigger request 2026-11-21T16:25:06.049Z-0c7b1ca9 accepted 16:25:06.366Z; active attempt game-dll-20261121T162506366821Z-0079b9. This wait record DOES carry an active attempt id and the attempt was progressing (contrast issue-4c12dde1d7ba, whose 16:23 review found a false-positive wait lacking an active attempt id).
- dev-loop.wait.over_budget and dev-loop.wait.reviewer-wake fired at 16:26:21.564-565Z; the issue was created exactly once for this wait_id.

First divergence / dominant cause = pre-compile stage, not game compile:
- codegen:resident_state_codegen.py 16:25:06.437 -> 16:25:07.965 (1.53s)
- codegen:packet_registry_codegen.py 16:25:08.631 -> 16:26:32.950 = 84.328s duration. This single stage exceeds the 60s budget by itself.
- manifest_snapshot 6.50s, manifest_validation 0.015s, source_snapshot 7.25s.
- First translation unit (src/actor/player-ai.cpp) did not start until 16:26:51.348Z, ~105.0s after the request; compile/link began only after the wait had already fired.
- The wait reason embeds the packet_registry STARTED event whose elapsed_ms=2266 is that stage's offset from build start, not its 84s runtime. A reviewer reading only reason under-attributes the cause. Recommendation: the wait snapshot should name the currently-running stage and its own elapsed/duration (or the dominant completed stage), separately from game_build's start offset.

Build outcome is separate evidence, not the wait: attempt 0079b9 completed 16:30:45Z success=false, failed_phase=link (worker "link failed in 33.688s", total 338.688s), with undefined references to MimitaNet::resolveServerContact, MimitaNet::queueServerDamageConfirmedEvent, and MimitaNet::sendReloadRequestForWeapon. Those symbols ARE defined in the current source tree (src/network/server-contact.cpp:342, src/network/server-damage.cpp:564, src/network/weapon-runtime-reconciliation.cpp:193), so this is not a missing source definition; it indicates the link ran without the defining objects (skipped/cached TU whose object was not included). The linker also names a cached object with debug paths from the prior snapshot .dev/build-snapshots/game-dll-20261121T155427526457Z-6240d5/source/src/terminal/weapon-commands.cpp while compiling snapshot ...162506...-0079b9, and the prior attempt 6240d5 (15:54:27 -> 16:07:57) also failed at link. This is a distinct object-cache/scan correctness signature, already partly captured by the link evidence on issue-4c12dde1d7ba; recommend attaching it to issue-136d4553ff92 (build-stage accounting) and the existing link-failure evidence rather than opening another wait duplicate.

Recurrence: same "game-dll-build at game_build over budget" signature is recorded many times (e.g., issue-84ec2bf27598, issue-e84cfa28c7f7, issue-c2cebb751fb4, issue-9a9807fa0128, issue-63dccbd8f8b6, issue-2008e743c710, issue-450cfc80a0b9, issue-9aeec1fbae9d, issue-9215d710021d). Recorded here as the current occurrence; no duplicate issue created.

Recommendations (no fix claimed, no code changes):
1. Keep the 60s budget; this wait is valid. Split wait accounting by named stage so a >60s codegen/snapshot interval reports the running stage and its own elapsed.
2. Investigate why packet_registry_codegen.py took 84.3s for packets=78 (I/O, redundant work, or serialized dependency); it is now the dominant pre-compile latency and blocks the first compile.
3. Track the link failure separately: verify the object cache/scan includes every defining TU and that source changes invalidate dependents; add an explicit check that skipped objects still exist and match their source content hash.

Evidence: logs/2026-11-21/20261121_144350/dev-loop/events.jsonl (dev-loop.game-build.stage rows 16:25:06-16:26:47, dev-loop.wait.over_budget, dev-loop.game-build.failed at 16:30:45); .dev/build-timings/game-dll-20261121T162506366821Z-0079b9.json/.jsonl; docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-dc6792df3815.md.

Verdict: confirmed-build-wait. Evidence review only; no behavior proof claimed. Suggest state in_review; no owner assigned here.
