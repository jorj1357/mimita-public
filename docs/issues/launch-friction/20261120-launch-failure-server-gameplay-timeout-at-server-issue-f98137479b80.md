# issue-f98137479b80: Launch failure: server_gameplay_timeout at server_gameplay_readiness

## Human report (verbatim, immutable)

> server did not become locally playable before the readiness timeout


## Scheduled independent review

### 2026-11-24T07:15:13.198Z — hot-reload-latency-root

A bounded independent review was queued as wake wake-a204a728ec13 after 529 completed work sessions (trigger=sync). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (confirmed-60s-readiness-timeout; original-journal-pruned; same funworld3/mode1 path now passes at 57.1s vs 60s gate; acceptance-not-met; no-fix-claimed)

### 2026-11-24T07:37:01.421Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Bounded independent review (evidence-only; no source edit, build, close, or fix claimed).

READINESS / PROOF
- issue readiness: ready=true, missing=[], possible=8, tasks=0; issue state was new with no owner.
- proof status: no task/proof record is linked to issue-f98137479b80, so nothing here is runtime-proven.
- Linked artifact: .dev/launch-reports/2026-11-20T23-20-14.122Z/{launch.json,report.md,stages.jsonl} still exists and is internally consistent.

OBSERVED
1. The report's first failed stage is server_gameplay_readiness. Ordered stages: request_accepted accepted; artifact selected 2026-11-20T20:20:51.788061Z:64cd9304...; artifact validation ok; launch_configuration map=funworld3, launch_mode=1, mode=current; firewall LOOPBACK_ONLY; server_process_start pid=11912; server_logger_readiness ready (stage_elapsed_ms=338.3, cumulative 2702.0); server_gameplay_readiness failed with cumulative elapsed_ms=62746.8 and timeout_seconds=60; complete failed. Delta logger-ready -> gameplay-failure = 60044.8 ms, i.e. the fixed 60 s readiness wait expired.
2. The pinned journals are gone. The report/stage names logs/2026-11-20/20261120_232014/dev-loop/events.jsonl; logs/2026-11-20 does not exist, logs/11-20-2026 exists but contains 0 entries, and a recursive search found no 20261120_232014 directory. The original first divergence inside the server cannot be reconstructed from this issue.
3. The failure is not a deterministic build defect. The same build (2026-11-20T20:20:51.788061Z:64cd9304...) succeeded on launch_mode=1/current on dust2cyberiav4 at 2026-11-20T22:16:34.875Z and 2026-11-20T22:40:06.789Z. Within ~20 min after the failure the path also produced client_logger_timeout (23:02:14) and render_init_failed (23:22:00), which fits a transient host/session condition more than one broken binary.
4. The exact failing launch path now passes, but narrowly. Launch 2026-11-24T05:25:52.119Z used launch_mode=1, map=funworld3, mode=current on build 2026-11-24T03:30:52.372066Z:3ae36c93... and succeeded. server_logger_readiness cumulative 12949.3 ms; server_gameplay_readiness cumulative 70082.7 ms, so the true logger->gameplay delta is 57133.4 ms, only ~2.9 s under the 60 s gate. (Note: stage_elapsed_ms is recorded as cumulative here, not a per-stage delta, matching the measurement defect tracked in issue-7d3dcfd7433b.)
5. Related trace in issue-c8ac61cf07fc (2026-11-22 run 20261122_224628/gen541) shows local-gameplay-ready is gated after world_load, measured 24,563 ms there, with an NPC Recast prewarm bake of 11,700 ms inside NPC collision build; total world_load 24,563 ms. That is the same gate this issue timed out on, though on a different build/map.

INFERENCE / UNCERTAINTY
- Most likely class: funworld3 startup reached or exceeded the fixed 60 s local-gameplay-ready gate on 2026-11-20, not a crash (the server process was alive at logger readiness and the harness stopped it after the timeout). Confidence: moderate.
- The specific phase that exceeded 60 s on 2026-11-20 is unknown: no journal survives. world_load/NPC-collision prewarm is the strongest candidate by analogy (item 5) and by the 11-24 funworld3 57.1 s near-miss (item 4), but I cannot prove it for this occurrence.
- Contrary signal: the same build passed the same mode on dust2cyberiav4 twice immediately before the failure, so map/world-load scale or momentary host load is a better explanation than the build being globally broken.
- Top-level launch.json records generation=0 even though the artifact_selection stage recorded source_generation=38 and a non-empty build_id; report identity is therefore incomplete even where the report survives.

NEXT DISCRIMINATING CHECK
- With a pinned EXE/DLL and no build running, launch the exact same path (launch_mode=1, map=funworld3, mode=current) 2-3 times through the root dev-loop and read each server journal for wall timestamps of logger.started, server.startup.stage world_load start/complete, npc.navmesh.bake-success build_ms, npc.collision-world.phase started/complete, and network.server.local-gameplay-ready. Compute logger->local-ready deltas and the dominant phase. If the delta approaches/exceeds 60 s, the fixed gate is the cause and issue-c8ac61cf07fc owns the fix; if one phase never completes, that phase is the first divergence.
- Same run should also confirm the reporter gap: require run_id, server/client PIDs, exact journal paths, and per-stage delta in the launch report before accepting the original acceptance ("corrected build passes the same launch path").

VERDICT: occurrence confirmed as a real 60 s readiness timeout; original journal unrecoverable; the exact path now passes with ~2.9 s margin; acceptance is not yet satisfied for the failed build/path and no proof exists. Keep open with the run above as the next check.
