# Restore impact-decals blood settings

- Timestamp: `2026-10-07T14:50:00-04:00`
- Branch/commit: current checkout; `00dc25ebe0f4da8d338440024c3a542369c98905`
- Result: `PASS_WITH_HUMAN_REVIEW`

## Change

Restored the blood configuration blocks in `config/impact_decals.json`:

- `blood.spray.debris`, including count, color, size, motion, gravity, drag,
  rotation, and lifetime controls.
- `blood.clientFeedback`, including local camera-facing feedback count,
  offsets, size, lifetime, alpha, color, and damage/force scaling.
- Blood render distance settings: start fade `1000.0`, end fade `1250.0`,
  maximum render distance `1250.0`.

## Diagnosis

Before restoration, the file content hash exactly matched `HEAD` and contained
none of these keys. The worktree reported the file as modified only because of
file metadata/stat state, while `git diff` had no content changes. This proves
the JSON had been replaced with the repository baseline, but does not identify
which external action performed the replacement; likely causes are a checkout,
merge, restore, or another local synchronization operation.

## Validation

- PowerShell JSON parsing: `PASS`.
- Verified `clientFeedback=true`, `spray.debris.enabled=true`, and blood render
  distance values `1000/1250/1250`.
- No executable rebuild was required because this is a hot-reloadable JSON
  restoration.

Existing unrelated worktree changes were preserved.
