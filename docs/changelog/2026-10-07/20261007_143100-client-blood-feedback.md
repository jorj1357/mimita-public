# Configurable blood debris and local victim feedback

- Timestamp: `2026-10-07T14:31:00-04:00`
- Branch/commit: current checkout; `00dc25ebe0f4da8d338440024c3a542369c98905`
- Result: `PASS_WITH_HUMAN_REVIEW`

## Change

Added `blood.spray.debris` to `config/impact_decals.json`. The former hard-
coded blood debris box values are now configurable for enabled state, count
fraction, color, alpha, size, speed, lifetime, gravity, and drag. The default
count fraction remains `0.33`, matching the previous approximately one-third
debris split.

Added `blood.clientFeedback` to `config/impact_decals.json`. Confirmed local
victim damage now spawns 2–5 camera-locked red billboard particles by default.
Their forward, left/right, up/down offsets, size, lifetime, alpha, color, and
damage/force scaling are JSON-controlled. The particles are client-side only,
follow the current camera while alive, and are not world-distance culled.

Both authoritative network-confirmed local damage and direct local damage use
the feedback owner. Direct damage is gated to `gpPlayer`, so damage applied to
remote replicas does not create a local victim overlay.

## Validation

- `config/impact_decals.json`: parsed successfully with PowerShell JSON parsing.
- `python build_agent.py`: `PASS`; compiled 6 translation units and linked
  `mimita.exe` after the final local-victim ownership correction.
- `mimita.exe --versioninfo`: `PASS`; journal path:
  `logs/10-07-2026/20261007_142957/events.jsonl`.
- New structured events: `presentation.local_blood_feedback` under the
  Rendering category.

## Review and limits

No live combat visual acceptance was performed. Verify a local player receives
the camera-facing blood particles while moving forward, verify remote players
do not create the local overlay, and tune the JSON values against the desired
first-person presentation. Existing unrelated worktree changes were preserved.

Documents/skills applied: `docs/ROUTER.md`,
`docs/workflows/runtime-scenario-validation.md`,
`docs/skills/spec-behavior-review-v1.md`,
`docs/skills/logging-checker-v1.md`,
`docs/operations/task-completion/task-completion.md`, and
`docs/architecture/time-and-formatting/time-and-formatting.md`.
