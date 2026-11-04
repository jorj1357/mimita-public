Time created: 2026-11-03T15:25:00Z
Time last updated: 2026-11-03T15:25:00Z

Purpose:
Restore full scenario recordings under `devscripts/scenarios/user/`, keep the
split journal as-is, recover the existing tdmtest3 recordings, show the saved
path in the terminal and in-game tip, and make collision impacts bounce on
walkable slopes instead of snagging.

## Scenario recording

- Reverted the journal-only canonical recording path in
  `src/devtools/scenario-recorder.cpp`.
- The client writes the full per-tick scenario to
  `devscripts/scenarios/user/scenario-YYYYMMDD_HHMMSS-name.jsonl`.
- The server writes the authoritative `.server.jsonl` sidecar beside it; the
  client merges it at stop.
- The journal keeps only compact scenario start/completed/invalid/merge
  summaries. Journal quota and rotation cannot abort the scenario file.
- On successful stop the terminal prints the exact saved path and
  `scenario_play <path>`; the in-game notification shows the same saved path.
- On invalid/partial stop the terminal and tip show the path plus the failure
  reason. The partial file and sidecar are kept.

## Recovery tool

- Added `devscripts/scenario_export.py`.
  - `--list` lists canonical scenarios from a journal.
  - `--scenario <id>` exports a legacy schema-2 `.jsonl` into
    `devscripts/scenarios/user/`.
- Exported the two existing recordings:
  - `devscripts/scenarios/user/scenario-20261103_195642-tdmsnag.jsonl`
  - `devscripts/scenarios/user/scenario-20261103_195931-tdmcoll.jsonl`

## Snagging / bounce

- `src/physics/movement/physics-collision-shared.h`: removed the blanket
  walkable-normal bounce suppression. Every impact surface, including walkable
  slopes, now takes the configured bounce path and retains tangential velocity.
- Only an already-supporting flat floor (`normal.z > 0.995`, grounded/stable)
  remains project-only so ordinary walking does not micro-bounce.
- Recovery records now point at the useful tdmtest3 recording and its first
  grounded collapses: ticks 8929 and 9534, surfaces 263/311.

## Documentation

- Updated `docs/specs/dev-loop/scenario-input-recording.md` to document the
  `devscripts/scenarios/user/` storage and journal-summary split.
- Updated `docs/specs/movement/movement.md` with the every-impact-surface bounce
  status and flat-support exception.
- Updated the 2026-11-03 regression records and master ledgers.

## Validation

- `python build.py build-only`: BUILD SUCCESS (16 files compiled, link OK).
- Scenario export/list verified against `logs/2026-11-03/20261103_195321`.
- No runtime replay or human surface-feel acceptance was performed in this
  session.

## Not claimed

- The snagging fix is source/build only until the exported `tdmsnag` scenario is
  replayed and ticks 8929/9534 no longer collapse speed.
- Scenario merge and terminal/tip output are not yet runtime-verified.
