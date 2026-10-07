# Dynamic-light distance policy and Counter-Strike hit flinch

- Timestamp: `2026-10-07T14:08:00-04:00`
- Branch/commit: current checkout; `00dc25ebe0f4da8d338440024c3a542369c98905`
- Result: `PASS_WITH_HUMAN_REVIEW`

## Change

Added JSON-controlled dynamic-light visibility in `config/lighting.json`:

- `dynamicLights.renderDistance`: `1250.0`
- `dynamicLights.renderFadeStartDistance`: `1000.0`
- `dynamicLights.renderFadeEndDistance`: `1250.0`

The previous hard-coded dynamic-light cutoff of `light.radius + 20.0` is no
longer used. Lights are submitted through the configured range and fade by
distance between the configured start and end values.

Enabled Counter-Strike hit flinch with `presentation.hit_flinch: true` in
`config/gamemodes/counterstrike.json`. The mode presentation now overrides the
camera flinch enable state while the match is active and clears the override
when the match resets. `config/camconfig.json` now uses positive pitch/yaw
strengths so the incoming direction determines the response: horizontal hits
push the view away from the attacker, hits from above push down, and hits from
below push up.

Direct damage and confirmed network damage now share `Player::applyHitFlinch`.
This fixes the confirmed network path bypassing `Player::takeDamage`, which was
the primary reason the existing enabled camera setting produced no flinch for
remote/NPC damage. The old 30-unit source-distance attenuation is not applied
to the local victim flinch. A structured `presentation.hit_flinch` event is
also emitted when the camera punch is applied.

## Validation

- `git diff --check`: `PASS`.
- `python build_agent.py`: `PASS`; compiled 7 translation units and linked
  `mimita.exe`.
- `mimita.exe --versioninfo`: `PASS`; journal path:
  `logs/10-07-2026/20261007_140730/events.jsonl`.
- Dynamic-light and hit-flinch owners were force-compiled after the final
  direction-sign correction.

## Review and limits

No live multiplayer or visual gameplay acceptance was performed in this
session. A real Counter-Strike hit from the left/right/above/below should be
used to verify the visible punch and the JSONL `presentation.hit_flinch` event.
Long-range dynamic-light visibility should likewise be checked with a light at
roughly 1000m, 1125m, and 1250m from the camera.

Documents/skills applied: `docs/ROUTER.md`,
`docs/workflows/runtime-scenario-validation.md`,
`docs/skills/spec-behavior-review-v1.md`,
`docs/skills/logging-checker-v1.md`,
`docs/operations/task-completion/task-completion.md`, and
`docs/architecture/time-and-formatting/time-and-formatting.md`.
