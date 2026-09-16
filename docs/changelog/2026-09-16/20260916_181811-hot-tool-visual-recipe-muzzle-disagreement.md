# Generic hot tool-visual recipe, ownership fix, muzzle + disagreement migration

- UTC timestamp: 2026-09-16T18:18:11Z
- Branch: `8292026stash`
- Base commit: `b35a0eb`
- Commits: none (working tree; concurrent unrelated animation edits preserved)
- Result: `PASS_WITH_HUMAN_REVIEW` (cold build + all relevant selftests PASS; live/human pending)
- Evidence class: source + hot-DLL build + cold EXE build + headless selftests

## Task

Replace the split weapon/effect presentation paths with one generic, hot-
reloadable tool-visual system: `ToolVisual` recipes, readiness-gated ownership so
a hot claim never suppresses the cold viewmodel, a real muzzle-flash primitive
(no blue cube, with light), and server-disagreement presentation moved from JSON
to hot C++. Phase 1 + Phase 3 of the plan.

## Specification and skill review

- `docs/skills/spec-behavior-review-v1.md` applied. No spec/code contradiction
  was introduced: the change moves presentation policy behind the declared hot
  boundary and keeps the cold fallback. `docs/features/live-code-development/
  live-code-development.md` "effect and visual presentation behavior" is
  replaceable hot code; `docs/architecture/live-development/game-api-freeze-rule.md`
  is respected (no new `GameplayContextV1` field; a generic `effect.spawn` kind
  and generic primitive meshes were used).
- Unresolved warnings: the disagreement hot path is equivalent in shape but not
  frame-verified against the JSON path (no screen); `serverdisagree.json`
  appearance values are NOT deleted this pass.

## What changed

### Hot (`mimita-game` DLL, live-editable)
- NEW `src/hot-reload/hot-tool-visual.h`: `ToolVisualRecipeV1` (model path +
  logical mesh/texture, view/world transform, socket + muzzle offset, muzzle
  style, dynamic-light style, projectile style, trail/impact/explosion style,
  sounds, lifetime/scale/color/alpha/visibility flags), plus
  `ServerDisagreementVisualV1` / `LocalDisagreementVisualV1`.
- NEW `src/hot-reload/modules/presentation/tool-visuals.cpp`: the single recipe
  registry. `findToolVisual(gameHash(weaponId))` resolves revolver, shotgun,
  rocket launcher, grenade launcher, spyknife, swordsword; disagreement recipes
  are here too. No weapon enum/switch.
- `modules/presentation/attachment.cpp`: recipe-driven presentation; a recipe is
  claimed only when complete AND `resource.register` returned `ok` with a
  non-zero generation (retries throttled, no unconditional latch). Claim now
  carries `toolEntity`/`meshResourceId`.
- `modules/presentation/effect-composition.cpp`: muzzle uses the recipe's bright
  untextured sphere (never the cube/default texture) with configurable one-tick
  lifetime and an optional dynamic light via `effect.spawn` kind
  `light.dynamic`; disagreement and local-indicator composition branches; fixed
  `EffectLifetime.scale0` (it previously discarded every recipe scale after the
  first lifecycle tick).
- `hot-presentation.h`: `HotPresentationStateV1` gains per-axis `scaleXYZ`
  (generic cylinder -> beam/tracer); `HotToolClaimV1` gains
  `toolEntity`/`meshResourceId`; generic `mesh.sphere`/`mesh.beam` and
  `light.dynamic` constants.
- `modules/presentation/debug-presentation.cpp`: honors per-axis `scaleXYZ`.
- `hot-modules.json`: new header registered for change detection.

### Cold (EXE — requires one intentional cold build/install)
- `hot-reload/game-api.h`: `EffectRequestV1` append-only `correction[3]`,
  `reason`, `sourcePlayerId`, `targetPlayerId`, `localIndicator`.
- `live-code/live-behavior.cpp`: `effect.request` schema bumped to v2;
  `capEffectSpawn` handles `light.dynamic` via the EXISTING
  `DynamicLightManager` (`capResourceRegister` keeps its "registration accepted"
  contract; a load failure is observable via `generation == 0`).
- `render/presentation-render.cpp`: generic `mesh.sphere`/`mesh.beam` primitive
  registration; idempotent `registerLogicalResource` (no unbounded path-storage
  growth on repeated hot registration).
- `combat/weapon-viewmodel.cpp/.h`: `weaponViewModelHotOwnsEquippedTool` requires
  the claimed tool's mesh to resolve before the cold viewmodel yields.
- `hot-presentation.h`: `HotPresentationStateV1` gained append-only per-axis
  `scaleXYZ` (and the two locally-mirrored test structs in
  `hot-combat-selftest.cpp` were updated to match the component layout).
- `network/disagreement-visuals.cpp`: forwards the plain event to hot and returns
  when handled; JSON composition remains the fallback. Enable gate and tick rate
  gate unchanged.
- `network/hot-combat-selftest.cpp`: recipe/ownership/muzzle/light/disagreement
  checks; synthetic mesh adoption for headless head-of-frame resolution.

## Validation

- `python build_game_dll.py` -> `DLL build success: build\mimita-game.dll`
  (hot compile PASS, including unrelated concurrent animation sources).
- `python build_agent.py` -> `Status: SUCCESS` (cold EXE relinked; `mimita.exe`
  was closed by the operator; no process was killed and no force-cold was used).
- `mimita.exe --hot-combat-selftest` -> **PASS**, including the new checks:
  - `local possessed body is submitted exactly once (generic path yields)`
  - `spyknife recipe resolves to its model and claims ownership`
  - `unresolved recipe mesh never claims ownership (cold stays the owner)`
  - `muzzle is the bright untextured sphere (no blue cube)`
  - `revolver muzzle recipe emits a dynamic light`
  - `server disagreement presentation is hot-owned (pulse/beam/tracer/particles + audio)`
  - `local correction indicator is hot-owned`
- `--live-code-selftest` PASS, `--production-loop-selftest` PASS,
  `--glb-consumer-selftest` PASS, `--tool-entity-continuity-selftest` PASS,
  `--content-resource-selftest` PASS.
- `git diff --check` -> clean (only LF->CRLF warnings).
- One pre-existing check (`local possessed body ...`) compares global
  submission deltas and was sensitive to a transient effect expiring between its
  two frames once new effects existed; it now runs those two frames with `dt=0`
  so the only difference is the local actor's own body.

## Evidence classes (separate)

- Source proof: yes (files above; `git diff --check` clean).
- Hot-module build proof: yes (`build_game_dll.py` success).
- Cold build proof: yes (`build_agent.py` `Status: SUCCESS`).
- Runtime activation proof: headless selftests call the real hot systems through
  the real capability/dispatch path; no rendered frame.
- Live visual proof: no (no screen).
- Multiplayer proof: no.
- Human acceptance: pending.

## Human review needed

1. Live: revolver/shotgun/rocket models visible; muzzle flash + light + sound,
   no blue cube; spyknife model/swing intact; real disagreement effect; edit a
   hot tool-visual recipe and confirm the change appears in the same world/
   session (PID/session/EntityIds unchanged).

## Pre-existing changes

The working tree contained unrelated concurrent-session edits (animation state
v2 / action bridge: `hot-action.h`, `hot-animation.h`, `hot-animation-clips.h`,
movement/config/NPC files, and a concurrent changelog). They were preserved
untouched and were not claimed as part of this work.

## Next

Live/human acceptance (item 1 above), then Phase 4 (delete only demonstrably-
unused JSON visual fields) and the projectile trail/impact recipe migration.
