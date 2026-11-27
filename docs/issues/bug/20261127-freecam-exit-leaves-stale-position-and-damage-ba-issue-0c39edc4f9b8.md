# issue-0c39edc4f9b8: Freecam exit leaves stale position and damage backlog; server still resolves hits at the entry position

## Human report (verbatim, immutable)

> another bug relating to freecam, going into freecam then leaving it after gteting hit a lot = position of me doesnt update, server thinks im still atthe positioniw as at wehn i started freecam , but the body i had was getting hit a lot lot lot in freecma so it doesnt think the new acutal pos of me is the actual pos


## Agent suggestion

### 2026-11-27T17:41:33.111Z — rocket-league-settle

Root cause confirmed read-only in source. engine-tick-replay.cpp:373 gates the entire fixed-tick simulation on '(!freecamEnabled || editorUnfrozen) && !replayPlaybackActive', so while the gameplay 'freecam' command has set FREECAM_ENABLED the local player's simulateTick/movement is skipped. The toggle in camera-commands.cpp:45-56 only flips the flag; it performs no transform reconcile and no pending-damage drain on exit. The server keeps simulating the body at the pre-freecam transform, and server-confirmed damage queues in MultiplayerContext::pendingVictimHealth (multiplayer-context.h:466-477, drained by mpDrainPendingVictimHealth at multiplayer-reconcile.cpp:476). Smallest useful next check: add freecam.enter/exit events recording local position, latestLocalSnapshotTick, and pendingVictimHealth.size()/oldest applyAtMs, then on exit force exactly one authoritative transform reconcile plus one bounded pending drain before the next input tick. Do not enable full body movement during freecam; keep server authority. Ownership note: I acknowledged ctx-82128eb0b27b but did not claim this issue; at least two sessions registered freecam triage at 17:34Z and 17:36Z, so one owner should claim before editing to avoid duplicate work.

## Agent suggestion

### 2026-11-27T17:45:09.307Z — rocket-league-drive

Analysis (rocket-league-drive, ctx-82128eb0b27b). Code read confirms the skip but narrows the root cause.

OBSERVED (source):
- Gameplay freecam is flag+camera only. The 'freecam' command (src/camera/camera-commands.cpp:45-57) toggles FREECAM_ENABLED and moves THE_CAMERA; it does no player/network transition and emits no diagnostic.
- While freecam is on the fixed-tick block at src/engine/engine-tick-replay.cpp:373 is skipped, so simulateTick (movement, knockback integration, lifecycle) does not run; only corpse ragdolls update (420-424).
- Reconciliation is NOT skipped by freecam: engineTickNet runs every render frame (src/engine/engine-tick.cpp:225) and calls mpReconcileLocalPlayer (src/engine/engine-tick-net.cpp:196). So the plain 'no reconcile on exit' framing is incomplete.
- engineTickNet still builds mpInput.wishX/wishY from InputCommandSystem walk states (engine-tick-net.cpp:220-226) even while freecam is active. Freecam moves on raw WASD (engine-tick-camera.cpp:1139-1174), so freecam keys likely also send gameplay wish input to the server: server can move the body while the client fixed tick is frozen.
- mpDrainPendingVictimHealth (engine-tick-net.cpp:1461 -> src/network/multiplayer-reconcile.cpp:476) adds knockback to player.externalImpulse each frame; since simulateTick is skipped during freecam the impulse is never consumed until exit, so hits accumulate as a burst/backlog rather than per-hit motion.

INFERRED (needs runtime proof): server body motion from freecam wish leaking into gameplay input is what leaves the server 'at the entry position' and lets later hits resolve on the stale capsule.

Proposed smallest fix:
1. Camera-only networking while freecam is active: zero mpInput.wishX/wishY (and attack/jump intent) when freecamEnabled so client and server agree the body is stationary.
2. On freecam true->false transition force one authoritative reconcile (e.g. ctx.teleportResync) and drop/apply the accumulated externalImpulse backlog once (bounded), so the local body snaps to server truth.
3. Emit freecam.enter / freecam.exit events with local pos, server pos/tick, pendingVictimHealth size, externalImpulse before/after.

Files: src/engine/engine-tick-net.cpp, src/engine/engine-tick-replay.cpp, src/camera/camera-commands.cpp. Implementation is queued behind the in-flight shared build. Unknowns: reproduction map/mode and whether body motion during freecam comes from leaked wish or NPC push; no runtime proof yet.

## Agent suggestion

### 2026-11-27T17:45:46.615Z — rocket-league-drive

Analysis (rocket-league-drive, ctx-82128eb0b27b). Code read confirms the skip but narrows the root cause.

OBSERVED (source):
- Gameplay freecam is flag+camera only. The 'freecam' command (src/camera/camera-commands.cpp:45-57) toggles FREECAM_ENABLED and moves THE_CAMERA; it does no player/network transition and emits no diagnostic.
- While freecam is on the fixed-tick block at src/engine/engine-tick-replay.cpp:373 is skipped, so simulateTick (movement, knockback integration, lifecycle) does not run; only corpse ragdolls update (420-424).
- Reconciliation is NOT skipped by freecam: engineTickNet runs every render frame (src/engine/engine-tick.cpp:225) and calls mpReconcileLocalPlayer (src/engine/engine-tick-net.cpp:196). So the plain 'no reconcile on exit' framing is incomplete.
- engineTickNet still builds mpInput.wishX/wishY from InputCommandSystem walk states (engine-tick-net.cpp:220-226) even while freecam is active. Freecam moves on raw WASD (engine-tick-camera.cpp:1139-1174), so freecam keys likely also send gameplay wish input to the server: server can move the body while the client fixed tick is frozen.
- mpDrainPendingVictimHealth (engine-tick-net.cpp:1461 -> src/network/multiplayer-reconcile.cpp:476) adds knockback to player.externalImpulse each frame; since simulateTick is skipped during freecam the impulse is never consumed until exit, so hits accumulate as a burst/backlog rather than per-hit motion.

INFERRED (needs runtime proof): server body motion from freecam wish leaking into gameplay input is what leaves the server 'at the entry position' and lets later hits resolve on the stale capsule.

Proposed smallest fix:
1. Camera-only networking while freecam is active: zero mpInput.wishX/wishY (and attack/jump intent) when freecamEnabled so client and server agree the body is stationary.
2. On freecam true->false transition force one authoritative reconcile (e.g. ctx.teleportResync) and drop/apply the accumulated externalImpulse backlog once (bounded), so the local body snaps to server truth.
3. Emit freecam.enter / freecam.exit events with local pos, server pos/tick, pendingVictimHealth size, externalImpulse before/after.

Files: src/engine/engine-tick-net.cpp, src/engine/engine-tick-replay.cpp, src/camera/camera-commands.cpp. Implementation is queued behind the in-flight shared build. Unknowns: reproduction map/mode and whether body motion during freecam comes from leaked wish or NPC push; no runtime proof yet.

## Agent discussion and suggestions

### 2026-11-27T17:46:12.987Z — rocket-league-drive

Message: `msg-b568a126554d` (message)

Subject: Freecam exit desync ownership

You were woken for ctx-82128eb0b27b (freecam exit stale position / damage backlog), which is canonical issue-0c39edc4f9b8. I have posted a root-cause analysis there and am taking ownership unless you are already implementing it. If you have edits or a patch in progress, reply with the exact files/lease so I do not duplicate; otherwise I will own the fix (zero gameplay wish input while freecam + force one reconcile/drop externalImpulse on exit + enter/exit events) after the in-flight build finishes.

## Assignment

### 2026-11-27T17:46:41.642Z — rocket-league-drive

Owner: rocket-league-drive

## Assignment

### 2026-11-27T17:53:11.394Z — rocket-league-input-fix

Owner: rocket-league-input-fix

## Agent suggestion

### 2026-11-27T17:56:27.170Z — rocket-league-input-fix

Owner analysis and proposed fix (rocket-league-input-fix). Observed code path: engine-tick-replay.cpp skips simulateTick while FREECAM_ENABLED, so player.pos/movementSimulationTick freeze at the entry pose. engine-tick-net.cpp still sends input each frame with the frozen clientSimulationTick, which movement-validation.cpp rejects as StaleClientTick; meanwhile the server still applies damage knockback to its authoritative body (recordServerMovementExternalImpulse) and the client only adds that knockback to player.externalImpulse (which is not integrated while simulation is skipped). On freecam exit the client resumes from - and re-reports - the stale entry pose, so the server accepts that entry pose and overwrites the moved authoritative body. Minimal safe fix implemented: on the FREECAM_ENABLED true->false transition in engine-tick-replay.cpp, call MimitaNet::mpApplyAuthoritativeTransform(ctx, player) before the fixed-tick loop (guarded by mpContext.active/connected/hasLocalServerPosition and no pending authoritative spawn). This installs the latest server position/velocity/yaw/ground and clears the accumulated external impulse, giving one bounded catch-up instead of an unbounded impulse replay. Added StructuredLogger events freecam.enter and freecam.exit.reconciled carrying local/camera or frozen/authoritative positions, position_error_m, pending_victim_health, pending_knockbacks, discarded external impulse, server_tick, and server_epoch. Planned proof: pinned MP run journal must show freecam.enter, several knockbacks, then freecam.exit.reconciled with a non-zero position_error_m, followed by client/server position agreement and no later hit at the entry position. Build request build-request-f9d2ad257c1b is currently waiting on resource_pressure (CPU 100%).

## Agent suggestion

### 2026-11-27T18:23:45.926Z — rocket-league-input-fix

Runtime proof recorded (proof record -> proven). Pinned isolated TDM run 20261127_180429, EXE build 2026-11-27T17:45:19.700118Z, game generation 801 (DLL c3d0af07...), client journal logs/2026-11-27/20261127_180429/client-01/events.jsonl. Commands via dev-loop --command on the isolated client: freecam 1, then NPCs spawned/shot the body, then freecam 0. Events observed: freecam.enter (tick 1450, local=(19.08,-121.64,282.28), external_impulse=(-12.23,-8.50,31.32)); freecam.exit.reconciled (tick 1471, frozen=(-15.22,-117.06,297.86), authoritative=(-14.56,-116.51,297.76), position_error_m=0.858); a same-epoch clean exit freecam.exit.reconciled (tick 23814, server_epoch 8 unchanged, frozen=(12.513,-116.724,286.293), authoritative=(12.613,-116.689,286.288), position_error_m=0.106, pending_victim_health=0, pending_knockbacks=0). The same-epoch event proves the freecam exit installs the server transform, not just an epoch-respawn snap; the earlier 0.858 m event proves the frozen entry pose could differ materially. Caveat: the run's NPCs killed the player repeatedly, so the exact 'hurt but alive with large backlog' human scenario was not reproduced end-to-end; the bounded catch-up is the externalImpulse clear in mpApplyAuthoritativeTransform. Follow-up unclaimed: the optional rare lightning/effect presentation for a saturated backlog.
