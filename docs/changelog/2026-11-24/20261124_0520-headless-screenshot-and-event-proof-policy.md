// 2026-11-24 05:20 UTC
/* purpose
* record the headless screenshot and event-proof policy documentation update
* preserve build/runtime evidence separately from visual acceptance
* link the new issues and the Bloodtest visual residual
* this file DOES NOT claim visual acceptance or implement screenshot code
* this file DOES NOT rewrite the earlier Bloodtest implementation changes
*/

# Headless screenshot issue and event-proof policy

# Task

- Task IDs: `bodycam-full-spec-implementation`, `headless-framebuffer-screenshot`, `document-event-first-proof-policy`
- Session ID: `session-2a7e16833716`
- Owner: opencode-deepseek-v4-1
- Focus: document the headless screenshot requirement, record the Bloodtest completion/visual residual, and require event-based proof for every gameplay change.
- Status: PASS for docs/issues. Bloodtest code/build/event proof already present; visual acceptance remains pending.
- Date, time, timezone: 2026-11-24T05:20:21Z
- Branch: `checkpoint/one-checkout-condensation-20261111`
- Base commit: `186eebd23ce5f8f3f10377c47277ed58bd3b4d83`
- Final commit: `51306c08` (a concurrent shared-checkout commit included these paths; this session did not create that commit)

# Requested behavior

The user asked for an issue that `agent.screenshot` must work headlessly, an issue that Bloodtest is effectively complete except visual confirmation, and a policy/spec update that every new gameplay change is proven with events or other evidence. No code implementation was requested in this follow-up.

# Specification alignment

- `docs/specs/dev-loop/visual-proof-screenshots.md` now requires post-UI framebuffer capture and rejection of desktop/window composites.
- `docs/specs/dev-loop/agent-proof-and-verification.md` now requires pinned runtime event proof for gameplay changes and an explicit visual residual when human visual acceptance is pending.
- `docs/specs/gamemodes/bodycam.md` now points visual proof at the headless framebuffer contract and keeps visual acceptance separate.
- `docs/workflows/runtime-scenario-validation.md` now states that event proof and visual acceptance are separate claims.

# Issues

- `issue-130ee745052a` — `agent.screenshot` captures desktop content instead of the hidden client framebuffer.
- `issue-dad84e4e7c50` — require runtime event proof for every gameplay change and record visual-only residuals.
- `issue-89efbbc86535` — existing screenshot-at-tick issue; cross-linked and given a headless framebuffer task.
- `issue-e9f582f8d2d0` — Bloodtest bodycam issue; completion/event evidence appended, visual residual retained.

# Evidence

- Build: EXE artifact `2026-11-24T03:30:52.372066Z`, game generation `581`, DLL `build/game/generation-581/game-g581.dll`, SHA-256 `45926450d0262aa23bc1324a6f285525d2d4c969edbec7d9ec1d46b2d6d0ba65`.
- Runtime run: `20261124_043709`.
- Server: `network.mode.start`, `stage=active`, `participants=2`, `server_tick=2366`.
- Client: `gamemode.bodycam.applied` with `head_follows_body=true` and `settle_deadband_degrees=0.35`.
- Client: `client.movement.profile.applied` with `preset=slowdm`, `source_max_speed=9.0`, `sprint_speed_multiplier=1.35`.
- Client: `ui.postfx.presentation` with `use_output_framebuffer=true`, `1920x1080`.
- Screenshot failure: run `20261124_041854`, command `agent.screenshot logs/2026-11-24/20261124_041854/client-01/bloodtest-crisp.bmp` completed, but the image was an unrelated Rocket League window. The BMP was deleted after inspection.

# Exact implementation changes

This follow-up changed documentation and issue records only. No C++, config, or runtime behavior was changed.

## `docs/specs/dev-loop/visual-proof-screenshots.md`

- Added framebuffer-not-desktop capture rules, hidden/covered/headless requirements, sidecar fields, rejection of unrelated images, and hidden-window acceptance.

## `docs/specs/dev-loop/agent-proof-and-verification.md`

- Added the gameplay event-proof policy, visual-acceptance separation, visual-residual rule, and headless screenshot requirement.

## `docs/specs/gamemodes/bodycam.md`

- Replaced the visible-window BMP description with the headless framebuffer contract and explicit visual-residual rule.

## `docs/workflows/runtime-scenario-validation.md`

- Added the event-proof and visual-residual policy for gameplay work.

# Changed files and reverse steps

| File | Before | After | Why | Reverse |
|---|---|---|---|---|
| `docs/specs/dev-loop/visual-proof-screenshots.md` | Desktop/window screenshot ambiguity | Headless framebuffer contract and rejection rules | Prevent unrelated desktop images as proof | `git checkout -- docs/specs/dev-loop/visual-proof-screenshots.md` |
| `docs/specs/dev-loop/agent-proof-and-verification.md` | Event proof and visual acceptance implied | Explicit event-first proof and visual residual policy | Make completion honest | `git checkout -- docs/specs/dev-loop/agent-proof-and-verification.md` |
| `docs/specs/gamemodes/bodycam.md` | Mentioned visible-window BMP | Points to headless contract and residual rule | Align bodycam visual proof | `git checkout -- docs/specs/gamemodes/bodycam.md` |
| `docs/workflows/runtime-scenario-validation.md` | Events primary, visuals separate | Added explicit gameplay proof/residual rule | Align workflow with proof spec | `git checkout -- docs/workflows/runtime-scenario-validation.md` |
| `docs/issues/bug/20261124-agent-screenshot-captures-desktop-content-instea-issue-130ee745052a.md` | Did not exist | New bug record | Track headless capture defect | Delete the untracked issue doc or keep as shared record |
| `docs/issues/improvement/20261124-require-runtime-event-proof-for-every-gameplay-c-issue-dad84e4e7c50.md` | Did not exist | New improvement record | Track proof policy and Bloodtest residual | Delete the untracked issue doc or keep as shared record |

# Validation

- `git diff --check` on the four edited docs: PASS.
- Build/publication evidence from the shared dev loop: generation `581` published.
- Runtime event proof from run `20261124_043709`: PASS for the listed events.
- Headless screenshot capability: NOT IMPLEMENTED; tracked as `issue-130ee745052a`.
- Visual confirmation: PENDING; not claimed.
- Human acceptance: not claimed.

# Remaining work

- Implement hidden-window/headless framebuffer capture and prove it with a hidden/covered-window run.
- Capture or have a human accept the Bloodtest visual presentation on a pinned build/run.
- Keep event proof and visual acceptance as separate completion claims.
