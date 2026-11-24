# issue-dad84e4e7c50: Require runtime event proof for every gameplay change and record visual-only residuals

## Human report (verbatim, immutable)

> but put also an issue that u are pretty much done like the  visual confirmation its just that  but the intention is  to prove every new gameplauy change  with events or otehr evidence

## Summary

The Bloodtest implementation slice now has pinned runtime event proof at
generation 581, including active mode with two participants, bodycam
application with head-follows-body and settle deadband, slowdm movement, and UI
drawn after PostFX at 1920x1080. Visual confirmation remains unproven because
the available screenshot command captured unrelated desktop content.

This issue records the completion state and requires future gameplay changes to
carry runtime event evidence, with visual-only gaps kept explicit rather than
silently claimed.

## Reproduction

Review run `20261124_043709`.

- Server journal: `network.mode.start` `stage=active`, `participants=2`,
  `server_tick=2366`.
- Client journal: `gamemode.bodycam.applied` with `head_follows_body=true` and
  `settle_deadband_degrees=0.35`.
- Client journal: `client.movement.profile.applied` with `preset=slowdm`,
  `source_max_speed=9.0`, `sprint_speed_multiplier=1.35`.
- Client journal: `ui.postfx.presentation` with
  `use_output_framebuffer=true`, 1920x1080.

The attempted visual capture at run `20261124_041854` wrote a 1920x1080 image of
a Rocket League window instead of MiMITA. Visual acceptance therefore remains
unproven and is tracked in `issue-130ee745052a`.

## Expected behavior

Every new gameplay behavior change must have pinned runtime event or equivalent
owner-level evidence before it is reported complete. Build success alone is not
proof. Visual acceptance is a separate claim: either a human observes the exact
build/run, or the issue remains open with an explicit visual residual. Visual
proof must come from a hidden-window/headless framebuffer capture, not an
arbitrary desktop screenshot.

## Acceptance

Specs state the event-first proof requirement, the separate visual-acceptance
requirement, and the rule that an unresolved visual check remains an explicit
open residual. The Bloodtest task/issue records its event proof and visual gap.
Future gameplay changes can be reviewed against the same rule without inventing
per-feature exceptions. Documentation-only and tooling-only changes still use
focused checks or reasoned exemptions.

## Paths

- `docs/specs/dev-loop/agent-proof-and-verification.md`
- `docs/specs/dev-loop/visual-proof-screenshots.md`
- `docs/specs/gamemodes/bodycam.md`
- `docs/workflows/runtime-scenario-validation.md`

## Evidence

- Run: `20261124_043709`
- Build: `2026-11-24T03:30:52.372066Z:3ae36c9314a776a66c842b97356f14e128171e1442d28e49cd18a110b6030db6`
- Game generation: 581
- Server journal: `logs/2026-11-24/20261124_043709/server-01/events.jsonl`
- Client journal: `logs/2026-11-24/20261124_043709/client-01/events.jsonl`
- Visual blocker: `issue-130ee745052a`

## Risk

A blanket event requirement could be misread as forcing synthetic events for
trivial or visual-only changes. Keep documented exemptions for docs/tooling,
require events only for behavior changes, and never treat event proof as human
visual acceptance.

## Proof expectation

Documentation review plus board state: the proof and visual-residual rules are
present, Bloodtest retains its runtime event proof, and the visual gap is
visible until a pinned headless capture or human observation closes it. Do not
implement code in this issue; do not claim visual acceptance from a desktop
screenshot.

## Related

- `issue-e9f582f8d2d0` (Bloodtest bodycam implementation)
- `issue-130ee745052a` (headless screenshot defect)
- `issue-89efbbc86535` (screenshot-at-tick)


## Triage

### 2026-11-24T05:28:03.062Z — opencode-deepseek-v4-1

Summary: (unchanged)
Owner: (unchanged)
Priority: high

## Agent suggestion

### 2026-11-24T05:30:15.181Z — hot-reload-latency-root

Verification by hot-reload-latency-root (2026-11-24): the event-first policy is present in docs/specs/dev-loop/agent-proof-and-verification.md:199-227 (gameplay changes name a pinned runtime event; visual acceptance is a separate claim; unresolved visual checks stay open as explicit residuals; desktop screenshots are disallowed as proof). The headless-framebuffer contract is present in docs/specs/dev-loop/visual-proof-screenshots.md:59-73. Bodycam visual residual rule is recorded in docs/specs/gamemodes/bodycam.md:234-235. Bloodtest runtime proof exists at game generation 581, run 20261124_043709 (network.mode.start stage=active, gamemode.bodycam.applied head_follows_body=True, client.movement.profile.applied preset=slowdm, ui.postfx.presentation use_output_framebuffer=True). No duplicate issue was created. Remaining residual is visual confirmation, which depends on the headless capture work in issue-130ee745052a; this issue's docs task is complete and exempt (task-proof-2ac97de25302).

## Review (docs-verified)

### 2026-11-24T05:32:40.686Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Independent build-wait review of the high-priority user context. Verified in the working tree that the policy text exists: agent-proof-and-verification.md:199-227 requires a pinned owner-level event/field per gameplay change, keeps runtime_proven separate from human_accepted, and forbids desktop-screenshot visual claims; visual-proof-screenshots.md:52/63-66 requires post-UI framebuffer capture plus headless/window sidecar; bodycam.md:224-235 keeps visual acceptance separate and the residual open; runtime-scenario-validation.md:130 treats event proof and visual acceptance as separate claims. Verified issue-e9f582f8d2d0 records generation 581 run 20261124_043709 event proof (network.mode.start stage=active participants=2 tick=2366; gamemode.bodycam.applied head_follows_body=true; client.movement.profile.applied preset=slowdm; ui.postfx.presentation use_output_framebuffer=true) and the visual gap. The exemption for the documentation task is appropriate: no gameplay or screenshot behavior is claimed. Remaining residual: issue-130ee745052a has no owner and no pinned headless capture or human visual observation; that is the blocking next step. No source edits or builds performed by this reviewer; event proof is not treated as visual acceptance.
