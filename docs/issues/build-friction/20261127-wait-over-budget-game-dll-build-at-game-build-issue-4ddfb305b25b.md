# issue-4ddfb305b25b: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Context

### 2026-11-27T18:28:33.822Z — build-wait-reviewer

related_build: game-dll-20261127T170504574022Z-3ad5f1 (generation 799)
related_run: 20261127_170216

## Review (confirmed-expected-long-build; evidence-quality defects (reason/build_id/generation) and per-build wait-policy noise; no fix claimed)

### 2026-11-27T18:28:49.671Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Bounded independent review by build-wait-reviewer (wake-503995bbdbf2). Read-only: no source edited, no build started, no runtime controlled. Evidence is source/build/journal, not behavior proof.

OBSERVED - occurrence and wake (acceptance)
- Wait record: wait_id "game-dll-build:2026-11-27T17:05:04.518Z", kind game-dll-build, stage game_build, elapsed 60.832s, budget 60.0s, started_utc 2026-11-27T17:05:04.518Z, source_generation 30, request_ids [build-request-3acdde335999].
- Exactly one issue opened for this wait (agent-coord events.jsonl line 22212, coord.issue.opened).
- Exactly one reviewer wake request queued: wake-503995bbdbf2 -> agent build-wait-reviewer, issue issue-4ddfb305b25b (agent-wake events.jsonl line 3944 wake.queued; line 3945 wake.claimed codex; line 3974 wake.retry opencode; lines 4008-4009 claimed/launched opencode). The opencode launch is the retry/fallback of the same single request, not a second wake.
- No duplicate issue: occurrences [] and _wait_issue() dedupes on wait_id (devscripts/build_friction.py:131-169).

OBSERVED - the underlying build was a real long build, not a hang
- .dev/build-timings/game-dll-20261127T170504574022Z-3ad5f1.json: success=true, generation 799, total_seconds 700.557, compile 628.355, link 43.276, publish 0.498, completed_utc 2026-11-27T17:16:45.138Z.
- build/game/current.json: status ok, generation 799, compile_seconds 628.355, compiled 15, skipped 632. build/game/ready.json: published 2026-11-27T17:16:44.609Z.
- Cause: changed header src/physics/movement/actor-pair-collision.h (fanout 2) plus 12 changed sources -> 15 TUs compiled. src/network/server-gamemode.cpp took 622.768s; top-5 TUs all >300s; queue_wait_max 311.39s, mean 82.707s. postmortem resource: peak host CPU 100.0%, mean 99.24%, min free memory 53.8 MB. These are the sampler's host samples.

OBSERVED - evidence-quality defects in the automatic record
1. reason is not human-readable: it is a 240-char tail of a long build progress line. devscripts/dev-loop.py:3112 sets detail = line.rstrip()[-240:], and check_wait_budget() stores it as reason (dev-loop.py:3893). Here that produced the fragment "30747.545,\"build_attempt_id\":...". A reader cannot tell the build stage/state from it.
2. build_id is unrelated to this wait: 2026-11-27T08:12:40.178185Z is the latest EXE build (.dev/builds/2026-11-27T08-12-40.178185Z), while the game-DLL attempt is game-dll-20261127T170504574022Z-3ad5f1. The attempt id survives only inside the garbled reason string.
3. generation recorded 798 while the attempt completed as generation 799 (self.game_build_generation read at wait time was the prior value).
Acceptance "build/run IDs" was therefore only partially met in structured fields; this review filled related_build and related_run (see context event).

OBSERVED - recurring noise (issue risk confirmed)
- issue list shows 20+ distinct open/in_review "Wait over budget: game-dll-build at game_build" build-friction issues on 2026-11-27 alone. Because wait_id embeds started_utc, every long build yields a new issue even when cause/title are identical. Routine multi-minute incremental DLL builds therefore generate one friction issue per build. This matches the issue's own risk field ("A noisy wait policy could create duplicate friction issues").

EVIDENCE GAP (build resource sampling)
- gap_key build-resource-sampling:game-dll-20261127T170504574022Z-3ad5f1 (brain_check.build_resource_gaps). Unavailable: matched_prebuild_resource_baseline, host_commit_charge_and_limit, client_mode_independent_confirmation.
- Consequence: peak host CPU 100% / 53.8 MB free is a direct observation, but "resource pressure caused the 622s TU" is INFERENCE, not proven, because there is no aligned pre-build baseline, commit charge, or independently confirmed client mode. Do not cite host pressure as cause without those samples.

INFERENCE / UNCERTAINTY
- Likely sequence: header change invalidated 15 TUs; one TU (server-gamemode.cpp, 622s) ran long while the worker pool queued work (queue_wait_max 311s) under ~100% host CPU and an active server (19 NPCs) + client (55.9 fps) session. Confidence in "genuine long build": high. Confidence that host contention is the dominant cause: low/unknown pending the gap samples.

NEXT DISCRIMINATING CHECKS (no fix claimed here)
1. Make wait reason a stable human label (e.g. last [GAME_STAGE] summary) instead of the [-240:] tail; record build_attempt_id and the attempt generation explicitly in wait evidence.
2. Decide wait-policy aggregation: per build attempt/generation or only above a phase-appropriate threshold, to stop one issue per routine long build.
3. Capture the three missing resource fields for the next long build so cause vs. consequence is provable.
Candidate owner: dev-loop/build-friction owner; left unowned here so the next agent can claim with next --claim.

STATUS: not closed; no runtime_proven claim; reviewer did not edit source or start a build.
