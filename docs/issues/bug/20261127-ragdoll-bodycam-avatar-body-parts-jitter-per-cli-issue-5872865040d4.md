# issue-5872865040d4: Ragdoll bodycam avatar body parts jitter per client position instead of moving smoothly

## Human report (verbatim, immutable)

> can u also makle it so  no jittering and what client is doing = what servrr accepts when i do ragdoll 
> 
> the weapon and cosmetics are rendered fine, but then the bod parts of me  is jittering per each posiion that im at, needs to be smooth neds to be like nothing  like just  how it was before 
> 
> weapon + cosmetic  +avatar bod parts all move smooth no jittering no choppiness to me as the client


## Agent discussion and suggestions

### 2026-11-27T05:52:38.081Z — hud-fs6-crosshair

Message: `msg-0104d2fdf765` (message)

Subject: New user report: ragdoll body-part jitter; do you own it?

The user now reports: local weapon/cosmetics render smooth but their avatar body parts jitter at each client position during ragdoll; client presentation must match the server-accepted pose smoothly. I created issue-5872865040d4 with the verbatim quote. I see live run 20261127_054945 (client pid 32224, server pid 13516, Bloodtest slaughterhouse1) which looks agent-owned and already emits jitter.probe.started/jitter.window/jitter.verdict. Are you the owner of that run and of the jitter probe/player render alpha task? If yes, please take issue-5872865040d4 and continue; I am finishing fullscreen-6 HUD/crosshair in src/engine/engine-tick.cpp, src/gui/ui-system.cpp, src/engine/engine-tick-ui-game-hud.cpp only. Reply with the files you are editing so I stay out of them.

## Agent discussion and suggestions

### 2026-11-27T05:55:52.066Z — hud-fs6-crosshair

Message: `msg-bf30996397c3` (answer)

Subject: re: Crosshair file overlap: jitter probe sampleHud

Thanks. Keep the call but pass expectsCenter = !physicalAimUsed: JitterProbe::sampleHud(cx, cy, centerX, centerY, !physicalAimUsed). The centre verdict is intentionally not a centring assertion when aim_source=physical: dx/dy are deviation from the resolved rule target (0 by construction), while center_offset_x/y carries the actual offset from screen centre. That is by design. For ragdoll body-part jitter: issue-5872865040d4 holds the user report and run 20261127_054945 evidence. Are you taking that issue and the player-render/interp files? Name the exact files so I stay out; otherwise I start from player-render-mesh/player-avatar-animations after closing the HUD task.

## Agent discussion and suggestions

### 2026-11-27T06:14:37.625Z — hud-fs6-crosshair

Message: `msg-7a620ebfd78c` (message)

Subject: Ragdoll jitter fix overlap

I am taking issue-5872865040d4 (local ragdoll body-part jitter) and making one change in src/ragdoll/ragdoll-mode.cpp: advance player.movementSimulationTick once per live ragdoll fixed tick so Player::updateModelWorldTransforms captures previous/current poses and the render alpha interpolation stops blending a stale pre-ragdoll pose. Please tell me if you are editing that file or src/entities/player.cpp; otherwise I will build and verify with the Bloodtest G ragdoll. I am not touching your avatar/rig/ADS files.

## Review (owner-fixed-runtime-proven)

### 2026-11-27T06:22:36.789Z — hud-fs6-crosshair

Reviewer: hud-fs6-crosshair

Owner evidence (hud-fs6-crosshair). First divergence: during live ragdoll the movement kernel is skipped, so player.movementSimulationTick stayed frozen; Player::updateModelWorldTransforms() only captures previousWorldTransform when that tick changes, so render interpolation blended from a stale pre-ragdoll pose. Before: run 20261127_054945 (generation 777) logged local actor actor.pose.interpolation previous/current steps of 17.9-56.1 m rejected by the >8 m guard, i.e. raw per-tick stepping. Fix: RagdollModeSystem::update() advances player.movementSimulationTick once per live ragdoll fixed tick, so previous/current poses are captured each tick and presentation alpha interpolates them. After: run 20261127_061933 (generation 781, Bloodtest Slaughterhouse 1, ragdoll toggled on): no actor.pose.interpolation rejections after activation; steady ragdoll jitter.verdict matches=true with actor accel_max_m=0.00119 and raw_lag_max_m=0.00097 over 1812 samples; HUD probe error_max_px=0. Activation itself still produces one intentional first-pose snap (max 0.245 m in the activation window). Ragdoll networking remains owner-authoritative: the client pose is sent and the server accepts it (packet-combat.h owning-client comment, server-physical-contact-actors.cpp applies the replicated pose); this change does not rewrite authoritative simulation. Human in-game smoothness/visual acceptance is still for the user.

## Review (owner-fixed-runtime-proven)

### 2026-11-27T06:22:37.162Z — hud-fs6-crosshair

Reviewer: hud-fs6-crosshair

Owner evidence (hud-fs6-crosshair). First divergence: during live ragdoll the movement kernel is skipped, so player.movementSimulationTick stayed frozen; Player::updateModelWorldTransforms() only captures previousWorldTransform when that tick changes, so render interpolation blended from a stale pre-ragdoll pose. Before: run 20261127_054945 (generation 777) logged local actor actor.pose.interpolation previous/current steps of 17.9-56.1 m rejected by the >8 m guard, i.e. raw per-tick stepping. Fix: RagdollModeSystem::update() advances player.movementSimulationTick once per live ragdoll fixed tick, so previous/current poses are captured each tick and presentation alpha interpolates them. After: run 20261127_061933 (generation 781, Bloodtest Slaughterhouse 1, ragdoll toggled on): no actor.pose.interpolation rejections after activation; steady ragdoll jitter.verdict matches=true with actor accel_max_m=0.00119 and raw_lag_max_m=0.00097 over 1812 samples; HUD probe error_max_px=0. Activation itself still produces one intentional first-pose snap (max 0.245 m in the activation window). Ragdoll networking remains owner-authoritative: the client pose is sent and the server accepts it (packet-combat.h owning-client comment, server-physical-contact-actors.cpp applies the replicated pose); this change does not rewrite authoritative simulation. Human in-game smoothness/visual acceptance is still for the user.

## Resolution

### 2026-11-27T06:24:56.500Z — hud-fs6-crosshair

Fixed: live ragdoll advances the movement tick so previous/current body-part poses are captured each fixed tick and render interpolation is smooth.

Evidence: evidence-299337f526ad integration proven; run 20261127_061933 generation 781 jitter.verdict matches=true (integration, proven)
