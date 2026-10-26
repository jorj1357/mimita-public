# TDM NPC intake and Payload priority summary — 2026-10-26

- **Timestamp:** 2026-10-26T19:24:46Z (2026-10-26 15:24:46 EDT)
- **Branch:** `20261011zt`
- **HEAD:** `4d96b119f54c3e08cfc840c7f41ae77709ee4fdd`
- **Result:** PASS_WITH_HUMAN_REVIEW

## Change

Updated `docs/specs/open-gameplay-issues.md`:

- Added a separate 2026-10-26 TDM report at line 25: nearby NPCs sometimes do
  not acknowledge or damage the player. The reported journal path/run ID was
  not included. The inspected candidate journals identify Payload and Sandbox,
  not a confirmed TDM run; no cause is inferred.
- Expanded the NPC accuracy/movement report at line 108 to include the user's
  report that human movement is much faster, TDM NPCs do not combine dash,
  down-dash, bunny hop, and air strafe effectively, and an approximately
  500 m/s competitive speed example. The example remains aspirational; the user
  prefers skilled, risky movement over a hard speed cap.
- Connected the report to the existing NPC movement specification: it already
  describes dash chains, dash bursts, bunny hops, and air strafing, while its
  implementation status says current chance-based choices do not provide
  authored event-sequenced recipes.
- Expanded map/enemy readability at line 205: enemies reportedly remain hard
  to see even when made larger; map scale and sightlines are possible causes,
  not established facts.

The two reports were added after the existing TDM post-death shooting issue as
distinct items; the observed non-response was not merged with the post-death
report. Existing Payload map-editor/cart-route notes at lines 510–595 were not
changed in this session.

## Review and evidence

Read `AGENTS.md`, `docs/ROUTER.md`,
`docs/specs/open-gameplay-issues.md`,
`docs/specs/npc-behavior/npc-behavior.md`, and
`docs/architecture/player-npc-systems/npc-movement.md`.

- The 2026-10-26 log `20261026_140242` identifies Payload mode on `payload1`;
  `20261026_145155` identifies Sandbox on `funworld3`. Neither validates the
  new TDM report. No specific TDM journal path was supplied with this request.
- Documentation/spec review: PASS_WITH_HUMAN_REVIEW. The NPC report is framed
  as user-observed; target selection, attack gates, damage, and movement stages
  remain investigation targets rather than diagnoses.
- `git diff --check -- docs/specs/open-gameplay-issues.md`: passed.
- No build, test, or gameplay run was performed. This was a documentation-only
  intake.
- Existing working-tree changes in configuration, source, specs, and earlier
  changelogs predate this update and were preserved.

## Human review and priority guidance

Recommended ordering was given in the response around the user's stated goal:
first make a small Payload route playable through the editor and cart-path
authoring; next remove join/collision/position failures that invalidate play;
then tune NPC and general gameplay balance; then run controlled map,
enemy-readability, presentation, and high-speed movement experiments. The
existing Payload editor notes already record search/scroll/selection, route
editing, and cart-origin alignment tools as implemented, but human usability,
route pacing, and visual/gameplay acceptance remain open.

Reproduce the TDM NPC non-response in a confirmed TDM run. Record whether the
NPC can see and target the player, passes its attack gates, emits an attack,
and applies authoritative damage. For movement speed, compare ability
availability and sequences against a skilled human on the same map and mode;
do not infer a speed cap or required target from the 500 m/s example. Test
enemy readability on a compact Payload map while varying map sightlines,
landmark/encounter spacing, enemy size, and movement speed independently.
