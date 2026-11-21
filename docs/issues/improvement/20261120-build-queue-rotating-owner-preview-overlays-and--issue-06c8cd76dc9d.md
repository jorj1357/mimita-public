# issue-06c8cd76dc9d: Build queue, rotating owner, preview overlays, and per-agent DLL pointers

## Human report (verbatim, immutable)

> 


## Assignment

### 2026-11-20T03:17:29.117Z — build-runtime-subagents

Owner: build-runtime-subagents

## Context

### 2026-11-20T23:57:31.595Z — build-coordinator

evidence: {'revision_gate': 'requests keyed by source revision; newer pending supersedes older; failure latch', 'dep_cache': 'build/game/objects/<flags>/dependency-index.json', 'prune': 'logs/snapshots/generations before disk check', 'tests': '67 focused tests pass', 'runtime': 'not measured; daemon restart required'}

## User quote

### 2026-11-20T23:57:58.198Z — build-coordinator

Via build-coordinator; supports Implementation scope:

> Build queue, rotating owner, preview overlays, and per-agent DLL pointers

## Context

### 2026-11-20T23:57:58.636Z — build-coordinator

reproduction: Queue three build requests for one revision, save another source revision during the active build, then inspect coalescing and follow-up behavior.
expected_behavior: One active whole-game build per revision; same-revision requests attach; newer pending revision supersedes older; at most one follow-up; failed revision does not auto-retry.
acceptance: Focused tests show one build for multiple same-revision requests, one follow-up for a newer revision, failure latch, transient retry, and dependency-cache reuse; live build timing is recorded.
paths: ['devscripts/dev-loop.py', 'devscripts/game_build_worker.py', 'devscripts/dev_loop/game_build.py', 'devscripts/quality/test_build_coordinator.py', 'devscripts/quality/test_game_build_snapshot.py']
evidence: {'tests': '67 focused tests pass', 'dep_index': 'build/game/objects/<flags>/dependency-index.json', 'prune': 'pre-build logs/snapshots/generations', 'runtime': 'not measured; daemon restart required'}
risk: Revision gating can delay a needed rebuild if its revision key is stale; the explicit retry path must remain available.
proof_expectation: A live queue trace and measured one-TU build demonstrating one active build, coalescing, and dependency parse reuse.
