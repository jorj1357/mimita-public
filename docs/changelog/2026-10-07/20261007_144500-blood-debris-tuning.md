# Expose blood debris cube visuals

- Timestamp: `2026-10-07T14:45:00-04:00`
- Branch/commit: current checkout; `00dc25ebe0f4da8d338440024c3a542369c98905`
- Result: `PASS_WITH_HUMAN_REVIEW`

## Change

Extended `blood.spray.debris` in `config/impact_decals.json` so the small box
debris emitted with blood spray no longer relies on hard-coded visual or motion
values. The JSON now controls color, alpha, count fraction, force-scaled size,
size jitter, spawn offset, speed range, upward velocity range, cone spread,
lifetime, gravity, gravity enable, drag, initial rotation range, and angular
speed range.

The default values preserve the existing beige cube presentation while making
the effect independently tunable from the blood particle spray.

## Validation

- `config/impact_decals.json`: parsed successfully.
- `python build_agent.py`: `PASS`; compiled 6 translation units and linked
  `mimita.exe`.
- `mimita.exe --versioninfo`: `PASS`; journal path:
  `logs/10-07-2026/20261007_144354/events.jsonl`.

## Review and limits

No live combat visual acceptance was performed. Edit the `blood.spray.debris`
values and use hot reload/in-game testing to tune the desired cube appearance.
Existing unrelated worktree changes were preserved.

Documents/skills applied: `docs/ROUTER.md`,
`docs/workflows/runtime-scenario-validation.md`,
`docs/skills/spec-behavior-review-v1.md`,
`docs/skills/logging-checker-v1.md`,
`docs/operations/task-completion/task-completion.md`, and
`docs/architecture/time-and-formatting/time-and-formatting.md`.
