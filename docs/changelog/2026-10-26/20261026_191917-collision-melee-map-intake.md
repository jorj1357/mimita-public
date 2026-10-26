# Collision, melee, and map-design intake — 2026-10-26

- **Timestamp:** 2026-10-26T19:19:17Z (2026-10-26 15:19:17 EDT)
- **Branch:** `20261011zt`
- **HEAD:** `4d96b119f54c3e08cfc840c7f41ae77709ee4fdd`
- **Result:** PASS_WITH_HUMAN_REVIEW

## Changes

Updated `docs/specs/open-gameplay-issues.md`:

- Added current intake items at lines 114–187 for actors snagging on block
  edges and the steep `tdmtest3` spawn-ramp seam; falling through geometry;
  repeated position snaps far outside world geometry; physical-melee modes and
  NPC use; weapon-specific SwordSword, Spy Knife, chainsaw, heavy weapons, and
  HAFS concepts; physical weapon blocking/clashes; and map scale/enemy
  readability experiments.
- Cited the two supplied human scenario captures. The caught scenario is
  Sandbox on `tdmtest3`, 5,174 input frames, with 231 frames where primary fire
  is held or pressed. The fall-through scenario is Sandbox on `tdmtest3`,
  5,883 frames, with 2,875 frames marked ungrounded. Neither file records
  collision contacts, so the entries do not claim either file proves the
  collision cause or ties a specific snag to shooting.
- Refined the Spy Knife and SwordSword rows at lines 592 and 670, and the melee
  priority row at line 947. Fixed their links to the existing nested melee
  specification paths.

## Specification conflict and evidence limits

The intake preserves two explicit open decisions instead of rewriting weapon
specifications:

- `docs/specs/weapons/melee/swordsword.md` says: “An attack input never adds
  forward velocity or an impulse to the wielder.” The user's new SwordSword
  impulse idea is recorded as a proposal requiring explicit resolution.
- The same SwordSword spec says its defensive pose is animation only and does
  not block or reduce damage. The broad proposal that all melee items physically
  block attacks needs reconciliation with that contract.
- `docs/specs/weapons/melee/melee-weapons.md` defines reusable physical
  weapon-contact behavior and authoritative validation. The Spy Knife request
  asks whether its current path matches that reusable owner; no source/runtime
  audit was performed here.
- No code, weapon configuration, map asset, or specification owner was changed.
  This session only updated the issue inbox and this changelog.

## Review and validation

Read `AGENTS.md`, `docs/ROUTER.md`,
`docs/architecture/collision/collision.md`,
`docs/specs/gamemodes/supersmashbrosmelee.md`,
`docs/specs/weapons/melee/melee-weapons.md`,
`docs/specs/weapons/melee/swordsword.md`, and the relevant sections of
`docs/specs/open-gameplay-issues.md`.

- Documentation and spec-behavior review: PASS_WITH_HUMAN_REVIEW. Reports,
  evidence, proposed behavior, and unresolved specification conflicts are
  labeled separately.
- `git diff --check -- docs/specs/open-gameplay-issues.md`: passed.
- No build, synthetic test, or live gameplay run was performed; this is an
  issue-intake documentation change.
- Existing working-tree changes were present across source, configuration,
  specs, and changelogs before this update. They were preserved. This session
  added no new source or asset files.

## Human review still needed

Reproduce the `tdmtest3` snag at the spawn-ramp seam and at block edges while
capturing collision contacts; identify whether shot input is causal or simply
co-occurs. Reproduce the geometry fall-through with contact and grounded-state
diagnostics. Decide whether void movement should remain unconstrained or use a
different safety policy. Resolve SwordSword impulse and physical blocking
against its current contract. Confirm each weapon's model, contact, and
authoritative damage behavior before tuning the melee concepts. Test map-scale
ideas through live map edits and play rather than treating 10 m / 50 m examples
as fixed requirements.
