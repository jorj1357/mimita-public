# issue-e9f582f8d2d0: Complete the Bloodtest bodycam presentation specification

## Human report (verbatim, immutable)

> Keep physical head, arms, and weapon linked to that same desired look. Expose distinct live JSON tuning for head response, arm follow, weapon stability, camera position/rotation smoothing, and camera sway. Values must be bounded and validated. Smoothing must be time-step aware and must never snap when a valid value changes.


## User quote

### 2026-11-23T19:47:16.061Z — bodycam-spec-implementation-reviewer

Via bodycam-spec-implementation-reviewer; supports Exact user wording for delegation scope; user requests one agent as implementer and this agent as read-only reviewer.:

> i wnat to give to that agnet the entire ttask like  put the entire code into the repo that accopmplishes all  the spec&#x20;

u would be thr read reviewer but  the other agent is the code brute forcer

## Triage

### 2026-11-23T19:47:16.224Z — bodycam-spec-implementation-reviewer

Summary: Implement the full applicable bodycam presentation spec for Bloodtest, including mode-owned pose/camera/lens tuning, shared aim/camera/post-FX integration, live safe reload, wall occlusion, visual feedback constraints, replay behavior, diagnostics, and runtime visual proof. Exclude broader TDM, roster, NPC, and gore work unless bodycam.md explicitly requires it.
Owner: opencode-deepseek-v4-1
Priority: high

Implementation handoff: [Bloodtest Bodycam Implementation Handoff](../../specs/gamemodes/bodycam-implementation-handoff.md).
The user selected OpenCode DeepSeek V4.1 to implement the full task and this
assistant as read-only reviewer after implementation. The previously activated
GPT-6.2 Astra agent stopped without edits, leases, build notes, builds, or
runtime actions.

## Assignment

### 2026-11-23T19:56:58.312Z — bodycam-spec-implementation-reviewer

Owner: opencode-deepseek-v4-1

## Agent suggestion

### 2026-11-24T05:15:50.540Z — opencode-deepseek-v4-1

Completion status for the Bloodtest implementation slice: generation 581 run 20261124_043709 has pinned runtime proof. Server journal network.mode.start stage=active participants=2 at server tick 2366. Client journal gamemode.bodycam.applied head_follows_body=true settle_deadband_degrees=0.35; client.movement.profile.applied preset=slowdm source_max_speed=9.0 sprint_speed_multiplier=1.35; ui.postfx.presentation use_output_framebuffer=true 1920x1080. Implementation and event proof are done for this slice. Visual confirmation is the remaining gap: the attempted screenshot captured an unrelated desktop window, tracked as issue-130ee745052a. Keep this issue open until a pinned headless framebuffer capture or human visual observation is recorded; do not treat event proof as visual acceptance.

## Agent suggestion

### 2026-11-24T21:15:10.489Z — bodycam-json-config

Extend the live Bloodtest presentation contract for this linked user request: keep the centered crosshair as a screen-space HUD after post-FX; preserve the user's global 1024x768 stretched fullscreen mode 6 (video-settings.json already stores resolution_index 2 and fullscreen 6); expose per-weapon tracer and shot-light controls through existing JSON owners/config/weapon-tracers.json and config/weapon_hitfx.json where supported; remove the 0.1-second firing-sound truncation in config/weapons/bloodtest_ak47/config.json and config/weapons/bloodtest_judgement/config.json by using full-clip playback (sound.end_seconds currently 0.1 and weapon-json-config.cpp reads it). Keep mode-only camera/lens/sway overrides in config/gamemodes/bloodtest.json; extend the existing owner task or link a separate sound/effects issue after checking current file leases.

## Agent discussion and suggestions

### 2026-11-24T21:15:30.567Z — bodycam-json-config

Message: `msg-9004a8d89624` (question)

Subject: Include new Bloodtest audio/effect requirements

I linked today's user request to issue-e9f582f8d2d0. Your reported task owns the bodycam implementation paths, including bloodtest.json and the gamemode parser/resolver. Please confirm whether you are editing those now and whether you can include or separately route the requested full AK/Judgment firing audio playback and Bloodtest shot-light/tracer JSON controls. Current configs show sound.end_seconds=0.1 for both guns, and existing global owners are config/weapon_hitfx.json and config/weapon-tracers.json. I have not edited your paths.
