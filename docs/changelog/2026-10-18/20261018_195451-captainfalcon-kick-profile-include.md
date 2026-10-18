# Captain Falcon kick animation profile include

- Timestamp: 2026-10-18T19:54:51Z (2026-10-18 15:54:51 EDT)
- Branch: `20261011zt`
- HEAD: `3bf384e4c6c0f5378b42f29762fef7e828c800df`
- Result: `PASS_WITH_HUMAN_REVIEW`

## Finding and fix

The prior config change set `captain_falcon_moveset.components.melee.actions.a_air_kick.animation_clip_id` to `kick_a`, but the Captain Falcon avatar's `animations.json` included only `punch_a.json`. The grounded punch therefore resolved from the avatar's loaded clip set, while the kick clip was absent there. `src/entities/player-animation.cpp` lines 350–360 confirms Melee-exclusive authored pose mode does not fall back to the global procedural animation set for a missing clip. Lines 375–393 select the avatar profile default when the requested clip is absent. This is why the air kick showed the default pose after the old procedural pose was suppressed.

Added the kick clip to the avatar profile's existing include list. Both JSON files and their link now validate. The `kick_a` clip resolves to the `a_air_kick` action through the Captain Falcon profile.

## File change

### `assets/avatars/captainfalcon/animations.json`, lines 375–378

- Before:

```json
  "include": [
    "config/animations/library/melee/captainfalcon/punch_a.json"
  ]
```

- After:

```json
  "include": [
    "config/animations/library/melee/captainfalcon/punch_a.json",
    "config/animations/library/melee/captainfalcon/kick_a.json"
  ]
```

## Validation and review

- Parsed the avatar animation profile and all referenced include files.
- Confirmed the Captain Falcon `a_air_kick` definition references `kick_a`, and the profile include merges a clip named `kick_a`.
- `git diff --check -- assets/avatars/captainfalcon/animations.json` passed.
- No build or live game was run. Runtime hot reload and visible kick playback still need verification.
- Focused skill: `docs/skills/spec-behavior-review-v1.md` — PASS_WITH_HUMAN_REVIEW. `docs/specs/animations/animations.md` requires an explicit profile fallback and forbids silently restoring a suppressed overlay.
- Router-required `docs/regressions/regressions-v1.md` is absent in this checkout.
- Existing unrelated worktree edits were preserved.
