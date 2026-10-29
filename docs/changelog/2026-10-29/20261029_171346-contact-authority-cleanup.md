# Contact Authority Documentation, Quality Gates, and Inert Deletion

Time created: `2026-10-29T17:13:46Z`
Time last updated: `2026-10-29T18:10:00Z`

## Summary

Defined the target contact/damage authority contract, added repository quality
tooling and the actor/weapon/network matrix contract, created the regression
records, and deleted confirmed inert legacy melee/projectile networking code.

This pass stopped after the safe deletion commit. The shared contact pipeline and
behavior fixes are not implemented yet.

## Documentation

Added:

- `docs/specs/networking/contact-authority.md`
- `docs/specs/dev-loop/quality-gates.md`
- `docs/regressions/2026-10-29/client-prediction-server-authority-REG.md`
- `docs/regressions/2026-10-29/v2.0.7-actor-matrix-coverage-REG.md`
- `devscripts/scenarios/matrix/actor-weapon-matrix.json`
- `devscripts/scenarios/matrix/README.md`

Updated:

- `docs/specs/networking/networking.md`
- `docs/architecture/code-ownership/code-ownership.md`
- `docs/specs/weapons/melee/melee-weapons.md`
- `docs/specs/weapons/weapons.md`
- `docs/specs/debug-logging/debug-logging.md`
- `docs/specs/dev-loop/scenario-input-recording.md`
- `docs/specs/dev-loop/dev-loop.md`
- `docs/workflows/runtime-scenario-validation.md`
- `docs/ROUTER.md`

The new spec defines one geometry query -> one `ContactCandidate` -> one shared
resolver -> `applyActorDamage`. It also records the future arbitrary-GLB
projectile and model-triangle collision roadmap, while keeping spheres, capsules,
and authored melee boxes as the current implementation.

## Tooling

Added report-only tools under `devscripts/quality/`:

- `lint.py`
- `check_damage_authority.py`
- `check_contact_ownership.py`
- `dead_code_report.py`
- `duplicate_report.py`
- `coverage_report.py`
- `README.md`

Added `devscripts/dev-loop-matrix.py` and the matrix contract. The full
one-server/N-client launcher is not implemented yet; the current tool validates
and prints the required matrix.

Baseline report on the current tree:

- damage-authority findings: `91`
- weapon-ID branch findings: `40`
- duplicate function-name findings: `4`
- TODO-DELETE / `#if 0` markers: `9`
- repeated normalized windows: `479`
- optional C++ tools not installed: `clang-format`, `clang-tidy`, `cppcheck`,
  `jscpd`
- coverage artifacts: none

These are report-only. They expose the work for the next phase; they are not
acceptance failures.

## Deletion

Removed:

- `src/network/server-melee.cpp`
- `handleMeleeHitRequest`
- `tickServerSwordCombat`
- `computeBladeCapsule`
- `computeHitDamage`
- local `sweptSphereOverlap`
- `mpSendMeleeHitRequest`
- `MeleeHitRequestPacket`
- `PACKET_MELEE_HIT_REQUEST`
- unreachable `startServerSwordAttack` branch and helper
- `mpSendProjectileFireRequest`
- `handleProjectileFireRequest`
- `ProjectileFireRequestPacket`
- `PACKET_PROJECTILE_FIRE_REQUEST`
- `projectileConfig(uint8_t)` and its `#if 0` block
- unused `ServerPlayer` fields: `lastMeleeAttackSerial`, `swordswordState`,
  `meleeCooldownTimer`
- unused client `nextLocalMeleeAttackSerial`
- duplicate melee claim flush gate in `melee-client.cpp`

A swordsword weapon without a move set now receives an explicit rejection
instead of the removed fallback timeline.

## Evidence

Source and docs:

- docs/tooling commit: `413d701` (`docs: define contact authority and quality gates`)
- deletion commit: `6c488994` (`refactor: remove inert legacy contact networking`)

Build:

- dev-loop build `3716` succeeded after deletion

Runtime:

- `command_proof_smoke_v1` passed on run `20261029_172617`
- `melee_combat_v1` produced `melee.action.start` for the move-set attack but
  failed its `melee.impulse` assertion; this is not accepted as deletion proof
- a temporary mace smoke did not reach attack because `equipweapon mace`
  produced `weapon.equip.rejected` for the selected weapon set; the temporary
  scenario was removed

Human acceptance:

- none claimed for this pass

## Remaining Work

- commit the deletion
- stop for review
- implement the shared contact pipeline
- add full one-server/N-client matrix launching
- make quality gates blocking after the baseline is clean
- update the regression record with fix evidence


## Follow-up implementation pass

Implemented the first correctness fixes after the deletion stop:

- `AttackRequestPacket` carries `movesetHash`.
- Client sends `MeleeMoveSet::hash`; server compares content hash and logs
  `melee.attack.hash_mismatch` instead of rejecting process-local revisions.
- Melee claims carry `targetGeneration`.
- Melee claims use the per-target rendered server tick.
- Client prediction deduplicates per target/tick.
- Server claim validation uses the claimed hit point and OBB against a lenient
  multi-height target volume; reach is computed from the claimed hit point.
- Added `combat.max_authoritative_damage` with a huge default
  (`999999999999999`) and removed the hardcoded `1..500` damage sink clamp.
- Added the RTT/accuracy targets, late-contact policy, no-offline rule, and
  dev-loop package split to the specs.

Evidence:

- build `3718` succeeded
- `command_proof_smoke_v1` passed
- weapon-set-29 mace smoke passed with `melee.action.start` and accepted
  `weapon.attack.result`
- no `melee.attack.hash_mismatch` or revision-mismatch event in that run
- contact-claim smoke did not reach an NPC and is not accepted as contact proof

Not yet implemented:

- client team/friendly-fire prediction parity
- shared melee damage/knockback resolver
- shared contact pipeline for hitscan/projectile/deathball
- full N-client launcher and matrix execution
- server-view overlay
- human PvP acceptance
## Shared melee formula and client team parity

Added shared melee HP damage/knockback helpers in `WeaponExecution` and routed
the client prediction, server claim, and server animated-contact paths through
them. This removes the `force_scale`/`damage_scale` drift between those paths.

Client prediction now skips same-team targets when the active gamemode has
friendly fire disabled. This is prediction parity only; the server remains the
final authority.

Build `3722` succeeded. Weapon-set-29 mace smoke passed with `melee.action.start`
and accepted `weapon.attack.result`; no hash/revision mismatch was emitted.

Still pending:

- shared contact pipeline for hitscan/projectile/deathball
- full N-client matrix runner
- server-view overlay
- PvP and bad-connection accuracy measurement
