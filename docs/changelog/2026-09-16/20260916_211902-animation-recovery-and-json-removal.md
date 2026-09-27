# Animation recovery + JSON removal: hot-only procedural animation

Date: 2026-09-16 17:19 EDT (UTC 2026-09-16T21:19:02Z)
Branch: `8292026stash` (worktree, uncommitted)
Result: `PASS_BUILD_AND_TESTS; LIVE VISUAL ACCEPTANCE PENDING`

## Task

Phase R (recover the frozen animation) and Phase J (delete the JSON + legacy
procedural animator so animation is hot-only and JSON-free).

## Root cause of the freeze (evidence)

Not a logic bug. The running pair was half-migrated:
- The running EXE was the earlier cold build (old ABI).
- The client had activated a **mixed hot generation 57** whose
  `animation-policy.cpp`/`pose-generation.cpp` matched current code but whose
  `tool-visuals.cpp`/`game-api.h`/`hot-tool-visual.h` were older intermediate
  versions (hash‑verified).
- My Part C changed `game-api.h` — the ABI — which must never be edited live.
- The reloader logged 47 `compile_failure` events from my intermediate
  `gameHash` edits, plus server/client `generation_mismatch`.
- With no consistent active package and the cold animator gated off
  (`gHotAnimationOwnsGameplay=true`), zero animation played.

## Phase J — JSON and legacy animator removed

Deleted:
- `src/entities/player-animation.cpp` (legacy procedural animator)
- `src/entities/player-animation-config.{h,cpp}` (JSON loader)
- `src/entities/player-config.cpp` (procedural config + ownership flag)
- `config/animations.json`, `config/player-procedural.json`
- manifest entries for both config files (`manifests/1.0.0.json`)

Removed config types/globals/decls from `player.h` (`AxisLock`, `AnimClip`,
`AnimKeyframe`, `WeaponPoseConfig`, `LayeredAnimConfig`, `PoseOverlayConfig`,
`DashPoseConfig`, `FreezePoseConfig`, `PlayerProceduralConfig`,
`gPlayerProcedural`, `gHotAnimationOwnsGameplay`, `updateProceduralAnimation`,
`updatePlayerProceduralHotReload`, `reloadPlayerProceduralConfig`).

Call sites / consumers removed: `physics-mini.cpp`, `client.cpp`,
`multiplayer-interpolation.cpp`, `live-behavior.cpp` (`capAnimationUpdate` +
its capability registration; the `animation.update` capability is no longer
registered), `engine-tick-setup.cpp`, `debug-commands.cpp` (`hotanim`),
`weapon-commands.cpp` (`animation_config_reload`/`animation_config_inspect`),
`terminal-debug-toggles.cpp` (`idle_test`), `gamemode-manager.*` and
`bomb-tag-state.cpp` (`setArmToWeaponPose`).

Avatar preview migrated to the hot path: `PresentationEntities::projectPreviewPlayer`
/ `applyHotPoseToPreview` project the preview `Player` onto a dedicated ECS
entity (id 2) and copy the hot skeleton pose onto the typed body. The typed
apply helper now also writes `perfectPoseSkeleton` node transforms and calls
`updateModelWorldTransforms()`, so the typed renderer reflects the hot pose.

## Phase R — build recovery

- `python build_agent.py` -> `BUILD SUCCESS` (cold EXE; ABI consistent).
- `python build_game_dll.py` -> DLL current.
- `mimita.exe --live-code-selftest` -> `PASS`.
- `mimita.exe --hot-combat-selftest` -> all animation/weapon tests PASS
  (phase2 idle/walk/jump/dash/freeze/equip/shoot/reload/just-shot/unequip/
  slash/lunge/death/respawn/determinism; `skeleton.validate`; `tool.definition`;
  per-tool phases; **brand-new hot weapon registered with no cold edit**).
  Only two failures remain, both in the concurrent effect-composition work
  (`real explosion fact...`, `explosion fact composes flash/smoke/debris`), not
  animation.

## Verification

- `Select-String` across `src` for `animations.json`, `player-procedural`,
  `gPlayerProcedural`, `updateProceduralAnimation`,
  `gHotAnimationOwnsGameplay`, `player-animation-config` -> no code references
  remain (only two now-removed comments).

## Hot edit surface after this change

| Edit for animation | File | Hot? |
|---|---|---|
| Body clips | `src/hot-reload/hot-animation-clips.h` | hot |
| Weapon phases + gameplay | `src/hot-reload/modules/presentation/tool-visuals.cpp` | hot |
| State machine | `src/hot-reload/modules/presentation/animation-policy.cpp` | hot |
| Pose composition | `src/hot-reload/modules/presentation/pose-generation.cpp` | hot |
| `game-api.h` | ABI — must not be edited live | cold |

## Known behavior deltas (human review required)

- Aim-body pitch: the legacy animator tilted torso/head with aim; the hot clips
  do not yet. Visual verification needed.
- Bomb-holder arm pose (`setArmToWeaponPose`) removed; held items are now
  expected to present through the hot tool/attachment path.
- Avatar preview now depends on the hot render domain posing its entity; verify
  the preview animates in the menu.

## Qualification

- Cold build/test evidence: yes.
- Runtime visual proof: no (no screen). Human acceptance pending.
