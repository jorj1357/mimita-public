# Human-recorded collision scenario spec alignment

Time created: 2026-11-02T15:33:08Z
Time last updated: 2026-11-02T15:33:08Z

Purpose:
Make the desired human-recorded collision regression workflow easy to find
and unambiguous across its owner spec, runtime validation, logging, dev-loop,
quality-gate, issue index, and TAS relationship notes.

## Documentation changes

- Clarified that current scenario playback is legacy input/partial-track
  behavior and does not meet the desired collision replay contract.
- Specified canonical `events.jsonl` per-process capture, high-detail
  per-tick actor/dynamic-object state, containment as inside/outside/unknown,
  bookmark association, visible path notification, and human-controlled
  retention.
- Specified continue-through-divergence behavior, restore-after full gameplay
  checkpoints at human bookmarks, and event-driven scenario completion without
  elapsed-time verdicts.
- Specified fail-loud incomplete status for missing trace writes/state and
  configurable logger quotas in `config/debuglogger.json` with current values
  as defaults.
- Routed the scenario topic from `docs/ROUTER.md`, aligned related workflow,
  quality-gate, logger, dashboard, TAS, and OGI-010 text.
- Updated OGI-003/004 with the named `atdm`, `tdmtest3`, and
  `dust2cyberiav4` human-recorded regression targets and linked those targets to
  existing collision ownership requirements.

## Evidence and limits

This was documentation-only. No code, configuration, build, test, runtime,
deployment, or human gameplay acceptance was performed. The written target is
not implemented or proven by this changelog. Existing implementation notes are
retained and labeled as current/legacy behavior where they differ.
