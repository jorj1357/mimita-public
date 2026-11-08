# Outline diagnostics enabled

Timestamp: 2026-11-08T14:25:08Z (09:25:08 EST)
Branch: `20261011zt`
HEAD at start: `b12077119c82f6bf75b586284f822855b8ab065e`

## Scope and pre-existing work

Enabled renderer-category logging and actor outline diagnostics in the current
Payload game session. Documented how agents can adjust `debuglogger.json` for
investigations. The repository already contained many unrelated modified and
untracked files before this task; they were left untouched.

## Changes

- `config/debuglogger.json`, Rendering category: changed `level` from
  `important` to `verbose`; kept `file_output` false because it controls
  category-specific sinks, not canonical `events.jsonl`.
- `docs/specs/debug-logging/debug-logging.md`, “Category controls and agent
  investigations”: documented category-level filtering of `events.jsonl`,
  supported levels (`off`, `errors`, `important`, `verbose`, `trace`), agent
  authority to tune levels, hot reload, and the separate feature diagnostic
  switch. Clarified that `minimal` is not a supported level.
- Current client: issued `outline_debug 1` and `outline_dump` through the
  existing dev-loop control surface. Per-actor diagnostics remain enabled in
  the running client; the one-frame dump was also requested.

## Evidence

- Runtime identity: run `20261108_141222`, build ID
  `2026-11-08T14:12:08.114903Z:2f650a9378a562c9b722aaa7aabeae449244125a6bb36cd75590fe0f65b6f3c5`.
- Client journal: `logs/2026-11-08/20261108_141222/client-01/events.jsonl`.
- Dev-loop command results confirmed `outline_debug 1` and `outline_dump` were
  delivered and completed. Journal records named `player_visuals.actor` then
  contained actor ID, avatar, position, bounds, outline settings, and part
  origins. Records were emitted for 23 actors.
- Example lead: actor `100016`, avatar `monsterchainsawbossv2`, had current
  `bounds_max_offset` about 3.96 m and `bounds_max_previous_offset` about
  475.50 m. Current bounds were roughly 0.83 × 1.27 × 4.19 m. Several part
  meshes reported vertices while `bounds_mesh_parts` was zero, so the recorded
  bounds did not cover mesh batch extents. This is a candidate for further
  investigation, not proof of the screenshot's red shape cause.
- Config JSON parsed successfully. No build was needed or run; the game source
  was unchanged. No new gameplay test was run. Human visual acceptance of the
  outline remains open.

## Review

- `docs/skills/logging-checker-v1.md`: PASS_WITH_GAPS. Existing diagnostics
  are bounded to one actor record per second plus an explicit one-frame dump.
  The missing mesh batch bounds prevent concluding that logged actor bounds
  match all rendered vertices.
- Unrelated dirty work was preserved. No regression was confirmed.
