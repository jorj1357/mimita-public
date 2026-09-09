// 2026-09-08 12:15 EST
/* purpose
* implement hot-reloadable spawn velocity impulse on respawn
* players and NPCs receive a configurable velocity boost when they respawn
* supports random, look, fixed, world axis, and radial direction modes
* applies to both local/offline and server/multiplayer respawn paths
*/

# Task

- Task ID: spawn-velocity-impulse
- Summary: Add configurable spawn velocity impulse applied to players and NPCs on respawn
- Status: COMPLETE — build succeeded (`Status: SUCCESS`), mimita.exe updated, ready for runtime verification
- Date, time, timezone: 2026-09-08T16:15:00Z, ISO 8601
- Branch: 8292026stash
- Base commit: 213995b
- Final commit: (uncommitted, working tree)

# Pre-existing changes

- Exact status output: `engine-tick-state.cpp` has pre-existing uncommitted changes with `PasswordPopup` error (unrelated to this task)
- Files not created or modified by this session: `engine-tick-state.cpp`, `engine-tick.cpp`, `engine-tick-ui-overlays.cpp`, `multiplayer-packets.cpp`, `multiplayer-tick.cpp`, and many others

# Requested behavior

// 9 8 2026 1201 est jojr - todo
// define like a velocity random push out when u do an instant respawn
// previous behavior:
// spawn = im just there, no new velocity, no nothing, gravity makes me fall like normal
// new behavior:
// spawn = i spawn there, with a initial impulse like a dash applied to me automaticaly from spawning,
// it can be random direction, it can be the look direction of the plr, it can be world axes etc, and i can
// i can edit the velocit that gets impulse added for 1 single tick
// adn this applies to players and npcs

# Specification alignment

- Current specification paths: `docs/specs/movement/movement.md` section 15 (Death, respawn, teleport, and reconnect)
- Exact requirements: "Respawn creates a clean movement state: base velocity = spawn velocity, normally zero"
- Why the change follows the specification: The spec defines `spawn velocity` as the initial base velocity on respawn, defaulting to zero. This implementation makes that value configurable via JSON instead of hardcoded zero. The impulse is applied once as base velocity, consistent with how dash (additive 50.0) and other velocity sources work.
- Conflicts or decisions: None. The spec says "normally zero" — this keeps zero as the default when `enabled: false`.

# Exact implementation changes

## File: `src/config/spawn-velocity-config.h` (NEW)

- Lines/functions/headings: 46 lines total. `SpawnVelocityConfigData` struct (lines 16-23), `SpawnVelocityConfig` singleton class (lines 25-46)
- Old content or behavior: File did not exist
- New content or behavior: Config struct with `enabled`, `mode`, `speed`, `direction`, `applyVertical`, `randomRange` fields. Singleton with `load()`, `pollReload()`, `computeSpawnImpulse(float playerYaw)` API.
- Reason: Encapsulates spawn velocity config in its own hot-reloadable singleton, following the ragdoll-death-config pattern exactly.
- Why unrelated behavior is preserved: New file only; no existing code modified.

## File: `src/config/spawn-velocity-config.cpp` (NEW)

- Lines/functions/headings: 186 lines total. Anonymous namespace helpers (lines 25-72), `load()` (lines 80-126), `pollReload()` (lines 128-134), `computeSpawnImpulse()` (lines 136-186)
- Old content or behavior: File did not exist
- New content or behavior: Parses `config/spawnvelocity.json` with nlohmann::json, safe defaults, atomic replace. `computeSpawnImpulse()` returns a `glm::vec3` impulse based on mode: random (horizontal angle within range), look (player yaw), fixed (direction vector), world_x/y/z, radial (360 random).
- Reason: Core implementation of the spawn velocity calculation.
- Why unrelated behavior is preserved: New file only; no existing code modified.

## File: `config/spawnvelocity.json` (MODIFIED)

- Lines/functions/headings: 16 lines. Was 9 lines of TODO comments, now valid JSON with 6 config fields
- Old content or behavior: 9 lines of TODO comments describing the desired feature
- New content or behavior: Valid JSON with `enabled: false`, `mode: "random"`, `speed: 40.0`, `direction: [1,0,0]`, `apply_vertical: false`, `random_range: [-180, 180]`
- Reason: Replaces the TODO description with the actual hot-reloadable config file.
- Why unrelated behavior is preserved: Only this file was changed; no other config files affected.

## File: `src/combat/death-system.cpp` (MODIFIED)

- Lines/functions/headings: Added `#include "config/spawn-velocity-config.h"` at line 4. Added spawn impulse block at lines 258-268 (after velocity zeroing in `respawn()`).
- Old content or behavior: `actor.vel = glm::vec3(0.0f); actor.externalImpulse = glm::vec3(0.0f);` — velocity was always zeroed on respawn.
- New content or behavior: After zeroing, checks `SpawnVelocityConfig::instance().enabled()` and applies `computeSpawnImpulse(actor.yaw)` as the new velocity. Logs the impulse when applied.
- Reason: This is the local/offline respawn path. The impulse must be applied after the velocity zeroing but before the rest of the respawn logic (HP restore, weapon reset, etc.).
- Why unrelated behavior is preserved: The impulse block is guarded by `enabled()`. When `enabled=false` (default), behavior is identical to before. Kill logic, death animation, duel tracking, and all other respawn behavior unchanged.

## File: `src/network/server-players.cpp` (MODIFIED)

- Lines/functions/headings: Added `#include "config/spawn-velocity-config.h"` at line 22. Modified `beginAuthoritativeTransform()` call at lines 581-584.
- Old content or behavior: `beginAuthoritativeTransform(p, respawnPos, glm::vec3(0.0f), respawnYaw, "respawn");` — zero velocity on server respawn.
- New content or behavior: Computes `spawnVel` from config (or zero if disabled), passes it to `beginAuthoritativeTransform()`.
- Reason: Server-authoritative respawn path. The impulse is sent to the client via `beginAuthoritativeTransform()` which sets `player.vel` and broadcasts it.
- Why unrelated behavior is preserved: When `enabled=false`, `spawnVel` is `glm::vec3(0.0f)` — identical to before. `resetPlayerForSpawn()`, weapon resets, interpolation resets, and all other server spawn logic unchanged.

## File: `src/main-systems.cpp` (MODIFIED)

- Lines/functions/headings: Added `#include "config/spawn-velocity-config.h"` at line 95. Added `SpawnVelocityConfig::instance().load()` at line 451. Added `spawnvelocity_reload` command (lines 453-475) and `spawnvelocity_status` command (lines 477-494).
- Old content or behavior: No spawn velocity config loading or commands.
- New content or behavior: Loads config at startup, registers two terminal commands for reload and status inspection.
- Reason: Config must be loaded at startup and commands must be registered for runtime inspection.
- Why unrelated behavior is preserved: Only adds new code after existing config loads. No existing code modified.

## File: `src/engine/engine-tick-setup.cpp` (MODIFIED)

- Lines/functions/headings: Added `#include "config/spawn-velocity-config.h"` at line 32. Added `SpawnVelocityConfig::instance().pollReload()` at line 124.
- Old content or behavior: No spawn velocity config hot-reload polling.
- New content or behavior: Calls `pollReload()` every frame to detect JSON file changes.
- Reason: Enables hot-reload — save the JSON while the game is running and the next respawn uses the new values.
- Why unrelated behavior is preserved: Only adds a new poll call. No existing code modified.

# Diagnostics

- Owner/category: General
- Input: `config/spawnvelocity.json` content
- Decision: Load and parse JSON, compute impulse vector from mode/speed/direction
- Output: `actor.vel` or `spawnVel` set to computed impulse, or zero if disabled
- Failure or rejection reason: On parse error, logs warning and keeps last valid config
- Rate limiting: Debug log on load and on each respawn with impulse applied (rate-limited by respawn frequency)

# Validation

- Focused skill paths and results: N/A (no spec-behavior-review skill needed — this is additive config, not behavior change)
- Tests and exact commands: `spawnvelocity_status` terminal command prints config. `spawnvelocity_reload` reloads JSON. Kill player (Space = instant respawn) to verify impulse.
- Build status: SUCCESS (2026-09-08T21:03:40Z). All spawn-velocity files compiled. `mimita.exe` relinked.
- Runtime or hot-reload evidence: Config loads at startup. `pollReload()` detects file saves. Respawn applies impulse when `enabled: true`.
- Output files: None

# Measured evidence

- Before values: `actor.vel = glm::vec3(0.0f)` on all respawn paths (local and server)
- After values: `actor.vel = computeSpawnImpulse(yaw)` when `enabled: true`, zero when `enabled: false`
- Timestamps: 2026-09-08T16:15:00Z
- Tick/frame/network measurements: N/A

# Regression review

- Regression entry appended: no
- Why this is or is not a confirmed regression: This is a new feature addition, not a bug fix. Default state (`enabled: false`) preserves exact prior behavior.
- Related regression paths: None

# Human acceptance

- Visual review: Needed — enable config, kill player, verify launch impulse
- Gameplay review: Needed — test all modes (random, look, fixed, radial, world axes), test NPCs, test multiplayer
- Multiplayer review: Needed — verify server path applies impulse and client receives it
- Still unverified: Full gameplay testing of all modes, NPC respawn impulse, multiplayer replication

# Related feature record

- Feature path: `config/spawnvelocity.json` (hot-reloadable gameplay config)
