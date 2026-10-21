# Gameplay issues inbox intake — 2026-10-21

## Summary

Changed the open gameplay issue document into a living inbox for reports from
any task or play session, then recorded the player's 2026-10-21 observations
and screenshot evidence. Reports remain explicitly unverified; uncertain
design details remain open. No gameplay code or configuration changed.

## Files changed

### `docs/specs/open-gameplay-issues-2026-10-16.md`

- Lines 29-38: generalized the title and purpose. Before: `# Open gameplay
  issue index — 2026-10-16` and `This index collects user-reported issues and
  links to the behavior owner.` After: `# Open gameplay issue inbox — started
  2026-10-16` and `This living inbox collects gameplay observations from any
  task or play session and routes them to the relevant behavior owner.`
- Lines 40-62: added a dated intake with eleven areas: NPC combat, shot and
  cooldown reliability, blood/world-impact marks, shotgun pellet direction,
  score and actor presentation, uncertain frame pacing, Rocket Launcher
  presentation, replay export, Spy Knife, Duel avatars, and aim feel. The
  section links the relevant owners and records the three screenshot
  filenames. Before, this section did not exist. Exact addition begins:
  `## New intake — 2026-10-21`.
- Existing dated raw notes above the title were retained unchanged.

## Review and evidence

- Read `docs/ROUTER.md`, `docs/skills/documentation-checker-v1.md`,
  `docs/skills/spec-behavior-review-v1.md`,
  `docs/architecture/time-and-formatting/time-and-formatting.md`, the target
  inbox, and the relevant NPC, weapon, network, Duel, GUI, performance, replay,
  and visual owner documents.
- The router points to `docs/doc-review-09-03-2026.md`, which is absent in this
  checkout. The router's `docs/specs/gui/gui.md` target is also absent; the
  current GUI owner found is `docs/specs/gui/gui-general.md`.
- Whole-`docs` TODO search found many pre-existing notes. Relevant exact
  examples: `docs/specs/visuals/blood-bulletholes-viscera.md:2` says
  `jorj todo explain`; suggested follow-up: add a short explanation of the
  blood/impact visual owner and its acceptance criteria. The GUI route,
  `docs/specs/gui/gui-general.md:1`, says
  `2026 10 19 1813 jorj todo explainnnnnnn`; suggested follow-up: explain the
  GUI spec's purpose and how its general presentation rules relate to
  feature-specific HUD requirements. `docs/specs/replay/replay.md:21` says
  `2026 10 08 1259 jorj todo explain -`; suggested follow-up: explain the
  current replay lifecycle and export boundary. These TODOs were not edited.
- `git diff --check -- docs/specs/open-gameplay-issues-2026-10-16.md` passed;
  Git printed only its line-ending conversion warning (LF to CRLF).
- No build, tests, live runtime, or human visual acceptance was performed; this
  was a documentation-only intake.
