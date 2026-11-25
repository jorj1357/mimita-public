// 2026-11-24 17:48 EST
/* purpose
* record the issue-review-sweeper evidence review for the fullscreen mode 6 UI-stretch report
* record the spec/implementation conflict and the real HUD-layer control
* connect the work to the existing issue and its unowned verification task
* this file DOES NOT replace the issue record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance or runtime proof that did not happen
*/

# Fullscreen mode 6 UI-stretch evidence review (issue-e0a6bcbacad0)

# Task

- Task ID: work-881fee15e327
- Session ID: session-4146a5dd9df9
- Owner: issue-review-sweeper
- Focus: high-priority user context review, evidence handoff
- Summary: Reviewed linked issue issue-e0a6bcbacad0 for ctx-3761425dadee. Appended one evidence-backed suggestion that the two graphics specs disagree on mode 5/6 HUD placement and that the documented `ui_in_scene_target` flag does not exist; corrected the issue's paths to include the real render/HUD/gamemode owners. No source edits.
- Status: new (issue), task fullscreen6-ui-stretch-proof unowned
- Date, time, timezone: 2026-11-24T22:48:00+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: eb92b02b21e62fd5ec496eee4dcad832f901bb7a
- Final commit: eb92b02b21e62fd5ec496eee4dcad832f901bb7a

# Pre-existing changes

- Exact status output: captured below in Captured git state.
- Files not created or modified by this session: all modified and untracked issue/changelog files other than `docs/issues/bug/20261124-preserve-fullscreen-mode-6-full-frame-stretch-th-issue-e0a6bcbacad0.md` (edited through coordination tooling) and this changelog.

# Requested behavior

User (ctx-3761425dadee): "fullscreen 6 behaves how i tdoes before the shader edits bc that made the ui and stuff not good, ui should be stretched too on purpose and docuemtned in spec how to ensure its streetched and how to not stertch it for prettier more put together things"

# Specification alignment

- Current specification paths: `docs/specs/graphics/graphics-resolution.md`, `docs/specs/graphics/graphics-post-processing.md`
- Exact requirements: graphics-resolution.md:51-56 says modes 5/6 compose world+GUI+HUD+crosshair at 1024x768 and stretch the whole image. graphics-post-processing.md:25-30 says modes 5/6 stretch the world then draw GUI/HUD on the output framebuffer at output resolution.
- Why the change follows the specification: this session changed no behavior; it recorded the conflict so the owner can reconcile the specs and decide the mode-6 default.
- Conflicts or decisions: direct conflict between the two graphics specs; `ui_in_scene_target` is documented in graphics-post-processing.md:52-73 but is not parsed anywhere in `src/` or present in `config/postfx.json`. The real control is the gamemode `presentation.hud.layer == "scene"`.

# Exact implementation changes

## File: `docs/issues/bug/20261124-preserve-fullscreen-mode-6-full-frame-stretch-th-issue-e0a6bcbacad0.md`

- Lines/functions/headings: evidence/suggestion discussion and `paths` field, via `agent-coord issue suggest` and `issue context`.
- Old content or behavior: paths listed only `config/video-settings.json`, `src/video/video-settings.cpp`, `src/render/post-fx.cpp`, `src/engine/engine-tick-ui-game-hud.cpp`, `shaders/basic.frag`, `docs/specs/graphics/graphics-post-processing.md`.
- New content or behavior: appended the spec-conflict evidence with exact file:line references and added the real owners `src/engine/engine-tick.cpp`, `src/gui/ui-system.cpp`, `src/gamemode/gamemode.cpp`, `src/gamemode/gamemode-runtime-resolver.cpp`, `config/gamemodes/bloodtest.json`, `docs/specs/graphics/graphics-resolution.md`.
- Reason: make the bundled issue complete enough for a future owner to act without re-deriving the render path.
- Why unrelated behavior is preserved: no source, config, or spec file was changed.

# Changed files and reverse steps

| File | Before | After | Why | Reverse |
|---|---|---|---|---|
| `docs/issues/bug/20261124-preserve-fullscreen-mode-6-full-frame-stretch-th-issue-e0a6bcbacad0.md` | issue with 6 paths and no discussion | issue with 12 paths and one evidence suggestion | complete the issue record | `git checkout -- docs/issues/bug/20261124-preserve-fullscreen-mode-6-full-frame-stretch-th-issue-e0a6bcbacad0.md` |
| `docs/changelog/2026-11-24/20261124_2248-fullscreen6-ui-stretch-evidence-review.md` | absent | this file | session changelog | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_2248-fullscreen6-ui-stretch-evidence-review.md' |

# Contract / ABI changes

- Contract: none
- Version bump: none
- Compatibility: unchanged
- Reverse: not applicable

# State / schema changes

- Domain: coordination issue record
- Old schema: unchanged
- New schema: unchanged
- Migration: none
- Reverse: not applicable

# Deleted paths

- Path: none
- Before:
- Why:
- Reverse:

# Diagnostics

- Owner/category: issue record
- Input: ctx-3761425dadee, issue-e0a6bcbacad0
- Decision: append one evidence-backed suggestion; correct paths; acknowledge context
- Output: issue updated at 2026-11-24T22:44:54Z; context acknowledged
- Failure or rejection reason: none
- Rate limiting: single suggestion to avoid duplicate reports

# Validation

- Focused skill paths and results: not applicable (review only)
- Tests and exact commands: `python devscripts/agent-coord.py issue show issue-e0a6bcbacad0`; `rg -n` over `src/` and `docs/specs/graphics/`
- Build status: no build requested or performed
- Runtime or hot-reload evidence: none claimed
- Output files: this changelog

# Measured evidence

- Before values: config/video-settings.json fullscreen=6, resolution_index=2, width=1024, height=768, vsync=false; bloodtest presentation.hud.layer="normal"
- After values: unchanged (no runtime change)
- Timestamps: inspection at 2026-11-24T22:35Z-22:45Z
- Tick/frame/network measurements: none

# Regression review

- Regression entry appended: no
- Why this is or is not a confirmed regression: no runtime behavior observed by this session; the mode-6 unstretched HUD is reported by the user and consistent with source, but not independently runtime-proven here.
- Related regression paths: `src/engine/engine-tick.cpp:258-266`, `src/engine/engine-tick.cpp:446-460`, `docs/specs/graphics/graphics-post-processing.md:25-30`

# Human acceptance

- Visual review: not performed
- Gameplay review: not performed
- Multiplayer review: not performed
- Still unverified: mode-6 post-UI framebuffer stretch, crosshair centering, non-mode-6 comparison policy

# Agent handoff context

- Owner: unassigned (task fullscreen6-ui-stretch-proof); original reporter bloodtest-quality-triage is inactive
- Focus: reconcile graphics-resolution.md vs graphics-post-processing.md, decide the mode-6 stretched-HUD default, then runtime-prove
- Files to check: `src/engine/engine-tick.cpp`, `src/gui/ui-system.cpp`, `src/gamemode/gamemode.cpp`, `src/gamemode/gamemode-runtime-resolver.cpp`, `config/gamemodes/bloodtest.json`, both graphics specs
- What another agent should not redo: the spec-conflict discovery and path correction; do not create a duplicate fullscreen issue
- Next exact step: owner chooses scene-stretched HUD for modes 5/6 (default) or keeps the crisp-HUD default, removes/implement `ui_in_scene_target`, documents the stretch and no-stretch policies, then captures pinned mode-6 plus comparison-mode framebuffer evidence

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / eb92b02b21e62fd5ec496eee4dcad832f901bb7a / eb92b02b21e62fd5ec496eee4dcad832f901bb7a
- Git status: captured below
- Diff stat: captured below
- Build identity: not requested by this session
- Generation: not applicable
- Artifact: not applicable
- Journal path: not applicable
- Session ID: session-4146a5dd9df9

# How to reverse

- Uncommitted source file: `git checkout -- <file>`
- Deleted file: `git restore --source=<base> -- <file>`
- Committed change: `git revert <commit>`
- Generated file: rerun its generator
- ABI/state/schema change: restore the old generation and state backup; do not
  assume a migration reverse is automatic.
- Reverse only files touched by this session. Preserve pre-existing changes.

# Related feature record

- Feature path: docs/issues/bug/20261124-preserve-fullscreen-mode-6-full-frame-stretch-th-issue-e0a6bcbacad0.md
