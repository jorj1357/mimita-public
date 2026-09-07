# Document replay effect-consumption progress

- Branch: `8292026stash`
- Timestamp: `2026-09-07T19:30:00Z`
- Scope: documentation-only update requested after live replay export retest.
- Pre-existing changes: user and other-agent source/config changes, existing replay fixes, regressions, changelogs, and untracked documentation were preserved.

## Documentation changes

- Appended an ISO-8601 regression update to `docs/regressions/regressions-v1.md`.
- Recorded the exact old behavior: consumed replay batches were obtained through `swap()` but caller-owned reusable vectors were not cleared.
- Recorded the exact new behavior: `effects.clear()`, `sounds.clear()`, and `killEvents.clear()` run after dispatch.
- Recorded human evidence that the third export no longer showed the prior effect spam.
- Recorded the remaining first-export camera recurrence: first export stuck at `(0,0,0)`, second allowed looking but movement was not tested, third worked.
- Recorded that revolver muzzle flash, white muzzle sphere, tracers, rocket projectile/smoke/explosion, dynamic lighting, and other weapon effects remain untested.
- Added `docs/gold/2026-09-07-replay-export-effect-consumption-fix.md` as a reusable evidence example. It explicitly does not claim complete replay/live parity or a fixed camera startup path.

## Validation

- Documentation paths and routed replay/effects requirements were reviewed.
- No source code was changed in this documentation-only session.
- No build or automated test was rerun because the source state was unchanged.
