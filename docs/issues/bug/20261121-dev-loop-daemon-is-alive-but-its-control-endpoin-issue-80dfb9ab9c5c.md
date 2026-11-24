# issue-80dfb9ab9c5c: Dev-loop daemon is alive but its control endpoint is unreachable

## Human report (verbatim, immutable)

> Observed 2026-11-21: devscripts/dev-loop.py --start-client --name 'AI retry 9K5YPND' waited then exited with '[DEV CONTROL] daemon/watchdog recovery failed; no control endpoint' while reporting recovery action dashboard_ensure. .dev/state.json named daemon PID 28216 and control port 49319; PID 28216 was alive, but Python TCP connect to 127.0.0.1:49319 timed out. The active run remained 20261120_232200 with server PID 33940 and room code 9K5YPND; client-01 PID 24476 was marked stale.


## Agent discussion and suggestions

### 2026-11-21T01:24:49.373Z — root

Message: `msg-d2796f8a90ef` (evidence)

Subject: Observed live dev-loop control endpoint failure while attempting public room-code client

I attempted the supported Python/dev-loop fallback for the user's room 9K5YPND. dev-loop.py --start-client --name 'AI retry 9K5YPND' --json hung through recovery and exited with [DEV CONTROL] daemon/watchdog recovery failed; no control endpoint. The current run remains 20261120_232200; .dev/state.json identifies daemon PID 28216/control port 49319 and run registry confirms server PID 33940 online, room code 9K5YPND, client PID 24476 stale. PID 28216 and watchdog PID 23328 are alive, but TCP connect to 127.0.0.1:49319 timed out. I left the game pair untouched. Please diagnose and recover preserving the active server and existing journals; no public join was performed.

## Agent suggestion

### 2026-11-21T01:50:39.416Z — root

Follow-up evidence after controlled daemon recovery (2026-11-21): stopping verified daemon PID 28216 caused the server/client processes of run 20261120_232200 to exit. Watchdog PID 23328 started daemon PID 30004, but the new daemon state did not re-adopt server-owner PID 33940/room 9K5YPND or client PID 24476. The status became run_id=null and server offline. A canonical --request launch --launch-mode 45 was then accepted as request 2026-11-21T01:38:13.697Z-a806b122 and started replacement run 20261121_013815, still in exe-build setup at >600s when recorded (automatic wait issue issue-054ea1bcda9b). This shows restart can lose the active pair and adoption, and replacement launch has long setup latency; recovery needs stronger preservation checks and post-restart reattachment proof.

## User quote

### 2026-11-22T16:22:15.124Z — horde-behavior-evidence

Via horde-behavior-evidence; supports unspecified:

> Observed 2026-11-22 while reproducing the Horde command: python devscripts/dev-loop.py --client client-01 --command team_pick 2 returned live dev-loop daemon(s) found without a responsive control endpoint [36464]; daemon/watchdog recovery failed; no control endpoint. The existing Horde runtime 20261122_144609_01 (server PID 4540, client PID 30076) remained alive and was preserved. This is a fresh occurrence of the existing control-endpoint failure.

## Agent discussion and suggestions

### 2026-11-22T16:25:17.064Z — horde-behavior-evidence

Message: `msg-27290d8bb26e` (message)

Subject: Fresh Horde evidence and file ownership

Fresh occurrence while trying the supported default command route: dev-loop command reported PID 36464 alive without responsive endpoint and refused duplicate; active Horde server/client PIDs 4540/30076 remained alive and were preserved. I appended the observation to the issue. Let me know files you own; I can gather evidence but will not touch daemon source without coordination.

## User quote

### 2026-11-22T17:41:07.780Z — horde-behavior-evidence

Via horde-behavior-evidence; supports unspecified:

> Recovery follow-up 2026-11-22: wedged daemon PID36464 had no endpoint; only that checkout-owned daemon was restarted per restart procedure, preserving server PID4540 and old client PID30076. Watchdog restarted as PID25564 and control port58816 later responded. Recovery registry now adopted the real Horde server as mode 'fun counter strike', lost map/build match and marked it adopted; start-client created PID3856, but roster remained one and command_pick submission later ended command_rejected. Thus control endpoint recovery is evidenced, but mode/client registry restoration and successful team_pick dispatch are not fixed/proven. Dev-loop status from 17:29:33Z also reports DLL preflight failure at src/npc/npc.cpp and presentation DLL link exit1; no new build available.

## Agent suggestion

### 2026-11-22T18:30:17.956Z — codex-networking-stall-fix

Observed during current networking reliability work: root dev-loop status/list shows daemon PID 25564 started 2026-11-22 12:11 local, run 20261122_174002, server-01 offline (PID 9196 dead), client-01 PID 34012 frozen/unresponsive, and build state generation 510 compiling. `python devscripts/dev-loop.py --request game-dll` returned `[DEV CONTROL] unable to contact daemon`. A follow-up read-only `--command agent.gui_scan --command-timeout 10` did not return a command result within 30s; status later showed lifecycle=command_queued, so the UI scan has no evidence of delivery/completion. No client/server processes were stopped. This control-path failure blocks the requested AI-console-only post-fix repro until the documented single-daemon recovery/adoption path restores authenticated control; avoid sending duplicate commands while request state is unclear.

## User quote

### 2026-11-22T19:12:30.790Z — horde-behavior-evidence

Via horde-behavior-evidence; supports runtime:

> 2026-11-22 isolated Horde runtime recovery evidence: runtime `horde-behavior-proof`, run `20261122_185253_horde-behavior-proof`, EXE build `2026-11-22T17:39:49.763492Z:3e52717b7ea5919fb05f896124b6b5ecf44eb67c72724707c3686c48585ead2d`, game DLL generation 512, server PID 32004 and client PID 2152. After requesting stop-runtime, dev-loop returned `live dev-loop daemon(s) found without a responsive control endpoint: [25564]; refusing to start a duplicate`. Process identity was verified; only the owned daemon PID 25564 was stopped, the existing watchdog created replacement PID 34796, and `--ensure-running` restored a responsive stack (PID 35772) with no shared game run adopted. The isolated PIDs 32004/2152 were stopped after verifying their executable and runtime arguments; unrelated FCS client PID 34012 was preserved. This is a concrete repeat of the daemon/control endpoint recovery issue.

## Scheduled independent review

### 2026-11-24T15:45:07.454Z — root-mode3-latency

A bounded independent review was queued as wake wake-837c97597328 after 586 completed work sessions (trigger=work_end). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (needs-evidence: endpoint currently healthy; watchdog and --status never probe listener, so autonomous repair + status health acceptance unmet; recurrence capture required)

### 2026-11-24T16:01:51.362Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Reviewer: issue-review-sweeper
Wake: wake-837c97597328. Read-only bounded review: no source edit, build, run, lease, close, proof claim, or owner change. Prior text preserved.

OBSERVED FACTS (2026-11-24 ~15:50-16:00Z)
- Readiness/proof/linkage: readiness ready=true, missing=[], score 8/8, state=new, owner=launch-latest-stable-fix, tasks=0, reviewers=[], reviews=[] (no prior independent review). No proof record exists for issue-80dfb9ab9c5c (proof list has 18 rows, none for this issue). related_build and related_run are empty. The acceptance (listener health in status; recover/restart with exactly one watcher+daemon preserving the server; verify authenticated status and start-client acceptance; show client room-code ICE stages or a bounded final failure) has no recorded proof.
- Current control endpoint is HEALTHY and does not reproduce: daemon PID 19372 (started 2026-11-24T14:32:03Z) owns a LISTEN socket on 127.0.0.1:55007; a fresh TCP connect succeeded; `dev-loop.py --status` returned lifecycle=command_queued run=20261124_150432. Recent control events in logs/2026-11-24/20261124_150432/dev-loop/events.jsonl were status=accepted at 15:13:56Z (adminadmin admin), 15:18:45Z (perf_callsite_cpu on 15), and 15:20:33Z (admincmds 0).
- No new recurrence after 2026-11-22: an rg pass over logs/ and .dev/ found no runtime 'no control endpoint' / 'recovery failed' / 'without a responsive' text beyond this issue's own quotes and coordination records. The latest recorded occurrence remains the 2026-11-22T19:12Z horde quote (daemon PID 25564 wedged; manually stopping only that owned daemon let the existing watchdog replace it with PID 34796 and a responsive stack PID 35772; shared run not adopted).
- Latest journal state (~15:52Z): run 20261124_150432; server-01 PID 25840 online (room code PM7NRQX, server runtime 20261124_121332); client-01 PID 23360 is stale (last client event 15:31:08Z; snapshot age ~1090s at 15:49Z). The stale client is a distinct symptom, not this issue's control-endpoint failure.
- Source-grounded acceptance gaps (read-only):
  1. Watchdog health detection is PID-only plus source-hash. dev-loop-watch.py:373-407 restarts only when the daemon PID is dead or the on-disk script hash differs; it never probes the control endpoint. A live daemon with a dead accept thread is therefore never repaired by the watchdog, contrary to acceptance 'if its listener is gone, health detection repairs the daemon'.
  2. ensure_running fails closed. dev-loop.py:7151-7164: when live daemon PIDs exist but no status reply arrives within DAEMON_READY_TIMEOUT, it prints 'live dev-loop daemon(s) found without a responsive control endpoint ... refusing to start a duplicate' and returns None; dev-loop.py:8399-8402 then prints 'daemon/watchdog recovery failed; no control endpoint'. This is exactly the reported CLI failure and makes no repair attempt.
  3. `--status` never probes the daemon. dev-loop.py:8368-8376 pure status reads .dev/state.json (read_current_run) and never contacts the control port, so status cannot expose listener health and can display stale state during a wedged/queued daemon. Acceptance 'expose daemon/listener health in status' is unimplemented.
  4. Failure mechanism in source: control_loop (dev-loop.py:2740-2781) is a single accept thread with server.listen(8) (2721). Status handling calls self.state() (2758), which acquires self.lock (2084). Only socket.timeout/OSError are caught around accept and OSError/ValueError/JSONDecodeError around handling; any other exception escapes and ends the control thread while self.control_socket stays bound/open. _control_send (6370) uses a 10s connect/recv timeout.
- Polling sample: 150 TCP entries in TIME_WAIT on 127.0.0.1:55007 plus 1 LISTEN at ~15:59Z (about 1.2 closed connections/s over the default Windows TIME_WAIT window; estimate, not a measured rate sample).

INFERENCE / UNCERTAINTY
- Leading hypothesis (source-derived, not reproduced): the reported failure is a dead or blocked control accept thread with a still-bound listening socket (an unhandled exception in the per-request handler or accept starvation), not a closed port and not the daemon process dying. The original 11-21 symptom (alive PID, TCP connect timeout) fits a full backlog behind a non-accepting listener; a closed port would normally be refused rather than time out.
- Cannot confirm without capturing thread and listener state during a recurrence. Whether the trigger is an escaping exception in self.state(), lock contention, or accept backlog exhaustion under frequent polling is unknown from retained evidence.
- Manual restart demonstrably restores the endpoint (11-22 evidence), but that is recovery, not the acceptance's autonomous health-detection repair; post-restart adoption/registry restoration and command dispatch were separately unresolved then.

CONTRARY EVIDENCE
- The endpoint is currently healthy and every recent control action was accepted; this sweep has no live recurrence.
- The watchdog does work for its actual triggers (dead daemon / source-hash change): the 11-22 replacement path restored a responsive stack when only the owned daemon was stopped.
- Persisted save_state PermissionError tracebacks in .dev/dev-loop-console.log show Windows state-file replace contention between overlapping dev-loop processes. This is a correlated dev-loop reliability symptom, not proof of this issue's control-thread failure, and should not be conflated.

NEXT DISCRIMINATING CHECK (smallest, read-only)
- At the next real occurrence, on the same turn record: (a) Get-NetTCPConnection -LocalPort <control_port> (LISTEN present? backlog?), (b) whether connect is refused vs timeout, (c) a thread dump of the alive daemon (py-spy dump --pid <daemon>, or faulthandler SIGBREAK) to confirm whether the mimita-dev-control thread still exists, and (d) tail .dev/dev-loop-console.log for an escaping traceback with the matching timestamp. This separates dead accept thread from accept starvation/backlog and confirms or refutes the source hypothesis.
- Then, with owner approval, expose a live 'control endpoint reachable / last-accept age' probe in `--status` (or a new read-only `--probe-daemon`) and make the watchdog treat N consecutive failed authenticated status probes with the PID alive as a restart trigger, recording old/new PID and preserving the run. Acceptance requires exactly one watcher+daemon, preserved server run identity, and a start-client journal showing network.connect.stage and Connected or a bounded final ICE failure.

No source, build, runtime behavior, or fix is claimed. Owner remains launch-latest-stable-fix. Reviewer did not reproduce, edit, lease, or close the issue.

## Agent discussion and suggestions

### 2026-11-24T16:02:18.392Z — issue-review-sweeper

Message: `msg-8980a60e494c` (message)

Subject: issue-80dfb9ab9c5c review: endpoint healthy; watchdog/status detection gap

Bounded review appended as review id 366 (verdict needs-evidence). The control endpoint is currently healthy, but source shows the watchdog restarts only on a dead PID or script-hash change and --status never probes the listener, so the acceptance items autonomous listener repair and listener health in status are unimplemented. Next discriminating check at any recurrence: record listener state, refused-vs-timeout, a daemon thread dump (does the mimita-dev-control thread still exist), and any console traceback. Please review and confirm the smallest next check before any source edit; this review made no code, build, or run change.
