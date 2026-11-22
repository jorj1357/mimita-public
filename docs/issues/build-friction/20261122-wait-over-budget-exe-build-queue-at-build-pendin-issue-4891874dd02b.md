# issue-4891874dd02b: Wait over budget: exe-build-queue at build_pending

## Human report (verbatim, immutable)

> exe-build-queue wait at build_pending exceeded the wait budget


## Context

### 2026-11-22T02:12:24.888Z — build-wait-reviewer

related_build: 2026-11-21T02:26:50.306117Z
related_run: 20261122_011031

## Review (reviewed)

### 2026-11-22T02:14:28.847Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Independent build-wait review (build-wait-reviewer, read-only). Verified against the exact run journal; no source edits, no build, no second loop.

OBSERVED FACTS
- Wait occurrence confirmed in logs/2026-11-22/20261122_011031/dev-loop/events.jsonl:
  - line 106 dev-loop.wait.reviewer-wake, wait_id exe-build-queue:42402.296, issue issue-4891874dd02b, wake queued=true, adapter=codex, wall_time 2026-11-22T01:35:05.885Z.
  - line 107 dev-loop.wait.over_budget, kind exe-build-queue, stage build_pending, elapsed 60.922s vs budget 60.0s, reason "idle; waiting for a source change or explicit build/launch request", build_id 2026-11-21T02:26:50.306117Z, generation 6, source_generation 6, request_ids ["2026-11-22T01:33:57.208Z-bc836913"], created=true, wall_time 2026-11-22T01:35:05.886Z.
- Dedupe held for this wait_id: exactly one over_budget event, one issue, and one reviewer wake. Only this wait_id's two events carry it.
- related_build and related_run were empty and the evidence carried no run id; this review filled related_build=2026-11-21T02:26:50.306117Z and related_run=20261122_011031 via issue context.

FIRST DIVERGENCE (root cause of the wait)
- The EXE build was requested, not idle. Control event line 104 records action=build, request_id=2026-11-22T01:33:57.208Z-bc836913, requested_by=cpu-build-efficiency, status=accepted, wall_time 2026-11-22T01:33:58.643Z. queue_build_request (devscripts/dev-loop.py:2726-2750) added the request and set build_pending=True.
- start_build_worker (devscripts/dev-loop.py:3658-3663) returns immediately while self.game_build_worker is not None. A game-DLL build worker was active continuously across this window: attempts game-dll-20261122T012524059314Z-64e10a, -6abdff, then -d09614 (stage events from ~01:25:39 to compile start 01:42:41), and generation 440 published at 2026-11-22T01:47:28.618Z (ready.json).
- The queued EXE build only reached stage setup at 2026-11-22T01:47:30.748Z (event 155, exe-build:6, started_utc 2026-11-22T01:47:30.748Z), i.e. ~13.5 minutes after the accepted request and ~2s after the game-DLL generation 440 publication. This is EXE-build starvation behind the game-DLL worker, not an idle queue.

DIAGNOSTIC DEFECT
- The reported reason is the constructor default "idle; waiting for a source change or explicit build/launch request" (devscripts/dev-loop.py:1389-1391). queue_build_request sets build_pending but never updates last_message or active_build_detail, and the game_build_worker guard returns without recording the blocker. The wait reason therefore mislabels an actively-requested EXE build as idle and hides the real blocker (an in-flight game-DLL build).

ACCEPTANCE ASSESSMENT
- Required wait fields, evidence, one issue, and exactly one reviewer wake are present. Build id is present; the run id was missing from the record and is now linked. Issue mechanics pass.
- The underlying 13.5-minute EXE-build starvation is a separate build-friction/code defect and is not fixed by this review. It recurs across many duplicate "exe-build-queue at build_pending" issues in the brain. I added this as an occurrence here rather than opening another duplicate.

SUGGESTED FIX (needs human approval; claim separately, do not treat as done)
- In start_build_worker, when game_build_worker is active, set active_build_detail/last_message to state the queued EXE request id(s) and the blocking game-DLL attempt id, so the wait reason is truthful.
- Decide policy per docs/specs/dev-loop/agent-lanes-build-authority.md: "One EXE build runs at a time" constrains EXE builds, while game-DLL builds may run concurrently within CPU/memory limits. Either allow a queued EXE build to proceed alongside the game-DLL worker within measured limits, or explicitly document and surface the mutual exclusion so the wait is understood as policy, not idle.
- Carry run_id into the wait evidence so every over-budget wait issue satisfies the build/run-id acceptance directly.

UNCERTAINTY
- I did not measure whether concurrent EXE + game-DLL builds would exceed CPU/memory limits or contend on the object cache; that decision needs a measured trial. No source, build, or run was changed here.

## Agent suggestion

### 2026-11-22T02:15:04.026Z — build-wait-reviewer

﻿Fix proposal (separate claim, human approval): EXE-build starvation + misleading queue reason. Evidence in review id 90 on this issue. start_build_worker (devscripts/dev-loop.py:3658-3663) returns while a game-DLL worker is active and records no blocker, so request 2026-11-22T01:33:57.208Z-bc836913 waited ~13.5 min and the reason stayed the constructor default "idle; waiting for a source change or explicit build/launch request" (dev-loop.py:1389-1391). Smallest safe fix: when game_build_worker is active, set active_build_detail/last_message to the queued EXE request id(s) and blocking game-DLL attempt id. Then decide, per docs/specs/dev-loop/agent-lanes-build-authority.md, whether a queued EXE build may run alongside the game-DLL worker within measured CPU/memory limits or stays mutually exclusive; document the choice. Also carry run_id into the wait evidence. No source changed by the reviewer.
