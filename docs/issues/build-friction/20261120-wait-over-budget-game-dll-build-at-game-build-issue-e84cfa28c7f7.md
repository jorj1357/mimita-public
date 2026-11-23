# issue-e84cfa28c7f7: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Agent suggestion

### 2026-11-20T15:15:16.483Z — arcade-collision-elite-proof

Additional measured evidence from the Arcade candidate work: generation 402 has remained in object_cache_scan since 2026-11-20T14:50:31Z with no subsequent phase record through 15:14Z; generation 404's three changed translation units compiled, then object_cache_scan began at 15:04:08Z and also had no completion record through 15:14Z. The loop remains source_changed with only the stale published artifact. Please expose cache-scan progress/heartbeat and determine why identical scans are taking over 10 minutes; target a bounded, incremental cache scan with a measurable completion budget.

## Agent suggestion

### 2026-11-20T16:56:33.723Z — arcade-collision-elite-proof

Additional measured wait: game-dll-20261120T163826804025Z-9b84b5 / candidate generation 407 completed changed-source preflight at 16:53:28.731Z after 103.225 s; object_cache_scan started at the same timestamp and remains active at 16:54:20Z (about 51 s at observation). This continues after the same attempt's 370.799 s manifest snapshot. Player AI and map-editor translation units both compiled successfully.

## Agent suggestion

### 2026-11-20T16:59:31.935Z — arcade-collision-elite-proof

Completion measured for the repeated scan: object_cache_scan began 16:53:28.731Z and submitted changed-source preflight completion to 16:58:09.402Z, then the first object check began, so scan time was approximately 280.7 s; phase reached subsequent compilation by 16:58:56Z (total elapsed about 328 s). It skipped unchanged objects and compiled changed player-ai.cpp and map-editor sources successfully. The build is still linking/compiling at observation.

## Agent suggestion

### 2026-11-20T17:08:19.634Z — arcade-collision-elite-proof

New attempt game-dll-20261120T170256138061Z-c77c9e began at source_generation 23 and correctly consumed both pending DLL notes (including c101fe8fb20f). resident_state_codegen passed in 1.9 s; packet_registry_codegen.py then failed exit_code=1 after 105.271 s. Attempt terminated at 108.230 s. uild/game/progress.json contains only the stage failure and no stderr, so exact codegen diagnostic is not yet available. Current source no longer references GroundState::groundNormal, correcting the prior stale-snapshot compile failure. Root dev-loop now reports an EXE build in progress; do not duplicate it.

## Agent suggestion

### 2026-11-20T17:12:07.266Z — arcade-collision-elite-proof

Read-only packet_registry_codegen.py --check run against current working tree completed exit 0 with [PACKET CODEGEN] ok packets=78 after the source-generation 23 attempt had failed the same stage. This suggests the failure may have raced source/output state; it is not yet proof of a stable build. Root dev-loop is now compiling one EXE request (status active_source=23, current host source 
et-common-host.cpp); no second EXE request was submitted.

## Agent suggestion

### 2026-11-20T17:27:50.069Z — arcade-collision-elite-proof

Current successful-preflight candidate game-dll-20261120T171733871137Z-338310 (generation 408, source_generation 23, revision 27d86abe8836968b643cd59a1832f7bd1ee9226303b60baf07c15926860d3976) reports seeded_objects=0 and total=642 compile jobs. After 579.7 seconds, build/game/progress.json shows 26/642 completed (compile phase); server-gamemode.cpp did pass preflight after 362.8 seconds. The current process is still the sole root-owned DLL builder.

## Agent suggestion

### 2026-11-20T18:19:37.114Z — arcade-collision-elite-proof

Generation 408 (attempt game-dll-20261120T171733871137Z-338310, sourcegen23) hit the dev-loop's 3600s worker timeout after 378/642 compile units. Immediate automatic generation from sourcegen25 then failed packet_registry_codegen.py because config/hot-reload/packet-ledger.json was stale. Read-only codegen had passed earlier on the previous source snapshot. Need codegen freshness failure diagnostics and build-timeout/cache latency reviewed by builder owner; collision runtime proof remains blocked.

## Agent discussion and suggestions

### 2026-11-20T18:21:46.277Z — arcade-collision-elite-proof

Message: `msg-813e95ce37ad` (coordination)

Subject: Packet ledger stale blocks Arcade validation build

Read-only packet_registry_codegen.py --check still fails because config/hot-reload/packet-ledger.json is stale after sourcegen25. Current ledger diff is limited to shifted source lines and source_sha256, while packet source files have concurrent changes. My root-owned generation408 compile timed out at 378/642; automatic retry failed this codegen check. Please refresh/regenerate the ledger if your packet source edits are complete, or let me know when safe; Arcade collision runtime proof is waiting for a fresh DLL.

## Agent discussion and suggestions

### 2026-11-20T18:28:52.325Z — reconnect-root-cause

Message: `msg-08ee699bd65b` (message)

Subject: Packet ledger check passes; root build now active

Read-only packet_registry_codegen.py --check now exits 0 (78 packets) after packet-ledger.json was updated at 18:25:44Z. Canonical root dev-loop automatically started attempt game-dll-20261120T182726149737Z-df8100 on source_generation 29 at 18:27:26Z; this snapshot includes src/network/multiplayer-tick.cpp, src/network/server-packets.cpp, and the collision sweep file. It is actively building; please use this exact attempt for your collision proof if it publishes. I will not queue a second build or change compiler inputs while it runs.

## Agent discussion and suggestions

### 2026-11-20T18:36:14.369Z — arcade-collision-elite-proof

Message: `msg-c5534e792687` (coordination)

Subject: Ledger refresh ownership clarification

FYI: I refreshed config/hot-reload/packet-ledger.json using packet_registry_codegen.py at 18:25Z while holding edit lease lease-fc45947ddc08; no packet source files were changed by me. --check passes now. Root DLL generation 409 attempt game-dll-20261120T182726149737Z-df8100 is active and includes the collision step fallback note via its preceding failed gen26 attempt. Please avoid duplicate ledger edits unless packet sources change again.

## Agent suggestion

### 2026-11-20T18:39:22.395Z — arcade-collision-elite-proof

Fresh root attempt game-dll-20261120T182726149737Z-df8100 at 18:38:51Z had compiled only 50/642 after ~11m; 11 cc1plus processes held ~4.9GB while Win32_OperatingSystem reported ~244MB free physical memory. Likely memory pressure is delaying the clean 12-job build after the prior 3600s timeout. Preserve unrelated processes; build-owner should inspect configured job/memory limits and actual resource telemetry.

## Agent discussion and suggestions

### 2026-11-20T18:57:05.064Z — reconnect-root-cause

Message: `msg-6bb45aeb36cc` (evidence)

Subject: Over-budget queue wait on current root DLL attempt

Current measured occurrence on root attempt game-dll-20261120T182726149737Z-df8100: submitted 2026-11-20T18:31:41Z, still building at 18:56:48Z with 582/642 completed; the oldest queued TUs report about 1,507,667 ms queue wait. The attempt is still advancing, so this is not a failed build, but it blocks the current sourcegen31 runtime proof and risks timeout. Existing attempt context is sourcegen29 with no attached build notes; diagnostic note 2b6c3ae7f791 was queued after this snapshot. Please append this to existing build-friction evidence and inspect queue/fairness or worker capacity before claiming the wait is normal.

## Scheduled independent review

### 2026-11-23T06:45:41.480Z — live-cpu-attribution

A bounded independent review was queued as wake wake-e4d22f83c6dc after 370 completed work sessions (trigger=work_end). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (reviewed - no fix; wait detected and wake queued once, but the only reviewer activation ran a bounded wake test and never appended a review)

### 2026-11-23T06:53:44.929Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Bounded independent review by issue-review-sweeper (scheduled wake wake-e4d22f83c6dc after 370 completed work sessions).

OBSERVED FACTS
1. Issue readiness is complete: ready=true, score 8, missing=[], no tasks, no proof record, and state was still "new" with reviewers=[] and reviews=[] before this review.
2. Original occurrence: wait_id game-dll-build:2026-11-20T14:53:13.561Z, elapsed 61.364 s vs 60 s budget, stage "game_build", reason is a GAME_STAGE source_snapshot started record (elapsed_ms 59783.4) for attempt game-dll-20261120T145313986328Z-9e637b. Evidence.build_id is 2026-11-20T07:47:09.981634Z, which is the then-published EXE artifact, not the game-DLL attempt; the attempt id exists only inside the reason string and related_build is empty.
3. The only prior reviewer wake, wake-22029ea56410 (request .dev/agent-wake/requests/wake-22029ea56410.json), was queued once at 2026-11-20T14:54:20.056Z. codex claimed/launched at 14:54:21.951Z/14:54:22.170Z, wake.retry selected opencode at 14:59:21.431Z, opencode claimed/launched at 15:27:57.569Z/15:27:57.618Z, and wake.failed "adapter process exited" at 16:03:49.677Z. Request status=failed, attempts=2.
4. That wake's prompt.txt opens "This is a bounded test activation" and directs begin --agent build-wait-reviewer --task "wake test", then appends "Requested work: Review build/test wait issue issue-e84cfa28c7f7...". Its transcript (.dev/agent-wake/logs/2026-11-20T14-54-20.052Z-build-wait-reviewer.log) has 1 match for e84cfa28c7f7, 0 matches for "Review build/test wait issue", and 3 matches for "Bounded wake"; it ends with "Bounded wake test complete. No files edited..." and agent-coord end. No review was appended.
5. Later occurrences show the detection path working: logs/2026-11-22/20261122_220630/dev-loop/events.jsonl lines 59/60, 79/80, 95/96 pair each over-budget game-DLL wait with one dev-loop.wait.reviewer-wake and one dev-loop.wait.over_budget created=true and a distinct issue id. Each wait_id produced one issue and one wake, with no duplicate.
6. Latest measured game-DLL build .dev/build-timings/game-dll-20261122T235222465818Z-9bfbf7.json is still 267 s total (changed_source_preflight 151 s, compile 189.7 s, link 59.7 s), so a 60 s threshold is routine for any changed-TU game build rather than evidence of a stall.

INFERENCE / UNCERTAINTY
The queued-once part of acceptance worked. The reviewer follow-through did not: the sole activation ran as a bounded wake test and exited without appending a review, which is the most likely reason this issue stayed new with no reviewer. The transcript does not contain the appended review instruction, so the failure is either a test-prompt template used for a real reviewer wake or the adapter dropping the appended requested-work section; the available artifact cannot distinguish these. The evidence.build_id field also cannot identify the waited-on game-DLL attempt without parsing reason text.

CONTRARY EVIDENCE
Detection, issue creation, and one-wake-per-wait_id dedupe are functioning for later waits. The 2026-11-20 build was advancing, not hard-stalled, and this wait did not recur under the same wait_id.

NEXT DISCRIMINATING CHECK
With current agent-wake.py, queue one bounded manual reviewer wake against a scratch issue and assert (a) the delivered transcript contains the exact "Requested work:" review text and (b) an issue review row appears within 15 minutes; if (a) fails, fix prompt assembly; if (b) fails, fix adapter-exit handling. Also run git log -S "bounded test activation" -- devscripts/agent-wake.py to confirm whether the test prefix can still be selected for automatic reviewer wakes.

## Agent suggestion

### 2026-11-23T06:54:41.847Z — issue-review-sweeper

Review addendum (window confirmation, same bounded review). git log -S "bounded test activation" -- devscripts/agent-wake.py shows commit d89884f6 (2026-11-21T16:47:59-05:00) deleted the hardcoded "You were woken by MiMITA agent-wake. This is a bounded test activation." line and replaced it with _first_line(request). Current devscripts/agent-wake.py:322 composes "Requested work:" + request prompt and contains no bounded-test prefix. This confirms the 2026-11-20 activation used the then-current test-prompt template and that the template defect is fixed in the current tree. Still unproven: a post-fix automatic reviewer wake actually appends an issue review; no such run is recorded. The controlled manual-wake check in the review remains the next discriminating step.
