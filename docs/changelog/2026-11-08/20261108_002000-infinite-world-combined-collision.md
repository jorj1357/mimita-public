# Task

- Task ID: infinite-world-combined-collision-20261108
- Summary: Compose streamed chunk collision into the world static surface, make
  actor seam response hot-editable, and give server NPCs a live streamed
  collision tail.
- Status: Implemented; build and runtime evidence collected; human feel review
  still required.
- Date, time, timezone: 2026-11-08T00:20:00Z, ISO 8601
- Branch: current working branch
- Base commit: 1fc06d59
- Final commit: not committed by this session

# Pre-existing changes

- Exact status output: `config/accounts/default.json`,
  `config/analytics.json`, `config/audio/music-settings.json`,
  `config/video-settings.json`, and `devscripts/blender-ai` were already
  modified/untracked before this session. They were not edited here.
- Files not created or modified by this session: the four config files above
  and the `blender-ai` submodule pointer.

# Requested behavior

- Move streamed chunk collision into a hot module.
- Treat nearby chunks as one solid surface; do not collide with faces between
  chunks.
- Make NPCs collide with the same world.
- Keep loaded render chunks and collided chunks in sync.
- Allow live editing of collision/seam policy without an EXE rebuild.

# Specification alignment

- Current specification paths:
  - `docs/specs/procedural-infinite-world/procedural-infinite-world.md`
  - `docs/specs/movement/movement.md`
  - `docs/specs/npc-behavior/npc-behavior.md`
  - `docs/architecture/collision/collision.md`
- Exact requirements:
  - Streamed world generation must support players and NPCs on both client and
    server.
  - Static hidden-face removal runs before the broadphase and is shared by
    client and server.
  - Gameplay collision runs on the fixed 60 Hz simulation boundary.
  - Movement feel and existing seam response must be preserved.
  - NPC navigation uses the collision triangles actors physically collide with.
- Why the change follows the specification:
  - Streamed chunks now enter `World.collisionMesh` as normal static triangles
    (`entityId == 0`) before `buildCollisionChunks`, so existing player, NPC,
    navigation, and seam rules receive them through one surface.
  - Cross-chunk face removal runs on the combined streamed set before the
    broadphase rebuild.
  - The server maintains a matching NPC collision tail around player/NPC
    anchors.
- Conflicts or decisions:
  - Bounded union remains supported but is not the default for streamed
    surfaces; Manifold cost on every streamed change was too high. Face removal
    is the default merge mode.
  - Connector-mode server streaming uses the grid approximation only when all
    authored candidates match the profile footprint. Mixed-size connector
    graphs remain client-owned and are explicitly reported.

# Exact implementation changes

## File: `src/physics/physics-types.h`

- Added streamed tail metadata to `CollisionMeshCache`:
  `streamedTailTriangles`, `streamedTailSolidIds`, `streamedChunkCount`,
  `streamedRevision`. `clear()` resets them.

## File: `src/world/streamed-world-layer.{h,cpp}`

- Removed per-chunk `PhysicalEntity` collision proxies.
- Chunk records now store raw collision triangles and solid ids.
- Added `flushCollision(World&)`: trims the previous streamed tail, composes all
  loaded chunks, runs face removal/bounded union, appends the tail, and rebuilds
  the broadphase.
- Added status accessors for loaded/collided counts, triangle count, revision,
  and dirty state.
- Exposed `buildStreamedChunkCollision` for the server stream.

## File: `src/world/chunk-collision-module.cpp` and
`src/physics/movement/actor-contact-policy-module.cpp`

- Extended the `chunk-collision` module with actor contact/seam policy and
  streamed surface merge policy events.
- Default merge mode is face removal; policy controls opposing-face distance,
  feature collapse, low flush-seam suppression, and same-part collapse.

## File: `src/modules/api/actor-contact-policy-api.h`

- Added plain-data `ContactPolicyV1` and `SurfacePolicyV1`.

## File: `src/physics/movement/actor-triangle-solver.cpp`

- Loads the hot contact policy per solve.
- Uses policy distances/flags in `removeTouchingFaceSeams` and
  `collapseCloseFeatureContacts`.
- Allows the solver to run when the base world mesh is empty but entities or a
  streamed collision surface exist.

## File: `src/procedural/infinite-world-runtime.{h,cpp}`

- Runs every fixed tick before actor simulation via `engine-tick-replay.cpp`.
- Flushes collision after chunk install/remove and before movement.
- Added 1.25x unload hysteresis to stop edge-chunk thrash.
- Throttled profile file polling to 250 ms.
- `worldstream status` now includes collided chunks, collision triangles,
  collision revision, and dirty state.

## File: `src/network/server-infinite-collision.{h,cpp}`

- Added a server-owned streamed collision tail for `npcWorld`.
- Builds regular-grid chunks around active player and NPC anchors, composes
  their triangles, runs face removal, and rebuilds `npcWorld` broadphase.
- Supports connector worlds only when every authored candidate matches the
  profile footprint; otherwise logs the unsupported case and retains the
  authored map footprint.

## File: `src/network/server.cpp`, `src/network/server-npcs.cpp`

- Calls `serverInfiniteCollisionTick` before `simulateSharedNpcs` in dedicated
  and listen-server paths.
- Resets the stream when `buildNpcWorldCollision` rebuilds the NPC world.

## File: `config/modules/modules.json`

- `chunk-collision` now includes both module sources, the actor policy header,
  and events 400/401/402.

## Files: `docs/architecture/collision/collision.md`,
`docs/specs/procedural-infinite-world/procedural-infinite-world.md`

- Documented the world-tail composition, hot seam policy, and server stream
  scope.

# Diagnostics

- Owner/category: World / NPC movement.
- Input: loaded chunk records, chunk transforms, player/NPC anchors.
- Decision: compose streamed surface, face-remove internal seams, rebuild
  broadphase, apply actor contact policy.
- Output: `world-stream.collision.sync`,
  `npc.collision-world.stream.sync`, `worldstream status`.
- Failure/rejection reason: module unavailable, merge cap, unsupported
  mixed-size connector server placement.
- Rate limiting: profile polling 250 ms; server collision rebuild 250 ms.

# Validation

- Focused skill paths and results: routing and collision architecture reviewed.
- Tests and exact commands:
  - `python devscripts/dev-loop.py --request build --wait-for-build`
  - `python devscripts/module-build-worker.py --audit`
- Build status: SUCCESS, artifact `2026-11-08T00:15:44.940248Z`.
- Module audit: PASS, files=11, events=15.
- Runtime or hot-reload evidence:
  - Client: `world-stream.collision.sync` reported
    `loaded_chunks == collided_chunks == revision_match == true` for mixed and
    cramped runs.
  - Mixed run: 180 loaded chunks, 3708 raw stream triangles, 2852 after
    face/union processing, 856 removed internal faces.
  - Cramped run: 160-164 loaded chunks, 1920 raw triangles, 752 after face
    removal, 1168 removed internal faces.
  - Player moved 140 m across cramped seams (x 26.5 → 166.9, z ~1.6-1.8) with
    no speed collapse, progress stall, or last-valid rollback in the sampled
    tail.
  - Hot policy edit: module generation 7 → 8 activated in the same PID with
    changed source path and new code hash, no EXE rebuild.
  - Server: `npc.collision-world.stream.sync` reported 256 chunks and 1168
    streamed triangles; NPC remained grounded/supporting after the initial
    pre-stream spawn, with no repeated below-map respawns in the 60 s sample.
- Output files:
  - `logs/2026-11-07/20261107_234212/client-01/events.jsonl`
  - `logs/2026-11-08/20261108_000750/client-01/events.jsonl`
  - `logs/2026-11-08/20261108_000750/server-01/events.jsonl`

# Measured evidence

- Before values:
  - Streamed chunks were per-chunk physical entities; seam rules skipped them
    because `entityId != 0`.
  - Server NPC collision was a one-time base world build.
- After values:
  - Client collision sync reported loaded/collided counts equal.
  - Cramped rebuild cost was about 10-40 ms per streamed-set change.
  - Server rebuild produced 256 chunks / 1168 streamed triangles.
- Timestamps: client syncs 2026-11-07T23:43:42Z onward; server syncs
  2026-11-08T00:08:22Z onward.
- Tick/frame/network measurements: collision changes were observed on the fixed
  pre-simulation boundary; client `collision.speed_collapse` / progress stall
  were absent in the successful cramped move sample.

# Regression review

- Regression entry appended: no; no confirmed regression found.
- Why this is or is not a confirmed regression: the known seam regression path
  was not reproduced; the change intentionally restores world-mesh seam rules
  for streamed chunks.
- Related regression paths:
  - `docs/regressions/collisions/block-seam-speed-REG-2026-10-31.md`
  - `docs/regressions/collisions/surface-snagging-issue-REG-2026-11-03.md`
  - `docs/regressions/collisions/tdmtest3-npc-world-REG-2026-11-03.md`

# Human acceptance

- Visual review: not performed by the agent.
- Gameplay review: automated movement sample only; human feel review required.
- Multiplayer review: client + dedicated server run observed; not a human
  multiplayer acceptance.
- Still unverified:
  - Mixed-size connector server streaming around NPCs.
  - Long-duration 240 FPS frame-time acceptance while streaming.
  - Human feel review of seam bumps and bounce response.

# Related feature record

- Feature path: none created this session.
