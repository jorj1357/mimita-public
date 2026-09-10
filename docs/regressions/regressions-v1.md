// 09 03 2026, 15 42
/* purpose
* record confirmed behavior breaks discovered through human review or playtesting
* preserve the exact wrong behavior, cause, fix, and proof for future agents
* connect each regression to the changelog session that introduced or fixed it
* this file DOES NOT record normal AI work history
* this file DOES NOT replace current specifications
* this file DOES NOT allow old entries to be rewritten or deleted
*/

# MiMITA Regression Records

This is an append-only record of confirmed regressions. Normal AI work history
belongs in `docs/changelog/`. A new entry must include the expected behavior,
actual behavior, exact specification, wrong code, corrected code, cause, fix,
proof, observed time, and related changelog file.

Whats this

- 9 2 2026 this tracks like  
- Ok if the behavior we want worked before, then later, it stopped working,   
- Write it here and why and then what fix etc 

9 2 2026 format

1. Issue: lala  
   1. Bad behavior  
      1.    
   2. Date and time first observed:   
      1.    
   3. Why bad behavior  
      1.     
   4. What fixed it, date and time  
      1.  
   5. What we learned  
      1. 

newest at top 9 3 2026

2026-09-10T17:46:58Z — Left-leg-only wrong-axis class: replay root-local flattening and quaternion hemisphere (confirmed root cause for replay; ragdoll inherits the same class)

1. Expected behavior: a body part's orientation is composed in its real skeleton
   parent space, and quaternion extraction is hemisphere-consistent, so the left
   and right legs (which are symmetric in the model) rotate identically.
2. Actual behavior (historical, replay): the left leg rendered flat/wrong while
   the right leg was correct. Human also reports the same visual asymmetry in
   live ragdoll mode (both first and third person).
3. Exact specification:
   1. `docs/specs/replays/replay-editor-and-export-v2.md`: the export must
      reconstruct the local client's experience, including skeletal pose.
   2. `docs/specs/ragdoll-retrograd/ragdoll-retrograd.md`: RAG-006/008/010/011
      (physically simulated, per-part collider, self-collision, no tunneling).
4. Why the old left-leg behavior was bad (two compounding causes):
   1. Replay capture removed only the player root yaw and stored each part as
      root-local; `applyReplayPose` then applied
      `root * translate(position) * mat4_cast(rotation)`, which flattened the
      skeleton and bypassed the real parent chain. The left leg could be
      interpreted in the wrong axis space.
   2. `glm::quat_cast` can return `q` or `-q` for the same rotation. Near the
      90-degree X rest pose plus large Z animation, extraction alternated
      hemispheres across frames, flipping the leg.
5. What fixed it previously:
   1. Parent-relative capture/apply: store `parentPartId`, capture
      `localTransform = inverse(parentWorld) * partWorld`, and reconstruct
      `parentWorld * localTransform` in two passes
      (`src/replay/replay-recorder.cpp`, `replay-player-load.cpp`,
      `replay-player-interp.cpp`, `player-render.cpp`).
   2. Hemisphere enforcement in `captureReplayBodyParts()`: after normalizing the
      extracted quaternion, negate it when the rotated local +Y points down
      (`src/replay/replay-recorder.cpp:108-120`).
   3. Human confirmation of the fix is recorded at
      `2026-09-07T20:29:18Z — NPC replay rockets and left-leg rotation confirmed
      working`.
6. New investigation finding (2026-09-10): the left/right asymmetry cannot come
   from parent space or mesh data in the current model. Parsing
   `Characters/DefaultGuy/character.glb` proved:
   1. the skeleton is flat: all six body parts are direct children of
      `plrOrigin` (node 6), so `skeletonParentPart` is the root for all parts;
   2. `leftLeg` and `rightLeg` have identical local rotation
      `[0.707,0,0,0.707]` and identical translation except the Y sign;
   3. their POSITION accessors and collider bounds are byte-identical.
   Therefore ragdoll derives symmetric frames for both legs, and the replay
   root-local/hemisphere class cannot explain a ragdoll left-only result through
   parent space.
7. Corrected code direction implemented for ragdoll:
   1. Canonicalize the bind orientation from the mesh matrix
      (`quatFromMatrixCanonical`, sign-normalized) so no part keeps an
      alternate hemisphere, mirroring the replay fix.
   2. Keep parent-relative node composition (`inverse(parentWorld) * childWorld`)
      that already existed; no flattening.
   3. Add the `[RAGDOLL SYM]` diagnostic (rate-limited, `Debug::Category::Ragdoll`)
      comparing left/right body position, composed node position, and quaternion
      so any remaining mismatch is observable instead of asserted.
8. Proof and status: source change + diagnostic; canonical build result recorded
   in the related changelog. Human visual confirmation still required. If the
   left leg still diverges after this, the next checks are the renderer's
   `physicalBody.partMeshes`/`nodeIndex` mapping and the head-camera framing,
   not the parent-space or hemisphere causes.
9. Related changelog:
   `docs/changelog/2026-09-10/20260910_174658-ragdoll-capsules-attachments-left-leg.md`.

2026-09-10T16:43:40Z — NPC rocket kills credited nobody, and GO was skipped on the first round after joining

1. Issue: NPCs never scored or logged a killfeed line, and `GO!!!` only appeared
   on the second FFA round, not the first countdown after joining.
   1. Expected behavior: an NPC killing a player (or another NPC) awards one
      point to the NPC and produces one killfeed/chat line such as
      `NPC-1000 killed NPC-1001 with Rocket Launcher`; `GO!!!` shows on every
      countdown, including the first.
   2. Actual behavior: the NPC's rocket killed the player, but the
      `DamageConfirmedEventPacket` carried `attacker=0` and no gamemode kill
      event was queued; the score stayed 0. The first countdown showed `3,2,1`,
      then stuck on `1`, then ACTIVE with no GO.
   3. Date and time first observed: 2026-09-10 12:25 EDT (NPC) and 2026-09-10
      12:25 EDT (GO).
   4. Why the bad behavior happened (cause): the projectile explosion path in
      `server-projectiles.cpp` used `projectile.ownerPlayerId` for attribution,
      but NPC-fired rockets have `ownerPlayerId=0` and store the NPC in
      `ownerNpcId`; `queueServerDamageConfirmedEvent` was called without
      `attackerNpcId`, so the kill owner was lost. Separately, `GO` was gated on
      the client applying the short `DUEL_PHASE_GO` packet. On the first join the
      client is busy loading and missed it: `Network_log_121030.txt` shows
      `phase=1` count 1, `phase=7` count 0, `phase=2` count 255, with
      `stateVersion` jumping 5→7. The countdown numbers survived because they are
      computed from `matchStartTick`/`serverTick`, but GO was phase-driven.
   5. Wrong code (examples):
      1. `src/network/server-projectiles.cpp`:
         `applyServerDamage(players, victim, projectile.ownerPlayerId, ...)` and
         `queueServerDamageConfirmedEvent(... projectile.id)` with no
         `attackerNpcId`.
      2. `src/network/server-damage.cpp`: re-attribution required
         `effectiveAttackerPlayerId != 0`, so ownerless/self killing blows could
         not be credited to a recent NPC attacker.
      3. `src/network/community-match-client.cpp`: GO visibility depended on the
         GO-phase packet, not on authoritative time.
   6. What fixed it:
      1. Resolve the projectile owner once (`ownerNpcId ? ownerNpcId :
         ownerPlayerId`) and pass it through both splash loops; credit NPC kills.
      2. Allow re-attribution when there is no real player attacker.
      3. NPC target selection now includes other NPCs (respecting teams) and
         routes victim damage/credit to the `ServerNpc`.
      4. Add `DuelStatePacket.goSeconds`; the client shows `GO!!!` for that
         window starting at the first ACTIVE when no GO packet was seen.
   7. Proof: `python build_agent.py` -> `BUILD SUCCESS`, return code 0. Source
      trace: `serverGamemodeRecordKill` is now reached from the rocket path with
      the NPC as killer, and `goVisible()` is time-driven in
      `community-match-client.cpp`. Runtime two-client/NPC acceptance remains.
   8. Related changelog:
      `docs/changelog/2026-09-10/20260910_164340-npc-kill-attribution-go.md`.

2026-09-10T15:52:52Z — FFA player→NPC kills gave no score and duplicated NPC-kill code hid the authoritative kill owner

1. Issue: In FFA and TDM, killing an NPC produced no score for the player and no
   NPC score when an NPC killed the player; killfeed/chat lines were missing or
   inconsistent.
   1. Expected behavior: every valid kill in either direction awards one point
      to the killer, updates the replicated score/leaderboard on every client,
      and produces exactly one killfeed/chat line for the killer, victim, and
      all observers. Specification: `docs/specs/gamemodes/gamemodes.md`
      lifecycle, feature record `docs/features/gamemodes/ffa mode issues.md`.
   2. Actual behavior: `[SERVER NPC KILL]` printed on the server, but
      `GAMEMODE_ENQUEUE`, `GAMEMODE KILL QUEUE`, `KILL_QUEUE_PROMOTE`, and
      `FFA_SCORED` never appeared and every `HEARTBEAT` read
      `scores=[NPC-1000=0, admin=0]`. The killer's own NPC-kill killfeed line was
      suppressed.
   3. Date and time first observed: 2026-09-10 11:17 EDT.
   4. Why the bad behavior happened (cause): duplicated kill handling. The
      authoritative score owner is `server-gamemode.cpp`, fed only by the
      gamemode kill queue. The live lethal sites instead incremented
      `ServerPlayer::kills`, healed, and emitted persistence directly and never
      enqueued a gamemode kill event. The client rebuilt killfeed lines from
      damage packets in three places and `DeathSystem::kill` added a fourth, so
      a kill could be missing or duplicated.
   5. Wrong code (examples):
      1. `src/network/server-attack.cpp` killed the NPC and only ran
         `attacker->second.kills += 1; attacker->second.health = serverMaxHp();`
         plus `emitNpcKillPersistenceEvent`, with no gamemode kill enqueue.
      2. `src/network/multiplayer-shots.cpp` skipped the killfeed when
         `localShooterPredictedKill` was true, so the killer never saw their own
         NPC kill.
      3. `src/network/server-projectiles.cpp` and
         `src/network/server-packet-handlers.cpp` (godball, spy knife) had the
         same missing enqueue.
   6. What fixed it: one authoritative kill owner and one killfeed event.
      1. Added `serverGamemodeRecordKill(...)` in `server-gamemode.cpp` and
         `KillEventPacket` (type 68, `PROTOCOL_VERSION` 33) in `packets.h`.
      2. Routed every lethal site through it and removed the per-site credit,
         heal, and persistence copies.
      3. Removed the three client killfeed reconstruction paths and gated
         `DeathSystem::kill`'s local killfeed on a non-networked session.
      4. The client presents `KillEventPacket` once via
         `KillfeedManager::onKill` with a session+event dedup key.
   7. Proof: `python build_agent.py` -> `BUILD SUCCESS`, return code 0. Source
      trace: `[KILL EVENT]` is emitted by `serverGamemodeRecordKill`, which
      queues one `ServerGamemodeKillEvent` consumed by the FFA scorer that
      increments `d.ffaKills[killerId]` and is broadcast by
      `broadcastDuelState`. Runtime two-client/NPC acceptance still required.
   8. AI-model note: the duplicated implementation is believed to originate from
      AI-generated code from model `mimo v2.5`. The hypothesis that the model is
      the cause is not strong enough to assert; the falsifiable test is to solve
      this same issue using that model. That experiment is not part of this fix.
   9. Related changelog: `docs/changelog/2026-09-10/20260910_155252-unified-kill-event.md`.

2026-09-09T00:00:00Z — AI diagnosis must verify link integration before blaming a stale EXE — PROCESS REGRESSION

1. Confirmed recurring failure pattern:
   1. AI investigations often suggest that `mimita.exe` is stale when a feature appears inactive.
   2. Human experience indicates this was correct only rarely; in roughly fifty comparable cases, the more common cause was that the feature logic existed or compiled but was not linked through all surrounding runtime paths required for the behavior.
   3. A related pattern is that the core logic works, but the change fails to update the several related producers, packet fields, reset paths, snapshots, handlers, or consumers needed for the value to survive end to end.
2. Required investigation behavior:
   1. Treat a stale executable as a hypothesis, not the default conclusion.
   2. Trace the value from configuration through every producer, boundary, reset, consumer, and final application point.
   3. Verify the actual link/build result with `build_agent.py` and `build/changelog.txt` before claiming the binary is stale.
3. Status: CONFIRMED PROCESS LESSON — applies to future investigations and does not by itself identify a gameplay bug.

9 7 2026 — Spy Knife contact ticks use render/network-update time instead of the fixed 60 Hz simulation domain — CONFIRMED NOT FIXED

1. Evidence:
   1. Server trace at `Server_log_184741.txt` shows `serverTick=2035` while rejected knife contacts carry `contactTick=8002` and `contactTick=8006`.
   2. The server rejects these contacts as `reason=invalid_contact` because the claimed contact tick is in the future.
   3. The same trace proves packet transport and slot resolution now work: `[SPYKNIFE_NET] DISPATCH batch=1 bytes=272` and `[SPYKNIFE_AUTH] EQUIP_RESOLVED ... equippedSlot=12 logicalSlot=4 nativeSlot=12`.
2. Exact code disagreement:
   1. `src/combat/weapon-spyknife.cpp:374` stamps `hitResult.contactTick` from `gpMpContext->tick`.
   2. `src/network/multiplayer-tick.cpp:2139` increments `ctx.tick` once per `mpTick()` update, which is tied to network/frame updates rather than the fixed gameplay simulation loop.
   3. `src/network/multiplayer-tick.cpp:683-691` separately advances `ctx.clientSimulationTick` using a 1/60-second accumulator with a five-step catch-up cap.
   4. `src/engine/engine-tick-combat.cpp:68-75` has another fixed 60 Hz prediction counter, confirming the project has distinct frame/update and simulation clocks.
3. Expected behavior: physical contact and its claimed tick must use the fixed 60 Hz gameplay/simulation time, then be mapped to the server tick domain using the existing snapshot/server-tick estimate. A frame/network-update counter must not be sent as a historical collision tick.
4. Actual behavior: client contacts are stamped with a faster/different counter, producing future claims that are rejected before historical NPC collision validation and damage application.
5. Status: NOT FIXED — this is now the primary blocker after packet dispatch and weapon-set slot resolution were confirmed working.

9 7 2026 — Spy Knife reaches server dispatch but still applies no NPC damage — CONFIRMED NOT FIXED

1. Evidence from the 18:22-18:24 run:
   1. `pasted-text.txt` records `[SPYKNIFE_NET] DISPATCH batch=1 bytes=272` repeatedly, proving the generic request reaches the server dispatch path in this run.
   2. The server trace records the NPC remaining at `hp=9999` and contains no authoritative Spy Knife NPC-applied damage event.
   3. The older `Server_log_182251.txt` also records `SERVER EQUIP REJECT ... requestedSlot=10 ... reason=not-owned-or-unknown`, while the active Spy Knife configuration is slot 12. This is the leading current rejection hypothesis, not yet proven by a handler-level diagnostic.
2. Exact current code path:
   1. `src/combat/weapon-spyknife.cpp` records local contact, predicts presentation, and sends the six-contact batch.
   2. `src/network/server-packets.cpp` recognizes the generic request and logs `DISPATCH batch=1`.
   3. `src/network/server-packet-handlers.cpp:handleSpyKnifeHitClaim` validates attacker existence, active state, spawn generation, equipped Spy Knife, contact fields, historical NPC position, and distance before applying health.
   4. Several of those rejection branches previously returned silently, so the reason was invisible.
3. This attempt: added centralized `[SPYKNIFE_AUTH]` diagnostics for batch size, attacker state, spawn generation, equipped weapon, contact validation, dead NPC, distance rejection, and authoritative NPC application. No damage rules or collision rules were changed.
4. Expected next proof: one `[SPYKNIFE_AUTH] REJECT reason=...` or `[SPYKNIFE_AUTH] NPC_APPLIED ... healthBefore=... healthAfter=...` for every dispatched batch.
5. Status: NOT FIXED — rebuild and runtime NPC acceptance still required.

9 7 2026 — Spy Knife predicted hits do not produce authoritative NPC damage — CONFIRMED NOT FIXED

1. Issue: Spy Knife displays local hit effects and damage numbers, but the server does not reduce NPC health.
   1. Expected behavior: physical contact is detected at 60 Hz, contacts are batched at approximately 10 Hz, the server validates each historical contact, and authoritative NPC health changes according to `docs/specs/weapons/melee-weapons.md`.
   2. Actual behavior: `SpyKnife_log_181350.txt` records local hits and batches, including `HIT id=1000 ... hpBefore=100 hpAfter=100` and `CONTACT_BATCH_SENT ... count=6`; `Server_log_181344.txt` rejects the received payloads as `unknown-type` with `type=62` and `type=64`, including knife-sized payloads of 1196 bytes.
   3. Root cause status: the current runtime still has a client/server packet-protocol mismatch. The current source intends to send the generic NPC damage request type 17, but the observed server receives types 62/64 and rejects them before dispatch. The new diagnostics added in this session will identify whether the rebuilt client sends the wrong type or a transport/runtime binary is stale.
   4. Attempts recorded:
      1. Knife-only batched packet: rejected before `handleSpyKnifeHitClaim` as unknown type 64.
      2. Generic damage-request transport: implemented in source and rebuilt, but the supplied runtime log still shows types 62/64, so end-to-end acceptance is not yet proven.
      3. Client-authoritative NPC health mutation: removed; client now keeps presentation/prediction only and cannot be the final NPC health owner.
   5. Next evidence required: matching `[SPYKNIFE_NET] SEND` and server `[MELEE_NET] RECEIVED` records with type 17 and 1196 bytes, followed by `[SPYKNIFE_NET] DISPATCH batch=1` and authoritative damage/health-before/after output.
   6. Related specification: `docs/specs/weapons/melee-weapons.md` sections 7 and 8; `docs/specs/networking/networking.md` client prediction/server authority.
   7. Status: NOT FIXED — runtime two-client/NPC acceptance remains required.

9 7 2026 1255 — Camera stuck under the map in exported MP4 — NOT FIXED

1. Issue: exported MP4 shows camera at (0,0,0) instead of the player's recorded POV
   1. Bad behavior
      1. Exported video shows static view under the map at position (0, 0, 0)
      2. Camera does not follow the player's recorded perspective at all
      3. Every scene frame in the clip has `camera.position: [0.0, 0.0, 0.0]`
      4. The player's actual camera position during gameplay was (-1.50, 8.94, 61.11) — completely different
   2. Date and time first observed: 2026-09-07T16:04:25Z
   3. Why bad behavior
      1. The export subprocess's camera controller is skipped by the `anyFreecam` gate in `engine-tick-camera.cpp:628`
      2. `anyFreecam = (freecamEnabled || replayFreecam) && isKeyboardEnabled()`
      3. `isKeyboardEnabled()` defaults to `true` and is never reset in the subprocess
      4. So `anyFreecam = true`, and the `else if (!anyFreecam)` block that reads camera from the clip is skipped entirely
      5. Camera stays at default (0,0,0) for the entire export
      6. The clip itself is saved with (0,0,0) camera data because the recording block in `engine-tick-replay.cpp:343` captures `camera.pos` BEFORE the camera controller updates it (and in the subprocess, the camera controller never runs)
   4. What we tried and what happened

      Attempt 1 (2026-09-07T15:15:00Z) — Effects pipeline fix
         - Changed: `commitFrame()` in `replay.h` to merge `mPendingEffects` into scene frames
         - Thought: "maybe effects and camera share the same recording pipeline issue"
         - Result: Effects fix worked (effects now record), but camera still (0,0,0). Different bug entirely.

      Attempt 2 (2026-09-07T15:15:00Z) — Quaternion hemisphere fix
         - Changed: `captureReplayBodyParts()` in `replay-recorder.cpp` to enforce consistent quaternion hemisphere
         - Thought: "left leg flips because glm::quat_cast returns antipodal quaternions near 90-degree rest pose"
         - Result: This was for a different bug (left leg rotation). Did not affect camera.

      Attempt 3 (2026-09-07T16:04:00Z) — RPLXDEBUG removal + clip.load() fix
         - Changed: Removed raw-printf RPLXDEBUG logging, replaced with Debug::log; fixed `ReplayClip::load()` to accept empty sceneFrames
         - Thought: "clip.load() returns false, so subprocess never spawns, so no export happens"
         - Result: Clip now loads, subprocess spawns, but camera is still (0,0,0) in the exported MP4. The clip file itself has (0,0,0) camera data.

      Attempt 4 (2026-09-07T16:04:00Z) — Diagnostic logging
         - Changed: Added `Debug::warn` at `beginRecording()` and `makeClip()` entry points
         - Thought: "sceneFrames was empty before, need to trace why"
         - Result: Logging confirmed `mSceneFrameCount=765 mFrames=765` — recording works, scene frames exist, but camera data is (0,0,0) inside them.

      Attempt 5 (2026-09-07T16:55:00Z) — anyFreecam gate fix (LATEST)
         - Changed: `engine-tick-camera.cpp:628` from `else if (!anyFreecam)` to `else if (!anyFreecam || isReplayExportActive())`
         - Thought: "the camera controller is skipped during export because anyFreecam is true. During export, the camera should always read from the clip data."
         - Result: BUILD SUCCESS. NOT YET TESTED BY USER. This is the current best theory. If the camera controller runs during export, it should read `currentSceneFrame()->camera.position` and set `camera.pos` to the player's POV.

   5. What we learned
      1. The clip file's scene frames have `camera.position: [0, 0, 0]` — the camera was never recorded correctly
      2. In normal gameplay, `camera.pos` is set from mouse input BEFORE the recording block captures it, so the first tick has the correct position
      3. In the export subprocess, the camera starts at (0,0,0) and the camera controller is skipped (by `anyFreecam`), so `camera.pos` is never updated from the clip data
      4. The recording block at `engine-tick-replay.cpp:354` captures `camera.pos` — it does NOT read directly from the scene frame
      5. The camera controller at `engine-tick-camera.cpp:628` is the code that reads from `currentSceneFrame()` and sets `camera.pos` — but it's gated by `!anyFreecam`
      6. The `isKeyboardEnabled()` flag defaults to `true` and is never reset in the subprocess, making `anyFreecam = true` even when no freecam is needed
      7. The old 1951 regression (beginPlayback blocking recording) is NOT the current root cause — the recording condition fix is in place and recording works
      8. The old 1839 regression (clip.load() failing) was fixed by accepting empty sceneFrames — but the underlying data was already (0,0,0)

   6. Status: NOT FIXED — Attempt 5 needs user testing
   7. If Attempt 5 does not work, next steps:
      1. Check the export subprocess log for `CAM_CTRL_STATE` after the fix — if it still shows (0,0,0), the camera controller ran but `currentSceneFrame()` returned a frame with (0,0,0)
      2. If `currentSceneFrame()` returns (0,0,0), check if `seekToTick(0)` properly calls `rebuildInterpolatedFrameAtTick()` — the interpolated frame might not be populated
      3. If the interpolated frame is empty, check if `mClip.sceneFrames` is actually populated after `loadFromJSON()` in the subprocess
      4. If scene frames are populated but camera is (0,0,0), check if `jsonVec3()` in `replay-io.cpp` is failing to parse the camera position from the JSON
      5. If all else fails, read camera position directly from the scene frame in the recording block instead of from `camera.pos`

9 7 2026 1604 — Replay export fails: clip.load() returns false because sceneFrames is empty

1. Issue: pressing P to export replay fails with "CLIP EXPORT FAILED" — clip file is saved OK but cannot be loaded back
   1. Bad behavior
      1. Clip file `replays\09-07-2026\11-44-52-replay.json` is saved successfully (valid JSON, 900 input frames, 153 sound events)
      2. `ReplayClip::load()` returns false because `sceneFrames` array is empty `[]`
      3. Export fails at `startReplayExport()` pre-check before subprocess spawns
      4. No `ReplayExport_log_*.txt` is created (subprocess never launches)
      5. User sees "CLIP EXPORT FAILED" notification
   2. Date and time first observed: 2026-09-07T16:04:25Z
   3. Why bad behavior
      1. The clip has `sceneFrames=0` but `frames=900` — the ring buffer's `mSceneFrameCount` was 0 when `makeClip()` was called
      2. Both input frames and scene frames are recorded in the same `if (recordingReplayTick)` block, so if 900 input frames exist, scene frames should also exist
      3. The only code that resets `mSceneFrameCount = 0` is `beginRecording()` at `replay-recorder.cpp:125`, but that also clears `mFrames` (the input vector)
      4. `ReplayClip::load()` at `replay-io-save.cpp:219` had `return !sceneFrames.empty()` which rejects valid clips with empty sceneFrames
      5. Root cause of empty sceneFrames is UNKNOWN — diagnostic logging added to `beginRecording()` and `makeClip()` to trace on next attempt
   4. What fixed it, date and time: 2026-09-07 16:04 UTC (partial)
      1. Fixed `ReplayClip::load()` to accept clips with empty sceneFrames but valid frames: `return !sceneFrames.empty() || !frames.empty()`
      2. Added `Debug::warn` at start of `beginRecording()` to log when recording is restarted (previous tick/sceneFrameCount/frames state)
      3. Added `Debug::warn` at start of `makeClip()` to log `mSceneFrameCount`, `mFrames.size()`, `mTick`, and requested range
      4. These diagnostics will reveal on next attempt whether `beginRecording()` was called unexpectedly or if `mSceneFrameCount` is 0 for another reason
   5. What we learned
      1. The old 1951 camera regression fix (recording condition `|| isReplayExportActive()`) is still in place and correct
      2. The current failure is NOT the camera stuck at (0,0,0) — it's that the clip can't be loaded at all because sceneFrames is empty
      3. `ReplayClip::load()` was too strict — it rejected valid clips with input data but no scene frames
      4. The relationship between `mSceneFrameCount` (ring buffer) and `mFrames` (input vector) needs investigation — they should always be in sync since both are written in the same `if (recordingReplayTick)` block
      5. The `replay_export_debug.txt` file (RPLXDEBUG) was superseded by the central `Debug::log` system per logging spec

9 6 2026 2136 — Replay export camera fix confirmed working via diagnostic logging

1. Issue: replay export camera stuck under the world, not following player POV
   1. Bad behavior
      1. Exported video shows camera at ground level / (0,0,0) for entire clip
      2. Camera does not follow the player's recorded perspective
      3. All three exports from 9/6 (18:53, 19:35, 19:54) had this bug
   2. Date and time first observed: 9 6 2026 ~19:35 EST (same as 1951 regression)
   3. Why bad behavior
      1. Same root cause as 1951 regression: `beginPlayback()` in subprocess blocked recording
      2. Recording condition `isRecording() && !replayPlaybackActive` was false during export
      3. Scene frames in clip had empty camera data (tick=0, camera=(0,0,0), no actors)
      4. The fix at 19:51 was correct in source code but the running binary was not rebuilt
      5. Clips recorded with old binary had empty camera data baked into JSON
      6. Rebuilding binary and re-recording clips was required to fix the output
   4. What fixed it, date and time: 9 6 2026 2136 EST (confirmation via diagnostic logging)
      1. Verified the 19:51 fix is correct by adding diagnostic logging to engine-tick-camera.cpp
      2. Ran export subprocess directly: `mimita.exe --export-replay <clip> --output <path> --visible --timeout 30 --replay-export-verbose`
      3. Diagnostic logs confirmed camera position follows clip data at every tick:
         - Tick 0: frameCamPos=(6.97, -26.14, 95.66) → controller sets it → finalPos=(6.97, -26.14, 95.66)
         - Tick 1: frameCamPos=(6.97, -26.14, 95.55) → correctly interpolating downward
         - Tick 2: frameCamPos=(6.97, -26.14, 95.43) → still following clip data
      4. Export produced 5.4MB MP4 with correct camera (vs 600-760KB from broken builds)
      5. User confirmed test export plays correctly with camera following player
   5. What we learned
      1. The 1951 fix (recording condition + removing beginPlayback) is correct and sufficient
      2. The camera controller in "Recorded" mode reads scene frames and sets camera.pos correctly
      3. The export subprocess is identical whether run directly or spawned from the game
      4. Old clips recorded with broken binary have empty camera data that cannot be fixed by rebuilding — must re-record
      5. The `test-diag-export.mp4` (5.4MB) vs user's broken exports (600KB) proves the camera fix works when clip has valid data
      6. The `anyFreecam` flag was 0 during export, `camCtrlMode=0` (Recorded), `hasFrame=1` — all conditions correct
   6. Proof: diagnostic log file at `logs/09-06-2026/ReplayExport_log_*.txt` shows camera position at every tick matching clip data
   7. Files changed:
      1. `src/engine/engine-tick-replay.cpp:303-304` — recording condition: `isRecording() && (!replayPlaybackActive || isReplayExportActive())`
      2. `src/replay/replay-export-subprocess.cpp:281-288` — removed `beginPlayback()` before `seekToTick(0)`
   8. How to prevent this from breaking again
      1. NEVER call `beginPlayback()` in the export subprocess — `seekToTick()` is sufficient
      2. NEVER change the recording condition to remove the `|| isReplayExportActive()` check
      3. When testing export, always rebuild binary BEFORE recording new clips
      4. Verify clip JSON has non-zero camera positions in scene frames before debugging export issues

9 6 2026 1951 — Replay export camera stuck at (0,0,0) instead of following player POV

1. Issue: replay export produces valid MP4 with outro, but camera is stuck at position (0,0,0) for the entire export instead of following the player's recorded POV
   1. Bad behavior
      1. Exported video shows static view under the map at (0,0,0)
      2. Camera does not follow the player's recorded perspective
      3. All 619 scene frames in the clip have tick=0, camera=(0,0,0), no actors
   2. Date and time first observed: 9 6 2026 ~19:35 EST
   3. Why bad behavior
      1. `replay-export-subprocess.cpp` called `beginPlayback()` which set `mPlaying=true`
      2. This made `replayPlaybackActive = gReplayPlayer.isPlaying()` return true
      3. The recording condition `isRecording() && !replayPlaybackActive` became false
      4. The recording block in `engine-tick-replay.cpp` was skipped entirely
      5. Scene frames were never populated with camera position, actors, or tick data
      6. Ring buffer got empty frames (tick=0, camera=(0,0,0), no actors)
      7. `makeClip` copied these empty frames into the clip
   4. What fixed it, date and time: 9 6 2026 19:51 EST
      1. Changed recording condition to `isRecording() && (!replayPlaybackActive || isReplayExportActive())`
      2. Removed `beginPlayback()` from the export subprocess (seekToTick already sets mPlaying=true)
   5. What we learned
      1. `beginPlayback()` has side effects beyond setting mPlaying — it blocks recording via the replayPlaybackActive check
      2. The recording condition must account for the export state to allow recording during export
      3. `seekToTick()` is sufficient for the export subprocess — `beginPlayback()` is redundant and harmful
      4. Empty ring buffer frames propagate through makeClip into the exported clip, producing camera at (0,0,0)
      5. The export subprocess and main process have separate REPLAY_RECORDER instances — the subprocess's recorder must be properly initialized for recording to work

9 6 2026 1839 — Replay export produces 261-byte empty MP4 (totalTicks=0)

1. Issue: pressing P to export replay produces a 261-byte MP4 with no video content, and the outro fails to append
   1. Bad behavior
      1. Export produces 261-byte file (MP4 header only, no video frames)
      2. Outro append fails because MP4 has no video stream
      3. Export completes in 0.5 seconds with only 1 frame captured
   2. Date and time first observed: 9 6 2026 ~18:12 EST
   3. Why bad behavior
      1. `startReplayExport()` in `replay-export-json.cpp` spawned a subprocess without loading the clip to determine `gJob.totalTicks`
      2. The main process's `gJob.totalTicks` remained at 0 (default)
      3. The export loop checked `doneTick >= gJob.totalTicks` → `0 >= 0` = true → stopped after 1 frame
      4. The subprocess loaded the clip and set its own `gJob.totalTicks`, but this was the subprocess's copy — the main process's value was never updated
   4. What fixed it, date and time: 9 6 2026 18:39 EST
      1. Added `ReplayClip::load(jsonPath)` in `startReplayExport()` before spawning the subprocess
      2. Set `gJob.totalTicks = clip.header.tickCount` from the loaded clip
      3. Added validation: fail if clip has 0 ticks and no scene frames
   5. What we learned
      1. The subprocess export architecture means the main process and subprocess have separate copies of `gJob` — changes in the subprocess don't propagate back
      2. The main process must load the clip to extract metadata (totalTicks) before spawning the subprocess
      3. The 261-byte file was an MP4 container header with no video content — FFmpeg creates the output file on `BeginWriting()` but writes no frames when totalTicks=0
      4. The outro append fails on empty MP4s because FFmpeg's concat filter requires valid video streams
      5. Always validate clip metadata before starting export — don't assume the subprocess will fix it

9 3 2026

1. Issue: the website https://mimita.fun the signing up and logging in is broken 9 3 2026 1526
   1. Bad behavior  
      1.    i go to the site and i log in and it sas database connection failed. check server logs.
      2. thats not good and sucks dick
   2. Date and time first observed:   
      1.    this hapepned before, but its happened first viewed at  9 3 2026 at 201 am est  bc of a discrod message in mimita discord
      2. viewed again  9 3 2026 1527
   3. Why bad behavior  
      1.    i need to be able to log in like thats a  core, if mimita is a tree, logging in/signingup is the roots of the tree, thats so so so imporatnt to work 
   4. What we learned  
      1.    
   5. What fixed it, date and time  
      1.  

9 2 2026

1. Issue: ui making performance suck dick   
   1. Bad behavior  
      1.  It suckds dick and t  
      2. Takes like 10ms on a integrapted graphics computer  w no gpu  to render ui thats bad   
   2. Date and time first observed:   
      1.  Like  prob aug 1 2026 but recent was  aug 30 2026  
   3. Why bad behavior  
      1.  Bc all the ui was getting drawn   
   4. What we learned  
      1.  Do not draw all the ui all in one, matter of fact  
      2. Batch evreuthing we can  
      3. Dont collide with all triangels in the whole world either  
   5. What fixed it, date and time  
      1.  Batching ui calls into 1 singel call  
      2. Made fps go to liek 100 minimum on a family computer  
      3. Just needs better  tweaks long term for better perofmance etc   
   
2. Issue: collisions sucked dick and made huge frame time lag when getting close to a big clinder  
   1. 9 2 2026 fill in later bc thats liek mimita preview liek v2.14 on mimtia youtube channel

9 6 2026 resolution for the 9 3 2026 website authentication outage

1. Expected behavior: `https://mimita.fun` signup and signin requests can query
   the account database and return normal validation or authentication results.
2. Actual behavior: PostgreSQL cluster `14/main` was down, with no PostgreSQL
   process or listener on port 5432. The online `mimita-api` process returned
   `500` and `ECONNREFUSED` for `/api/auth/signin`, `/api/auth/me`, and
   `/api/site/banner`.
3. Why it happened: the API's configured database dependency was unavailable;
   the application could not connect to PostgreSQL. This was an infrastructure
   service outage, not a bad username, password, signup form, or frontend route.
4. What fixed it, 9 6 2026 08:33 EST: started only the existing PostgreSQL
   `14/main` cluster on the VPS. It became `online` and `pg_isready` reported
   `accepting connections`; no code or VPS files were changed.
5. Proof: a correctly formatted non-mutating invalid-signin probe returned
   HTTP `401` with `invalid username/email or password`, proving the auth query
   path reached the database instead of failing with HTTP `500`.
6. Related session record: `docs/changelog/09-06-2026/09-06-2026-08-34-02-website-auth-recovery.md`.
7. 9 6 2026 0900 extra note: SO ENSURE THE VPS IS ALWAYS RUNNNING, AND IF DATABASE ISSUES HAPPEN, NEED TO BE ABLE TO START THE VPS FROM MOBILE PHONE NOT JUST PC

9 6 2026
1. Issue: all UI was drawn individually calls, making perfomrance very bad on low power devices and all devices in general. goes directly against a big assertion/invariant of  as close to 0ms frame times as we can possibly get, for any device at all
   1. Bad behavior  
      1.    all UI renderd individually, meaning each new letter = new draw call, unecessary work
   2. Date and time first observed:   
      1.    not sure it has been like that for like months maybe like 5 1 2026, but observerd again in a bad way like 8 31 2026s
   3. Why bad behavior  
      1.     because we didnt even know the code was bad bc i just assumed that it was fine, and also bc the pc i use to code it is powerful device so its not fair
   4. What fixed it, date and time  
      1.  we fixed it sometime between  8 31 2026 and like 9 6 2026, its in the git commit history, i committed from the low power device ihave
   5. What we learned  
      1. sometimes code is not efficient even if it seems like it, like who would thiink teh UI is taking 3ms to render every single frame, so question all assumptions over and over bc it might be the most randomest little thing making the issues happen 

   ## 2. 9 6 2026 2000 — Articles lost after VPS git pull

   1. Bad behavior
      1. 20-30 articles created through the admin editor at /admin/articles disappeared from the live site
      2. Only 1 article remains (welcome-to-mimita-news.md)
      3. The /articles page shows the article but clicking it does nothing (broken link)
   2. Date and time first observed: 9 6 2026 ~19:30 UTC
   3. Why bad behavior
      1. Articles are stored as .md files in content/articles/ on the VPS filesystem
      2. These files were never committed to git — they existed only on the VPS
      3. When we ran `git pull --ff-only` to deploy the data-saving persistence, the content/articles/ directory was updated to match the repo state (which only had 1 article)
      4. The untracked .md files were overwritten/lost by the pull
      5. Additionally, ArticlesIndex.jsx used `article.url` but the generated JSON has no `url` field — only `slug` — so links were broken even for the remaining article
   4. What fixed it, date and time
      1. Fixed ArticlesIndex.jsx: changed `article.url` to `/articles/${article.slug}` (9 6 2026)
      2. Articles cannot be recovered from git — they were never committed
      3. Added rule to vps-deployment.md: content/articles/ must be committed to git so articles survive deployments
   5. What we learned
      1. Files created on the VPS through the admin editor are NOT automatically committed to git
      2. `git pull --ff-only` does not preserve untracked files in tracked directories
      3. Always commit user-generated content (articles) to git before deploying
      4. The ARTICLES_DIR path in admin.js resolves to content/articles/ at the repo root — this directory must be tracked

   ## 3. 9 6 2026 2000 — Profile stats not visible on live site

   1. Bad behavior
      1. The profile page at /users/admin did not show gold, XP, kills, deaths, or playtime
      2. The API returned correct data but the frontend didn't render it
   2. Date and time first observed: 9 6 2026 ~19:30 UTC
   3. Why bad behavior
      1. ProfileStats.jsx and persistentStats.js were added to the repo on 9 6 2026
      2. The VPS frontend dist was built on 9 1 2026 — before these components existed
      3. The deployed JS bundle had 0 matches for ProfileStats, playtimeTicks, or formatPersistentStat
      4. The VPS deployment procedure did not include a frontend rebuild step
   4. What fixed it, date and time
      1. Added "rebuild frontend" step to vps-deployment.md (9 6 2026)
      2. Ran `npm run build` on VPS to rebuild dist with new components (9 6 2026)
   5. What we learned
      1. Every deployment that touches website/src/ MUST rebuild the frontend
      2. A stale dist/ serves old JavaScript that may lack new components
       3. The deployment procedure must include `cd /root/mimita-site/website && npm run build` as a mandatory step

9 6 2026 2000 — In-game sign-in fails with "Could not load account data" and status text overlaps error

   Game version: v2.0.6
   Git branch: 8292026stash
   Git HEAD: decc9fa
   Affected clients: all v2.0.x releases (v2.0.0 through v2.0.6)
   Affected endpoint: GET /api/game/me (bootstrap)

1. Issue: signing in through the in-game exe fails with "Could not load account data" error, and the green "Signing in..." / "Loading account..." status text overlaps the red error text at the same screen position
   1. Bad behavior
      1. User enters correct admin credentials and clicks Sign In
      2. Login succeeds (HTTP 200, accountId=1, username=admin returned)
      3. The `getGameBootstrap()` call to `GET https://mimita.fun/api/game/me` throws a JSON parse exception: `json.exception.type_error.302: type must be number, but is string`
      4. Bootstrap fails, error "Could not load account data" is set
      5. But the green status text ("Loading account...") is never cleared, so both green status and red error render at position (720, 504) overlapping each other
   2. Date and time first observed: 2026-09-06T19:40:00Z (first log capture), confirmed again at 2026-09-06T00:02:00Z
   3. Why bad behavior
      1. Three bugs combined:
         A. SERVER-SIDE: node-postgres returns PostgreSQL BIGINT columns (OID 20) as JavaScript strings by default. The `game_stats` table has BIGINT columns: `total_xp`, `gold`, `playtime_ticks`, `playtime_seconds`, `lifetime_player_kills`, `lifetime_npc_kills`, `lifetime_deaths`. The `/api/game/me` endpoint returns these as `"0"` (string) instead of `0` (number). The `db.js` file had no `pg.types.setTypeParser` configuration.
         B. CLIENT-SIDE: `parseStats()` in `api-client.cpp` used `s.value("total_xp", 0LL)` which throws when the JSON value is a string like `"0"` instead of a number `0`. All v2.0.x clients have this bug.
         C. CLIENT-SIDE: `setError()` in `auth-controller.cpp` set `mRuntime.state = Failed` and `mRuntime.errorMessage` but never cleared `mRuntime.statusText`. Since both `statusText` and `errorText` elements in `login-menu.json` are at the same position (x=720, y=504), both rendered on screen simultaneously.
   4. What fixed it, date and time: 2026-09-06T00:05:00Z
      1. Fix A — SERVER: Added `pg.types.setTypeParser(20, parseInt)` to `db.js` to convert BIGINT to JavaScript Number globally. This fixes ALL existing v2.0.x clients without requiring a new release.
      2. Fix B — SERVER: Fixed `defaultStats()` in `game-api.js` line 38: changed `playtime_ticks: "0"` (string) to `playtime_ticks: 0` (number). This fixes the fallback for new users with no stats row.
      3. Fix C — CLIENT: Replaced direct `.value()` calls with helper functions `jsonInt()`, `jsonLong()`, `jsonFloat()` in `parseStats()` that check `is_number_integer()`, `is_number()`, or `is_string()` and convert accordingly. This makes the client robust against both string and number values.
      4. Fix D — CLIENT: Added `mRuntime.statusText.clear()` to `setError()` so the green status text disappears when an error is set.
      5. Fix E — CLIENT: Added `Debug::warn(Debug::Category::Auth, ...)` at all 5 `getGameBootstrap()` failure return paths so future failures are diagnosable from logs.
   5. What we learned
      1. node-postgres returns PostgreSQL BIGINT as JavaScript strings by default (not numbers). This is deliberate to avoid precision loss beyond Number.MAX_SAFE_INTEGER. Any code that reads BIGINT columns must handle string values. The proper fix is `pg.types.setTypeParser(20, parseInt)` in `db.js`.
      2. Never assume JSON numeric fields are always numbers. Any field that goes through a database driver, HTTP transport, or JSON serialization boundary can be a string. Use defensive parsing (check type before get).
      3. `setError()` must clear all previous status text. If it doesn't, both status and error text render at the same GUI position and overlap visually.
      4. Silent failure paths in network calls are dangerous — the original `getGameBootstrap()` had 5 return paths with zero logging. Adding logging at each path immediately revealed the root cause on the first test run.
      5. The server-side `defaultStats()` must return numbers, not strings, for all numeric fields. A string default creates a type inconsistency that breaks clients.
      6. Server-side fixes (db.js type parser) help ALL existing clients immediately without requiring a new game release. Always prefer server-side fixes for backward compatibility.
   6. Proof: after deploying the server fix, v2.0.6 clients can sign in successfully. The `/api/game/me` endpoint now returns numeric fields as numbers. The client bootstrap completes and transitions to the main menu.
   7. Files changed:
      1. `website/server/db.js:17` — added `pg.types.setTypeParser(20, parseInt)` to convert BIGINT to Number
      2. `website/server/game-api.js:38` — fixed `playtime_ticks: "0"` → `playtime_ticks: 0`
      3. `src/auth/auth-controller.cpp:263-268` — added `mRuntime.statusText.clear()` to `setError()`
      4. `src/website/api-client.cpp:300-343` — added `jsonInt()`, `jsonLong()`, `jsonFloat()` helpers and rewrote `parseStats()` to use them
      5. `src/website/api-client.cpp:426-475` — added `Debug::warn` logging at all 5 `getGameBootstrap()` failure paths
   8. How to prevent this from breaking again
      1. NEVER remove or comment out the `pg.types.setTypeParser(20, parseInt)` line in `db.js`. Without it, all BIGINT fields return as strings and break C++ clients.
      2. NEVER use `.value("key", 0)` on a JSON field that comes from an external API without checking the type first. Use the `jsonInt`/`jsonLong`/`jsonFloat` helpers.
      3. ALWAYS clear `mRuntime.statusText` in `setError()` — any new code path that sets an error must not leave stale status text.
      4. When adding new fields to `GameStats` or `GameUserInfo`, add them to `parseStats`/`parseUserInfo` with the same string-or-number defensive parsing.
      5. The server-side `defaultStats()` in `game-api.js` must return numbers, not strings, for all numeric fields.
   6. When changing the database schema (adding BIGINT columns), always verify the API response types by curling the endpoint and checking JSON types.

## 9 7 2026 — VIP source changes not visible through SSH-tunnel development site (UNRESOLVED)

1. Bad behavior
   1. The local repository file `website/src/pages/Vip.jsx` contains the prepaid slider markup at lines 191-202, including `input type="range"`, `min="1"`, `max="12"`, and `step="1"`.
   2. The browser page opened through `website/npm-run-dev-ssh-v2.bat` shows only the monthly subscription buttons. It does not show the prepaid slider, prepaid summary, or lifetime purchase button.
   3. The user can see the source around line 190 as JSX closing syntax (`)}`), but the running page does not correspond to the current source file.
2. Date and time first observed: 2026-09-07, local development session.
3. Current evidence and likely boundary
   1. `website/npm-run-dev-ssh-v2.bat` starts an SSH tunnel with `ssh -L 3002:localhost:3002 root@107.191.48.226` and separately starts local Vite with `npm run dev`.
   2. The local React page calls `/api/vip/config`; the slider is conditionally rendered only when the API returns a purchase with `type: "prepaid"`.
   3. The VPS API can therefore supply an older configuration even when the local Vite bundle contains the newer `Vip.jsx`, especially because the API changes have not yet been deployed to the VPS.
   4. A previous regression on 2026-09-06 confirmed the same class of failure: source/frontend changes were invisible because the deployed `dist/` bundle had not been rebuilt. That entry is `docs/regressions/regressions-v1.md`, “Profile stats not visible on live site.”
4. Status: UNRESOLVED — do not mark fixed until the browser network response for `/api/vip/config`, the served Vite/bundle source, and the API/frontend deployment commit are compared and the slider is visibly accepted in the browser.
5. Required investigation
   1. Confirm which process owns ports 5173 and 3002 and whether the browser is actually at `localhost:5173`.
   2. Inspect the browser response from `/api/vip/config` and confirm it includes `prepaid` and `lifetime` for all three tiers.
   3. Confirm Vite is serving `C:\mimita-priv-v8\website\src\pages\Vip.jsx`, not a different checkout or stale `dist/` directory.
   4. If the API is the old VPS version, deploy only after confirming the exact branch and commit, then rebuild the frontend and restart the relevant service.

## 9 7 2026 — VIP slider missing because VPS API was behind local frontend (RESOLVED)

1. Bad behavior
   1. The local `website/src/pages/Vip.jsx` contained the prepaid slider, but the browser page showed only monthly subscription buttons.
   2. The slider was conditionally rendered only when `/api/vip/config` returned a `prepaid` purchase type.
2. Cause and fix
   1. The local frontend and VPS API were on different revisions. The local source expected the new prepaid/lifetime configuration, while the tunneled API was still serving the older VIP configuration.
   2. The VPS was updated from the confirmed Git revision, its website frontend was rebuilt, database migrations were run, and only `mimita-api` was restarted.
3. Resolution evidence
   1. VPS output reported a successful Vite build.
   2. VPS output reported `database migrations complete`.
   3. PM2 reported `mimita-api` online after restart.
   4. The prior untracked VPS files were preserved and reported.
4. Lesson
   1. For tunneled local testing, frontend source and the VPS API must be deployed from the same reviewed revision. A local Vite rebuild alone cannot make an old VPS API return the new purchase types.

## 9 7 2026 — Authentication endpoints return HTTP 500 after VIP deployment (UNRESOLVED)

1. Bad behavior
   1. On the local Vite page, `GET http://localhost:5173/api/auth/me` returns HTTP 500.
   2. `POST http://localhost:5173/api/auth/signin` returns HTTP 500 during sign-in attempts.
   3. The browser reports `auth state invalid` and cannot complete sign-in. `/api/vip/config` still returns HTTP success.
2. Date and time first observed: 2026-09-07, immediately after the VPS pull, frontend build, migration, and `mimita-api` restart.
3. Changes immediately preceding the symptom
   1. The VPS pulled the confirmed latest repository revision.
   2. `cd /root/mimita-site/website && npm run build` completed successfully.
   3. `npm run migrate` completed with `database migrations complete`.
   4. PM2 restarted `mimita-api`, which reported online.
   5. Existing untracked VPS files were preserved; no direct production file edits were performed.
4. Current status: UNRESOLVED. The browser output proves an API-side 500, but does not identify whether the cause is database connectivity, schema/migration state, environment loading, session configuration, or an application exception.
5. Required next investigation
   1. Read the `mimita-api` PM2 error/output logs at the exact sign-in request time.
   2. Check PostgreSQL service/readiness and the `mimita_db` migration version without exposing credentials or user records.
   3. Test an invalid sign-in request directly against the VPS API and require a controlled HTTP 401/400 response rather than 500.
   4. Compare the deployed commit, website environment-variable presence, and API startup logs with the pre-deployment state.
   5. Do not claim this is caused by the VIP UI or change authentication code until the first server-side exception is identified.

6. Evidence collected directly from VPS logs: 2026-09-07T16:20:00Z
   1. PostgreSQL service was active and `pg_isready` reported accepting connections.
   2. PM2 showed `mimita-api` online at `/root/mimita-site/website/server/server.js`.
   3. The deployed Git revision was `bbaf43d09ad0f2fb5bcb29d6115171f463c9490e`.
   4. Repeated API errors for `/api/auth/me`, `/api/auth/signin`, `/api/profile/131`, and game login all reported PostgreSQL error code `42703`: `column "style_revision" does not exist`.
   5. The same missing column also caused VIP entitlement subscription-sync errors, proving this is a shared schema mismatch rather than a signin-only failure.
   6. The deployed migration runner reported success because its version ledger can consider the historical bootstrap already applied; adding a statement to that historical bootstrap does not guarantee it runs on an existing database.
7. Corrective direction: create and test a new forward migration that adds `vip_name_styles.style_revision` with `ADD COLUMN IF NOT EXISTS`, deploy it through the repository migration path, then verify auth/profile/game-login requests. Do not apply an ad hoc production SQL patch or mark this regression resolved yet.

## 2026-09-07T17:22:55Z login issue
jorj - this not official format not good but  when we edit netowkring stuff or database stuff i noticeit makes like login issues, so we should make a centralized  data or netwroking info controller, bc we cant keep having failures just because we added 1 more field to a json and the database entirely fails bc it cant handle  one more, the database should get autoupdated somehow, same with the ingame mimita.exe code 

## 2026-09-07T17:30:26Z — Full-access Codex enabled VPS diagnosis and recovery (RESOLVED)

1. After the OpenAI Codex desktop app was changed to Full access mode, GPT-5.6 on Windows could run `ssh mimita-vps`, inspect VPS logs, identify the missing `style_revision` column, create and deploy the forward migration, restart the API, and verify login recovery in one pass.
2. Exact deployment proof: `C:\mimita-priv-v8\docs\changelog\2026-09-07\20260907_162530_vip-style-revision-migration.md`.

## 2026-09-07T17:30:26Z — Prepaid VIP slider amount differs from Stripe Checkout (UNRESOLVED)

1. Selecting 7 prepaid months can show one amount on `/vip`, while Stripe Checkout shows the fixed 12-month amount such as `$19.98`.
2. `Vip.jsx` sends `purchase_type: "prepaid"` and `months`; `vip-payments.js` must calculate the amount server-side and use inline Stripe `price_data.unit_amount` for that exact amount.
3. Required evidence: browser request body, server checkout log, Stripe session line item, and `vip_orders.amount_cents` for the same order.

## 2026-09-07T17:30:26Z — Lifetime VIP buttons report Stripe not configured (UNRESOLVED)

1. Lifetime buttons are disabled because `/api/vip/config` reports `configured: false` when the API process does not see the lifetime Stripe Price environment keys.
2. The VPS must independently contain the three lifetime key values; the local `.env` is not automatically used by the VPS.
3. Status remains unresolved until VPS key presence is verified without printing values and the API reports all three lifetime options configured.

## 2026-09-07T17:45:00Z — Replay quick-export camera stuck at world origin (UNRESOLVED recurrence)

1. Issue: pressing `P` after gameplay or a server kill creates an export, but the MP4 camera remains under the map near `(0,0,0)` instead of following the exporting player's camera.
   1. Bad behavior
      1. Export completes, but the camera is static or otherwise does not move with the local player's recorded POV.
      2. The result is contrary to the replay export requirement that the MP4 reconstruct the local client's experience, including the local camera and camera mode.
   2. Date and time first observed in this investigation: 2026-09-07, user-reported current behavior.
   3. Specification
      1. `docs/specs/replays/replay-editor-and-export-v2.md` section 1.2 requires quick export to recreate what the local player experienced, including `local camera`, `local camera transform`, and camera mode.
      2. Section 7.10 requires the MP4 to reconstruct the local client's experience for the replay period, including `local camera` and `camera mode`.
      3. Section 10.1 lists `correct local camera` and `correct camera mode` as hard quick-export correctness requirements.
   4. Exact current code path
      1. `src/engine/engine-tick-replay.cpp:303-304` records while exporting with `gReplayRecorder.isRecording() && (!replayPlaybackActive || isReplayExportActive())`.
      2. `src/engine/engine-tick-replay.cpp:352-356` copies `camera.pos`, rotation, and FOV into each `ReplaySceneFrame`.
      3. `src/replay/replay-export-subprocess.cpp:337-345` intentionally uses `seekToTick(0)` and does not call `beginPlayback()`.
      4. `src/engine/engine-tick-camera.cpp:628-636` obtains the current scene frame and sends it to `ReplayCameraController::update()`.
      5. `src/replay/replay-player.cpp:276-286` in recorded mode assigns `camera.pos = frame.camera.position` and rebuilds the camera vectors.
   5. Why the behavior is wrong
      1. If the exported clip contains default scene-frame camera data, the exporter faithfully reconstructs the default `(0,0,0)` camera; the exporter cannot recover the live player's camera after the snapshot has been made.
      2. The 2026-09-06 regression established the earlier cause: `beginPlayback()` made `replayPlaybackActive` block recording, leaving scene frames with tick 0, camera `(0,0,0)`, and no actors. That fix is present in the current source, so this report cannot yet prove whether the recurrence is a stale executable, a newly produced clip with empty/default camera fields, or another runtime state transition.
   6. Corrected code direction (not implemented in this investigation)
      1. Preserve the existing recording condition and subprocess `seekToTick(0)` fix unless runtime evidence disproves them.
      2. Add or use diagnostics that correlate: P press and clip path, first/last scene-frame camera position, `isRecording`, `replayPlaybackActive`, `isReplayExportActive`, subprocess pre-loop camera, and camera-controller output per export tick.
      3. Reject or clearly report a newly captured clip whose scene-frame camera data is default/invalid rather than exporting a misleading origin view.
   7. Evidence and status
      1. User runtime report confirms the visible failure.
      2. Source inspection confirms the intended data flow exists and confirms the prior root-cause fix remains in source.
      3. `mimita.exe` was last successfully built at 2026-09-07 12:31:59 local build time and is newer than the inspected source snapshot, but no new user reproduction log or exported clip JSON was available in this investigation.
      4. Status: UNRESOLVED. Do not claim the prior fix is effective for this current reproduction until a fresh clip's scene-frame JSON and export diagnostics are inspected.
   8. Related records
      1. Existing regression: `9 6 2026 1951 — Replay export camera stuck at (0,0,0) instead of following player POV`.
      2. Existing follow-up: `9 6 2026 2136 — Replay export camera fix confirmed working via diagnostic logging`.

## 2026-09-07T17:06:45Z — Replay export camera intermittent; projectile/effect replay duplicates; left-leg orientation wrong (UNRESOLVED)

1. Issue: the first replay export after opening MiMITA can still start at `(0,0,0)` with no usable player camera/pose, while a later export after approximately five minutes can have a working camera. The later export also has incorrect replay effects and left-leg orientation.
   1. Working-camera evidence: `C:\mimita-priv-v8\replays\exports\09-07-2026\13-02-22-clip-duel.mp4` was reported by the user as a working-camera export at `2026-09-07T17:06:45Z`.
   2. Bad effect behavior: the replay shows rocket/effect activity repeated too often; a rocket appears to be spawned or replayed from its original firing position repeatedly instead of one projectile continuing through its recorded path.
   3. Bad killfeed behavior: after the first rocket kills the player, the NPC kill is logged several times in replay chat instead of once for the one historical kill event.
   4. Bad body-pose behavior: the right leg is oriented vertically along world Z when standing, but the left leg lies flat in the X/Y plane and does not rotate into the expected upright orientation.
2. Specification disagreement
   1. `docs/specs/replays/replay-editor-and-export-v2.md` requires the export to reconstruct the local client's experience, including projectiles, effects, kill effects, player avatars, animations, and the local camera.
   2. The effects specification requires: `same event + same tick + same seed + same configuration = same visuals`, and says replay should record the event and replay it through the same normal implementation rather than creating repeated approximations.
   3. The hard replay correctness test requires correct bullets/projectiles, effects, kill effects, and replay presentation.
3. Exact current code path and evidence
   1. `src/replay/replay-player-interp.cpp:112-129` adds every scene frame's effects whose tick falls in the playback interval to `mTriggeredEffects`; this is event delivery, not continuous projectile-state simulation.
   2. `src/engine/engine-tick-camera.cpp:902-905` consumes those triggered effects during replay playback.
   3. `src/engine/engine-tick-camera.cpp:1032-1051` converts every `projectile_spawn` event into a new `EffectPart` at the recorded spawn position with the recorded velocity. If the same spawn event is delivered more than once, a new rocket is created more than once.
   4. `src/combat/weapon-rocket-launcher.cpp:227-239` records a `projectile_spawn` event when a rocket is fired. The event is supposed to represent one firing event, not a new rocket on every replay tick.
   5. The 2026-09-07 export log `logs\\09-07-2026\\ReplayExport_log_130223.txt` contains many `net_rocket_trail` entries at successive ticks and multiple `projectile_spawn`/explosion entries, confirming that the export is loading and processing a large effect stream; it does not by itself prove whether the duplication was recorded upstream or delivered twice during playback.
   6. `src/replay/replay-player-load.cpp:155-175` reconstructs body parts from JSON by iterating serialized body-part names and loading each quaternion. The current source does not yet prove that the serialized quaternion's local basis matches the renderer's expected leg bone basis, so the left-leg axis issue remains a pose-space/coordinate-space investigation.
4. Camera conclusion
   1. The successful later export means the freecam-gate change can allow the replay camera controller to run; it does not prove that every earlier clip had valid camera data.
   2. The earlier origin symptom remains consistent with a clip whose recorded scene-frame camera was default/empty, or with export beginning before the relevant replay state was initialized.
   3. Camera recording and camera playback must be checked separately for the first failed clip and the later working clip. The timing difference means this is not yet safe to call a permanently fixed camera issue.
5. Effect/killfeed conclusion
   1. A projectile spawn event should create one projectile with one historical start tick and then advance through the replayed state/path. The current playback branch explicitly creates a fresh `EffectPart` whenever it receives a `projectile_spawn` event.
   2. Trail events are expected to occur across ticks only if they represent the recorded trail presentation; they must not also cause the original projectile spawn to be recreated repeatedly.
   3. Repeated killfeed chat means either the same kill event is being triggered more than once, the playback event cursor is being reset/re-entered, or the event is duplicated in the saved clip. The current evidence does not distinguish these yet.
6. Left-leg conclusion
   1. The previously attempted quaternion-hemisphere correction addressed sign ambiguity (`q` versus `-q`), but the user's persistent flat-left-leg result indicates a different problem may remain: the left-leg source/renderer basis, part ordering, or local-versus-world rotation conversion.
   2. The fact that the right leg is correct while the left leg is consistently flat argues against a general Z-up convention failure and points toward a left-leg-specific transform or rest-axis mismatch.
7. Required next proof
   1. Compare the first failed clip and `13-02-22-replay.json`: scene-frame count, first/last camera positions, actor count, killfeed event count, `projectile_spawn` count, projectile trail count, and left/right leg quaternions.
   2. For one projectile, compare its single `projectile_spawn` event, every trail event, and every replay-side `spawn(projectile)` call by event identity and tick.
   3. For the repeated kill, compare saved killfeed events with `takeTriggeredKillfeedEvents()` delivery and chat append calls.
   4. For the legs, inspect the recorded left/right body-part quaternion, the JSON quaternion, `applyReplayPose()`, and the renderer's left/right bone/model basis at the same tick.
8. Status: UNRESOLVED. The camera is partially demonstrated by one working export, but camera initialization, projectile/effect duplication, repeated killfeed delivery, and left-leg orientation are not fixed or fully localized.

### Evidence update — 2026-09-07T17:06:45Z clip inspection

1. The source replay `replays\\09-07-2026\\13-02-22-replay.json` contains 900 scene frames and valid moving camera data: first camera position `(4.7011, -24.4159, 87.6256)` and last camera position `(6.4519, -26.9685, 91.7210)`. This proves the later working export did capture camera data; it does not explain why an earlier clip started at the origin.
2. The same JSON contains 8 distinct `projectile_spawn` events at ticks 47, 103, 598, 662, 712, 753, 806, and 856. The event data is not one projectile spawn per tick.
3. `logs\\09-07-2026\\ReplayExport_log_130223.txt` shows the tick-47 `projectile_spawn` being processed repeatedly by the exporter at multiple later log times, each time at the same original position `(-19.41, -35.57, 83.97)`. This confirms a replay-side duplicate delivery/reprocessing problem for at least that event.
4. The current playback code at `src\\engine\\engine-tick-camera.cpp:1032-1051` creates a new `EffectPart` whenever it receives `projectile_spawn`. Therefore, repeated delivery creates repeated rockets from the same historical origin instead of advancing one historical projectile.
5. The JSON contains three killfeed events at ticks 75, 821, and 822, all with `killerId=unknown`, `victimId=admin`, and `weaponName=unknown`. The export log shows repeated chat lines for these events. This proves the saved killfeed data is already semantically wrong or duplicated around the death; it is not yet proven that one JSON event alone is appended five times.
6. The effect specification requires event/tick identity and shared replay presentation. The current event data lacks a stable event identity visible in this path, making it difficult to distinguish a legitimate second event from the same event being delivered again.
7. The camera issue is now narrowed: the later clip's camera is correctly present in JSON and reaches the export subprocess (`pre-loop camera` matches the JSON first camera), so the freecam-gate change is effective for that clip. The intermittent first-export origin issue remains a capture/initialization difference between clips, not a universal inability of the exporter to apply cameras.
8. The left-leg issue remains separate from projectile duplication. The JSON loader does deserialize body-part quaternions, but no inspected evidence yet proves whether the wrong axis is recorded, serialized, converted by `applyReplayPose()`, or interpreted by the left-leg mesh/bone basis.

## 2026-09-07T17:18:13Z — VIP checkout HTTP 500 because VPS Stripe account did not own configured Prices (RESOLVED)

1. Bad behavior
   1. Clicking the prepaid slider's 1-month purchase button from the SSH-tunnel development site sent `POST /api/vip/payment/checkout` and returned HTTP 500.
   2. Monthly and lifetime buttons failed in the same way.
   3. Authentication and `/api/vip/config` succeeded, so the failure happened after the request reached the checkout server.
2. Exact evidence
   1. VPS PM2 logs recorded `StripeInvalidRequestError` with `No such price: 'price_1UCp8YGvytRPXxx5PrzYedv9'`.
   2. Direct Stripe checks using the VPS secret identified account `acct_1U05yIGgyshRntvw`; all six configured Price IDs returned `resource_missing`.
   3. Direct Stripe checks using the local test `.env` identified account `acct_1U05r0GvytRPXxx5`; all six Prices were found with amounts 333, 11111, 888, 22222, 1777, and 33333 cents.
3. Cause
   1. `website/npm-run-dev-ssh-v2.bat` forwards local port 3002 to the VPS, so the browser's localhost checkout executes the VPS API.
   2. The VPS had Price IDs from the local Stripe test account but a secret key from a different Stripe test account. Presence-only configuration reporting incorrectly said checkout was configured.
4. Fix
   1. Backed up the VPS environment as `/root/mimita-site/website/.env.backup-20260907_172107`.
   2. Replaced only the VPS test `STRIPE_SECRET_KEY` with the matching local test-account key; no key value is stored in this regression.
   3. Restarted `mimita-api` with the updated environment.
5. Resolution evidence
   1. All six Stripe Price IDs are now retrievable from the VPS using the active test key.
   2. Startup reports `[VIP CONFIG] mode=test configured=true missing=none`.
   3. `/api/vip/config` reports every prepaid, monthly, and lifetime option configured for all three tiers.
4. Status: RESOLVED for configuration. Human test-mode checkout, webhook, email, entitlement, and refund acceptance remain required.

## 2026-09-07T17:30:00Z — VIP slider displayed the base-month discount instead of total savings (RESOLVED)

1. Bad behavior
   1. For VIP at 12 months, the server/Stripe amount was `$19.98`, but the browser displayed `$38.29`.
   2. The browser displayed savings of `$1.67` instead of the correct `$19.98`.
   3. The browser and Stripe therefore appeared to disagree even though Stripe had the correct server-created amount.
2. Exact wrong code
   1. `website/src/pages/Vip.jsx` calculated `prepaidCents` by subtracting the discount from `prepaid.amount_cents` instead of from the full `prepaid.amount_cents * selectedMonths` total.
   2. At 12 months this computed `3996 - 167 = 3829` cents rather than `3996 - 1998 = 1998` cents.
3. Fix
   1. `website/server/vip-config.js` now returns server-calculated `amounts_cents` and `savings_cents` arrays for every tier and every integer month from 1 through 12.
   2. `website/src/pages/Vip.jsx` reads those arrays and no longer recomputes the amount in the browser.
   3. The display text now uses the selected month count and discount percentage instead of hardcoded “buy 12 months for the price of 6” text.
4. Resolution evidence
   1. VPS `/api/vip/config` now returns VIP prepaid amounts `[333,636,909,1151,1363,1544,1696,1817,1908,1968,1998,1998]` cents.
   2. The 12-month amount and savings are both `1998` cents (`$19.98`).
   3. Focused VIP tests pass 25/25; local and VPS website builds pass.
4. Status: RESOLVED for browser/server quote disagreement. Stripe test checkout and webhook acceptance remain human tests.

## 2026-09-07T17:55:09Z — VIP Stripe test-mode end-to-end display and Checkout amounts agree (TEST-MODE RESOLVED)

1. Human acceptance evidence
   1. VIP prepaid values matched between the website and Stripe Checkout: VIP 1 month `$3.33`, 7 months `$16.96`, and 9 months `$19.08`.
   2. VIP monthly subscription matched at `$3.33` on the website and Stripe.
   3. VIP lifetime matched at `$111.11` on the website and Stripe.
   4. Ultra VIP Checkout entered Stripe's loading/checkout flow successfully for the admin test account.
   5. The success page was visible and reassuring after Checkout, reducing buyer uncertainty.
2. Infrastructure lesson
   1. Local `.env` values alone were insufficient because the SSH-tunnel development page uses the VPS API.
   2. Putting the matching Stripe test secret, monthly/lifetime Price IDs, and configuration metadata on the VPS made the tunneled local test flow work.
   3. The VPS must use Price IDs belonging to the same Stripe account and mode as its `STRIPE_SECRET_KEY`; presence-only key checks are not enough.
3. Status
   1. Test-mode website display, server quote, and Stripe Checkout amount are accepted for the observed cases.
   2. Live-mode payments, live webhook delivery, live receipts/emails, refunds, subscription cancellation, and production monitoring are not yet tested.
## 2026-09-07T17:43:06Z — Replay export camera/effect fidelity follow-up (CAMERA INITIALIZATION PATCHED; EFFECTS UNRESOLVED)

1. Human replay evidence remains: the first two or three exports after starting `mimita.exe` may open with the camera at `(0,0,0)`, while a later export can contain a valid moving camera. The observed later clip `replays\\exports\\09-07-2026\\13-02-22-clip-duel.mp4` was working for camera placement, but still showed projectile, damage-number, dynamic-light, tracer, hit-sphere, and left-leg problems.
2. The source replay contains valid camera frames and eight distinct `projectile_spawn` events, so the saved camera/event data is not inherently empty and the rocket is not legitimately spawned once per tick.
3. The export subprocess previously called `REPLAY_PLAYER.seekToTick(0)` but relied on the first engine camera pass to copy the replay camera into the live camera. The new guarded handoff in `src\\replay\\replay-export-subprocess.cpp` seeds `gpCamera` immediately from the first scene frame and logs the source tick/position/yaw/pitch before capture. This removes the startup-order dependency for the initial export frame.
4. Replay effect reconstruction is still not spec-compliant. `src\\engine\\engine-tick-camera.cpp` creates a replay-only `EffectPart` for `projectile_spawn`, and separately reconstructs muzzle flashes, tracers, damage numbers, hit bursts, and damage spheres. This is not the same shared live projectile/effect path required by `docs\\specs\\effects\\effects.md`; it explains why historical projectiles do not collide like live projectiles and why lifetime/decay behavior can diverge. The current event-ID delivery guard reduces repeated delivery but does not make one historical projectile into one advancing collision-aware projectile.
5. Current status: `PASS_WITH_HUMAN_REVIEW` for deterministic initial camera seeding after a successful build; `UNRESOLVED` for projectile duplication/collision, damage-number and dynamic-light decay, tracer flicker, damage-sphere fade, and left-leg orientation. A live export must still verify the first-export case and inspect the new `[EXPORT-SUBPROCESS] camera seeded` log.

## 2026-09-07T18:20:00Z — Replay export redraw re-delivered historical events (FIXED IN PLAYER CURSOR; LIVE EFFECT PARITY UNRESOLVED)

1. Exact failure: the export capture loop seeks to each requested historical tick and then calls `ReplayPlayer::update(0.0f)`. `ReplayPlayer::seekToTick()` previously cleared `mDeliveredEventIds` and reset event counters on every export redraw. Re-rendering the same historical tick therefore delivered its rocket, damage, hit, and killfeed events again.
2. Fix: `ReplayPlayer::seekToTick(uint32_t tick, bool resetEvents)` now preserves the event cursor when export calls it with `resetEvents=false`. Normal explicit/editor seeks retain the default reset behavior. Camera recording now stores `camera.pitch`, `camera.roll`, and `camera.yaw` instead of replacing roll with zero and using `player.yaw` for camera yaw.
3. Validation: canonical build passed with `Status: SUCCESS`; existing replay export self-check passed `26/26`; its synthetic camera assertions confirmed camera position advances with replay ticks and MP4 output remains valid.
4. Remaining status: live first/second/third export acceptance is still required. Replay-only projectile/effect reconstruction in `src\\engine\\engine-tick-camera.cpp` still does not use the shared collision-aware projectile/gameplay event path, so projectile collision, effect lifetime, dynamic-light repetition, tracer behavior, hit-sphere decay, and left-leg orientation remain unresolved.

## 2026-09-07T18:45:00Z — Replay export restarted the clip and re-presented effects (EXPORT LOOP FIXED; SHARED EFFECT PARITY REMAINS)

1. Evidence: `logs\\09-07-2026\\ReplayExport_log_142914.txt` contained approximately 4,967 replay-side `projectile_spawn` dispatches, 4,064 `damage_number` dispatches, 18,541 `footstep` dispatches, and 63,994 `net_rocket_trail` dispatches for a 900-tick clip. The source replay itself contained 19 projectile-spawn events, so the export was replaying the same history rather than showing a normal event count.
2. Cause: `src\\engine\\engine-tick-combat.cpp` looped `FinalKillReplay` back to tick 0 whenever playback reached the end. The export subprocess shares this engine path, so the loop repeatedly reset playback and event delivery.
3. Fix: the loop now requires `!isReplayExportActive()`. Export reaches the end once; normal in-game final-kill replay looping is unchanged.
4. Validation: canonical build passed with `Status: SUCCESS`; `mimita.exe --replay-export-selftest --timeout 60 --no-coordinator` passed `26/26`.
5. Remaining: replay effects are still reconstructed in `src\\engine\\engine-tick-camera.cpp` rather than submitted through the complete shared live projectile/hit-effect path. A fresh live export is required to confirm the screenshot-level effect reduction and to continue the projectile collision, lifetime, tracer, lighting, and left-leg work.

## 2026-09-07T19:15:00Z — Replay consumed event batches were requeued by swap buffers (FIXED; LIVE EFFECT PATH REMAINS)

1. Exact failure: replay dispatch used `takeTriggeredEffects()`, `takeTriggeredSounds()`, and `takeTriggeredKillfeedEvents()`, which transfer vectors with `swap()`. The static reusable caller vectors were not cleared after processing. On the next frame, the old batch was swapped back into `ReplayPlayer` and presented again, so effects disappeared by lifetime and then respawned indefinitely.
2. Fix: `src\\engine\\engine-tick-camera.cpp` now clears the consumed effect and sound batches after their loops. `src\\engine\\engine-tick-ui-hud.cpp` clears the consumed killfeed batch after updating `KillfeedManager`.
3. This fixes the exact “effect expires, then returns” delivery loop. It does not yet convert replay rockets/hit effects to the shared gameplay collision/effect path; that remains a separate spec-compliance item.
4. Camera recurrence remains confirmed: the first export after starting the executable can still have no movable camera and `(0,0,0)`, while the second/third may work. The source-camera readiness/recording path still needs live evidence and repair.
5. Validation: canonical build passed `Status: SUCCESS`; existing replay export self-check passed `26/26`. Fresh live export acceptance is still required.

## 2026-09-07T19:30:00Z — Replay export effect spam stopped by clearing consumed batches (INCREMENTAL FIX CONFIRMED; WEAPON EFFECTS UNTESTED)

1. Human acceptance update: after the consumed-batch fixes, the third export no longer showed the previous effect spam. The first export still had no movable camera and a camera position of `(0,0,0)`. The second export allowed looking around but movement was not tested. The third export camera worked. This confirms progress on effect re-presentation, not camera startup correctness.
2. Exact old effect code in `src\\engine\\engine-tick-camera.cpp`: `takeTriggeredEffects(effects);` processed the static reusable vector, but there was no `effects.clear()` after the loop. The same omission existed for `takeTriggeredSounds(sounds)`.
3. Exact new effect code:

   ```cpp
   gReplayPlayer.takeTriggeredEffects(effects);
   for (const ReplayEffectEvent& effect : effects) {
       // existing effect dispatch
   }
   effects.clear();

   gReplayPlayer.takeTriggeredSounds(sounds);
   for (const ReplaySoundEvent& sound : sounds) {
       // existing sound dispatch
   }
   sounds.clear();
   ```

4. Exact old killfeed behavior in `src\\engine\\engine-tick-ui-hud.cpp`: `takeTriggeredKillfeedEvents(killEvents);` processed the static reusable vector without clearing it afterward.
5. Exact new killfeed code:

   ```cpp
   gpReplayPlayer->takeTriggeredKillfeedEvents(killEvents);
   for (const ReplayKillfeedEvent& ev : killEvents) {
       // existing KillfeedManager dispatch
   }
   killEvents.clear();
   ```

6. Cause: the take functions use `swap()`. Without clearing the caller-owned reusable vector, already-consumed events were swapped back into `ReplayPlayer` on the next frame and spawned again after their normal lifetime ended.
7. Untested: revolver muzzle flash, one-tick white muzzle sphere, revolver tracer, rocket-launcher projectile, rocket smoke, rocket explosion, dynamic lighting, and other weapon effects still require live export acceptance.
8. Status: effect re-presentation fix is confirmed incrementally by human observation; camera startup remains unresolved; full replay/live weapon-effect parity remains unresolved.

## 2026-09-07T19:00:00Z — Active prepaid/lifetime VIP had no management or refund entry point (RESOLVED)

1. Bad behavior
   1. `/vip` showed a management button only when an active recurring subscription existed.
   2. Users with active prepaid or lifetime VIP saw status text, but had to navigate away and find `/vip/success` themselves before they could reach purchase details or the refund request link.
2. Cause
   1. `website/src/pages/Vip.jsx` rendered the existing Stripe Billing Portal action only for active subscription statuses.
   2. The page did not load the user's existing VIP orders, even though `/api/vip/orders` and `/vip/success?order_id=...` already exposed the account email, receipt status, purchase details, and one-time refund route.
3. Fix
   1. `/vip` now loads the authenticated user's orders and selects the paid one-time order matching the active tier.
   2. Active recurring subscriptions retain `manage subscription`, which opens Stripe Billing Portal for cancellation and billing management.
   3. Active prepaid/lifetime entitlements now show `manage VIP purchase / refund`, which opens the matching success page and its refund action.
4. Status: RESOLVED in source and deployed after local validation. Human acceptance remains required for one recurring cancellation and one refundable prepaid/lifetime purchase.

## 2026-09-07T19:30:00Z — One-time VIP refunds required manual support review (RESOLVED)

1. Bad behavior
   1. Prepaid and lifetime VIP users could only open a support refund request after purchase.
   2. A refund required manual handling instead of being sent directly to Stripe after the user confirmed the exact amount.
2. Fix
   1. Added an authenticated, ownership-scoped refund endpoint for paid prepaid/lifetime orders inside the existing 30-day window.
   2. The server sends Stripe the stored Payment Intent and stored order amount; browser-supplied payment data is ignored.
   3. Stripe webhook confirmation remains authoritative: the order and entitlement are marked refunded only after Stripe reports the refund.
   4. Added refund status, Stripe refund ID, error, timestamp, and refund-email tracking fields.
   5. The success page now shows a final confirmation step and a completed-refund state.
3. Safety behavior
   1. Monthly subscriptions are excluded and remain managed through Stripe Billing Portal.
   2. Duplicate clicks and already-refunded orders are rejected.
   3. Email bookkeeping failures cannot turn a completed Stripe refund into a failed webhook.
4. Status: RESOLVED in source. Test-mode Stripe refund and live production refund acceptance remain required.

## 2026-09-07T19:38:47Z — Replay rocket gunshot incorrectly rendered as a tracer (PARTIALLY FIXED)

1. Observed bad behavior: an exported replay of an NPC rocket-launcher shot played the rocket-launcher sound, then showed a tracer; the visible projectile was delayed, slow, non-colliding, and did not reproduce the live rocket explosion path.
2. Expected behavior: the replay must preserve the recorded weapon identity and present a rocket-launcher projectile, not a hitscan tracer. The replay specification requires the recorded player's experience tick by tick, including rocket projectiles, smoke, and explosions through shared effect behavior.
3. Exact old code: `src\\engine\\engine-tick-camera.cpp` handled every `gunshot` by calling `spawnMuzzleFlash(...)` and then unconditionally calling `EffectPartSystem::instance().spawnTracer(...)`. `src\\combat\\weapon-rocket-launcher.cpp` recorded both rocket events without setting `ReplayEffectEvent::assetId`, so replay could not reliably identify the weapon.
4. Exact new code: rocket fire now sets `projEvent.assetId = def.id` and `gunshotEvent.assetId = def.id`. Replay resolves the weapon through `WeaponRegistry`; it spawns a tracer only when the weapon definition is hitscan and logs `[REPLAY EFFECT] gunshot has projectile behavior; tracer suppressed` for projectile weapons.
5. Cause: the replay gunshot dispatch treated the generic event type as hitscan regardless of the weapon, while the source event omitted the weapon contract. The replay `projectile_spawn` branch remains a replay-only `EffectPart` approximation and is not yet the full `WeaponRocketLauncher::update` collision/explosion path.
6. Validation: canonical `mimita.exe` build passed with `Status: SUCCESS`; `mimita.exe --replay-export-selftest --timeout 60 --no-coordinator` passed 26/26. Human MP4 verification of rocket travel, world collision, explosion timing, smoke, and muzzle effects remains required.
7. Status: PARTIALLY FIXED. Tracer suppression and weapon identity are implemented. Full shared gameplay projectile replay and the left-leg rotation issue remain open.

## 2026-09-07T20:13:40Z — Replay rockets used a non-colliding visual approximation and legs lost skeleton parent space (IMPLEMENTED; LIVE REVIEW REQUIRED)

1. Observed bad behavior: replay rocket launchers showed delayed/slow projectiles, tracer-like presentation, no reliable world collision or explosion timing, and the left leg used the wrong world-axis orientation.
2. Exact old replay rocket path: `src\\engine\\engine-tick-camera.cpp` constructed an `EffectPart`, assigned `replayType = "replay_rocket"`, copied the recorded position/velocity/lifetime, and spawned it. This made every delivered event a visual object rather than a `RocketLauncherState::Rocket` advanced by the live weapon update.
3. Exact new rocket path: replay creates one `RocketLauncherState::Rocket` per event ID, advances `WeaponRocketLauncher::update(..., presentationOnly=true)` at `1.0f / 60.0f`, renders the shared rocket state through `WeaponRocketLauncher::render`, and reuses `spawnExplosionFx` plus the live smoke/collision/orientation code. Presentation-only mode suppresses NPC/player damage, knockback, health, kill, and replay-authority changes; it also suppresses duplicate in-air and explosion sounds because recorded replay audio is authoritative.
4. Exact old leg path: `captureReplayBodyParts()` removed only the player root yaw and stored each world transform as root-local. `Player::applyReplayPose()` then applied every part as `root * translate(position) * rotate(rotation)`, bypassing the skeleton parent chain.
5. Exact new leg path: body-part records carry `parentPartId`; capture computes the nearest recorded body-part ancestor and decomposes `inverse(parentWorld) * partWorld`; replay reconstructs `parentWorld * localTransform` in two passes. Files include `src\\replay\\replay-scene.h`, `src\\replay\\replay-recorder.cpp`, `src\\replay\\replay-io.cpp`, `src\\replay\\replay-player-load.cpp`, `src\\replay\\replay-player-interp.cpp`, and `src\\entities\\player-render.cpp`. Old replay files remain root-local through the `0xFF` fallback.
6. Automated proof: canonical build succeeded with `Status: SUCCESS`; replay export self-test passed `28/28`, including rocket event identity and body-parent serialization checks. This proves source/build contracts, not final MP4 appearance.
7. Status: IMPLEMENTED; LIVE REVIEW REQUIRED. Human export testing must verify rocket travel, wall collision, one explosion, smoke/lifetime decay, no duplicate audio/effects, and the left/right leg world-axis result. The automated test does not yet instantiate a real wall-collision rocket or compare rendered leg axes.

## 2026-09-07T19:45:00Z — VIP order management returned 500 because migration 008 was not registered (RESOLVED)

1. Bad behavior
   1. `/api/vip/orders` returned HTTP 500 on the live site.
   2. The `/vip` page therefore displayed `Purchase details are still loading` forever, even after refresh.
2. Exact evidence
   1. VPS PM2 logs reported `DatabaseError: column "refund_status" does not exist` for `GET /api/vip/orders`.
   2. `/api/auth/me`, `/api/vip/config`, and `/api/vip/me` returned successfully, isolating the failure to the order query.
3. Cause
   1. `website/server/migrations/008_vip_refunds.sql` existed and added the queried columns.
   2. `website/server/db.js` only iterated migrations `[1, 5, 6, 7]`, so `npm run migrate` reported success while never applying migration 008.
4. Fix
   1. Registered migration version 8 in the migration list.
   2. Added the version-8 filename mapping to `008_vip_refunds.sql`.
   3. Deployment reran the migration and verified the order endpoint after restart.
4. Status: RESOLVED in source and deployment. Cloudflare/Metricool/font warnings are unrelated analytics/browser warnings.

## 2026-09-07T20:29:18Z — NPC replay rockets and left-leg rotation confirmed working; player rockets were removed at launch (PLAYER ROCKET FIXED; LIVE MP4 REVIEW REQUIRED)

1. Human acceptance update: NPC rockets now appear in exported MP4s with visible smoke, and the left-leg rotation issue is no longer observed. These are confirmed working changes from the shared replay rocket and parent-relative transform implementation.
2. Exact wrong left-leg code: replay capture stored each part after only removing the player root yaw, and replay applied it as `root * translate(found->position) * glm::mat4_cast(found->rotation)`. This flattened the skeleton and bypassed the original parent chain, so the left leg could be interpreted in the wrong axis space.
3. Exact corrected left-leg code: capture now finds the nearest recorded body-part ancestor, computes `localTransform = inverse(parentWorld) * wt`, stores `parentPartId`, and replay applies `parentWorld * localTransform` in two passes. Older root-local replay files use the `parentPartId == 0xFF` fallback.
4. Exact wrong player-rocket code: presentation-only replay reused the live owner collision check with the current local player as a temporary owner. A player-fired replay rocket starts inside/near that player capsule; after the arming distance it could be treated as an owner hit and erased immediately, before it became visible in the MP4.
5. Exact corrected player-rocket code in `src\\combat\\weapon-rocket-launcher.cpp`: `if (!presentationOnly && dist < 0.5f && rocket.distanceTraveled >= IGNORE_OWNER_DIST) { hitOwner = true; }`. Replay presentation skips only temporary-owner self-collision; world collision, shared movement, smoke, orientation, lifetime, and explosion presentation remain active. Replay damage/authority remains suppressed.
6. Automated proof: canonical build passed with `Status: SUCCESS`; `mimita.exe --replay-export-selftest --timeout 60 --no-coordinator` passed `28/28`. The test covers replay event identity and parent metadata, but not a rendered player-fired rocket in a live MP4.
7. Status: NPC rocket smoke/export and left-leg rotation: HUMAN CONFIRMED WORKING. Player-owned rocket visibility: SOURCE FIXED; fresh live MP4 confirmation still required. Remaining acceptance includes player rocket travel, wall collision, explosion, smoke decay, and no duplicate effects/audio.

## 2026-09-07T20:48:52Z — Local-player rocket replay event was missing in multiplayer prediction (IMPLEMENTED; LIVE MP4 REQUIRED)

1. Observed bad behavior: NPC rockets and smoke appeared in exported MP4s, but the local player's rocket and its smoke did not. Other local effects, including damage numbers, impact visuals, lighting, and explosions, continued to appear.
2. Exact wrong code: `src\\combat\\weapon-system.cpp` sent multiplayer projectile shots through the `remotePlayers` branch, which only consumed ammo, set cooldown, and played the sound. It did not call `WeaponRocketLauncher::fire()`. Then `src\\network\\multiplayer-projectiles.cpp` recorded replay projectile events only inside `if (!localOwner)`, assuming local prediction had already recorded one.
3. Exact new code: `src\\network\\multiplayer-projectiles.cpp::mpPredictProjectileAttack()` now records one `projectile_spawn` event for a locally predicted rocket using the predicted position, velocity, lifetime, `rocket_launcher` asset ID, and local owner ID. The authoritative spawn path records a local-owner fallback only when prediction/adoption was absent; predicted plus authoritative delivery therefore cannot create two replay events.
4. Replay behavior: the recorded event continues through the existing shared `RocketLauncherState::Rocket`, `WeaponRocketLauncher::update(..., presentationOnly=true)`, shared renderer, collision, smoke, lifetime, and explosion path. No replay-only projectile approximation was added.
5. Diagnostics: `[REPLAY ROCKET RECORD]` logs whether the event came from local prediction, authoritative fallback, or remote authority, including fire serial, owner ID, and spawn data. The new self-test assertion preserves the source actor identity in the replay event.
6. Validation: the shared build process was already running under the repository build lock and had not produced a new result at completion; the previous executable result remains `Status: SUCCESS` with `28/28 passed`. This change therefore still requires a fresh build result and live multiplayer MP4 acceptance.
7. Status: source fix implemented; automated build/self-test and human acceptance pending. Acceptance must verify local-player rocket visibility, smoke, live-speed travel, wall collision, one explosion, and no duplicate projectile/effect output.

## 2026-09-07T21:00:00Z — Spy Knife authoritative contacts are accepted intermittently because client ticks lead server history and distance rejection aborts batches (CONFIRMED)

1. Observed bad behavior: the client shows many predicted Spy Knife hit effects, but the NPC receives substantially fewer authoritative damage applications and eventually appears to stop taking damage.
2. Runtime evidence from `logs/09-07-2026/Server_log_191651.txt`: the server recorded 21 `NPC_APPLIED` contacts, 71 `CONTACT_REJECT reason=invalid_contact`, 31 `CONTACT_REJECT reason=distance`, 4 attacker-inactive batch rejections, and 3 already-dead-target rejections.
3. Exact primary rejection: `src/network/server-packet-handlers.cpp` rejects any contact when `pkt.contactTick > tick`, when it is older than 600 ticks, or when required values are non-finite. The supplied log shows claims such as `contactTick=30850` through `30854` arriving at `serverTick=30794`, so those contacts are rejected before NPC history lookup or damage application.
4. Exact secondary rejection: the same handler compares the attacker to the historical NPC position and rejects when `dist > 3.0f`. The supplied log shows valid-looking contacts rejected at distances such as 3.13, 4.16, 6.60, and 146.08 units.
5. Batch-loss behavior: the NPC distance branch currently executes `return` rather than `continue`, so one out-of-range contact ends processing for the entire remaining batch. The dead-NPC and missing-player branches have similar whole-handler early exits. This amplifies the difference between predicted contacts and authoritative damage.
6. What is already fixed: the old slot mismatch is no longer the active cause in this run. The server logs `EQUIP_RESOLVED ... equippedSlot=12 logicalSlot=4 nativeSlot=12`, and packets reach the Spy Knife handler as `bytes=276`. The historical-tick change also fixed the earlier extreme mismatch caused by using the per-network-update `ctx.tick`; the remaining mapping still produces future claims in this runtime.
7. Status: regression confirmed. Do not simply increase damage or trust client health. The next fix must align or safely clamp the contact tick to the server history domain, make per-contact validation skip only the bad contact, and separately decide the shared physical-contact tolerance from the melee specification. Preserve the immediate client prediction while treating only accepted server contacts as authoritative.

## 2026-09-07T21:15:00Z — Spy Knife future-tick and batch-abort safeguards implemented

1. `src/combat/weapon-spyknife.cpp` now clamps the rendered contact tick to the newest server snapshot tick before batching. Client prediction remains immediate; only the historical lookup tick is bounded so the server never receives a future pose claim.
2. `src/network/server-packet-handlers.cpp` now continues to the next contact when an NPC is already dead, when an NPC contact is out of range, when a target player is missing/inactive, or when a player contact is out of range. One bad contact no longer discards the remainder of a batch.
3. The player distance diagnostic now uses centralized `Debug::warn` logging instead of `printf`, and includes the contact/server ticks and contact ID.
4. Status: source fix implemented. The shared 3.0-unit physical-contact tolerance remains unchanged pending a focused runtime measurement against the melee collision geometry; widening it is intentionally separate from the safe tick/batch fix.

## 2026-09-07T21:25:00Z — Spy Knife client OBB and server root-distance validation diverged (FIXED CONFIGURATION ALIGNMENT)

1. Exact mismatch: the client uses `hitboxHalfX/Y/Z`, `hitboxOffsetX/Y/Z`, and hitbox rotation to test the configured blade OBB, while `src/network/server-packet-handlers.cpp` only compared attacker root position to historical target position using a hard-coded `3.0f` distance.
2. This caused legitimate client OBB contacts to be rejected even when the visible/configured hitbox overlapped the target. It also made the server tolerance invisible to weapon balance tuning.
3. `config/weapons.json` now exposes hot-reloadable `serverContactRadius: 4.0` for Spy Knife. The server reads it from the authoritative weapon definition for both NPC and player validation and logs the active radius and configured hitbox dimensions for every batch.
4. Status: source fix implemented; this intentionally increases the acceptance envelope but still does not trust client damage or health. Runtime testing must verify that the configured radius is generous enough for the visual OBB without accepting obviously separated targets.

## 2026-09-07T21:35:00Z — Spy Knife configured hitbox was invisible because it used the wrong debug render queue (FIXED)

1. Exact wrong code: `src/combat/weapon-spyknife.cpp` drew both the OBB edges and the supposed filled faces with `DebugVis::drawLine`.
2. The render loop flushes weapon collision visuals through `DebugVis::flushWeaponLines()` after the normal debug-line stage. The knife OBB was therefore queued in the wrong buffer and could be absent from the frame even when `hitboxVisible` was enabled.
3. The configured alpha was also reduced by `alpha * 0.3`, so `hitboxAlpha: 1.0` could never produce full configured opacity.
4. Fix: route the OBB diagnostics through `DebugVis::drawWeaponLine` and clamp/use the configured alpha directly. The current implementation still draws wire geometry; true filled OBB faces require a triangle primitive/API and are separate from this visibility fix.
5. Status: source fix implemented; runtime visual confirmation remains required.

## 2026-09-08T16:10:00Z — General gamemode lifecycle was split across duel and map-only server paths (UNRESOLVED)

1. Expected behavior:
   1. Gamemode behavior is selected by the gamemode JSON ID and implemented by reusable match functions.
   2. Players and NPCs are equivalent match actors for participation, teams, kills, deaths, score, spawning, and respawning.
   3. A server mode selected in the GUI starts the same lifecycle as `modestart` when a player joins and continues indefinitely through rounds.
   4. Team membership remains stable unless an authoritative team-change request is accepted.
   5. Map changes and `respawn_all` reset every player and NPC using the new map's spawn points.
2. Actual behavior before this session:
   1. General match behavior was owned by `server-duel` and split from a separate map-only community runtime.
   2. Exact community mode strings controlled scoring and HUD routing instead of the gamemode JSON ID.
   3. NPCs were omitted from the participant, team, and player-death score flow.
   4. Weapon-set selection was tied to the legacy `mapOnly` state instead of the gamemode JSON.
   5. Map changes teleported duel-specific players and destroyed NPCs without one shared actor reset contract.
3. Current correction:
   1. Gamemode JSON files now provide `weapon_set_id` and server startup/modestart apply it through the active gamemode.
   2. The unified FFA/TDM participant path admits one active player plus NPCs, assigns stable match teams, and routes NPC-caused player deaths into the pending score event.
   3. `teamlist`, `teampick`, and host-only `respawn_all` now use server-authoritative command handling.
   4. Map reloads reset active players and re-seed NPC state from the new map's spawn points.
   5. The match packet/client HUD path now uses the gamemode IDs `ffa` and `tdm`.
4. Remaining unresolved acceptance:
   1. A real good-connection run must prove human-vs-NPC scoring, NPC scoreboard visibility, stable teams, team switching, automatic GUI-selected startup, repeated rounds, map-change visual reset, and weapon-set inventory.
   2. The old map-only runtime remains present as deprecated compatibility code and must be removed after the unified path is accepted.
   3. Bad-connection behavior remains intentionally untested until good-connection behavior passes.
5. Related source evidence: `src/network/server-duel.cpp`, `src/network/server-npcs.cpp`, `src/network/server-damage.cpp`, `src/network/server-packet-chat.cpp`, `src/gamemode/gamemode.cpp`, and `config/gamemodes/*.json`.

## 2026-09-08T14:12:27Z — Gamemode GUI presentation is not uniformly JSON hot-reloadable (UNRESOLVED)

1. Expected behavior:
   1. The GUI specification states: "IF IT IS A GUI ELEMENT, ITS PRESENTATION MUST BE JSON-DEFINED AND JSON HOT RELOADABLE."
   2. Gamemode HUD presentation must be editable while the game is running, without rebuilding or restarting.
   3. The active gamemode ID from `config/gamemodes/*.json` must select the presentation definition.
2. Actual behavior before this migration:
   1. `src/engine/engine-tick-ui-overlays.cpp:541-547` drew room status at hardcoded centered `y=18`, scale, and colors.
   2. `src/engine/engine-tick-ui-overlays.cpp:583-620` loaded some match text from `config/gui/match-hud.json`, but mode selection and several presentation fallbacks remained in C++.
   3. `src/gui/hud/match-leaderboard.cpp:139-178` hardcoded the `YOUR TEAM` text and its vertical offsets.
   4. `src/engine/engine-tick-ui-replay-hud.cpp:95-109` assembled reconnect status text in C++.
   5. `src/engine/engine-tick-ui-replay-hud.cpp:111-121` hardcoded replay recording indicator coordinates, text, scale, and colors.
   6. `src/game/duel.cpp:299-313` still contains hardcoded duel result and countdown presentation paths.
   7. `src/game/gamemode-manager.cpp:266-301` previously loaded `config/gui/bomb-tag-hud.json` separately from the requested gamemode metadata source.
3. Specification disagreement:
   1. `docs/specs/gui/guiv2.md` requires feature code to supply data while JSON supplies presentation.
   2. `docs/architecture/json-configuration/json-configuration.md` requires an obvious owner, validation, atomic last-valid replacement, reload signaling, and a documented application boundary.
   3. The hardcoded and split paths above made the desired live JSON editing incomplete and made it possible for the visible HUD to disagree with the edited JSON file.
4. Current correction:
   1. `config/gui/gamemode-meta-gui.json` now defines sections for `tdm`, `ffa`, `bombtag`, `duel`, and `sandbox`.
   2. `src/gui/gui-layout.cpp` and `src/gui/gui-layout.h` reuse the existing parser and reload manager for selected gamemode sections.
   3. Network mode names are normalized to the current gamemode IDs: `team_deathmatch` to `tdm`, `free_for_all` to `ffa`, and `bomb_tag` to `bombtag`.
   4. TDM, FFA, Bomb Tag, local duel, and network duel HUD consumers now read from the gamemode metadata owner.
5. Status and proof boundary:
   1. UNRESOLVED until live runtime testing proves that editing each migrated gamemode HUD element changes the visible result without rebuild or restart.
   2. Room status, reconnect suffix text, replay recording presentation, duel result presentation, and other debug/overlay paths remain outside the migrated gamemode metadata owner.
    3. No overlap detection was added; it is intentionally out of scope for this regression fix.

2026-09-08T16:30:00Z — Spawn velocity feature compiles but never reaches the EXE due to pre-existing build failure — CONFIRMED NOT WORKING

1. Issue: Spawn velocity impulse on respawn does not apply at runtime.
   1. Expected behavior: when `config/spawnvelocity.json` has `enabled: true`, the player
      receives a velocity impulse in the configured direction on respawn.
   2. Actual behavior: player respawns with zero velocity, identical to pre-feature behavior.
   3. Root cause: the overall build fails before linking due to a pre-existing compile error
      in `src/engine/engine-tick-state.cpp:294` (`PasswordPopup` not declared). Because the
      build fails, `mimita.exe` is never relinked. The binary on disk is the old one without
      any spawn velocity changes.
   4. All spawn-velocity-related files compile cleanly: `spawn-velocity-config.cpp`,
      `death-system.cpp`, `server-players.cpp`, `main-systems.cpp`, `engine-tick-setup.cpp`.
      The failure is in an unrelated file with pre-existing uncommitted changes.
    5. Fix required: resolve the `PasswordPopup` compile error in `engine-tick-state.cpp`,
       then rebuild with `python build_agent.py`.

2026-09-09T15:30:00Z — Spawn velocity impulse on respawn: network linkage incomplete, now FIXED — RESOLVED

1. Issue: Spawn velocity impulse did not apply on respawn despite config being enabled and server computing the correct velocity.
   1. Expected behavior: when `config/spawnvelocity.json` has `enabled: true`, the player
      receives a horizontal velocity impulse on every respawn.
   2. Actual behavior: player respawned with zero velocity every time. The server computed
      the correct spawn velocity and passed it to `beginAuthoritativeTransform`, but
      `PlayerRespawnedPacket` did not carry velocity fields. The client received the
      respawn packet and zeroed velocity in `multiplayer-tick.cpp:629` and
      `engine-tick-net.cpp:366`.
   3. Root cause: incomplete end-to-end network linkage. The server-side spawn velocity
      was correct but the packet schema and client consumers did not carry or read it.
   4. Fix applied (2026-09-09T11:27:05Z):
      1. Added `float velX`, `velY`, `velZ` to `PlayerRespawnedPacket` in `src/network/packets.h`.
      2. `src/network/server-players.cpp`: `completeAuthoritativeSpawn()` populates `spawnSync.velX/Y/Z`.
      3. `src/network/multiplayer-tick.cpp`: `applyAuthoritativeSpawn()` reads `{spawn->velX, velY, velZ}`.
      4. `src/engine/engine-tick-net.cpp`: pending spawn application reads `{spawn.velX, velY, velZ}`.
   5. Human verification: player exploded themselves repeatedly and received the spawn
      velocity impulse on every respawn. Considered resolved as of this timestamp.
   6. Lessons learned:
      1. Stale EXE is a hypothesis, not the default diagnosis. The more common failure
         is incomplete end-to-end linkage across packet schema, server sender, and
         client consumer.
      2. Always trace the full data path (config → server → packet → client → movement)
         before assuming a build or binary issue.
