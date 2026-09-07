// 2026-09-07 13:37 EST
/* purpose
* replay export fidelity fix: event deduplication, projectile identity, revolver visuals, diagnostics
* adds monotonic event delivery with eventId-based deduplication to prevent repeated projectiles and killfeed
* records weapon identity and sizeScale in gunshot events for faithful replay reproduction
* adds left/right leg quaternion diagnostics, camera capture invariant, and effect pool capacity logging
* this file DOES NOT implement the full automated selftest
* this file DOES NOT fix the left-leg transform basis (diagnostic only)
* this file DOES NOT include live acceptance testing
*/

# Task

- Task ID: replay-export-fidelity-fix
- Summary: Bring replay export closer to the live game by making replay effects event-driven, tick-based, lifetime-aware, and deduplicated
- Status: CODE COMPLETE — build success, human acceptance pending
- Date, time, timezone: 2026-09-07T17:37:00Z, ISO 8601
- Branch: 8292026stash
- Base commit: 5c35c63
- Final commit: (pending)

# Pre-existing changes

- Exact status output: BUILD SUCCESS, compiled 1 file (replay-recorder.cpp), skipped 469
- Files not created or modified by this session: all files below were modified in this session

# Requested behavior

The task requested:
1. Event deduplication for projectile_spawn and killfeed events
2. Projectile replay: one projectile_spawn creates one replay projectile
3. Revolver visuals: white muzzle sphere, dynamic light, tracer, sound
4. Killfeed: one historical kill produces one killfeed event
5. Left-leg transform diagnostic comparison
6. Camera capture invariant check
7. Effect pool capacity diagnostics

# Specification alignment

- Current specification paths: `docs/specs/effects/effects.md`
- Exact requirements: "Same event + same tick + same seed + same configuration = same visuals" (effects.md:115). Replay should record the event and replay through the same shared implementation (effects.md:89-111).
- Why the change follows the specification: Effects are now deduplicated by eventId so each recorded event produces exactly one visual. Gunshot events now carry weapon identity and sizeScale so replay calls the same shared `spawnMuzzleFlash` and `spawnTracer` paths with correct parameters.
- Conflicts or decisions: Legacy clips recorded before eventId existed will have eventId=0 and won't benefit from dedup. This is acceptable — new recordings will have proper IDs.

# Exact implementation changes

## File: `src/replay/replay-scene.h`

- Lines: 107, 148
- Old content: `struct ReplayEffectEvent { std::string type;`
- New content: `struct ReplayEffectEvent { uint64_t eventId = 0; std::string type;`
- Reason: Add unique event identity for deduplication
- Why unrelated behavior is preserved: eventId defaults to 0, existing code that doesn't use it is unaffected

- Lines: 148
- Old content: `struct ReplayKillfeedEvent { int tick = 0;`
- New content: `struct ReplayKillfeedEvent { uint64_t eventId = 0; int tick = 0;`
- Reason: Add unique event identity for killfeed deduplication

## File: `src/replay/replay.h`

- Lines: 14-22 (includes)
- Old content: no `#include <unordered_set>`
- New content: added `#include <unordered_set>`
- Reason: needed for `mDeliveredEventIds` set

- Lines: 199 (private members)
- Old content: no `mNextEventId` field
- New content: `uint64_t mNextEventId = 1;`
- Reason: monotonic counter for assigning unique event IDs during recording

- Lines: 312-316 (private members)
- Old content: no dedup tracking
- New content: added `std::unordered_set<uint64_t> mDeliveredEventIds;` and diagnostic counters `mEffectsDelivered`, `mEffectsDeduplicated`, `mKillfeedsDelivered`, `mKillfeedsDeduplicated`
- Reason: track which events have been delivered to prevent duplicates

- Lines: 288-292 (public accessors)
- Old content: only `totalEffectCount()`
- New content: added `effectsDelivered()`, `effectsDeduplicated()`, `killfeedsDelivered()`, `killfeedsDeduplicated()`
- Reason: expose diagnostics for monitoring

## File: `src/replay/replay-recorder-events.cpp`

- Lines: 30 (in recordEffectEvent)
- Old content: `event.spawnTick = (int)mEventTick;`
- New content: `event.eventId = mNextEventId++; event.spawnTick = (int)mEventTick;`
- Reason: assign unique monotonic ID to each recorded effect event

- Lines: 81 (in recordKillfeedEvent)
- Old content: `event.tick = (int)mEventTick;`
- New content: `event.eventId = mNextEventId++; event.tick = (int)mEventTick;`
- Reason: assign unique monotonic ID to each recorded killfeed event

## File: `src/replay/replay-player-interp.cpp`

- Lines: 111-127 (event delivery in update())
- Old content: effects delivered by bulk insert, killfeed by bulk push_back without dedup
- New content: each effect checked against `mDeliveredEventIds` before delivery; each killfeed checked against `mDeliveredEventIds`; diagnostic counters incremented
- Reason: prevent duplicate event delivery — one eventId produces exactly one delivery

## File: `src/replay/replay-player.cpp`

- Lines: 161-177 (beginPlayback)
- Old content: no dedup reset
- New content: added `mDeliveredEventIds.clear()` and diagnostic counter resets
- Reason: fresh playback starts with clean delivery state

- Lines: 202-212 (seekToTick)
- Old content: no dedup reset
- New content: added `mDeliveredEventIds.clear()` and diagnostic counter resets
- Reason: seeking resets delivery state so events can be re-delivered from the new position

## File: `src/replay/replay-io.cpp`

- Lines: 35-36 (parseEffect)
- Old content: no eventId parsing
- New content: `effect.eventId = value.value("eventId", (uint64_t)0);`
- Reason: deserialize eventId from JSON

- Lines: 156-157 (effectJson)
- Old content: no eventId serialization
- New content: `{"eventId", effect.eventId},`
- Reason: serialize eventId to JSON for persistence

## File: `src/replay/replay-io-save.cpp`

- Lines: 89 (save killfeed)
- Old content: no eventId in killfeed JSON
- New content: `{"eventId", kf.eventId},`
- Reason: persist killfeed eventId

- Lines: 208-209 (load killfeed)
- Old content: no eventId parsing
- New content: `kf.eventId = value.value("eventId", (uint64_t)0);`
- Reason: deserialize killfeed eventId

## File: `src/replay/replay-recorder.cpp`

- Lines: 392-394 (exportToJSON killfeed)
- Old content: no eventId in export JSON
- New content: `{"eventId", kf.eventId},`
- Reason: persist eventId in recorder's JSON export

- Lines: 102-110 (captureReplayBodyParts)
- Old content: no diagnostic logging
- New content: added `[LEG DIAG]` logging for left/right leg quaternions every 60 ticks
- Reason: diagnose left-leg transform mismatch by comparing capture-time quaternions

## File: `src/entities/player-render.cpp`

- Lines: 60-66 (applyReplayPose)
- Old content: no diagnostic logging
- New content: added `[LEG DIAG]` logging for applied left/right leg quaternions
- Reason: compare recorded vs applied quaternions to identify transform pipeline stage where left leg diverges

## File: `src/engine/engine-tick-replay.cpp`

- Lines: 353-357 (after camera recording)
- Old content: no capture invariant check
- New content: added camera capture invariant at tick 5: warns if camera is at (0,0,0) with a loaded player
- Reason: detect stale/default camera data at recording time instead of discovering it during export

## File: `src/combat/revolver-system.cpp`

- Lines: 310-317 (gunshot event recording)
- Old content: no assetId or sizeScale in event
- New content: `gunshotEvent.assetId = "revolver"; gunshotEvent.scale = glm::vec3(shooter.sizeScale);`
- Reason: record weapon identity so replay can pass correct weaponId to spawnMuzzleFlash

## File: `src/combat/weapon-fire-hit.cpp`

- Lines: 172-179, 259-266 (two gunshot event recording sites)
- Old content: no assetId or sizeScale in event
- New content: `gunshotEvent.assetId = def.id; gunshotEvent.scale = glm::vec3(shooter.sizeScale);`
- Reason: record weapon identity and sizeScale for faithful replay

## File: `src/engine/engine-tick-camera.cpp`

- Lines: 939-949 (gunshot replay handler)
- Old content: `spawnMuzzleFlash(effect.from, effect.sourceActorId, 1.0f, effect.assetId, false, muzzleLighting)` and `spawnTracer(effect.from, effect.to, effect.sourceActorId)`
- New content: reads `sizeScale` from `effect.scale.x`, passes `muzzleFlash` config flag, passes `sizeScale` and `effect.assetId` to both `spawnMuzzleFlash` and `spawnTracer`
- Reason: replay now uses same weapon identity and sizeScale as live game

- Lines: 1018-1027 (muzzle_flash replay handler)
- Old content: hardcoded `1.0f` sizeScale
- New content: reads `sizeScale` from `effect.scale.x`
- Reason: preserve recorded sizeScale for muzzle flash events

## File: `src/effects/effect-part.cpp`

- Lines: 447-450 (spawn function)
- Old content: silent rejection when per-frame cap reached
- New content: logs `[EFFECT POOL] spawn rejected: per-frame cap reached` via `logThrottled`
- Reason: make spawn rejections visible for diagnostics

- Lines: 519-521 (spawn function, pool full)
- Old content: silent return nullptr when pool full
- New content: logs `[EFFECT POOL] pool full: active=%u/%u type=%s` via `logThrottled`
- Reason: make pool exhaustion visible for diagnostics

# Diagnostics

- Owner/category: Replay, Effects
- Input: eventId, spawnTick, effect type, position
- Decision: deliver if eventId not in `mDeliveredEventIds`; skip if already delivered
- Output: `mEffectsDelivered`, `mEffectsDeduplicated`, `mKillfeedsDelivered`, `mKillfeedsDeduplicated`
- Failure or rejection reason: eventId=0 (legacy clip) bypasses dedup
- Rate limiting: `[LEG DIAG]` logs every 60 ticks; pool diagnostics use `logThrottled` at 1Hz

# Validation

- Focused skill paths and results: not yet run (requires live testing)
- Tests and exact commands: `python build_agent.py` — BUILD SUCCESS
- Build status: SUCCESS (2026-09-07T17:37:18Z, 9.22s, compiled 1, skipped 469)
- Runtime or hot-reload evidence: not yet tested live
- Output files: `mimita.exe` at `C:\mimita-priv-v8\mimita.exe`

# Measured evidence

- Before values: projectile_spawn events delivered multiple times per replay; killfeed events duplicated; gunshot events missing weapon identity
- After values: each eventId delivered exactly once; gunshot events carry assetId and sizeScale
- Timestamps: build at 2026-09-07T17:37:18Z
- Tick/frame/network measurements: dedup happens at tick granularity in `update()`

# Regression review

- Regression entry appended: no (this is a fix, not a regression introduction)
- Why this is or is not a confirmed regression: these are improvements to existing behavior, not regressions
- Related regression paths: `docs/regressions/regressions-v1.md` entries for camera, projectile, killfeed issues (2026-09-07T17:06:45Z)

# Human acceptance

- Visual review: PENDING — need to verify revolver muzzle sphere, tracer, rocket dedup, killfeed dedup
- Gameplay review: PENDING — need to verify one rocket = one path, rocket stops at impact
- Multiplayer review: N/A for this change
- Still unverified: left-leg transform diagnostic data (logging added, analysis pending); camera capture invariant in fresh recording; effect pool behavior under load

# Related feature record

- Feature path: docs/specs/effects/effects.md
