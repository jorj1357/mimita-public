# Projectile Rifle MVP

Date: 2026-09-26

## Request

Add a slot-14 automatic projectile-rifle test weapon using the existing authoritative projectile path, a stretched revolver viewmodel/collision shape, a visible white sphere projectile, and lightweight staged world-impact visuals.

## Ownership decision

The rifle reuses the existing generic projectile architecture. `WeaponSystem` only prepares the local fire result and prediction; `weapon-commands.cpp` sends the shared `AttackRequest`; `server-attack.cpp` accepts every `WeaponExecutionType::Projectile`; `server-projectiles.cpp` owns fixed-tick movement, world/entity impact, and authoritative player/NPC damage; `multiplayer-projectiles.cpp` owns client prediction/adoption and rendering. No rifle-specific damage subsystem was added.

The rifle uses native slot 14 because slots 11, 12, and 13 are already Quick Hit, Spyknife, and Force Punch. The wildcard weapon set includes it without renumbering existing weapons.

## Changed files

- `config/weapons.json`: rifle tuning, automatic fire, revolver model reuse, local +Y stretch, projectile visual and impact parameters.
- `config/weaponcollisions.json`: stretched barrel capsule for the rifle world representation.
- `src/combat/weapon-data.{h,cpp}`: built-in definition and registration.
- `src/combat/weapon-system.{h,cpp}`, `src/combat/weapon-fire.{h,cpp}`: generic projectile fire result and configurable random/fixed/blended spray direction so the `Projectile` behavior no longer falls through to hitscan.
- `src/network/packets.h`, `src/network/network-weapons.cpp`: legacy packet identity and slot/name/family mapping.
- `src/network/server-attack.cpp`, `src/network/server-projectiles.cpp`: generic projectile acceptance and rifle config lookup.
- `src/network/multiplayer-projectiles.cpp`: rifle prediction/adoption/visual lookup.
- `src/combat/projectile-render.{h,cpp}`: configurable filled-sphere projectile presentation.
- `src/combat/explosion-fx.cpp`: three pooled effect stages for rifle impacts.

## Evidence

- Source: JSON parses successfully; rifle is registered as `projectile_rifle`, native slot 14, and legacy network type 11.
- Build: `python build_agent.py` completed successfully on 2026-09-26 at 22:34:03; 91 files compiled, 391 skipped, return code 0.
- Incremental build: completed successfully at 22:34:27; one linker emitted an existing `.rsrc merge failure: unexpected .rsrc size` diagnostic, but the build system linked/staged the executable and returned code 0.
- Final incremental build: completed successfully at 22:35:09 after the spray-direction adjustment; 2 files compiled, return code 0, with no linker diagnostic.
- Static validation: `git diff --check` completed without whitespace errors.
- Runtime: not performed in this pass. The built executable is not evidence that a live room loaded the weapon, sent the request, spawned the projectile, or applied damage.
- Human acceptance: still required in an actual room against a player and NPC, including automatic fire cadence, world disappearance, impact stages, and hot JSON tuning.

## Known scope

Projectile damage intentionally reuses the existing splash/contact damage owner. The rifle definition carries revolver-like damage/headshot values for future direct body-part projectile work, but this MVP does not duplicate hitscan headshot/falloff logic inside projectile simulation.
