Time created: 2026-11-03T17:45:00Z
Time last updated: 2026-11-03T17:45:00Z

Purpose:
Make `scenario_startrecord` capture complete, super-granular NPC/player
geometry evidence by default, reject incomplete capture, and verify the result
through a live recording on the real executable.

## What changed

- `scenario_startrecord` now activates an in-memory `scenario_debug_v1`
  profile. It does not edit `config/debuglogger.json`; normal diagnostics are
  restored when recording stops.
- The profile forces collision/NPC-movement/physics diagnostics on for the
  recording and captures one canonical tick row per fixed server tick.
- Added `scenario.record.capture_probe`:
  - client probe requires a live player, body parts, finite pose/velocity, and
    contact evidence when world contact exists;
  - server probe requires the player actor and every live NPC to have a
    simulated body, accepted input, containment classification, body parts, and
    finite pose/velocity.
  - a failed or missing probe writes `scenario.record.invalid`, stops capture,
    and never emits a completed marker.
- Added `scenario.record.capture_gap` for a live NPC with no simulated body
  after the probe.
- Added per-tick desired-versus-actual evidence:
  - desired direction/velocity, previous and actual speed, speed ratio,
    actual displacement, progress ratio, deflection angle;
  - expected versus actual grounded state;
  - expected versus actual penetration;
  - root containment, containment stage, and final-pose contacts.
- Added `scenario.record.anomaly` with expected/actual numbers for speed
  collapse, support loss while descending, grounded no-progress, penetration,
  and root-inside cases. Records are rate-limited per actor/type.
- NPC body parts now come from the simulated NPC body when available and fall
  back to the server's canonical `gServerBodyTemplate`. This keeps NPC geometry
  coverage populated even when the snapshot body-part array is empty.
- Movement contacts now carry world triangle and solid IDs; final-pose contacts
  carry actor part, world triangle, solid ID, normals, and penetration.
- Added per-tick capture timing (`build_us`, `last_write_us`,
  `last_total_us`).
- Updated `docs/specs/dev-loop/scenario-input-recording.md` to document the
  default super-granular profile, probe, hard failure, and the deferred
  buffered bookmark mode in `config/scenario-recording.json`.

## Runtime evidence

- Final build: `2026-11-03T17:34:12.057325Z`
  (`C:\mimita-v9\.dev\builds\2026-11-03T17-34-12.057325Z\mimita.exe`).
- Journal: `C:\mimita-v9\logs\2026-11-03\20261103_173450\events.jsonl`.
- Scenario: `scn-20261103_173536-18212-npcprobe8`.
- Client probe: `GENERAL_198`, `ok:true`, 6 body parts, world contact present.
- Server probe: `GENERAL_21`, `ok:true`, 20 live NPCs, 20 simulated NPCs, no
  missing evidence.
- Completion: client `scenario.record.completed` `GENERAL_2621` (1,715
  frames), server `scenario.server.record.completed` `GENERAL_2244` (1,644
  frames), both `valid:true`.
- NPC coverage in the run: 32,880 NPC actor rows, all with 6 body parts;
  34,817 movement contacts with world triangle/solid IDs; zero unknown or
  inside root-containment samples; zero unresolved penetration.
- Anomalies: 463 no-progress, 96 support-loss-descending, 36 speed-collapse.
- Capture cost: average build 2.19 ms, average write 3.88 ms, average total
  6.06 ms; worst tick 549 ms, driven by journal segment rotation.
- Full `python devscripts/quality/agent-gate.py` passed on the final build:
  canonical build, focused selftests, lint baseline, JSON sync, and both fast
  runtime scenarios.

## Session notes and limits

- The legacy `.jsonl` file was intentionally left unchanged. No decision was
  made about a canonical-to-legacy exporter.
- Old unpinned 2026-11-02 log runs were removed to free journal quota for this
  verification. The pinned `20261103_154741` run was preserved.
- `final_pose_contacts` was zero in this run because no final-pose penetration
  occurred; the capture path is implemented and was exercised as empty.
- The worst 549 ms capture write is a journal-rotation cost. Capture is
  synchronous by request, so the frame-time cost is recorded honestly rather
  than hidden.
- NPC fall-through on `tdmtest3` still needs a human-recorded focused case now
  that capture is bulletproof.
