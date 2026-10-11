# Hybrid body and weapon root bounce guard

Date: 2026-10-10
Scope: Apply body/weapon collision bounce to the player root at most once per fixed simulation tick while preserving limb/weapon blocking and depenetration.

## Source change

- Added `CollisionState::rootBounceAppliedTick` as the per-player fixed-tick gate.
- Updated the shared collision response used by both the active actor-triangle solver and legacy body fallback. Body/weapon contacts still project inward velocity and resolve penetration, but only the first qualifying body/weapon contact in a fixed tick may transfer bounce velocity to the root.
- Root-capsule contacts are unchanged. Weapon configuration and hybrid pose behavior are unchanged.

## Validation

- Canonical build succeeded on 2026-10-10 at 22:30:12: 188 units compiled, 361 skipped, executable relinked.
- Fresh executable version-info succeeded. Build time: 22:23:47. Journal: `logs/10-10-2026/20261010_223017/events.jsonl`.
- `mimita.exe --collision-selftest` passed all existing collision cases, including moving-limb root bounce, embedded limb minimum push, embedded root no-push, angled surface response, thin-wall swept limb contact, and old-contact release.

## Runtime status

- This is a narrow source/build/self-test validation only. The earlier real client/server probe connected and received server snapshot traffic but remained at `waiting for snapshot id=1`, so it did not reach the scheduled hybrid wall scenario.
- Live acceptance remains open: run the 60-tick walk-forward probe with hybrid mode, then compare root velocity and position against default mode at the first wall contact.
