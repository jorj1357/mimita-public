# Ragdoll: editable capsules/attachments, visible attachments, left-leg fix

- Task ID: ragdoll-capsules-attachments-left-leg
- Summary: Restore ragdoll.json control of capsule size/offset/axis; add
  independent parent/child attachment offsets so limbs pivot at the shoulder;
  draw attachment anchors and frames; canonicalize part quaternions and
  diagnose the left-leg asymmetry.
- Status: PASS_WITH_HUMAN_REVIEW
- Date, time, timezone: `2026-09-10T17:46:58Z` (2026-09-10 13:46:58 EDT)
- Branch: `8292026stash`
- Base commit: working tree; no commit created
- Final commit: none (uncommitted)

## Pre-existing changes

- Not created by this session: the FFA/kill-event/countdown network and gamemode
  changes already in the working tree, and the other `docs/changelog/2026-09-10/`
  files. Not claimed here.
- Earlier ragdoll work in this session:
  `20260910_155338-ragdoll-mode-rigid-body-core.md`,
  `20260910_163323-ragdoll-collision-solid.md`,
  `20260910_163852-ragdoll-stiffness-limits-camera.md`,
  `20260910_172838-ragdoll-model-frame-unify.md`.

## Requested behavior

1. Control capsule sizes and offsets in `config/ragdoll.json`, overriding the
   mesh-derived geometry, with the offset expressed relative to the limb (part)
   frame.
2. Make attachment points and their links visible.
3. Make arm extension pivot at the attachment at the top of the arm rather than
   the capsule center, with editable attachment offsets.
4. Fix the left-leg-only wrong-axis behavior the same way the replay left-leg
   bug was fixed, not just log it. Diagnostics must use the `Debug::` API.

## Specification alignment

- `docs/specs/ragdoll-retrograd/ragdoll-retrograd.md`: RAG-008/009/010/011
  (colliders, capsule representation, self-collision, no tunneling), RAG-016/017
  (arm extension follows camera-forward and obeys collisions), RAG-046
  (hot-reloadable tuning).
- `docs/specs/replays/replay-editor-and-export-v2.md` and the replay regression
  for the historical left-leg cause and fix.

## Implementation changes

### Part A - capsule authoring
- `RagdollModeCapsuleConfig`: added optional part-frame `axis` + `hasAxis`.
- `ragdoll-mode-config.cpp`: skips non-object capsule/attachment entries; parses
  `radius`, `half_height`, part-frame `offset`, and `axis`.
- `RagdollModeSystem::initParts`: config overrides win over the mesh-derived
  radius/halfHeight/center/axis.

### Part B - attachment authoring
- `RagdollModeAttachmentConfig`: added `parent_offset`/`hasParentOffset` and
  `child_offset`/`hasChildOffset`; `offset` is kept as a `parent_offset` alias.
- `initParts`: uses explicit parent/child offsets when present, otherwise the
  geometry-derived anchor. The child anchor is what the ball joint pivots on, so
  setting it at the top of an arm makes the arm rotate and extend from the
  shoulder. Cone rest direction is recomputed from the actual parent anchor.

### Part C - attachment visibility
- `attachments_visible` added to `ragdoll.json` (+ `RagdollModeConfigData`),
  also forced on by `DebugConfig::DEBUG_RAGDOLL`.
- `render` draws: the `plrOrigin` root marker and link, parent-anchor sphere
  (cyan), child-anchor sphere (magenta), anchor-to-anchor link, anchor-to-root
  link, anchor-to-body line, per-body X/Y/Z axes, and an anchor label.

### Part D - left-leg fix + diagnostics
- `quatFromMatrixCanonical` sign-normalizes every bind orientation extracted from
  the mesh matrix, mirroring the replay hemisphere fix; `bindRelativeRotation` is
  canonicalized too.
- `reinitPreservingState` re-derives geometry live when config reloads while
  ragdolled, preserving pose and momentum (immediate re-init).
- `[RAGDOLL SYM]` rate-limited diagnostic (`Debug::logThrottled`,
  `Category::Ragdoll`) compares left/right body position, composed node position,
  and quaternion. No `printf` was added; all output uses the `Debug::` API.
- Investigation finding documented in
  `docs/regressions/regressions-v1.md` (2026-09-10T17:46:58Z): the model is flat
  (all parts children of `plrOrigin`), left/right leg nodes and mesh POSITION
  data are identical, and collider bounds are identical, so a left-only result
  cannot come from parent space or mesh asymmetry in ragdoll.

## Diagnostics

- `[RAGDOLL SYM]` (Ragdoll, throttled 1s) plus the existing structured tick
  events. Attachment visuals are controlled by `attachments_visible`.

## Validation

- Build: `python build_agent.py` -> `Status: SUCCESS`, return code 0, duration
  15.17s; `mimita.exe` relinked 2026-09-10 13:47:42. No errors or warnings.
- Config: `config/ragdoll.json` parses; `attachments_visible=true`; capsule
  entries empty (derive) and attachments keep the human's limits.
- Runtime: not performed this session.

## Regression review

- Append-only entry added: `docs/regressions/regressions-v1.md`,
  `2026-09-10T17:46:58Z — Left-leg-only wrong-axis class`, recording the old
  root-local/hemisphere cause, the previous fix, and the new model-symmetry
  finding.

## Human acceptance

- Tune `capsules.<part>.radius/half_height/offset/axis` and
  `attachments.<part>.parent_offset/child_offset` in `config/ragdoll.json`;
  verify live re-init while ragdolled.
- Confirm the attachment spheres/axes/links are visible with
  `attachments_visible: true`.
- Confirm the arm extends from the top-of-arm attachment toward camera-forward.
- Confirm the left leg matches the right leg in first and third person; if not,
  capture the `[RAGDOLL SYM]` line for localization.
