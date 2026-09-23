# Stage B6: hot projectile splash falloff policy

Date (UTC): 2026-09-23T18:45:00Z
Status: implemented; cold build and selftests verified; hot provider resolved and exercised

## Scope

Further Stage B (server combat policy). Move the projectile explosion falloff
curves out of cold code and behind a generic `net.projectile-splash` capability:
damage by distance and the knockback scale by distance. The cold path still owns
projectile state, victim loops, line-of-sight, and the authoritative damage apply.

## Changes

1. **New `src/hot-reload/hot-projectile-splash.h`**: POD `GameSplashFalloffV1`
   and the ONE shared implementation (`damage` / `knockScale`) — the exact
   previous `splashDamageAt` / `splashKnockScaleAt` formulas (full-damage radius
   mix and the exponential falloff).

2. **New hot module `src/hot-reload/modules/projectile-splash-policy.cpp`**:
   registers the `net.projectile-splash` capability provider.

3. **`src/network/server-projectiles.cpp`**: the two `explodeProjectile` lambdas
   now resolve the hot provider (falling back to the shared implementation). All
   call sites are unchanged.

4. **`src/live-code/live-code-selftest.cpp`**: added two checks — provider
   resolves and full-radius damage.

5. **`src/hot-reload/hot-modules.json`**: added
   `src/hot-reload/hot-projectile-splash.h` to `headers`.

## Evidence

- Build (source/build evidence): `python build_agent.py` -> `Status: SUCCESS`,
  `mimita-20260923T184436.exe`.
- Automated tests (test evidence): `--live-code-selftest` -> PASS, including
  `[CAPABILITY_RESOLVED] provider=net.projectile-splash` and the new checks.
- Runtime / human acceptance: pending.

## Stage B status after this slice

Hot now: physical-contact damage/knockback, damage-application rules, attack
gates, kill attribution, projectile splash falloff, plus the pre-existing attack
policy/tool-use/fire-intent seams. Dead code removed.

Remaining Stage B: the attack client hit-claim acceptance ("shoot what I saw")
geometry block, and physical-contact contact detection/episode classification.
