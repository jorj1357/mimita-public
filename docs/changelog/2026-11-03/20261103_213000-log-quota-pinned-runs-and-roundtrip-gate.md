Time created: 2026-11-03T21:30:00Z
Time last updated: 2026-11-03T21:30:00Z

Purpose:
Restore structured journal writes when pinned scenario runs push the log tree
over its rolling quota, then prove the scenario recording roundtrip and soak
TDM NPC recovery on `dust2cyberiav4`.

## What changed

- `src/debug/structured-log.cpp`
  - Added `quotaCountedFileBytes()`. It counts the rolling log tree but skips
    run directories that contain `.scenario-bookmark-pin`.
  - `initializeQuotaState()` and `reconcileQuotaState()` now use the counted
    total instead of the raw recursive tree size.
  - Pinned scenario evidence stays until a human removes it, as required, but
    it can no longer suppress every new active journal.
  - `reconcileQuotaState()` now recomputes the counted total instead of taking
    the previous maximum, so an over-quota process can recover without a
    restart.

## Root cause

The `logs` tree was `1,513,901,508` bytes because three scenario runs were
pinned. That was above the configured `1,500,000,000` total limit, so
StructuredLogger dropped every new record. The gate command was delivered and
recording started, but `scenario.record.started` never reached the journal. The
gate runner therefore never queued `scenario_endrecord`; the recorder ran to its
10 minute / 36,000 frame cap and produced a `1,630,065,044` byte INVALID artifact.
That artifact and its sidecar were deleted after diagnosis.

## Validation

- `python devscripts/dev-loop.py --scenario-file devscripts/scenarios/scenario_record_roundtrip_v1.json --json`
  - PASS: `scenario.record.started` 1, `scenario.record.merged` 1,
    `scenario.record.invalid` 0, `scenario.record.merge_failed` 0.
- `python devscripts/quality/check_scenario_roundtrip.py <new artifact>`
  - PASS: `client_ticks=234 server_ticks=173 valid=true`; test artifact removed.
- `python devscripts/quality/agent-gate.py`
  - GREEN: build, focused selftests, lint baseline, weapon/gamemode JSON sync,
    grenade tick contract, NPC wall containment, recording roundtrip, and the
    merged scenario header/server/footer check.
- `python devscripts/dev-loop.py --ai-match --ai-seconds 60 --ai-map dust2cyberiav4 --json`
  - TDM run `20261103_212915`, map `dust2cyberiav4`, mode `tdm`, 9 NPCs.
  - `npc.root.recovered=114`, `npc.root.inside=9`, `npc.root.respawned=0`.
  - Every final `npc.trace` sample reported `inside_solid:false`; no NPC was
    left inside static geometry. The 9 inside samples each recovered on the
    next attempt (failure counts 1-2, below the respawn threshold).
  - Recovery is still capable of lifting an NPC by up to `16.01 m`; that
    displacement is recorded here for human surface-feel review.

## Weapon snag evidence (no behavior change)

`config/weaponcollisions.json` keeps the rocket launcher on
`"source":"boxes"` with half-size `[0.2,0.2,1.6]` centered at `[0.15,0,0.2]`.
The held collider is therefore `0.4 x 0.4 x 3.2 m`, with
`"bounce_mode":"inherit_actor"` and `"collision_skin":0.05`. This is consistent
with the reported held-rocket snagging. No weapon collision behavior was
changed in this session.

## Not claimed

- No human surface-feel acceptance.
- The 9 in-geometry samples and the `16 m` recovery lift still need review
  against the desired NPC presentation.
- The stale AI-match `npc.damage` assertion remains unrelated to this change.
