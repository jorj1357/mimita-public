// 2026-09-07T16:29:18-04:00 (2026-09-07T20:29:18Z)

# Replay player rocket visibility fix

## Result

Fixed replay presentation-only rocket simulation so player-owned rockets are not immediately removed by collision with the temporary local player used as the simulation owner. Added the human-confirmed NPC rocket and left-leg results to the append-only regression tracker.

## Exact cause and fix

Old code in `src/combat/weapon-rocket-launcher.cpp` allowed the temporary replay owner collision:

```cpp
if (dist < 0.5f && rocket.distanceTraveled >= IGNORE_OWNER_DIST)
    hitOwner = true;
```

New code:

```cpp
if (!presentationOnly && dist < 0.5f && rocket.distanceTraveled >= IGNORE_OWNER_DIST)
    hitOwner = true;
```

This suppresses only the invalid temporary-owner self-hit in replay. World collision, shared movement, smoke, orientation, lifetime, NPC collision, and explosion presentation remain active.

## Human-confirmed related fixes

The user confirmed NPC rockets now appear in exported MP4s with smoke and the left-leg rotation issue is working. The regression entry records the exact old flattened root transform and the new parent-relative transform implementation.

## Repository state

- Branch: `8292026stash`
- Commit observed: `a045b37`
- Pre-existing unrelated worktree edits were preserved, including configuration files and other source changes not owned by this replay fix.
- No commit was created by this session.

## Validation

- `git diff --check`: passed; line-ending normalization warnings only.
- `python build_agent.py`: canonical `C:\mimita-priv-v8\mimita.exe`, `Status: SUCCESS`.
- `mimita.exe --replay-export-selftest --timeout 60 --no-coordinator`: `28/28 passed, 0 failed`.
- `docs/skills/spec-behavior-review-v1.md`: PASS for source behavior; player-rocket MP4 proof remains human review.
- `docs/skills/logging-checker-v1.md`: PASS; replay diagnostics remain centralized under the Replay category.

## Remaining human review

Export a fresh replay containing a player-fired rocket and verify visible travel, wall collision, one explosion, fading smoke, and no repeated effects/audio.
