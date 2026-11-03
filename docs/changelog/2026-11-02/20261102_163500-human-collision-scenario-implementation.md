Time created: 2026-11-02T16:35:00Z
Time last updated: 2026-11-02T16:35:00Z

Purpose:
Implement the first complete pass of human-recorded collision scenarios on the
canonical journal: configurable journal limits, checked scenario writes,
bookmark-linked checkpoints, canonical recording, intent-driven replay with
continued divergence evidence, bookmark restore, a native dev-loop window, and
a replay report runner.

## What changed

- `config/debuglogger.json` now owns `storage_limits.per_segment_bytes`
  (default `100000000`) and `storage_limits.total_log_tree_bytes`
  (default `1000000000`). `StructuredLogger` validates them at launch and keeps
  them fixed across hot reload.
- `StructuredLogger` gained a blocking checked-write API for required scenario
  records and a run pin marker for bookmarked recordings. Pinned runs are
  skipped by automatic pruning.
- New recordings write canonical scenario events through `StructuredLogger`:
  `scenario.record.started`, `scenario.record.client_tick`,
  `scenario.record.tick`, `scenario.record.checkpoint`,
  `scenario.record.completed`, and `scenario.record.invalid`. Client and server
  share the dev-loop journal and correlate by scenario ID, process role, and
  tick.
- Per-tick records include actor identity, accepted input/intent, pre/post pose,
  velocity, grounding/support, collision contacts, containment evidence,
  NPC state/RNG/cooldowns, and server projectile state.
- `\` bookmarks the active scenario, writes the journal fields, captures a
  server checkpoint, pins the source run, and shows a notification. It no
  longer opens the terminal or requests annotation.
- `scenario_play <definition.json>` loads canonical recordings. Local input and
  NPC intents replay through the normal movement/collision path. Playback logs
  the first divergence and continues instead of stopping at 2 m; it never snaps
  the actor back except at an explicit checkpoint restore.
- Bookmark checkpoints restore actor pose, velocity, health, equipment, NPC
  state/RNG, and projectiles on the server. The client restores its local player
  pose/health/equipment at the same bookmark. Unrestored mutable-world state is
  reported explicitly.
- `devscripts/scenario_replay_report.py` reports `PASS` / `FAIL` / `PENDING` /
  `INVALID` with journal path, physical line, event ID, tick, first divergence,
  and checkpoint restores.
- `devscripts/dev_loop_gui.py` provides a native Tkinter dashboard with the
  existing daemon controls, journal paths, storage limits, scenario state, and
  activity feed; the terminal dashboard remains the fallback.

## Runtime evidence

- Final build: dev-loop published build 251
  (`C:\mimita-v9\.dev\builds\0251\mimita.exe`). Earlier verification used
  builds 246 and 250
  (`C:\mimita-v9\.dev\builds\0246\mimita.exe`,
  `246:3a71870723fdb1a6e4224bd203fd46c8b2c8f743720e0cb4c48f9fd94f351fc5`).
- Journal: `C:\mimita-v9\logs\2026-11-02\20261102_162354\events.jsonl`.
- Scenario: `scn-20261102_162440-27460-quick`; 318 recorded ticks, one
  checkpoint, client and server completion records valid.
- Bookmark: `bookmark.created` event `GENERAL_1320`, pinned=true,
  scenario_id `scn-20261102_162440-27460-quick`.
- Checkpoint: `scenario.record.checkpoint` event `GENERAL_146`.
- Replay: `scenario.playback.completed` event `GENERAL_2468`,
  `frames_played=318`, `max_position_error_m=0.0908203125`, `valid=true`.
- Checkpoint restore: server `scenario.replay.restore` event `GENERAL_347`,
  client `scenario.replay.restore` event `GENERAL_2181`.
- Build 247 verification: `scenario.playback.started` event `GENERAL_411` in
  `logs/2026-11-02/20261102_164008/events.jsonl` carries both `path` and
  `scenario_id`, proving playback events are selectable by scenario identity.
- A later replay on a fresh run failed readiness with `spawn_not_ready`; this
  was reported as `FAIL`, not hidden as a pass.
- Forced-quota runtime test: with `storage_limits.per_segment_bytes=32768`
  on build 250, run `logs/2026-11-02/20261102_164557/events.jsonl`, the server
  wrote `scenario.record.invalid` (`GENERAL_5`,
  reason `server_tick_write_refused`) and the client wrote
  `scenario.record.invalid` (`GENERAL_1647`,
  reason `server_capture_invalid`, `valid=false`). No completed marker was
  written. The default limits were then restored.
- `python devscripts/quality/agent-gate.py --no-build --skip-scenarios`
  passed: build selftests, lint baseline, weapon/gamemode JSON sync.
- Full `python devscripts/quality/agent-gate.py` passed on final build 251,
  including the canonical build, all focused selftests, lint, JSON sync, and
  the two fast runtime scenarios. An earlier full gate also passed on build
  246.

## Honest limits

- The forced-quota live failure test was not completed in this session. The
  checked-write path and configurable limits exist, but live quota exhaustion
  still needs acceptance evidence.
- Mutable world state and full projectile checkpoint restoration are not yet
  round-tripped; the restore event lists them as `unrestored_components`.
- Human recordings for `atdm`, `tdmtest3`, and `dust2cyberiav4` were not
  available, so those map cases remain `NOT_COVERED` until recorded.
- Old recordings whose live starting life cannot be matched are reported as
  readiness failures/`NOT_REPRODUCED`, not as collision passes.

## Documentation TODOs noted

- `docs/specs/dev-loop/dev-loop.md:7` — replace the raw note with a concise
  document purpose/status index.
- `docs/specs/tas/tas.md:1` — replace the raw note with a short TAS purpose and
  decision-boundary section.
- `docs/architecture/player-npc-systems/npc-movement.md:22` — explain the NPC
  goal -> route -> shared movement ownership and runtime evidence path.
