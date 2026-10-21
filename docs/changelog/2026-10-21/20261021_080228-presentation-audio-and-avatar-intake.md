# Presentation, audio, and avatar issue intake — 2026-10-21

## Summary

Added the player's new presentation, world-hit sound, sound organization, and
avatar-head reports to the current central gameplay issues inbox. Presentation
is explicitly first priority, with a clearly labeled discussion sketch rather
than a decided design. The reports remain unverified where appropriate. No
runtime source, configuration, or sound asset changed.

## Files changed

### `docs/specs/open-gameplay-issues.md`

- Lines 43-61: added a presentation-first discussion section. Before, the
  inbox had no prioritized presentation conversation. After, it begins
  `## First priority — decide what gameplay presentation should do` and offers
  a tentative Duel HUD sketch plus questions for the design conversation.
- Line 76: moved the Duel score/identity request to the first row of the intake
  and labeled it **Presentation — first priority**. Before, its row began
  `| Score and actor presentation | Make scores and actor/team identity much
  easier to read during play.` After, it begins
  `| **Presentation — first priority** | Make the game's presentation clearer
  overall.`
- Lines 83-84: recorded force-scaled, varied world-hit sounds, the referenced
  `assets/sound/hitworld2.mp3`, the user's ricochet comparison, and the proposed
  hierarchical sound naming example. Before, these entries did not exist.
  Exact additions begin `| World-hit sound |` and `| Sound asset organization |`.
- Line 89: recorded the uncertain avatar-head movement report. Before, this
  entry did not exist. Exact addition begins `| Avatar head movement |`.
- The note includes the uncertainty around when the head behavior changed,
  what “scale with force” should affect, and whether ricochet is an established
  working example.

## Review and evidence

- Read `docs/ROUTER.md`, `docs/skills/documentation-checker-v1.md`,
  `docs/architecture/time-and-formatting/time-and-formatting.md`, the current
  inbox, the graphics-presentation, GUI, Duel, audio-contract, asset-management,
  avatar, and ragdoll specs.
- `assets/sound/hitworld2.mp3` exists in the checkout. A targeted search did
  not verify a current ricochet implementation or recipe; the user's
  comparison is therefore recorded as a report, not repository fact.
- The audio contract already describes sound lists, weighted variants, and
  volume/pitch ranges. The inbox routes follow-up there without claiming the
  world-hit behavior is already wired.
- The inbox's current links in the edited section resolve to existing files.
  `git diff --check -- docs/specs/open-gameplay-issues.md` passed.
- Router targets `docs/doc-review-09-03-2026.md`, `docs/specs/gui/gui.md`, and
  `docs/regressions/regressions-v1.md` are absent in this checkout. Current
  alternatives found include `docs/specs/gui/gui-general.md` and
  `docs/specs/gui/guiv2.md`; no regression record was created for these
  unverified reports.
- The whole-docs TODO scan found existing relevant notes left unchanged:
  `docs/specs/gui/gui-general.md:1` says
  `2026 10 19 1813 jorj todo explainnnnnnn`; suggested follow-up: explain the
  general GUI spec's purpose and how shared HUD policy relates to mode-specific
  screens. `docs/specs/visuals/visuals.md:1` says
  `2026-09-07T18:40:16Z jorj - todo explain this  more like`; suggested
  follow-up: state the visual style and readability goals. The related asset
  note `docs/operations/asset-management/asset-management.md:17` says
  `9 2 2026 expand, because, we should add a folder of`; suggested follow-up:
  define the intended gameplay-sound folder and event/variant naming rules.
- This was documentation-only; no build, test, live runtime, or visual
  acceptance was performed.
