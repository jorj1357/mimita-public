# Body socket hierarchy and rig editor: runtime proof and per-player save fix

- Task/agent: `opencode-deepseek-v4-2`
- Date, time, timezone: 2026-11-27T07:05Z, ISO 8601
- Status: implemented, built (gen 783), runtime-proven for the logged events
- Linked handoff: `docs/handoffs/2026-11-27-body-rig-ads-damage-handoff.md`
  (continuation of `docs/handoffs/20261127_jitter-body-ads-damage-handoff.md`)

## Pre-existing changes

The shared checkout already contained extensive uncommitted work from other
agents (body-socket scaffold from the prior agent, jitter probe, Rocket League
vehicle/input work, Bloodtest/replay/camera work, build scripts, and configs).
This session preserved all of it and edited only
`src/entities/player-loader.cpp`. Builds went through the single root dev-loop;
gen 783 was the coordinated 07:00Z batch-build that also carried the Rocket
League owner's `src/vehicles/vehicle-system.cpp` and
`src/input/host-input-fallback.cpp` changes.

## Requested behavior

The prior handoff marked the body-socket hierarchy and rig editor as
"source implemented, not runtime-proven" and asked the continuation owner to
verify it live, fix issues, and either keep or remove it based on evidence,
then continue the queued items.

## Specification alignment

- `docs/specs/animations/animations.md` and
  `docs/specs/avatars/avatar-editor.md` (rig/socket model): torso owns head,
  arms and legs; per-avatar sockets override the model rest; the existing
  `bodyparts.json` remains model fallback. The implementation matches this.
- One owner for the body rest pose: `restLocalTransforms` /
  `perfectPoseSkeleton` are updated together, and the shared
  `playerRenderPartTransform` interpolation is unchanged.

## Exact implementation changes

Only `src/entities/player-loader.cpp`:

1. Verified the gen-782 scaffold as-is: `reparentBodyPartsToTorso()` converts
   the flat `plrOrigin` sibling rig to torso-relative sockets, preserving rest
   world transforms; `applyAvatarBodyRig()` applies `config/rig-sockets.json`
   overrides and emits `rig.socket`/`rig.hierarchy`/`rig.verdict`.
2. Fixed a real editor defect found in runtime: `rig_editor_save` wrote the
   edited local avatar's sockets under whichever avatar loaded last (observed:
   local big-girl edit saved under NPC `boiledboy`) because the editor read the
   global `gRigAvatarKey`/`gRigModelKey`. Added `syncRigEditorKeys(const
   Player&)` deriving keys from the edited player (`avatarName()`,
   `renderModelKey()`) and called it from toggle/select/nudge/rotate/save/reset/
   print.
3. Made save and lookup share one model key: `applyAvatarBodyRig()` sets
   `gRigModelKey` from `player.renderModelKey()`; `emitRigLayoutEvents()` now
   also records the human-readable `model_path` alongside `model_key`.
4. `Player::loadModel()` miss path now calls
   `setRenderModelKeyFromPath(cacheKey)` before `applyAvatarBodyRig()` so the
   key exists on first load (fast path and collider path already set it first).

## Evidence

Build (separate from runtime):
- Generation **783**, attempt `game-dll-20261127T065859749721Z-de4e7c`,
  status `ok`, `compiled: 4`, `total_seconds: 37.373`, no changed headers.
  Changed sources: `src/entities/player-loader.cpp`,
  `src/input/host-input-fallback.cpp`, `src/vehicles/vehicle-system.cpp`.
- Summary: `.dev/build-timings/game-dll-20261127T065859749721Z-de4e7c.json`.
- DLL: `build/game/generation-783/game-g783.dll`.

Runtime (isolated test run `20261127_070038`, pinned to gen 783, TDM mode 41):
- Client journal: `logs/2026-11-27/20261127_070038/client-01/events.jsonl`.
- `rig.verdict {valid: true}` for all three body types in one run:
  - boy `abusiveboy` / `mimita-char-no-animations-v4.glb`,
    attachment ~5.96e-08 m, `torso_scale_pct` 100, `child_follows_torso_m`
    ~0.1436;
  - girl `abusivegirl3` / `mimita-girl-v5.glb`, attachment ~2.98e-08,
    follows ~0.1948;
  - big-girl `girllargev3` / `girl-large-mimita-v10.glb`, attachment ~1.33e-07,
    `torso_scale_pct` 100, follows ~0.3160.
- `rig.hierarchy {root: "plrOrigin", torso: "torso", converted: 5,
  children: [0,1,3,2,4]}`.
- Editor: `rig.editor`, `rig.socket.edited` (moved `delta [0,0.05,0]`, rotated
  `euler_delta [5,0,0]`), and `rig.socket.saved {key: "girllargev3",
  section: "avatars", parts: 5}` — the edited local avatar, not the NPC.
- Persistence: reloading `girllargev3` produced a `rig.socket` head offset
  y `1.2606351` with `source: "avatar"`, proving the per-avatar override is
  read back. `config/rig-sockets.json` was then restored to its empty state.
- Proof record: task `body-rig-sockets`, id `task-proof-3063cf9b1c71`,
  `proven`.

Earlier gen-782 run `20261127_064114` first reproduced the save-key bug
(`rig.socket.saved {key: "boiledboy"}`) and exercised the same hierarchy on boy/
girl/big-girl; it is retained as the before-fix comparison.

## Known limits

- Only the body-socket/rig-editor item is done. ADS pose, fullscreen-6 UI,
  damage modes, bat viewmodel, NPC ranged fire, and walkableSlopeDot wiring are
  queued in the handoff and are not started or proven here.
- The rig-editor reset path is command-verified; it has no dedicated event, so a
  future change adding `rig.socket.reset` should prove base-restoration.
- No commit was made.
