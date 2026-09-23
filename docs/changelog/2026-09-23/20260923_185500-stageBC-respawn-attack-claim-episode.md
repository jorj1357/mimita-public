# Stage B/C finish-out: respawn rule, attack claim, physical-contact episode batching

Date (UTC): 2026-09-23T18:55:00Z
Status: implemented; cold build and selftests verified; hot providers resolved and exercised

## Scope

Finish the remaining policy slices of Stage B and Stage C that are pure policy
(not geometry/mechanism): the shared respawn rule (players + NPCs), the client
hit-claim eligibility/tolerance, and the physical-contact episode interval/confirm
batching.

## Changes

### `net.respawn` (players + NPCs)
- New `src/hot-reload/hot-respawn.h` (POD `GameRespawnRuleV1` + shared
  `initialTimer`/`tick`) and `src/hot-reload/modules/respawn-policy.cpp`.
- `src/network/server-players.cpp`: the dead-state respawn gating (one-life stay
  dead, instant request, countdown) now uses the hot rule.
- `src/network/server-npcs.cpp`: the NPC death timer and the per-tick respawn
  countdown now use the hot rule (shared with players).

### `net.attack-claim` (client hit-claim eligibility + tolerance)
- New `src/hot-reload/hot-attack-claim.h` (POD `GameAttackClaimV1` + shared
  `evaluate`) and `src/hot-reload/modules/attack-claim-policy.cpp`.
- `src/network/server-attack.cpp`: the claim structural gates (target present,
  not already confirmed, distance bounds, world occlusion) and the acceptance
  tolerance (`rewindHitTolerance + claimLagAllowance`) are now hot. The rewound
  pose reconstruction and the body-part volume test remain cold geometry.

### `net.physical-contact` episode batching
- Extended `src/hot-reload/hot-physical-contact.h` with `intervalTicks` and
  `shouldConfirm` (shared) and registered them from
  `modules/physical-contact-policy.cpp`.
- `src/network/server-physical-contact.cpp`: the damage-tick interval and the
  episode confirm batching now use the hot policy.

### Manifest + selftests
- Added the three new headers to `headers`.
- Added live-code selftest checks for respawn (one-life, countdown), attack claim
  (eligible + tolerance), and the physical-contact interval.

## Evidence

- Build (source/build evidence): `python build_agent.py` -> `Status: SUCCESS`,
  `mimita-20260923T185416.exe`.
- Automated tests (test evidence): `--live-code-selftest` -> PASS, including
  `[CAPABILITY_RESOLVED]` for `net.respawn`, `net.attack-claim`,
  `net.physical-contact` and all new checks.
- Runtime / human acceptance: pending.

## Stage B/C status

Stage B is now policy-complete: physical-contact (damage/knockback + episode
batching), damage-application, attack-gates, attack-claim, kill-attribution,
projectile-splash, and the pre-existing attack/tool-use/fire-intent seams. What
remains is pure geometry/mechanism (body-part volume test, contact detection).

Stage C is substantially complete: NPC targeting (hostility + score) and the
shared respawn rule are hot, alongside the pre-existing lifecycle/history/rewind
seams. Remaining cold policy is small: NPC combat/pellet-effect flags,
ground-clamp, player broadcast interpolation, and spawn safety/loadout selection.
