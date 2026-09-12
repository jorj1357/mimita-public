// 2026-09-12T01:14:56Z
/* purpose
* record the thrown-grenade weapon family (smoke / frag / fire) and its shared
* area-effect volumes, tick fuse, owner-velocity throw, server authority, and
* smoke vision fog
* preserve source and build evidence separately from unperformed runtime tests
* this file does NOT claim visual, multiplayer, or human acceptance
* this file does NOT replace the append-only regression record
*/

# Thrown grenade family: smoke, frag, fire

## Session

- Branch: `8292026stash`
- HEAD commit: `afad20a`
- Timestamp (UTC): `2026-09-12T01:14:56Z`
- Display timezone: America/New_York
- Display time: `2026-09-11 21:14:56 EDT`
- Pre-existing changes: the working tree already contained large unrelated
  uncommitted work (NPC mind/behavior, server-gamemode bomb refactor, map-config,
  gamemode changes). Those were preserved untouched. During the first build the
  uncommitted `src/network/server-gamemode.cpp` was mid-edit and failed to
  compile (`getEntityRootPos` not declared, `broadcastBombTagState` ambiguous);
  the file changed on disk during the build (21:12:11) and a subsequent build
  succeeded. That break was NOT introduced by this session.

## 1. Files changed

New:
- `src/combat/area-effect.h` / `area-effect.cpp`: shared persistent smoke/fire
  volume store, camera smoke query, fire damage-over-time, and rendering, with
  hot-reloadable params parsed from `WeaponDefinition::customParams`.

Modified:
- `config/weapons.json`: `grenade_smoke` (slot 13), `grenade_frag` (slot 14),
  `grenade_fire` (slot 15) definitions and all tunables.
- `config/weaponsets.json`: new "Grenades" set (id 8).
- `src/combat/weapon-types.h`: `WeaponBehaviorType::Grenade`, mapped to
  `WeaponExecutionType::Projectile`.
- `src/combat/weapon-data.cpp`: builtin factories and registration for the three
  thrown grenades.
- `src/combat/weapon-json-config.cpp`: `behavior_type: "thrown_grenade"` parse.
- `src/combat/weapon-system.cpp`: fire/update dispatch for `Grenade`.
- `src/network/packets.h`: `weaponDefNetworkId` (uint16) added to projectile
  spawn / state / explode event packets (same byte size as reserved padding).
- `src/network/server.h`: `ServerProjectile` gains `weaponDefNetworkId`,
  `fullDamageRadius`, `edgeDamage`, `splashEnabled`; `ServerDamageSource::Fire`.
- `src/network/server-attack.cpp`: projectile dispatch includes `Grenade`.
- `src/network/server-projectiles.cpp`: fuse-ticks/second conversion, owner
  velocity inheritance, `on_expire_effect` area spawn, linear frag falloff with
  damage-scaled knockback, per-tick fire DoT and damage replication.
- `src/network/network-weapons.cpp`: three grenade ids map to the projectile
  family; slots 13/14/15 recognized.
- `src/network/multiplayer-context.h`: `NetworkProjectile` gains
  `weaponDefNetworkId`, `fullDamageRadius`, `edgeDamage`.
- `src/network/multiplayer-projectiles.cpp`: prediction/reconciliation for the
  exact grenade definition, owner velocity, fuse, and area-effect spawn; the
  predicted frag/self-knockback falloff now mirrors the server's linear
  `full_damage_radius`/`edge_damage` curve.
- `src/network/server.cpp` and `src/network/multiplayer-packets.cpp`: clear the
  shared area-effect store on server state reset and client context reset so
  smoke/fire cannot outlive a reset whose tick counter restarted at 0.
- `src/render/render-world-mesh.cpp` + `shaders/basic.frag`: inside-smoke world
  fog (uniforms `uFogEnabled`, `uFogColor`, `uFogCameraPos`, `uFogMaxView`).
- `src/gui/hud/player-nameplates.cpp`: healthbars suppressed while the camera is
  inside smoke.
- `src/engine/engine-tick-render.cpp`: area-effect render pass.

## 2. Design

- Thrown grenades are a new `Grenade` behavior executed through the existing
  shared projectile kernel; no parallel physics path was added. The launch
  velocity is `aimDir * projectile_speed + up_bias + ownerVelocity`.
- Fuses are authored in server ticks (`fuse_ticks`, default 180) and converted
  once to the kernel's seconds so client prediction and server authority match
  at the fixed 60 Hz.
- On projectile end the authoritative server resolves the exact definition via
  the new `weaponDefNetworkId` and either performs a splash explosion (frag) or
  spawns an area volume (smoke/fire). `splashEnabled=false` skips the splash
  loops for area grenades.
- Frag falloff is `full_damage_radius:3 -> 150`, `splash_radius:10 -> 10`, with
  knockback scaled by the damage fraction.
- Smoke is a full-opaque grey sphere (default radius 10, lifetime 600 ticks)
  that fogs world geometry beyond `smoke_visibility_meters` (default 1) and can
  hide healthbars. Fire is a cycling red/orange/white cylinder (default radius
  8, height 8, lifetime 480 ticks) that deals 5 damage per 10 ticks while a
  player is inside; the server owns the damage and replicates it.

## 3. Hot-reload keys (config/weapons.json `custom_params`)

- Throw/fuse: `throw_speed`, `up_bias`, `inherit_owner_velocity`, `fuse_ticks`.
- Impulse: `explodeOnPlayerImpact`, `explodeOnWorldImpact`, `explodeOnLifetime`.
- Effect select: `on_expire_effect` (0 explosion, 1 smoke, 2 fire).
- Frag: `splashRadius`, `rocketDirectDamage`, `full_damage_radius`,
  `edge_damage`, `knockbackStrength`.
- Smoke: `smoke_radius`, `smoke_lifetime_ticks`, `smoke_visibility_meters`,
  `smoke_hide_healthbars`, `smoke_color_r/g/b/a`.
- Fire: `fire_radius`, `fire_height`, `fire_lifetime_ticks`, `fire_cycle_speed`,
  `fire_damage_per_interval`, `fire_damage_interval_ticks`.

## 4. Build evidence

- Command: `python build_agent.py`
- Result: `Status: SUCCESS` (`Return Code: 0`); final run compiled 1 changed
  translation unit and linked `mimita.exe` (481 skipped as unchanged).
- The successful builds compiled and linked all new/modified translation units
  (including `src/combat/area-effect.cpp`, `weapon-data.cpp`, `weapon-system.cpp`,
  `server-projectiles.cpp`, `server.cpp`, `multiplayer-projectiles.cpp`,
  `multiplayer-packets.cpp`, `packets.cpp`, `render-world-mesh.cpp`,
  `player-nameplates.cpp`).
- Note: a first build attempt failed only in the pre-existing uncommitted
  `server-gamemode.cpp`; that file was fixed externally and subsequent builds
  succeeded.

## 5. Runtime evidence

- NOT performed. No in-engine throw, fuse, explosion, smoke, fire, fog, or
  multiplayer test was run or observed by this session. Build success proves
  compilation and linking only.

## 6. Human acceptance still required

- Throw inherits player velocity and look direction (run forward vs stand still).
- 180-tick fuse; frag radius damage (3 m ~150, 10 m ~10).
- Smoke sphere opacity, inside-smoke 1 m fog, and healthbar toggle.
- Fire cylinder color cycle and 5 dmg / 10 ticks DoT.
- Online prediction/reconciliation with two clients and no duplicate projectiles.

## 7. Regressions

- No regression recorded. No previously working behavior was intentionally
  changed; the new weapon ids occupy slots 13/14/15 and are additive.
- The unrelated `server-gamemode.cpp` compile break described in Session is
  pre-existing uncommitted work and is not recorded as a regression.

## 8. Spec TODOs

- No `todo: explain this better` markers were found under `docs/`.

## 9. Smallest logical next phase

- Runtime-validate the smoke fog and fire DoT in a two-client match, then tune
  the hot-reload values; add the three grenades to role/gamemode loadouts once
  accepted.
