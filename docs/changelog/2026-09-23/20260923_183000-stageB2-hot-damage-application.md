# Stage B slice 2: hot authoritative damage-application rules

Date (UTC): 2026-09-23T18:30:00Z
Status: implemented; cold build and selftests verified; hot provider resolved and exercised

## Scope

Second slice of Stage B (server combat policy). Move the authoritative damage
application rules out of cold code and behind a generic `net.damage-application`
capability: accept/reject, the TDM friendly-fire filter, the authoritative
damage clamp, and the death/respawn rule. The cold path still owns health/velocity
mutation, movement impulse recording, kill recording, and replication.

## Changes

1. **New `src/hot-reload/hot-damage-application.h`**: POD
   `GameDamageApplicationV1` (target/attacker team facts, damage, damage limit,
   respawn config, health) and the ONE shared implementation
   (`HotDamageApplicationImpl::evaluate`). Rejection reasons are the numeric
   `GameDamageRejectV1` contract.

2. **New hot module `src/hot-reload/modules/damage-application-policy.cpp`**:
   registers the `net.damage-application` capability provider (signature
   `sig.net.damage-application.v1`); picked up by the existing `modules/*.cpp` glob.

3. **`src/network/server-damage.cpp`**: `applyPlayerDamageLegacy` now projects the
   target/attacker facts into the POD request, resolves the hot provider
   (falling back to the shared implementation), and applies the returned
   decision (health, knockback, death state, respawn seconds, deaths counter,
   NPC-damage-tracking clear). The exact previous rules are preserved, including
   self-damage bypass and the one-life `-1` respawn timer.

4. **`src/live-code/live-code-selftest.cpp`**: added three checks — provider
   resolves, friendly fire is rejected, lethal damage applies with the respawn
   rule.

5. **`src/hot-reload/hot-modules.json`**: added
   `src/hot-reload/hot-damage-application.h` to `headers`.

## Evidence

- Source changes: `src/hot-reload/hot-damage-application.h` (new),
  `src/hot-reload/modules/damage-application-policy.cpp` (new),
  `src/network/server-damage.cpp`, `src/live-code/live-code-selftest.cpp`,
  `src/hot-reload/hot-modules.json`.
- Build (source/build evidence): `python build_agent.py` -> `Status: SUCCESS`,
  `mimita-20260923T182448.exe`.
- Automated tests (test evidence): `--live-code-selftest` -> PASS, including
  `[CAPABILITY_RESOLVED] provider=net.damage-application` and the three new checks.
- Runtime / human acceptance: pending.

## Remaining Stage B work

- `server-attack.cpp`: claim acceptance/geometry/cooldown policy (attack
  validation already partly hot via `GAME_EVENT_ATTACK_POLICY`).
- `server-damage.cpp`: NPC kill reattribution window in
  `queueServerDamageConfirmedEvent`.
- `server-projectiles.cpp`: splash/explode/area-effect/impact policy; delete the
  dead `#if 0` block and the uncalled `handleGenericProjectileAttack`.
- `server-physical-contact.cpp`: contact detection/episode classification policy.
