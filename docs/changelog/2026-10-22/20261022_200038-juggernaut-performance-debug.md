# Juggernaut performance optimization in debug build

- Time: 2026-10-22T20:00:38Z (2026-10-22 16:00:38 EDT)
- Branch: `20261011zt`
- HEAD at measurement: `cbfa4b0df4e56b6f217c45dbe986b49df599b50a`
- Result: `BLOCKED` for the strict 240 FPS acceptance target. Real improvements
  were built and measured, but the game is not yet consistently at 240 FPS.
- No release acceptance run was performed. The measured build is debug (`-Og`).

## What was measured

- Build: **2845**, debug, `source_generation=30`, `source_current=true`
- Executable: `C:\mimita-v9\.dev\builds\2845\mimita.exe`
- SHA-256: `8e1c250ec7b1932ea9f263d554a8f28ef6fdd063eaf5f1fb9beb42df0f8bae92`
- Journal: `C:\mimita-v9\logs\2026-10-22\20261022_195815\events.jsonl`
- Scenario: Juggernaut mode 9 on `dust2cyberiav4`, visible client, human
  `team_pick 1`, 63 Fighter NPCs + 4 Juggernaut NPCs + human.
- Roster/identity evidence:
  - `actor.team-assigned`: `role=juggernaut_fighter`, `team=0`.
  - `npc.roster-profile-resolved`: 126 Fighter records and 8 Juggernaut records
    because the roster was built twice (initial spawn plus the team change);
    each build is 63 + 4.
  - Server `actor.collision.profile`: 67-68 actors across 51 samples.
  - Sampled `npc.movement-decision`: 17 distinct NPC actor ids. This is a
    sampled count, not proof that every roster member moved continuously.

### Latest continuous slice (not a passing 60-second acceptance run)

The latest 142 performance windows covered 8,520 rendered frames over 56.3 s of
wall-clock time:

- Average frame: **6.482 ms**
- Frames over the 4.17 ms budget: **5,967 / 8,520 (70.0%)**
- Maximum frame: **2,409.06 ms**
- Worst p95: **46.582 ms**, worst p99: **2,409.06 ms**
- Wall-clock rate: **151.25 FPS**
- Average main-thread CPU: **6.101 ms**

Best observed 60-frame window in this run: **6.81 ms average, 17.41 ms max,
55/60 frames over budget**. No measured window met zero misses.

## Baseline versus current

The prior active Juggernaut window (from the earlier build-2778 journal) was:

- **11.821 ms** average, **33.839 ms** maximum, **60/60** frames over budget,
  main-thread CPU **10.938 ms**.

The current debug build is materially faster in the same mode (roughly 6.5-8 ms
typical, with occasional spikes), but it still misses the 4.17 ms budget on
almost every frame. This is an improvement in measured frame time, not an
acceptance pass.

## Source changes that produced the improvement

### Player body rendering

- `src/entities/player-render-mesh.cpp`
  - Replaced the single global per-part VAO/generation cache with a cache keyed
    by `Player::renderModelKey()`; actors using the same model now reuse one
    upload instead of re-uploading their private mesh copies.
  - Added a combined body mesh per model (`ensureCombinedBodyParts`) and a
    single combined draw path (`drawCombinedBodyParts`). All body parts of an
    actor are transformed by a uniform `uPartModels[32]` array, collapsing many
    per-part draws into one draw when the actor uses one texture.
- `src/entities/player-render.cpp`
  - Uses the combined body path for normal remote bodies and falls back to the
    old per-part path for first-person head hiding, outline passes, white
    overrides, and debug culling.
  - Uses the per-model fallback VAO returned by `uploadPlayerMeshIfNeeded`.
- `shaders/basic.vert`
  - Added `aPartIndex`, `uUsePartModels`, and `uPartModels[32]`.
- `src/entities/player.h`, `src/entities/player-loader.cpp`
  - Added `renderModelKey` / `setRenderModelKeyFromPath` and populated it for
    synchronous, cache-hit, collider-only, and asynchronous model loads.

Measured effect: `Rendering::Actors::PlayerBody` fell from about **1.745 ms**
in the old active window to about **0.4-0.55 ms** for the same busy scene.

### Trails, effects, and corpses

- `src/render/render-player.cpp`, `src/render/render-player.h`
  - Trail geometry is accumulated into one batch and drawn once per frame
    (`beginPlayerTrailBatch`, `appendPlayerTrail`, `drawPlayerTrailBatch`).
- `src/effects/effect-part.h`, `effect-part.cpp`, `effect-part-system.cpp`,
  `effect-part-render.cpp`
  - Added a compact active-slot list with swap-remove positions. Update,
    render, render-text, and `collectAlive` iterate live slots instead of all
    4096 pool entries.
- `src/effects/effect-part-blood.cpp`, `effect-part-particles.cpp`
  - Blood spawns respect `mParticleCap` (now 128) and the default cap is lower.
  - Blood drag is computed once per update instead of once per particle.
- `src/engine/engine-tick-combat.cpp`
  - Effects and hit bursts advance in bounded fixed 60 Hz steps instead of once
    per render frame.
- `src/ragdoll/ragdoll-mode.h`, `ragdoll-mode.cpp`
  - Settled corpses stop running the full joint/self-collision/world solver.
  - At most 6 corpses step per fixed tick; older corpses keep aging, fading,
    and rendering from their last simulated pose.

Measured effect: effect update/render and corpse update costs dropped
substantially; `RagdollCorpseUpdate` is now roughly **0.5-0.7 ms** at the
measured tick rate instead of multiple milliseconds.

### Networking and animation

- `src/network/multiplayer-context.h`, `multiplayer-interpolation.cpp`
  - Procedural animation advances once per new server tick instead of once per
    render frame.
  - Cached weapon slot-to-id lookup by registry revision.
  - Avoids clearing/reassigning the equipped weapon id and username when they
    are unchanged.
- `src/combat/weapon-registry.h`, `weapon-registry.cpp`
  - Added a registry revision counter for the slot cache.
- `src/combat/weapon-system.h`, `weapon-system.cpp`
  - Remote weapon viewmodel animation can update on alternating render frames
    while still rendering the weapon every frame.

Measured effect: `Network::RemoteEntities` fell to roughly **0.4-0.9 ms**
depending on scene activity.

### Config polling, hot reload, shadows, and HUD

- `src/engine/engine-tick-setup.cpp`
  - Config polling is split into five bounded groups; a 167 ms poll cycle now
    spreads the work over consecutive frames instead of one large burst.
  - Game-DLL hot reload polling is throttled to the same 167 ms interval.
- `src/engine/engine-tick-render.cpp`
  - Shadow map depth is refreshed on alternating frames and reused otherwise.
  - Trail batch begin/draw and NetworkEntities/PostActors/Rendering prepare
    scopes were added for attribution.
- `src/engine/engine-tick-ui.cpp`, `engine-tick-ui-hud.cpp`,
  `engine-tick-ui-game-hud.cpp`
  - Added HUD sub-scopes (Begin, Text, HUD, Overlays, End, Panels, Hotbar,
    WorldOverlays, Replay, Killfeed, Leaderboard, Timer).
  - Hotbar entries, labels, and short names are cached instead of rebuilt every
    frame.
  - World healthbar loops are skipped when the mode hides world healthbars.
- `src/gui/hud/player-nameplates.cpp`
  - Healthbar anchors are resolved only after the disabled/dead gates, avoiding
    skeleton searches for hidden healthbars.
- `src/engine/engine-tick.cpp`
  - Added scopes for render prep, PostFX frame bind, PostFX, input keys, and
    listen server; restored the existing heartbeat macro after a diagnostic
    no-op test.

Measured effect: `Setup::ConfigPolling` max fell from ~2.97 ms to ~1.15 ms;
`Rendering::Shadows` fell to ~0.12-0.14 ms; HUD cost fell to ~0.2-0.5 ms
depending on scene/UI state.

## What is still failing

The strict target is not met:

1. Almost every rendered frame is still over 4.17 ms.
2. The measured wall-clock rate is about 151 FPS, not 240 FPS.
3. The slice contains a 2.4-second maximum frame. That spike is not explained
   by the steady-state scopes and must be isolated before any zero-miss claim.
4. The debug build uses `-Og`; no release `-O2` acceptance run exists.
5. The client remains single-threaded for render submission, network replica
   updates, HUD, and most per-frame presentation work.

Remaining per-frame costs in the measured windows include:

- `Network::RemoteEntities` ~0.4-0.9 ms
- `Rendering::Actors::NetworkEntities` ~0.5-0.7 ms
- `Rendering::Actors::PlayerBody` ~0.4-0.55 ms
- `Rendering::Actors::Cosmetics` ~0.25-0.35 ms
- `Rendering::Effects::EffectParts` + `HitBursts` ~0.5-0.9 ms
- `Swap` ~0.6 ms
- `HUD::Game` ~0.2-0.5 ms
- `Simulation::SimulateTick` + `RagdollCorpseUpdate` ~1.2 ms at the measured
  ~125 FPS tick rate (these amortize as render rate rises)

The full-detail-all-actors requirement prevents actor culling or LOD, so the
remaining rendering cost is draw submission and per-actor presentation work.

## Likely causes and confidence

- **Confirmed:** the old per-actor body re-upload and per-part draw path was a
  major cost; the combined body/upload cache materially reduced it.
- **Confirmed:** full-pool effect scans, per-frame effect updates, and unbounded
  corpse stepping were material per-frame costs; the active list, fixed-step
  effect update, and corpse cap/sleep reduced them.
- **Confirmed:** config polling was producing multi-millisecond bursts; grouping
  reduced the maximum burst.
- **Likely:** the remaining gap is dominated by single-threaded per-frame CPU
  work (network replica updates, draw submission, effects/HUD) plus the debug
  `-Og` build.
- **Needs profiling:** the 2.4-second stall and all-core/per-thread use. Current
  logging measures main-thread CPU only.

## Recommended next steps

1. Measure a release `-O2` build of this source. The debug build is not the
   shipped performance profile and the target may be reachable there with the
   current reductions.
2. Add bounded per-thread CPU evidence (main thread versus worker threads) to
   confirm the critical path before a concurrency redesign.
3. If single-threaded remains over budget, move independent presentation work
   off the main thread (network replica/animation preparation, effect update,
   config/logging I/O) with fixed-tick ordering preserved.
4. Batch or instance body/cosmetic draws across actors that share an avatar
   atlas; batch effect spheres/beams and HUD text vertex generation.
5. Isolate the 2.4-second stall in `20261022_195815/events.jsonl`; a single
   stall fails the zero-miss target even if steady-state frame time improves.
6. Run a true 60-second, zero-miss acceptance measurement only after those
   changes. The current evidence is not a passing acceptance run.

## Documents and skills read

- `AGENTS.md`, `docs/ROUTER.md`
- `docs/specs/performance/performance.md`
- `docs/workflows/runtime-scenario-validation.md`
- `docs/skills/efficiency-checker-v1.md`
- `docs/skills/logging-checker-v1.md`
- `docs/skills/spec-behavior-review-v1.md`
- `docs/operations/build-and-exe/build-and-exe.md`
- `docs/operations/task-completion/task-completion.md`
- `docs/architecture/live-development/live-development.md`
- `docs/architecture/time-and-formatting/time-and-formatting.md`
- `config/gamemodes/juggernaut.json`, `config/roles.json`, actor preset JSON,
  `config/playervisuals.json`, `config/debuglogger.json`, `config/shadows.json`
- Relevant source owners under `src/entities`, `src/render`, `src/effects`,
  `src/ragdoll`, `src/network`, `src/engine`, `src/gui/hud`, `src/perf`.

## Validation

- Debug builds completed through the dev-loop (through build 2845).
- Build 2845 ran the real Juggernaut mode on `dust2cyberiav4` with matching
  server/client build identities.
- `team_pick 1` was delivered and accepted; the authoritative human role is
  `juggernaut_fighter` on team 0.
- The roster journal shows 63 Fighter NPCs and 4 Juggernaut NPCs per roster
  build (the journal contains two builds because of the team change).
- Server collision profiles show 67-68 actors.
- The performance slice is still failing: 5,967/8,520 frames over budget,
  ~151 FPS wall-clock, 2.4 s maximum frame.
- No human visual/feel acceptance is claimed.
- Pre-existing/parallel working-tree changes and commits were preserved; this
  session did not create a commit.

## Build note

An aborted build request produced a release artifact during the session. It was
not used for the acceptance evidence. All measured runs reported here use the
debug build explicitly, per the user's instruction not to use release builds.
