# Ragdoll: restore limb-to-capsule lock, hitbox visibility toggle, alpha overlap fix

- Task ID: ragdoll-limb-lock-and-hitbox-toggle
- Summary: Fix the one-sided joint base that let every limb separate from its
  torso attachment; gate arm stretch to extending/grabbing; add a master debug
  hitbox toggle; reduce wire-capsule alpha overlap/flicker.
- Status: PASS_WITH_HUMAN_REVIEW
- Date, time, timezone: `2026-09-10T21:14:09Z` (2026-09-10 17:14:09 EDT)
- Branch: `8292026stash`
- Base commit: working tree; no commit created
- Final commit: none (uncommitted)

## Pre-existing changes

- Not created by this session: the FFA/kill-event/countdown network/gamemode
  work, the unrelated `src/gui/hud/chat-window.cpp` edit, and the other
  `docs/changelog/2026-09-10/` files. Not claimed here.
- Earlier ragdoll work: `155338`, `163323`, `163852`, `172838`, `174658`,
  `180706`, `183703`, `190049`, `190421`, `193151`, `205900`.

## Regression

Recorded in `docs/regressions/regressions-v1.md` at
`2026-09-10T21:13:29Z — Ragdoll limbs lost their rigid attachment to the
capsule`.

Wrong code in `solveJoints`:
`const float maxDist = part.restLength + part.maxStretch;`

`restLength` is the child COM-to-anchor distance (about the capsule half-length),
not the anchor separation (0 at bind), so every limb got ~0.5-0.59 m of free
separation and no longer stayed locked to its attachment.

## Implementation changes

- `solveJoints`:
  - base the one-sided limit on `part.maxStretch` only (anchors coincide at
    bind);
  - enable stretch only while that arm is **extending or grabbing**; otherwise
    the joint is rigid, so limbs stay locked at the attachment.
- `RagdollModeConfigData.debugHitboxesVisible` added, parsed from
  `debug_hitboxes_visible`; `RagdollModeSystem::render` returns early when false,
  hiding all ragdoll capsules/axes/links. `attachments_visible` still controls
  the attachment detail independently.
- `DebugVis::drawWeaponCapsuleWire` ring segments reduced 20 -> 8 to reduce
  overlapping alpha-blended line coverage that made low-alpha capsules appear to
  flicker between opaque and transparent.
- `config/ragdoll.json`: added `"debug_hitboxes_visible": true`.

## Validation

- Build: `python build_agent.py` -> `Status: SUCCESS`, return code 0, duration
  15.52s; `mimita.exe` relinked 2026-09-10 17:13:46. No errors or warnings.
- Runtime: not performed this session.

## Human acceptance

- Confirm limbs are rigidly locked to their capsules (no free rotation about the
  limb center); arm stretch should only occur while extending or grabbing.
- Confirm `debug_hitboxes_visible: false` hides all ragdoll debug hitboxes and
  `attachments_visible` still toggles the attachment details.
- Confirm low per-capsule `alpha` no longer flickers toward opaque.
