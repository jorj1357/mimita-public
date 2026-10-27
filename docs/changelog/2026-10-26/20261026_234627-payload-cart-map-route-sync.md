# Payload cart live map geometry and route sync

- Timestamp: 2026-10-26 19:46:27 EDT (2026-10-26T23:46:27Z)
- Branch: `20261011zt`
- HEAD before this work: `1cc507aa449b7aa6aae66bdf2749d62e3c84bc7f`
- Result: `PASS_WITH_HUMAN_REVIEW`

## Request and finding

The user reported that changing the Payload cart size in a map JSON did not
update its physical size live, and then asked why an extra cart appeared in the
middle of the map.

Source and runtime evidence found two connected gaps. The physical entity was
created once from `PayloadRuntime::cartSize` when a match started; active ticks
only copied facing and yaw from the map config. More critically, changing maps
loaded the new map config but retained the previous map's `PayloadRuntime`
route. The cart could consequently move along unrelated coordinates on the new
map. The run journal showed the match configured on `payload1`, then a live
load of `payloadsmall1` and a cart respawn at the new origin/size without a
runtime route reinitialization.

## Changes

- `src/network/server-gamemode.h`, `ServerGamemodeState`: added
  `payloadRuntimeMapId` to identify which map owns the active cart runtime.
- `src/network/server-gamemode.cpp`, `payloadSyncCartFromMap`: synchronize
  origin, size, yaw/facing, and interaction-box size/offset/rotation from the
  active map. Reinitialize the runtime route when the map identity changes.
  Recreate the physical entity for size changes and map switches so reliable
  entity replication sends the new collision dimensions and model scale.
  Disable the cart and log an error if a new map has no usable Payload route.
- `src/network/server-gamemode.cpp`, Payload match setup: record the map ID
  after a successful runtime initialization and clear it when Payload mode is
  reset.
- `docs/specs/gamemodes/payload.md`: document the live transform boundary and
  clarify that route edits on the same map apply next round.
- `docs/regressions/2026-10-26/payload-cart-keeps-previous-map-route-REG.md`:
  record the previous-map route regression and evidence status.

Prior source behavior copied only `cartYaw` and `cartFixedFacing` during the
active Payload tick. The corrected behavior polls map JSON at the existing 250
ms boundary, applies transform/size edits during any Payload phase, and rebuilds
the runtime route at a map switch.

## Evidence

- Specification: `docs/specs/gamemodes/payload.md` section 0.3 identifies map
  JSON as the owner of cart origin, size, and facing; the entity editor workflow
  permits map-config reload while running. This change extends the stated
  hot-reload boundary to size and interaction transforms.
- Focused review: `docs/skills/spec-behavior-review-v1.md` — completed. The
  previous-map route cache contradicted the map ownership contract.
- Runtime/build workflow: `docs/workflows/runtime-scenario-validation.md` —
  followed for source tracing, dev-loop build, executable identity, and journal
  inspection. Corrected-runtime gameplay validation remains open.
- Logging review: `docs/skills/logging-checker-v1.md` — completed. A bounded
  `payload.cart-map-reloaded` event records size, map-switch status, entity ID,
  route point count, position, facing, and interaction size.
- Regression: `docs/regressions/2026-10-26/payload-cart-keeps-previous-map-route-REG.md`.
- `git diff --check`: passed.
- Dev-loop build request completed successfully and published debug build
  3391. `latest_build_stale=false`; the running build stayed at 3389.
- Exact executable: `C:\mimita-v9\.dev\builds\3391\mimita.exe`.
  SHA-256: `68CEC571D4B976F13697C55AC4A65A320BF1EB3D3FAEB5D35400B20A4171FF24`.
- Exact `--versioninfo` journal:
  `C:\mimita-v9\logs\10-26-2026\20261026_194631\events.jsonl`.
- Pre-fix runtime journal:
  `C:\mimita-v9\logs\2026-10-26\20261026_233539\events.jsonl`.
  It records the cart initialized at `[3,3,2]` on `payload1`, then recreated at
  `[10,20,10]` on `payloadsmall1`. The initial `payload.configured` event says
  nine route points were active before the switch; `payloadsmall1.json` has
  three. The event at sequence 1665 recorded the new size/origin/facing. Source
  inspection confirmed there was no route reinitialization at that boundary.
- The correction was not launched into a new gameplay run because the active
  game/editor session was making repeated live edits to the untracked
  `config/maps/payloadsmall1.json`. Its content was not changed by this task.
  Client-side disappearance of the previous cart, corrected route movement,
  and human visual acceptance still need observation in build 3391.

## Unrelated workspace state

The worktree already contained numerous edits before this task, including
Payload mode/config changes, `payloadsmall1.json`, `payloadsmall1.glb`, map
authoring changes, other gamemode/config edits, and source/UI changes. They
were preserved. No user map JSON or asset was edited here.
