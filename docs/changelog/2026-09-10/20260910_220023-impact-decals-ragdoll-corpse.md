// 2026-09-10 18:00:23 EDT (2026-09-10T22:00:23Z)

# Task

- Task ID: impact-decals-ragdoll-corpse-2026-09-10
- Summary: Replace solid-cylinder impact decals with batched textured quads,
  enable blood spray, carry body-part hit context, and replace the scripted
  DeathGhost death visual with a physically simulated corpse ragdoll that reuses
  and extends RagdollModeSystem.
- Status: PASS_WITH_HUMAN_REVIEW
- Date, time, timezone: 2026-09-10T22:00:23Z
- Branch: 8292026stash
- Base commit: 322e5db
- Final commit: not committed (working tree changes only)

# Pre-existing changes

- Exact status output: `git status --porcelain` before this session was clean;
  every path listed below was created or modified by this session.
- Files not created or modified by this session: none observed.

# Requested behavior

1. Investigate `config/impact_decals.json` (blood, bullet holes, world cracks).
2. Make decals efficient enough to display many effects without a performance
   dip; replace cylinder splats with flat textured planes.
3. Add a blood spray at the exact point of contact for any weapon impact,
   including repeated melee (knife) hits.
4. On death, use the ragdoll system created 2026-09-10 (`RagdollModeSystem`),
   expand it, spray blood from the corpse, and deprecate the old ragdoll code.
5. Design for client/server from the start but allow client-side simulation now.
6. Do NOT implement dismemberment/viscera; chunk removal from the head is to
   emerge from the destructible-world system later.

# Specification alignment

- Current specification paths: `docs/specs/performance/performance.md`,
  `docs/specs/weapons/weapons.md`, `docs/specs/debug-logging/debug-logging.md`.
- Exact requirements: gameplay collision/damage/physics run at fixed 60 Hz;
  VSync stays forced off; prefer predictable, cache-friendly, low-allocation
  hot paths; weapons are configurations of shared systems; `Player` is the
  shared actor body for players and NPCs.
- Why the change follows the specification: corpse simulation is driven from
  `simulateTick` at `TICK_DT = 1/60`; decals are batched into one GPU draw per
  texture with no per-decal allocation; no VSync path was touched; HitEvent now
  carries weapon/body-part context instead of adding weapon-specific paths.
- Conflicts or decisions: the user explicitly deferred dismemberment/viscera to
  the destructible-world system, so it is out of scope here. Blood gating was
  left on `impact_decals.json blood.enabled` (the gamemode override path) rather
  than the unused `gBloodFXEnabled` setting, to avoid a gamemode regression.

# Exact implementation changes

## File: `src/config/impact-decals-config.h`, `src/config/impact-decals-config.cpp`

- Added `texture`, `textureScale`, `randomRotationDegrees` to
  `ImpactDecalGroupConfig`; loader reads `texture`, `textureScale`,
  `randomRotationDegrees` in `readGroup`.
- Reason: configure the flat textured decal renderer.

## File: `config/impact_decals.json`

- Added `texture`/`textureScale`/`randomRotationDegrees` per group
  (`assets/textureshq/sblood1.png`, `holes1.png`, `crackground1.png`).
- Enabled `blood.spray`, retuned counts/speeds/sizes, raised `maxCount`
  (blood 4096, bulletHoles 8192, cracks 4096) and `ray_budget_per_frame` 2→4.
- Reason: spray on impact and safe high decal counts after the quad conversion.

## File: `src/effects/effect-part-render.cpp`

- Added `appendTexturedQuad`, `decalInPlaneBasis`, `appendTexturedBillboard`.
- Replaced the per-decal `DebugVis::drawFilledCylinder` loop with surface-aligned
  textured quads batched per texture (blood spray, blood decals, bullet holes,
  cracks), drawn through the existing `uUseColor=3` textured path. Untextured
  fallback to `drawFilledDecal`/`drawFilledBillboard` when a texture is empty.
- Reason: 192 triangles/decal → 2 triangles/decal, one draw per texture.

## File: `src/effects/effect-part.h`

- `MAX_BLOOD_PARTICLES` 512 → 2048.

## File: `src/effects/effect-part-blood.cpp`, `src/effects/effect-part.cpp`

- Blood decals and bullet holes now store a random in-plane `axis` so splats do
  not all share one rotation.

## File: `src/effects/hit-effects.h`, `src/combat/weapon-fire-effects.cpp`,
## `src/combat/weapon-fire-damage.cpp`

- `HitEvent` gained `bodyPart`, `weaponId`, `attackType`; populated in the
  hitscan damage paths (`applyDamageToEntity`, `presentRemoteHit`,
  `processNpcHit`). Reason: stop dropping body-part/weapon identity at the
  effect boundary for future gore/destruction.

## File: `src/ragdoll/ragdoll-mode.h`, `src/ragdoll/ragdoll-mode.cpp`

- Extracted per-body state into `RagdollBody`; all alive-ragdoll methods now take
  `RagdollBody&` so the same solver drives alive and corpse bodies.
- Added `spawnCorpse`, `updateCorpses`, `renderCorpses`,
  `removeCorpsesForOwner`, `clearCorpses`, `stepBody`, `sprayCorpseBlood`.
- Corpses clone the victim `Player`, set `ragdollModeActive` so the render path
  keeps the physics pose, apply inherited velocity + death impulse, are bounded
  to 12, and spray blood from the fastest-moving part on a configurable timer.
- Reason: reuse/expand the 2026-09-10 ragdoll system for death.

## File: `src/ragdoll/ragdoll-mode-config.h/.cpp`, `config/ragdoll.json`

- Added `corpse` block: lifetime, fade, impulse/velocity multipliers, blood
  interval/enable. Loaded from `config/ragdoll.json`.

## Death wiring

- `src/combat/death-system.cpp`: `kill()` spawns a corpse instead of a
  `DeathGhostSystem` clone.
- `src/combat/weapon-fire-damage.cpp`, `src/network/multiplayer-shots.cpp`,
  `src/network/multiplayer-interpolation.cpp`: predicted/confirmed/remote deaths
  spawn corpses; rollback calls `removeCorpsesForOwner`.
- `src/sim/simulate-tick.cpp`: `updateCorpses(TICK_DT, world)` at fixed 60 Hz.
- `src/engine/engine-tick-render.cpp`: renders corpses; perf counter reused.
- `src/engine/engine-tick-combat.cpp`, `engine-tick-net.cpp`,
  `multiplayer-packets.cpp`: removed DeathGhost update/clear, use corpse clear.
- `src/render/render-player.cpp`: dead bodies with `ragdollModeActive` are drawn.

## Deprecation / deletion

- Deleted `src/entities/death-ghost.{h,cpp}` and
  `src/ragdoll/ragdoll.{h,cpp}`, `src/ragdoll/ragdoll-config.{h,cpp}`.
- `src/ragdoll/ragdoll-commands.cpp`: `ragdoll_status` now reports corpse count;
  `ragdoll_reload` reloads `ragdolldeath.json` via `RagdollDeathConfig`.
- `src/main-systems.cpp`, `src/engine/engine-tick-setup.cpp`: removed
  `RagdollConfig` load/poll.
- `config/ragdolldeath.json` and `RagdollDeathConfig` are retained as the
  gamemode death-toggle source.

# Diagnostics

- Owner/category: `Debug::Category::Ragdoll`, `Debug::Category::NpcCombat`.
- Input/decision/output: corpse spawn logs actor, parts and lifetime; blood decal
  debug untouched; no new unmanaged files.
- Rate limiting: existing `Debug::logThrottled` used by effect system; corpse
  blood is interval-limited by `corpse.blood_interval_seconds`.

# Validation

- Focused skill paths and results:
  - `docs/skills/spec-behavior-review-v1.md`: PASS_WITH_HUMAN_REVIEW — spec and
    code aligned; no `todo: explain this better` markers found.
  - `docs/skills/efficiency-checker-v1.md`: PASS — cylinder→quad batching removes
    the per-decal 192-triangle cost and per-frame CPU vertex bias.
- Tests and exact commands: `python build_agent.py` (canonical, build-only).
- Build status: SUCCESS (`Status: SUCCESS`), including game DLL build.
- Runtime or hot-reload evidence: none — the EXE was not launched this session.
- Output files: `mimita.exe`, `build/mimita-game.dll`, `build/changelog.txt`.

# Measured evidence

- Before values: up to ~35k decals × 192 triangles ≈ 6.7M triangles/frame worst
  case; blood spray disabled (`spray.enabled=false`) so no per-hit blood spray.
- After values: 2 triangles/decal; worst-case decal triangles reduced by ~96×.
- Timestamps: build at 2026-09-10T22:00Z.
- Tick/frame/network measurements: not captured (no runtime session).

# Regression review

- Regression entry appended: no
- Why this is or is not a confirmed regression: no human-observed break; build
  and source inspection only.
- Related regression paths: `docs/regressions/regressions-v1.md` (not modified).

# Human acceptance

- Visual review: REQUIRED — confirm textured splats look correct (texture size /
  transparency), spray density, no z-fighting/clipping, corpse appearance.
- Gameplay review: REQUIRED — confirm death ragdoll feel, blood trail from the
  corpse, and melee spray on repeated knife hits.
- Multiplayer review: REQUIRED — confirm predicted/confirmed/rolled-back deaths
  spawn and remove corpses correctly.
- Still unverified: any runtime behavior; corpse mesh rendering path
  (`ragdollModeActive` + `renderNetworkPlayer`) was not observed live.

# Related feature record

- Feature path: none created this session.
