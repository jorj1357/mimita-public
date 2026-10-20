# Geometry correction snapback

- Local timestamp: 2026-10-20 18:45:37 EDT
- UTC timestamp: 2026-10-20T22:45:37Z
- Branch: `20261011zt`
- HEAD before and after: `a3901adb9ce179fddf599d4ba6bd7c84af7800e1`
- Result: `PASS_WITH_HUMAN_REVIEW` for source implementation; executable/runtime proof is blocked by unrelated compile errors in the pre-existing actor-pair collision work in this working tree.
- Scope: prevent client world-collision overlap with a last validated-position fallback; make server movement correction snap independently of ordinary drift threshold; preserve tangent movement/velocity; show a correction effect for each distinct correction event.
- Existing user edits: the working tree already contained broad actor-pair collision, NPC damage, player aim, gamemode, and informal networking-spec changes. Those were preserved. The server-player and reconcile diffs also contain pre-existing actor-collision edits; this changelog claims only the correction-specific hunks described below.

## Specification and ownership

Added a normative contract near the top of `docs/specs/networking/networking.md`. It specifies full-tick swept movement without speed-based substeps, removal of only velocity into a blocking surface, a 0.1 m unresolved-overlap rollback threshold, immediate owner correction regardless of `hard_snap_distance`, and one effect per distinct server correction. Existing `hard_snap_distance=999.0` in `config/networkingconfig.json` remains unchanged for ordinary prediction drift.

The existing actor-triangle solver already sweeps the complete proposed movement for the fixed tick and preserves tangential movement. This session did not add substeps or duplicate the established slide response. It added a fallback for a residual overlap after the bounded solver correction passes.

## Changed files and exact before/after excerpts

The excerpts below identify this session's edits. Existing unrelated hunks in the same files are not included as this session's changes.

### `src/entities/player.h` — `Player` collision fallback state, lines 568-572

Before: no last-valid world-collision state followed `movementSimulationTick`.

After:

```cpp
    // Last root position the actor-triangle world solver finished with no
    // unresolved penetration. Used only as a bounded fallback if recovery
    // leaves the actor embedded after its fixed correction iterations.
    glm::vec3 lastValidWorldCollisionPosition{0.0f};
    bool hasLastValidWorldCollisionPosition = false;
```

### `src/entities/player.cpp` — `Player::reset`, lines 90-91

Before: reset cleared movement contacts, then continued to jump state.

After:

```cpp
    lastValidWorldCollisionPosition = pos;
    hasLastValidWorldCollisionPosition = false;
```

### `src/physics/movement/actor-triangle-solver.cpp` — bounded verification and rollback, lines 43, 550-617, 927-979

Before: four solver passes could leave `maxPenetration` describing the pose before the final correction; there was no last-valid-position fallback.

After: a fifth pass verifies the result of the four permitted corrections. A pose with residual penetration above 0.1 m restores the last pose the same solver verified clear. The fallback restores prior ground state and discards contacts from the rejected pose; sideways velocity remains intact. Rollback diagnostics are rate-limited to one event per 250 ms.

```cpp
constexpr float kLastValidRollbackPenetration = 0.1f;
```

```cpp
    for (int iter = 0; iter <= kMaxIterations &&
                       (!candidates.empty() || hasEntities); ++iter)
```

```cpp
    if (result.maxPenetration <= kSlop)
    {
        player.lastValidWorldCollisionPosition = player.pos;
        player.hasLastValidWorldCollisionPosition = true;
    }
    else if (result.maxPenetration > kLastValidRollbackPenetration &&
             player.hasLastValidWorldCollisionPosition)
    {
        const glm::vec3 rejectedPosition = player.pos;
        player.pos = player.lastValidWorldCollisionPosition;
        player.updateModelWorldTransforms();
```

### `src/network/movement-validation.h` — blocking normal output, line 236

Before:

```cpp
                             float tolerance);
```

After:

```cpp
                             float tolerance,
                             glm::vec3* outBlockingNormal = nullptr);
```

### `src/network/movement-validation.cpp` — swept server geometry validation, lines 154-205 and 680-700

Before: blocking geometry checks scanned every world triangle and returned only a boolean; a correction cleared all velocity.

After: the normal path uses the cached chunk broadphase with reusable thread-local candidate storage. If the path exceeds its 100-cell-per-axis broadphase cap, it scans all triangles so a very long movement cannot silently truncate the sweep. It returns the nearest blocking normal and removes only velocity directed into that surface.

```cpp
const bool broadphaseWouldTruncate =
    world->collisionChunkSize > 0.001f &&
    (queryBounds.max.x - queryBounds.min.x > world->collisionChunkSize * 98.0f ||
     queryBounds.max.y - queryBounds.min.y > world->collisionChunkSize * 98.0f ||
     queryBounds.max.z - queryBounds.min.z > world->collisionChunkSize * 98.0f);
```

```cpp
if (intoWall < 0.0f)
    velocity -= blockingNormal * intoWall;
```

Known boundary: this server validator still ray-tests the actor root path; the client actor-triangle solver performs the full actor-mesh sweep. This patch does not claim server-side detection of every partial-body edge overlap.

### `src/network/server.h` — correction state, line 304

Before: `ServerPlayer` had no pending position-correction snapshot state.

After:

```cpp
    // Remains set until a subsequent client movement report is accepted, so a
    // snapshot can reliably tell the owning client to apply a correction.
    bool positionCorrectionPending = false;
```

### `src/network/packets.h` — authoritative state flag, lines 47-50

Before: highest declared state flag was `NET_STATE_REVIVING = 1 << 11`.

After:

```cpp
    NET_STATE_POSITION_CORRECTED = 1 << 13
```

The adjacent bit 12 actor-collision flag was already present in this working tree before this change.

### `src/network/multiplayer-context.h` — 16-bit state flags and pending correction, lines 431-436, 551

Before: `localServerStateFlags` was `uint8_t`; disagreement event IDs had only an unordered set.

After:

```cpp
    uint16_t localServerStateFlags = 0;
    bool hasPendingServerPositionCorrection = false;
    glm::vec3 pendingServerPositionCorrection{0.0f};
```

```cpp
    std::deque<uint32_t> processedDisagreementOrder;
```

### `src/network/server-packets.cpp` — server correction decision, lines 1130-1165 and 1262-1277

Before: a corrected report could be broadcast only after a global disagreement rate-limit check; no structured event identified each server correction.

After: every corrected movement report sets the pending snapshot bit, logs reported and accepted positions, and sends a distinct disagreement event for every nonzero correction without the old global rate-limit check.

### `src/network/server-players.cpp` — correction flag in snapshots, lines 1720-1724

Before: snapshots had no position-correction bit.

After:

```cpp
        if (player.positionCorrectionPending)
            flags |= NET_STATE_POSITION_CORRECTED;
```

### `src/network/multiplayer-tick.cpp` — authoritative position seeds fallback, lines 791-797

Before: `mpApplyAuthoritativeTransform` assigned only `player.pos` before the other transform fields.

After:

```cpp
    player.pos = ctx.localServerPosition;
    player.lastValidWorldCollisionPosition = ctx.localServerPosition;
    player.hasLastValidWorldCollisionPosition = true;
```

### `src/network/multiplayer-packets.cpp` — correction packet and deduplication, lines 1205-1255

Before: after 128 IDs, the dedup set was cleared wholesale; the diagnostic `duplicate` value was computed after insertion and therefore always true.

After: retain a rolling 128-ID deque/set window, evicting only the oldest ID. A position-correction packet for the local player records its authoritative destination (`position + correction`) for fixed-tick reconciliation. Session resets clear both structures and pending state.

### `src/network/multiplayer-reconcile.cpp` — correction snap independent of drift distance, lines 166-205

Before: local correction used ordinary reconciliation thresholds and had no dedicated server-correction flag handling.

After: a correction snapshot bit or a local correction event bypasses the ordinary drift threshold. The event path snaps to the transmitted accepted position while preserving current velocity; the snapshot path applies the full authoritative transform. Both seed the last-valid fallback.

### `src/network/disagreement-visuals.cpp` — always present position corrections, line 346

Before:

```cpp
    if (!NetworkingConfig::instance().data().disagreement.enabled)
        return;
```

After:

```cpp
    if (!NetworkingConfig::instance().data().disagreement.enabled &&
        event.reason != DISAGREEMENT_POSITION_CORRECTION)
        return;
```

Other disagreement types still honor the existing global toggle.

### `docs/specs/networking/networking.md` — geometry and correction contract, lines 41-64, 1084-1090, 1115-1122, 4321-4324, 4362-4365

Before: the spec had no precise geometry fallback/snap contract; existing feedback language discouraged normal small corrections and limited event visibility.

After: it specifies no movement substeps, full fixed-tick sweeps, 0.1 m residual-overlap fallback, preserved tangential movement, immediate owner snap independent of the 999 m ordinary drift threshold, and an effect for each distinct server correction.

## Review

- Read-only collision review agent: found long-query truncation, rejected-pose contact state, root-ray scope, and duplicate tolerance expansion. The first two findings were fixed; the tolerance expansion was removed. Root-ray limitation is documented above.
- Read-only networking review agent: found correction-flag timing, dedup-window clearing, best-effort transport, and configuration gating. Added direct position correction from the disagreement packet, fixed rolling deduplication, and made position-correction effects bypass the global enable toggle. Existing transport retransmission remains best-effort when its fixed queue is full; the owner also receives a correction state bit in snapshots.
- No review agent edited files.

## Validation and evidence

- `git diff --check`: reported trailing whitespace in pre-existing user edits, including informal text at the top of `docs/specs/networking/networking.md`; no whitespace was removed from those notes.
- Canonical build: invoked with `python devscripts/dev-loop.py --request build --wait-for-build --build-timeout 1200`; result `failed`.
- The build compiler reached the changed `movement-validation.cpp`, `multiplayer-packets.cpp`, `multiplayer-reconcile.cpp`, `disagreement-visuals.cpp`, `multiplayer-tick.cpp`, `server-packets.cpp`, `server-players.cpp`, and `actor-triangle-solver.cpp` files without reporting errors in those units. The build stopped due to pre-existing errors in `src/physics/movement/actor-pair-collision.cpp` (`Capsule` is not declared) and `src/sim/simulate-tick.cpp` (`ActorCollisionData::profileFor` is missing). The working tree already contained these actor-pair edits before this fix.
- No new executable was published. The dev-loop status reports the running client on the earlier build 2682 and the latest successful artifact stale relative to source; no game/client/server was restarted or stopped.
- Runtime scenario and fresh `EVENTS_JSONL_PATH` validation: not run because the new executable could not be built. Human movement feel, multiplayer delivery, wall-slide behavior, and visual effect acceptance remain unverified.
- No tests were added or run.

## Documents and focused review

- `docs/ROUTER.md`
- `docs/specs/movement/movement.md`
- `docs/specs/networking/networking.md`
- `docs/architecture/collision/collision.md`
- `docs/workflows/runtime-scenario-validation.md`
- `docs/operations/build-and-exe/build-and-exe.md`
- `docs/operations/task-completion/task-completion.md`
- `docs/architecture/live-development/live-development.md`
- `docs/specs/debug-logging/debug-logging.md`
- `docs/skills/spec-behavior-review-v1.md` — reviewed; source contract aligned, runtime acceptance blocked.
- `docs/skills/logging-checker-v1.md` — reviewed; rollback diagnostic categorized and rate-limited.
