# Cutover direct census, generated packet routes, state migration, mouse Y

Date: 2026-11-16
Requested by: jorj1357
Agent: DeepSeek v4.1 Flash

## Source changes

- `src/camera.cpp`: mouse-down is positive `dy`; pitch subtracts it so
  mouse-down looks down. The direct camera path and the normalized router now
  use the same convention.
- `src/input/input-router.cpp`: `applyPendingLook` subtracts normalized pitch
  instead of adding it.
- `build.py`: writes `build/link-inputs-<mode>.json` with every object and
  source path actually passed to the linker. The file is written pending before
  link and finalized after publish.
- `devscripts/exe_cutover_gate.py` and
  `devscripts/build_target_gate.py`: read the actual link-input sidecar instead
  of inferring the EXE source set from module ownership. Missing or malformed
  sidecars fail closed.
- `devscripts/packet_registry_codegen.py`: rewritten to generate from
  `packets.h` plus actual inbound dispatch branches in
  `multiplayer-tick.cpp` and `server-packets.cpp`, and from active source
  references. Unknown is never converted to resident. The old
  `packet-schema.json` defaults/overrides are ignored and marked deprecated.
- `src/network/packet-route.h/.cpp`: bounded runtime `packet.route` events once
  per packet/direction/generation.
- `src/network/multiplayer-tick.cpp` and `src/network/server-packets.cpp`:
  record actual client/server receive routes.
- `src/host/abi/state-capability.h`: `StateMigrateFn` returns whether migration
  may commit; added `StateObjectId::MigrationProbe`.
- `src/host/host-runtime.cpp`: rejected migration frees the candidate and keeps
  the old state active.
- `src/network/server-gamemode.cpp`: existing gamemode migration returns true
  after validation.
- `src/main-systems.cpp`: added `state.migration-selftest` covering migration,
  new-field default, rejected migration, and old-state preservation.
- `src/entities/player-avatar-animations.cpp`: NPC authored animation tick-lock
  defaults on, so avatars without an explicit flag use authored clips at one
  authored tick per fixed simulation tick.
- CAP_RENDER cleanup: removed GLAD from type-only game files and shared render
  headers, converted remaining type-only GL aliases, moved GPU timer queries
  behind `GlResourceCapabilityV1`, routed `gl-debug` through CAP_RENDER
  `getError`, deleted six legacy bridge files, and made `pch.h` hide GLAD from
  `MIMITA_GAME_DLL`.
- `devscripts/module_ownership.py`: EXE source selection now uses the generated
  source manifest and keeps only resident/bootstrap/launcher/tool owners.
- Six legacy bridge source files were deleted:
  `game-gl-legacy.cpp`, `game-file-legacy.cpp`, `game-job-legacy.cpp`,
  `game-socket-legacy.cpp`, `game-state-legacy.cpp`, and
  `render-command-legacy.cpp`.
- `src/pch.h` hides GLAD from the hot game DLL. Shared render/UI/map headers now
  use semantic/uint32 types; `perf-gpu` uses GlResource timing ops; `gl-debug`
  uses CAP_RENDER `getError`.
- `build/hot-reload/exe-worklist.json` was generated from the actual
  `build/link.rsp` and source manifest: 610 linked sources, 571 game-owned,
  36 resident, 3 bootstrap.
- `build/hot-reload/audit-classified.json` was generated against the current
  570-source worklist: intentional exception 164, must-move 570,
  needs-state-review 24, needs-runtime-proof 10, needs-resident-adapter 4,
  static-closable 1, closed 21. These are buckets, not 570 confirmed bugs.
- `build/hot-reload/exe-worklist.json`: exact current pre-next-build worklist
  from `build/link.rsp`: 610 linked sources, 571 game-owned.
- `build/hot-reload/audit-classified.json`: four-bucket classification of the
  current audit rows.
- Existing NPC collision safety push and `npc.collision.stuck` evidence remain
  in place.

## Generated evidence

- `config/hot-reload/packet-ledger.json` and
  `src/host/generated/packet-registry-v1.h` regenerated.
- Current generated packet counts:
  - client: `dll=74`, `resident=0`, `unknown=4`, `tombstones=4`
  - server: `dll=74`, `resident=0`, `unknown=4`, `tombstones=4`
- The four unreferenced IDs are classified as tombstones:
  `PACKET_PROFILE`, `PACKET_ENTITY_SPAWN`, `PACKET_ENTITY_DESPAWN`,
  `PACKET_RESPAWN_REQUEST`. They remain reserved and are excluded from
  packet-completeness failure.
- `hot_reload_status.py` now computes packet completeness from the generated
  ledger instead of hardcoding false.
- `cap_render_gate.py`: current result is PASS with 0 raw-GL game sources.
- `config/debuglogger.json`: performance target changed from 999 to 240 FPS;
  `performance.fps` should report a ~4.16 ms frame budget after config reload.
- `hot_reload_status.py` generated state now reflects generation 256 build
  `status=ok`; no claim is made that this generation is the active runtime.

## Documentation

- `docs/handoffs/hotness-cutover-direct-plan-20261116.md`: direct plan,
  decisions, implemented status, remaining proof.
- `docs/specs/hotreload/hotreload.md` and
  `docs/specs/networking/networking.md`: packet ledger/codegen now describes
  generated actual-code evidence, not hand classification.
- `config/hot-reload/packet-schema.json`: marked deprecated.

## Build evidence

No build was invoked in this pass. `build.py` and the Python gates parse
successfully; packet codegen `--check` passes. The actual link sidecar will be
created by the next build.

## Runtime evidence

None for this pass. The mouse fix, packet route events, NPC animation default,
NPC collision safety, and state migration selftest require the next activated
generation. No same-PID acceptance is claimed.

## Remaining

- Regenerate/validate actual link census after the next build.
- Run `state.migration-selftest` in the live client and then prove a real
  generation-to-generation `player_core` migration.
- Observe `packet.route` events and classify the four unknown packet IDs.
- Exercise NPC collision and authored animation in the visible client; confirm
  local-player animation scope separately.
- Close audit buckets, perform the EXE/source cutover, delete fallback/legacy
  paths, and record the final same-PID edit/activate/rollback proof.
