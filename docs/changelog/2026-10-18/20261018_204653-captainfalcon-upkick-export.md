# Captain Falcon Upkick Export and A/B Wiring

- Recorded: `2026-10-18T20:46:53Z` (2026-10-18 16:46:53 EDT,
  America/New_York)
- Branch: `20261011zt`
- Starting commit: `3bf384e4`
- Result: `PASS_WITH_HUMAN_REVIEW`

## Request

Export the Blender `upkick20261018` animation, use it for Captain Falcon's A
and B upward air kicks, keep B as the stronger special, and document a workflow
the user can run without AI assistance.

## Specification and owner

`docs/specs/animations/animations.md` says: “A weapon action references one
`animation_clip_id` and may not repeat total duration, phase durations, or phase
clip IDs.” It also requires `phase.windup`, `phase.active`, `phase.recovery`,
and exclusive `action.end` markers. The Captain Falcon animation profile is the
explicit current clip-availability owner until automatic discovery exists.
`config/weapons.json` retains input, hitbox, damage, knockback, impulse, and
audio data.

The A and B actions now reference the same exported `upkick_a` clip. The B
action remains stronger in existing combat data: 8% versus 6% damage and 14
versus 6 upward impulse. Its animated hitbox now follows `rightLeg`, matching
the kick animation.

## Files changed

### `tools/mimita_animation_exporter.py`

- Lines 169–174: before, an `action.end` marker on the final keyed frame was
  converted to the final sampled tick; after, that marker maps to exclusive
  `durationTicks`, while other markers keep their frame-to-tick mapping. This
  makes the screenshot's frame-60 endpoint valid for a 60-tick clip.
- Lines 253–259: before, any assigned `plrOrigin` Action caused
  `Animated plrOrigin/root motion is not supported in this export version`;
  after, only root location, rotation, or scale curves are rejected. The
  screenshot's assigned root Action had no curves.
- Lines 397–401: the example now names `plrOrigin.032` and `rightLeg.028`, and
  the export report prints the selected rig root.

### `config/animations/library/melee/captainfalcon/upkick_a.json`

- New file; no prior text existed. Blender 5.0 exported the six canonical
  parts at ticks 0, 20, 40, and 59, with `durationTicks: 60` and markers at
  windup 0, active 20, recovery 40, and exclusive end 60.

### `config/weapons.json`

- Lines 1820–1822: A previously had separate phase clip IDs
  `cf_airup_kick_windup`, `cf_airup_kick_active`, and
  `cf_airup_kick_recovery`; it now uses `"animation_clip_id": "upkick_a"`.
- Lines 1840–1845: B previously had separate `cf_airup_special_*` phase clips
  and a `rightArm` fist hitbox; it now uses `"animation_clip_id": "upkick_a"`
  and an animated `rightLeg` foot hitbox.
- B's existing input, damage, knockback, upward impulse, and audio values were
  retained.

### `assets/avatars/captainfalcon/animations.json`

- Line 363: before, the include list ended with
  `config/animations/library/locomotion/captainfalcon/land.json`; after, it
  also includes `config/animations/library/melee/captainfalcon/upkick_a.json`.

### `docs/specs/animations/blender-animation-authoring.md`

- Lines 3–6: updated the status to distinguish verified export/source behavior
  from pending in-game acceptance.
- Lines 50–81: added the PowerShell `py tools\mimita_animation_exporter.py`
  workflow, prompt choices for this up-kick, output and profile paths, marker
  tick mapping, and the empty-root-Action rule.
- Lines 268–271: documented that empty root Actions are ignored while animated
  root transform channels remain unsupported.
- Lines 273–277: changed the remaining visual-acceptance note from the old punch
  export to the `upkick_a` clip.

### This changelog

- New file; no prior text existed.

## Validation and evidence

- Ran `py tools/mimita_animation_exporter.py` in an interactive terminal with
  Blender 5.0. The exporter selected `plrOrigin.032`, loaded the provided blend
  file and default rest GLB, exported `upkick_a` (60 ticks, 4 authored poses),
  and completed successfully.
- Parsed the exported clip, Captain Falcon profile, and weapon JSON. Confirmed
  the profile includes the clip, both actions reference `upkick_a`, the markers
  are ordered and end at tick 60, and B retains 8% damage and 14 upward impulse
  versus A's 6% and 6.
- Checked `docs/ROUTER.md`, the animation and weapon specifications, the
  networking specification, asset-management guidance, task-completion and
  runtime-validation workflows, and the asset, documentation, and
  spec-behavior review skills. The router's requested
  `docs/doc-review-09-03-2026.md` is absent in this checkout.
- The task began with numerous unrelated dirty files. Those edits were
  preserved. No C++ files, tests, regression records, or production files were
  changed.
- Build: `NOT APPLICABLE`; only exporter, JSON, and documentation changed.
- Runtime: not performed. `py devscripts/dev-loop.py --status --json` showed
  the existing loop on profile `npc-navigation`, a live client on build 2607,
  published build 2609, and no live server. The existing session was left
  untouched; the current A/B gameplay was not exercised.
- Human review: still needed to confirm the exported pose looks right in-game
  and to playtest both A and B, including B's stronger special behavior.

## Focused review results

- `docs/skills/asset-checker-v1.md`: `PASS` — clip is in the shared library and
  included by the Captain Falcon profile.
- `docs/skills/documentation-checker-v1.md`: `PASS_WITH_HUMAN_REVIEW` — the
  exporter command and clip-specific prompt sequence are documented; runtime
  visual acceptance remains open.
- `docs/skills/spec-behavior-review-v1.md`: `PASS_WITH_HUMAN_REVIEW` — A and B
  use one authored clip and B keeps stronger existing gameplay values; live
  behavior remains unverified.
