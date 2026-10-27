# Deathball radius and melee poses

**Date:** 2026-10-27 01:53:46 America/New_York (2026-10-27 05:53:46 UTC)  
**Branch:** `20261011zt`  
**Base commit:** `eb79b2ded3abc85af6646dab829456006c1b3cbc`

## Changes

- Kept Deathball's configured `projectile_radius`, `rocketRadius`, and visual radius at `5.0` in `config/weapons.json`. The renderer now takes its Deathball sphere radius from the weapon definition, matching the server projectile radius.
- Fixed JSON-only weapon definitions being skipped on `weapons.json` reload after their first registration. This was why changing Deathball's radius did not affect the running game. The fix also refreshes current melee move-set data while retaining constructor-owned definitions for built-in weapons.
- Added JSON arm poses in `config/animations.json`: SwordSword idle, slash one, slash two, right attack, and defense; Chainsaw idle and rev; Mace idle and windup, active, and recovery.
- Routed local and replicated melee action states to those poses. Configured arm poses now take precedence over the legacy procedural melee arm transform during those actions.
- Added actor pose-load, reload-rejection, and pose-selection diagnostics, plus Deathball server-spawn and client-receipt diagnostics.

## Files

- `config/animations.json`
- `config/weapons.json`
- `src/combat/melee-client.cpp`
- `src/combat/projectile-render.cpp`
- `src/combat/weapon-json-config.cpp`
- `src/combat/weapon-json-config.h`
- `src/combat/weapon-system.cpp`
- `src/entities/player-animation-config.cpp`
- `src/entities/player-animation.cpp`
- `src/entities/player.h`
- `src/network/multiplayer-interpolation.cpp`
- `src/network/multiplayer-projectiles.cpp`
- `src/network/server-projectiles.cpp`

Pre-existing edits in `config/accounts/default.json`, `config/audio/music-settings.json`, and unrelated weapon viewmodel placements were preserved. Deathball's three radius values were pre-existing user changes and remain at `5.0`.

## Validation and evidence

- `config/animations.json` and `config/weapons.json` parsed as valid JSON after restoring the temporary live-edit checks. `git diff --check` passed.
- Canonical dev-loop build 3449 succeeded for source generation 46 (2 compiled, 579 skipped). Build SHA-256: `1b45cf61787391f3368dd9f995fecee3786540b416f3cbfc2124ae908d817724`.
- The exact `.dev/builds/3449/mimita.exe --versioninfo` reported `logs/10-27-2026/20261027_015224/events.jsonl`. The dev loop launched build 3449 on `dust2cyberiav4`; client and server were ready and reported matching build identities.
- In a live run, temporarily changing Deathball's `projectile_radius` to `4.0` produced a server `deathball.spawn` record with `radius_m=4.0` and `visual_radius_m=4.0`. Restoring the config to `5.0` produced subsequent spawn records with both fields at `5.0`. This confirms the JSON-only weapon reload fix and live server/render radius selection.
- The current run loaded 72 weapon poses on both client and server. Runtime `weapon.pose.selected` records showed SwordSword `slash_one`, Chainsaw `rev`, and Mace `mace_swing_windup` selected on the local actor and NPCs.
- A live pose-angle edit caused client and server load events without restarting. A temporary malformed `animations.json` caused both to log `anim.weapon_poses.reload_rejected` with “previous valid pose set retained”; restoring valid JSON loaded the 72-pose set again.
- A live screenshot showed the cyan Deathball projectile. The test did not capture a remote-client receipt event because the exercised shot belonged to the local player and its server spawn is not echoed back to its owner. The client receive path is implemented, but remote projectile replication was not runtime-confirmed in this session.
- No `deathball.contact`/damage event was captured, so projectile damage range is source-traced but not runtime-confirmed. The arm poses were selected in runtime on local and NPC actors, but this camera view did not provide a clear human visual review of the arm silhouettes or the full Mace phase sequence.

## Review basis

Followed `docs/ROUTER.md` routes for the weapon and animation specifications, JSON configuration ownership, runtime-scenario validation, dev-loop/build procedures, task completion, and focused weapon/animation review. Applied the `computer-use` skill for in-game visual inspection. No synthetic gameplay test was added.
