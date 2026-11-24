# issue-0e981ef03d32: Desktop inspection bridge repeatedly times out before app inventory

## Human report (verbatim, immutable)

> why its timed out  can u mak ea fix and try again and add a issue/tracker for this like regressiions maybe\ncan u do it liek with a pthon script in mimita like if u have a fail liek that, AI auto is able to make a pthon script to accomplish wahtevr its tryying to do\nall things in miita should be doable by ai  and at least figure out able by AI , can u write that as a issue or direction for AI coolaboration and agents to move toward, as well as over time getting more and more  for AI to do , instead of human , making what used to taek 10 human actions, doable in 1 human action and AI does the rest  , that is goal over time, continuous improvemnt continnous efficiency improvment


## Triage

### 2026-11-21T00:22:01.628Z — root

Summary: Determine external CUA service_info timeout root cause and implement a supported fallback or health-aware recovery for MiMITA inspection workflows; do not claim repository code can fix the external bridge until ownership is identified.
Owner: juggernaut-npc-stuck-shooting
Priority: high

## Agent discussion and suggestions

### 2026-11-21T01:25:41.178Z — root

Message: `msg-3e3f23cff0aa` (evidence)

Subject: Python fallback evidence for desktop bridge timeout issue

Python fallback to Windows APIs successfully enumerated visible MiMITA windows and captured the visible dev-loop client window without the CUA service. Run registry and events identify run 20261120_232200, server PID 33940 online, room 9K5YPND, loopback bind 127.0.0.1:24632; client PID 24476 is marked stale. Python POST to mimita.fun was blocked by HTTP 403 / Cloudflare 1010. This proves a useful local read-only fallback exists, but does not expose the server browser row or public ICE join result. The separate dev-loop control endpoint issue is issue-80dfb9ab9c5c.

## Agent suggestion

### 2026-11-21T05:51:59.964Z — rocket-impact-investigation

Existing supported fallback found (bounded review 2026-11-21T05:4xZ, agent rocket-impact-investigation): devscripts/process-snapshot.py already provides a read-only MiMITA process/build-lock inspection path. Run 'python devscripts/process-snapshot.py'; it wrote .dev/process-snapshots/process_snapshot_20261121_053848.json with 13 MiMITA-related process rows using Toolhelp32Snapshot after the PowerShell CIM query timed out after 30s, and lists lock_owner_pids_found=[30004], lock_owner_pids_not_found=[27796]. This worked while the CUA bridge was not involved, so it is a concrete Python fallback for process/window awareness rather than a new harness. Suggested smallest change: reference it from agent-coordination/agent-self-improvement as the first fallback for process inventory and have it print the scan method and limitation, so agents do not present name/PID-only data as full window/UI state. No source edits made.

## Scheduled independent review

### 2026-11-24T14:40:28.175Z — root-mode3-latency

A bounded independent review was queued as wake wake-037e33aa7481 after 571 completed work sessions (trigger=work_end). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (needs-evidence)

### 2026-11-24T14:50:42.819Z — issue-review-sweeper

Reviewer: issue-review-sweeper

INDEPENDENT BOUNDED REVIEW (issue-review-sweeper, wake wake-037e33aa7481, 2026-11-24T14:4xZ). Read-only: no source edit, build, run, lease, close, owner change, or proof claim.

OBSERVED FACTS
- Readiness/proof: issue readiness ready=true, score 8, state triaged, tasks=0; coordination DB has no task_proofs row and no prior issue_review for issue-0e981ef03d32. The acceptance (bridge timing/health/timeout class + recovery + one reliable fallback + a public JOIN with matching run identity) has no recorded proof; 0 of its clauses are currently evidenced in this issue.
- Bridge evidence on this issue is unchanged from intake: 2026-11-21T00:20Z, cua.getState() reported a computer-use request timeout at service_info before app/window inventory (3 attempts that turn + 2 prior).
- Newest independent CUA observation I found is not on this issue: docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md, review (evidence) 2026-11-23T09:10:42Z by resource-efficiency-goal: cua_repl.getState returned GetCursorPos failed: Access is denied (0x80070005) and the initialized sky window inventory did not expose the consent window, on the secure UAC desktop. docs/changelog/2026-11-24/20261124_051913-live-mimita-thread-cpu-and-wpr-permission-check.md repeats that the CUA inventory could not enumerate a visibly elevated Administrator PowerShell window. So the most recent direct observations show the bridge initialized and enumerated windows, then failed per-call on a protected/elevated surface.
- Supported repo fallback: devscripts/process-snapshot.py (PowerShell CIM Win32_Process, Toolhelp32 fallback; names/PIDs only). Latest artifact .dev/process-snapshots/process_snapshot_20261123_073937.json used CIM successfully (method=PowerShell CIM Win32_Process, no limitation), listing 5 MiMITA/dev-loop candidate processes with PIDs/parents/command lines and lock owner. This contradicts the 2026-11-21 note that CIM timed out after 30s; the process-inventory fallback is at least intermittently reliable.
- Visual fallback is not reliable and is already tracked: issue-130ee745052a (run 20261124_041854) shows agent.screenshot used src/main-systems.cpp:246/251 GetDC(nullptr)+BitBlt at the host window rectangle and wrote an unrelated Rocket League window while reporting success; docs changelog 20261124_0520 records it as NOT IMPLEMENTED and the spec now requires framebuffer capture.
- Durable-evidence gap: issue-130ee745052a states the observation and journal remain, but logs/2026-11-24/20261124_041854/client-01/events.jsonl is absent; logs/2026-11-24 retains runs only from 20261124_053435 onward (041854 was about 04:18Z, issue opened 05:28Z). The .dev/agent-command-20261124_041854-client-01.jsonl request still exists, but the run journal/image do not. The repo prune functions only delete whole dated roots and keep the newest root, so this same-day intra-root disappearance is not explained by them; remover unknown from evidence checked.
- No public JOIN observation, connection journal, or bridge request-timing/health log exists for this issue. Build context gen594 (src/network/server-projectiles.cpp, unrelated) is the only current build.

INFERENCE / UNCERTAINTY
- The 2026-11-23 access-denied result is a different failure mode than the filed service_info timeout and shows the bridge can initialize and inventory windows at least once; it does not prove the service_info timeout is gone (different session/elevation/secure-desktop state).
- Acceptance mixes two owners with different fixability: external CUA service/host-integration health (not repo code) and a supported repo fallback (repo code, partly implemented). No task boundary or owner distinction exists, so this issue can stay triaged indefinitely while evidence accumulates elsewhere.
- No activity from the nominal owner (juggernaut-npc-stuck-shooting) appeared in this activation board/sync; the issue is effectively unowned for progress, not just blocked.

CONTRARY EVIDENCE
- The claim that every attempt times out at service_info is contradicted by the 2026-11-23 initialized-window-inventory observations.
- The Python process fallback worked via CIM on 2026-11-23, so a fallback for process/window awareness exists today, though it cannot supply UI/visual state and must be labeled name/PID-only.
- A screenshot fallback exists mechanically (agent.screenshot completes) but its content is wrong, so fallback exists is not the same as reliable fallback demonstrated.

NEXT DISCRIMINATING CHECK (smallest, bounded)
1. One fresh cua_repl.getState call on the normal (non-secure, non-elevated) desktop while recording wall-clock, phase (service_info vs UI inventory vs per-call permission), and the returned window list; repeat once only if it fails. If the service_info timeout recurs, keep an external-service blocker; if inventory returns, downgrade the timeout claim and record the permission/elevation and hidden-window behavior as the real gap.
2. For the fallback half, pick one path and prove it end to end: either (a) agent.screenshot from a covered-window run with the exact journal lines, image hash, and a copy of the evidence stored outside logs/ before pruning, or (b) process-snapshot.py for process inventory only, explicitly labeled as name/PID-only. Do not present either as public-server proof.
3. If the CUA bridge remains the blocker, split the acceptance: mark the public-JOIN half as externally blocked with a named owner, and keep only the repo fallback half actionable here.

No fix, build, runtime behavior, or public proof is claimed. Reviewer recommends state stays open/in_review with an explicit external-blocker owner for the CUA half.
