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
