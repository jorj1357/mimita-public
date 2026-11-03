Time created: 2026-11-03T18:52:13Z
Time last updated: 2026-11-03T18:52:13Z

Purpose:
Identify the recurring live freeze/1 FPS, restore journal evidence, and leave
bounded server tick diagnostics. This session also attempted the NPC template
body alignment fix; the NPC wall containment scenario remains red.

## What changed

- `src/debug/structured-log.cpp`: added a 5 second per-thread cooldown around
  the folder-quota prune path. When the log tree is over its ceiling and no
  eligible run can be deleted, writes now drop quickly instead of rescanning
  the whole tree on every record.
- `config/debuglogger.json`: raised `storage_limits.total_log_tree_bytes` from
  `1,000,000,000` to `1,500,000,000`. The logs tree was
  `1,000,518,697` bytes, so every new record was crossing the old ceiling.
- `src/network/server.cpp`: `performance.server-tick-window` now reports
  p50/p95/p99 and `over_100ms` counts. Added
  `performance.server-tick-stall` for ticks over 100 ms.
- `devscripts/perf-evidence.py`: reads server tick windows/stalls and
  `npc.final-pose.safe-point` phases.
- `src/physics/movement/actor-final-pose.cpp`: safe-point searches now emit
  `npc.final-pose.safe-point.begin/end` with candidate/sample counts,
  penetration, elapsed time, and budget state.
- `src/network/server-npcs.cpp`:
  - `ensureNpcTemplateCollisionBody` now builds each template box around the
    body part's mesh center instead of the bone origin.
  - It seeds `previousWorldTransform` from the first real world transform so
    the first actor-triangle sweep cannot use a root-local pose.
  - The spawn-pose check now ensures the template body exists first.
  - The fallback event includes the template offsets, local centers, and half
    extents for future containment diagnosis.

## Runtime evidence

- Old frozen run: `logs/2026-11-03/20261103_180438_01/events.jsonl`.
  Journal stopped at `2026-11-03T18:04:52Z`; the server thread kept running.
- Log quota proof: `logs` contained 37 files and `1,000,518,697` bytes before
  the quota change, above the configured `1,000,000,000` ceiling.
- After the quota change and rebuild `2026-11-03T18:30:45.738839Z`, run
  `20261103_183057` kept writing for more than 60 seconds and reached
  `29,251,937` bytes. The full freeze did not recur.
- That run still showed heavy NPC cost: recent server windows averaged
  `78-102 ms`, `npc_max` reached `156 ms`, and 200 ticks exceeded 100 ms in
  78 seconds.
- `npc.final-pose.safe-point` failures sampled 125 candidates in `12-15 ms`
  each and repeatedly failed with `0.712 m` initial penetration.
- Build `2026-11-03T18:49:25.936727Z` includes the template body center and
  previous-transform fixes. The `npc_wall_containment_v1` scenario still
  fails: 1,955 `npc.pose.reset` and 116 `npc.pose.penetration` events over its
  30 second soak. Most resets are `no_valid_pose` with final penetration
  between `0.03 m` and `1.45 m`.

## Validation

- `python devscripts/quality/agent-gate.py --no-build --skip-scenarios`:
  GREEN for build artifacts, focused selftests, lint baseline, and JSON sync.
- Full `python devscripts/quality/agent-gate.py`: RED.
  - `devscripts/scenarios/grenade_tick_contract_v1.json` timed out waiting for
    `round.active`; the `grenade.config.resolved` assertions passed, but the
    `area_effect.spawn` assertions were not reached.
  - `devscripts/scenarios/npc_wall_containment_v1.json` failed with the reset
    and penetration counts above.
- No claim of gameplay or human acceptance is made.

## Deferred

- NPC wall containment and the remaining 100 ms server ticks are unresolved.
- Journal tick-based rotation (`rotation.max_ticks`) was not implemented.
- The frame-time regression baseline/tracker was not added; server tick
  windows and stalls are now available as its input.
