# issue-9c05cbcd5b0d: ADS body pose: arms lock to weapon, torso/head aim, configurable slow blend, weapon.ads.pose

## Human report (verbatim, immutable)

> Both arms locked to the weapon (right hand at the grip, left at the foregrip); torso turns toward the aim; head tilts right/down into the sight; keep bodycam smoothing; slow the ADS blend to a configurable 0.30-0.40 s; emit weapon.ads.pose with sight alignment, arm-lock, torso/head angles, and blend time. (docs/handoffs/2026-11-27-body-rig-ads-damage-handoff.md item 2)


## Assignment

### 2026-11-27T07:22:09.673Z — opencode-deepseek-v4-3

Owner: opencode-deepseek-v4-3

## Agent discussion and suggestions

### 2026-11-27T07:22:45.707Z — opencode-deepseek-v4-3

Message: `msg-2e59a40aeabd` (question)

Subject: Single build coordination: c6fd approved, ADS batch ready

Build coordination (single-flight): build-request-c6fd97046c97 (rocket-league-full) is approved and unconsumed while the dev-loop is idle at gen 783. I have item-2 ADS body/config changes saved (build note cfa22cff3114, issue-9c05cbcd5b0d) and need one build. Proposal: consume c6fd once as the single shared build so all saved sources (vehicle/input + ADS) compile together; I will record the build and proof. If you are about to seal your own build, reply with the request id; otherwise I will record no-response per the AGENTS no-response rule and seal c6fd. Do not start a second concurrent build.

## Agent suggestion

### 2026-11-27T07:23:04.590Z — opencode-deepseek-v4-3

No-response seal (single build): asked all agents once at 07:22:45Z (msg-2e59a40aeabd, thread-e12d7df9b35e) to confirm consuming build-request-c6fd97046c97 as the one shared build or to reply with their own request id. No response event arrived before inspection; dev-loop --status showed lifecycle=source_changed, game-build published gen 783, no active build. Per the AGENTS 2026-11-27 no-response exception I will consume c6fd once as the single build for the saved ADS inputs (build note cfa22cff3114) plus the other saved sources, then record the postmortem and runtime proof here. This is not a general self-approval.

## Agent suggestion

### 2026-11-27T07:32:42.950Z — opencode-deepseek-v4-3

Build + runtime evidence 2026-11-27. Build: attempt game-dll-20261127T072316157011Z-06c929 (request c6fd97046c97), gen 784, success, total 68.594s (compile 42.94s, link 8.44s, scan 5.55s, publish 0.48s), compiled 10, skipped 637; postmortem largest cause resource pressure (mean host CPU 97.47%, peak 100%, minimum free memory 23 MB) with suggestion to move concurrent builds/tests off the critical path; header fanout 8 for weapon-config.h, queue wait ~0.002s. This is build evidence only, not behavior proof. Runtime: isolated run 20261127_072858, pinned gen 784, profile ads-ak47-proof (weapon_set 33), launch mode 41; journal logs/2026-11-27/20261127_072858/client-01/events.jsonl. Actions: agent.input equip 1 (slot resolve mapped bloodtest_ak47, weapon_set 33), agent.input look 0 120, agent.hold secondary 240. Observed weapon.ads.state for bloodtest_ak47: configured_blend_rate 18.0, effective blend_rate 9.0909, blend_seconds 0.3300000131. Observed weapon.ads.pose for actor ADS Dev: ads_requested true, blend rose to 0.99985/0.9999998, head_tilt_deg -7.9988, torso_pitch_deg -2.3582, torso_yaw_deg 0.0, bodycam_smoothing true; release samples show blend decaying with ads_requested false. Residual/gap: measured right_hand_grip_m and left_hand_foregrip_m are large and variable (0.46-6.6 m) instead of near zero, so the arm-lock visual claim is NOT proven; sight_alignment_m is 2.4-7.3 m. The blend/torso/head event contract is proven; true two-hand lock to grip/foregrip still needs a dedicated pose fix and visual/measurement rework. Do not claim item 2 fully complete.

## Review (needs-work)

### 2026-11-27T07:53:33.490Z — opencode-deepseek-v4-3

Reviewer: opencode-deepseek-v4-3

Blend/torso/head contract proven in run 20261127_072858 gen 784: weapon.ads.pose ads_requested true, blend 0.99985/0.9999998, blend_seconds 0.33, blend_rate 9.0909, head_tilt -7.9988, torso_pitch -2.3582, torso_yaw 0.0, bodycam_smoothing true; weapon.ads.state shows configured_blend_rate 18, effective 9.0909, blend_seconds 0.33. Residual: right_hand_grip_m and left_hand_foregrip_m are large/variable (0.46-6.6 m) and sight_alignment 2.4-7.3 m, so arms are not measurably locked to grip/foregrip. Keep open for a real two-hand pose fix and a cleaner measurement; do not claim full item 2.
