# Hot combat policy: generic tool-use + projectile-impact seams, rocket hot, new tool

- EST timestamp: 2026-09-14 12:36:57 EDT (UTC 2026-09-14T16:36:57Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS` (source + cold build + new `--hot-combat-selftest` + all other
  self-tests, including the recovered movement-parity test)

## Migration standard

1. **Old cold path.** Weapon/projectile type decisions lived in `combat/*` and
   `server-projectiles.cpp` as string/`behaviorType`/network-id switches
   (`projectileConfig` `server-projectiles.cpp:298`, `weaponExecutionTypeForBehavior`,
   `NETWORK_WEAPON_*`). Fire-intent, damage, and presentation were already hot
   seams, but the impact/use *decision* was cold and there was no way to add a
   new tool/projectile after startup.
2. **New hot path.** The kernel emits one generic fact per held tool use
   (`tool.primary-use`/`tool.alt-use`) and per projectile impact
   (`projectile.impact`). A hot DLL router dispatches the fact to the
   runtime-registered behavior for its key. Unregistered keys leave
   `handled = 0`, so the cold path remains in charge (temporary fallback).
3. **Kernel mechanisms retained.** Projectile spawn/id allocation, per-tick
   simulation, world/actor collision, knockback, networking, and the
   `GAME_EVENT_DAMAGE_POLICY`/`FIRE_INTENT`/`PROJECTILE_PRESENT` seams.
4. **New generic primitives.** Two general payloads (`ProjectileImpactPolicyV1`,
   `ToolUsePolicyV1`) with 64-bit runtime keys; an in-DLL behavior table
   (`HotPackageBuilder` tool/projectile registrars) so a tool/projectile is a
   composition, not a kernel category. No `WeaponType`/`ProjectileType` enum, no
   `GameWeaponModule`, no mode-specific ABI field.
5. **Why general.** The facts describe an occurrence; the key is a runtime id
   (network id or package hash); the kernel never switches on a weapon name.
6. **State ownership.** Tool state is package dynamic components; ownership is a
   generic relationship; the tool itself is a created entity.
7. **Migration / rollback.** Unchanged loader/generation guarantees; a failed
   candidate still retires and keeps last-good.
8. **Files changed.** See below.
9. **Evidence.** See below.
10. **Remaining cold pieces.** Authoritative item/projectile spawn from hot code,
    ammo/reload as generic state, a server-context damage capability, hitscan,
    and melee contact detection.
11. **Next cold call site.** The server needs a **server context capability
    provider** so hot behaviors can spawn projectiles and apply damage
    authoritatively (the blocker for full weapon ownership).

## What changed

- `game-api.h` (ABI 8): added generic `ProjectileImpactPolicyV1` and
  `ToolUsePolicyV1` payloads.
- `live-behavior.{h,cpp}`: `dispatchProjectileImpact` and `dispatchToolUse`
  (host context so handlers get capabilities).
- `server-projectiles.cpp`: at each projectile impact/lifetime event, dispatch
  `projectile.impact`; a handled result uses `outExplode` instead of the cold
  `explodeOn*` flags, and a handled non-explode lifetime end despawns cleanly.
- `server-attack.cpp`: dispatch `tool.primary-use` for each held-fire tick and
  each attack request; a handled `outFire == 0` suppresses the built-in fire.
- `hot-package.h`: DLL-side tool/projectile behavior tables + registrars +
  lookups.
- New hot modules: `modules/tools/combat-policy.cpp` (router),
  `modules/tools/rocket-policy.cpp` (real rocket impact policy),
  `modules/tools/banana-launcher.cpp` (brand-new tool + projectile + dynamic
  state + relationship). `hot-modules.json` gained the `tools/*.cpp` glob.
- New `network/hot-combat-selftest.{h,cpp}` + `--hot-combat-selftest`.

## Evidence

- `python build_agent.py` -> `Status: SUCCESS`.
- `mimita.exe --hot-combat-selftest` -> **PASS (11/11)**: a brand-new
  `banana.launcher` tool use is handled by its hot behavior (`outFire = 0`),
  records package dynamic state, and creates an owned tool entity across uses;
  the new `banana.projectile` and the real rocket (network id 5) impact policies
  are hot; unregistered tool/projectile keys correctly fall back to the cold
  path.
- Full self-test suite PASS: gamemode-hot, dynamic-lifecycle, movement,
  movement-parity (recovered), entity-slice, hot-authoritative, live-code,
  project, phase456, telemetry, creation, ragdoll-slice.
- `-fsyntax-only` clean for all changed cold sources and the new hot modules.

## Honest limitations

- A brand-new tool cannot yet be invoked from client input because the held-fire
  and attack paths are gated by registered network weapons; the headless test
  drives the generic fact directly. Wiring input to `tool.primary-use` for
  arbitrary tools is the next seam.
- Hot behaviors cannot yet spawn kernel-simulated projectiles or apply
  authoritative damage: the server keeps `players`/`projectiles` as function
  locals, so a generic **server context capability provider** is required. The
  new tool therefore proves registration/state/composition, not authoritative
  damage.
- No live single-process falsification or visual acceptance this pass.

## Next

- Add a server context capability provider (generic), then migrate the rocket
  launcher's authoritative spawn and damage fully hot; then hitscan/melee.
