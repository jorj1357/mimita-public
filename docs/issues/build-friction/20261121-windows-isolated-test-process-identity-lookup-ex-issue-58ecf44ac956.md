# issue-58ecf44ac956: Windows isolated test process identity lookup exceeds its four-second shell timeout

## Human report (verbatim, immutable)

> A valid identity probe for an existing MiMITA PID returned no result inside the isolated runner because the PowerShell/WMI subprocess has a four-second timeout and took over 25 seconds on this workstation.


## Triage

### 2026-11-21T01:04:19.788Z — fcs-npc-live-fix

Summary: Replace the slow per-process PowerShell/WMI probe with direct Windows process-handle identity retrieval; validate exact executable/start time and pinned live runtime.
Owner: fcs-npc-live-fix
Priority: high

## Agent suggestion

### 2026-11-21T01:24:44.985Z — fcs-npc-live-fix

Pinned live runtime evidence (2026-11-21): isolated runs 20261121_010727 and 20261121_011815 came online with the recorded EXE identity and DLL generation 417. Later dev-loop stop requests returned stop_partial for all four PIDs because process identity did not match the run manifest; no process was force-terminated. Investigate why start validation accepts these processes but cleanup identity rejects them, then prove start and safe stop against a fresh pinned run.

## Scheduled independent review

### 2026-11-24T15:14:51.449Z — root-mode3-latency

A bounded independent review was queued as wake wake-02add8f2f53a after 581 completed work sessions (trigger=sync). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (reviewed - fix present and functionally observed; proof record absent and acceptance pin stale (gen 417))

### 2026-11-24T15:31:53.020Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Independent bounded review (wake-02add8f2f53a, issue-review-sweeper), 2026-11-24.

OBSERVED FACTS
- Source fix is present: devscripts/dev_loop/parallel_test_runs.py:426-474 (_process_identity) now opens the PID with kernel32 OpenProcess and reads QueryFullProcessImageNameW + GetProcessTimes. The old PowerShell/WMI subprocess with a four-second timeout is gone. git log -S on QueryFullProcessImageNameW attributes this to commit d89884f6 (2026-11-21T21:47Z).
- Functional runtime evidence exists in a later production isolated run: logs/2026-11-24/20261124_110111/run.json records distinct process_started_utc for server-01 (PID 34928, 2026-11-24T11:01:11.808Z) and client-01 (PID 24020, 2026-11-24T11:01:25.268Z), both with the exact executable path, build_id, game_generation 590, and game_dll_sha256. Both journals exist and grew (server-01 2,515,887 bytes / 2,852 lines; client-01 976,704 bytes / 744 lines). Server journal shows server.policy.startup and gamemode.match.configured; client journal shows network.client.hello.sent. Run status is stopped with both instances offline.
- The start-vs-stop identity mismatch follow-up (issue suggestion 2026-11-21T01:24Z) was addressed separately by millisecond ISO-timestamp normalization (_same_process_start_time); changelog docs/changelog/2026-11-22/20261122_011811-fix-isolated-test-run-process-identity.md records runs 20261122_011502 and 20261122_012331 starting, joining, and stopping with failures=[].
- No retained symptom in current logs: rg over logs/ for cannot-verify-identity, process-identity-does-not-match, and stop_partial returns zero matches.
- Proof gap: proof list --issue issue-58ecf44ac956 returns [] and task windows-identity-fast-probe-live-proof shows latest_proof=null, state=open, evidence empty. The issue is still state=triaged with owner fcs-npc-live-fix.
- Linked artifacts are gone: the reproduction/paths cite logs/2026-11-21/20261121_001236 and _002119 (and reviews cited 010727/011815); logs/2026-11-21 does not exist and logs/11-21-2026 is empty. The original failing journals cannot be re-inspected.

INFERENCE / UNCERTAINTY
- The mechanical defect (WMI subprocess exceeding its four-second timeout) is fixed and the direct-handle path is demonstrably exercised by a real isolated run, so the behavior is functionally observed at generation 590. Confidence is high for the identity path, from the manifest fields and growing journals.
- The recorded acceptance/proof expectation is pinned to generation 417 and a specific DLL SHA that is no longer current and not shown to still exist; the literal gen-417 proof cannot be certified from artifacts at hand. The gen-590 run proves the mechanism but not that exact pinned generation.
- Because no proof record exists for the task, this issue cannot be marked resolved under proof-first policy even though the code is fixed; the remaining gap is bookkeeping/method, not a suspected live defect.

CONTRARY EVIDENCE
- No changelog explicitly names issue-58ecf44ac956 as the fix target; the nearby 20261122_011811 changelog is attributed to issue-cf9cd77eb056 and covers only the stop-path precision change, not the start-path fast probe.
- The gen-590 run is not an FCS npc-navigation run at the reproduction profile/launch-mode, so it does not exactly replicate the original repro command.

NEXT DISCRIMINATING CHECK
- Have the owner register one proof record for task windows-identity-fast-probe-live-proof against a current pinned isolated run manifest (a fresh FCS npc-navigation run at the current published generation is sufficient), asserting: server+client process_started_utc and executable present, readiness/active mode reached, and both events.jsonl growing. Also update the stale generation-417 proof_expectation/acceptance pin to the generation actually proven and record the exact run.json path. That single record closes the only remaining gap between fixed-and-functionally-observed and issue proof.

## Agent discussion and suggestions

### 2026-11-24T15:33:43.121Z — issue-review-sweeper

Message: `msg-423faa54dde2` (question)

Subject: issue-58ecf44ac956: register fast-probe proof record / refresh gen-417 pin

Independent review of issue-58ecf44ac956 added (review 365). Code fix is present and a real isolated run (logs/2026-11-24/20261124_110111) already shows server+client process_started_utc and growing journals at gen 590. The only remaining gap is that task windows-identity-fast-probe-live-proof has no proof record and the acceptance is still pinned to gen 417. Can you register one proof record against a current pinned isolated run manifest and update the stale gen-417 pin? No fix claimed or closed in this review.
