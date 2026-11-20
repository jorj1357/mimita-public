# issue-dd54a79216ad: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-benign-wait)

### 2026-11-20T15:45:31.123Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed the over-budget wait occurrence. Timeline from .dev/build-timings/game-dll-20261120T152655407656Z-5e6e30.jsonl:
- build.started 15:26:55.412Z (source_generation 9, request_ids auto-4..auto-9).
- codegen:packet_registry_codegen.py 15:26:56.611Z -> 15:27:50.496Z, duration 53.88s (dominant pre-wait stage).
- manifest_snapshot started 15:27:50.657Z at elapsed 55.25s; the 60s budget was crossed during this stage, which completed normally at 15:28:18.811Z (28.13s).
- No stall/crash: every stage completed exit_code 0; compile continued on 4 changed translation units (generation 405) after the wait.
Wake path verified in .dev/agent-wake/events.jsonl: exactly one wake.queued / wake.claimed / wake.launched for wake-25a134e3a30a (issue-dd54a79216ad), codex adapter, with one documented wake.retry to opencode fallback. No duplicate issue created.
Issue readiness is complete (score 8/8; reproduction, expected behavior, acceptance, paths, proof expectation all present), so no fields needed filling.
Conclusion: the automatic wait issue and single reviewer wake behaved as specified; this occurrence is a real one-minute-plus build, not a hang. No wait-policy code fix required. Follow-up latency observation only (do not create a duplicate): packet_registry_codegen took 53.9s and changed_source_preflight compiled 4 TUs over ~429s on this host; that is build throughput, already tracked by issue-e044a931c54b (Waiting is the enemy). Paths devscripts/dev-loop.py, devscripts/build_friction.py, devscripts/agent-wake.py unchanged.
