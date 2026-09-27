# Enable live authoritative explosion proof

- EST timestamp: 2026-09-12 12:36:04 EDT (UTC 2026-09-12T16:36:04Z)
- Branch: `8292026stash`
- Result: `PASS_WITH_HUMAN_REVIEW`

## Change

Enabled the temporary explosion-only hot policy in
`src/hot-reload/modules/rocket-behavior.cpp`:

```cpp
if (policy->source == GAME_DAMAGE_SOURCE_EXPLOSION)
    policy->outDamage = 999999;
```

The baseline remains `policy->outDamage = policy->baseDamage` for other damage
sources. No cold EXE source or persistent game state was changed.

## Validation boundary

The file edit is ready for the already-running hot-reload watcher to compile
and activate. No cold build was run and `mimita.exe` was not written. Human
runtime proof must confirm the new generation activates and authoritative rocket
explosion damage changes without a PID/session/entity reset. Revert this
temporary proof override after acceptance.

## Pre-existing edits

All unrelated working-tree changes were preserved.
