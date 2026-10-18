# Captain Falcon air kick clip link

- Timestamp: 2026-10-18T19:51:10Z (2026-10-18 15:51:10 EDT)
- Branch: `20261011zt`
- HEAD: `3bf384e4c6c0f5378b42f29762fef7e828c800df`
- Result: `PASS_WITH_HUMAN_REVIEW`

## Outcome

Connected the exported `kick_a` animation clip to the Captain Falcon airborne primary attack, `a_air_kick`, in `config/weapons.json`. The action now resolves the clip's duration and phase boundaries from its markers: 20 windup ticks, 20 active ticks, and 19 recovery ticks (59 total). Its hitbox, damage, impulse, and audio settings remain as they were.

## File change

### `config/weapons.json`, lines 1755–1758

- Before:

```json
          "a_air_kick": {
            "input": {"button": "primary", "actor_state": "airborne"},
            "phases": {
              "windup":   {"ticks": 10, "clip_id": "cf_airkick_windup"},
              "active":   {"ticks": 10, "clip_id": "cf_airkick_active"},
              "recovery": {"ticks": 30, "clip_id": "cf_airkick_recovery"}
            },
```

- After:

```json
          "a_air_kick": {
            "input": {"button": "primary", "actor_state": "airborne"},
            "animation_clip_id": "kick_a",
            "hitboxes": [
```

The old phase fields were removed because the animation spec requires a single clip to own action timing whenever `animation_clip_id` is present. The already-existing unrelated `a_ground_punch` hitbox edits in this worktree were preserved.

## Validation and review

- Parsed `config/weapons.json` and the referenced `config/animations/library/melee/captainfalcon/kick_a.json` successfully.
- Confirmed `a_air_kick.animation_clip_id` resolves uniquely to `kick_a`, whose required markers are ordered and match its 59-tick duration.
- `git diff --check -- config/weapons.json` passed.
- No build or game runtime was run. The config link and clip timing are verified; visible kick playback still needs in-game review.
- Focused skill: `docs/skills/spec-behavior-review-v1.md` — PASS_WITH_HUMAN_REVIEW. The behavior matches `docs/specs/animations/animations.md`'s single-clip timeline contract and the Captain Falcon air-kick roster in `docs/specs/gamemodes/supersmashbrosmelee.md`.
- `docs/regressions/regressions-v1.md` was referenced by the router but is absent in this checkout; confirmed regression history could not be reviewed.
