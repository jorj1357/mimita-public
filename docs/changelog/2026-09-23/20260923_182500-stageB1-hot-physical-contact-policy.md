# Stage B slice 1: hot physical-contact damage/knockback policy

Date (UTC): 2026-09-23T18:25:00Z
Status: implemented; cold build and selftests verified; hot provider resolved and exercised

## Scope

First slice of Stage B (server combat policy). Move the physical-contact weapon
damage and knockback formulas out of cold code and behind a generic
`net.physical-contact` capability, using the same header-only shared-implementation
pattern as Stage A. The cold bridge still owns the weapon-definition parameter
resolution, contact detection, authoritative apply, and episode batching.

## Changes

1. **New `src/hot-reload/hot-physical-contact.h`**: POD
   `GamePhysicalContactDamageV1` / `GamePhysicalContactKnockbackV1` (behavior
   kind, resolved params, shape/velocity facts, normal) and the ONE shared
   implementation (`HotPhysicalContactImpl::damage` / `knockback`). It is the
   exact previous formula for Godball, QuickHit, and Swordsword.

2. **New hot module `src/hot-reload/modules/physical-contact-policy.cpp`**:
   registers the `net.physical-contact` capability provider (signature
   `sig.net.physical-contact.v1`); picked up by the existing `modules/*.cpp` glob.

3. **`src/network/server-physical-contact.cpp`**: `physicalContactDamage` and
   `physicalContactKnockback` now resolve the weapon-definition params, build the
   POD request, resolve the hot provider (falling back to the shared
   implementation), and map the result back. Contact detection, movement-contact
   recording, authoritative damage apply, and episode batching are unchanged.

4. **`src/live-code/live-code-selftest.cpp`**: added two checks — "hot
   physical-contact provider resolves" and "hot physical-contact sword damage".

5. **`src/hot-reload/hot-modules.json`**: added
   `src/hot-reload/hot-physical-contact.h` to `headers`.

## Evidence

- Source changes: `src/hot-reload/hot-physical-contact.h` (new),
  `src/hot-reload/modules/physical-contact-policy.cpp` (new),
  `src/network/server-physical-contact.cpp`, `src/live-code/live-code-selftest.cpp`,
  `src/hot-reload/hot-modules.json`.
- Build (source/build evidence): `python build_agent.py` -> `Status: SUCCESS`,
  `mimita-20260923T181933.exe`.
- Automated tests (test evidence):
  - `--live-code-selftest` -> PASS, including the two new checks and
    `[CAPABILITY_RESOLVED] provider=net.physical-contact`.
  - `--hot-combat-selftest` -> no physical-contact/damage failures. The
    pre-existing animation/pose/action-state failures from the concurrent
    uncommitted audio/animation work remain (25 this run vs 20 before; the extra
    is `per-tool phase drives distinct arm poses`), none related to this change.
- Runtime / human acceptance: pending.

## Remaining Stage B work

- `server-attack.cpp`: attack-request claim acceptance/geometry/cooldown policy
  (the `GAME_EVENT_ATTACK_POLICY` seam already exists).
- `server-damage.cpp`: friendly-fire, respawn rule, kill reattribution.
- `server-projectiles.cpp`: splash/explode/area-effect/impact policy; delete the
  dead `#if 0` block and the uncalled `handleGenericProjectileAttack`.
- `server-physical-contact.cpp`: contact detection/episode classification policy
  (the damage/knockback formulas are now hot).
