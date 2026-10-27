# Rough-prompt implementation-first gold reference

- **Time:** `2026-10-27T17:14:31Z`; `America/New_York` `2026-10-27 13:14:31 EDT` (UTC-04:00).
- **Branch / base commit:** `20261011zt` / `ed4fb094e59bd16a7eef2a8cc8c61305bc766ba8`.
- **Result:** `PASS_WITH_HUMAN_REVIEW` (documentation only).
- **Scope:** Added a gold reference capturing the player's preference for spontaneous, rough spoken ideas and implementation-led discovery, then linked the existing vehicle idea to it. No implementation or runtime behavior changed.

## Changes

- Added `docs/gold/20261027-rough-prompt-implementation-first.md` (new file, 25 lines): records the player's random-idea intake use case, direct “put vehicles in the game” starting prompt, implementation-led discovery, outside-the-executable parallel work, and Blender animation export experience. It marks model recollection as uncertain and explicitly avoids presenting this as a universal no-planning rule.
- Updated `docs/specs/open-gameplay-issues.md` in the vehicle-focused gamemode bullet (around line 1399). Previous wording: “the vehicle rules, seating/respawn, weapon use, damage model, and match objective are open; make a small playable mode contract before expanding vehicle systems.” New wording: “the vehicle rules, seating/respawn, weapon use, damage model, and match objective are open. The player prefers starting uncertain features from a rough direct prompt and letting implementation expose the unknowns, rather than requiring a comprehensive speculative plan first; see [Start uncertain features with a rough prompt](../gold/20261027-rough-prompt-implementation-first.md).”

## Documents and review

- Read the user-provided `AGENTS.md` instructions, `docs/ROUTER.md`, `docs/specs/open-gameplay-issues.md`, `docs/skills/documentation-checker-v1.md`, `docs/architecture/time-and-formatting/time-and-formatting.md`, and `docs/operations/task-completion/task-completion.md`.
- `docs/doc-review-09-03-2026.md`, linked by the router, is absent from this checkout; documented as a routing gap, not changed.
- Documentation review: gold note has a clear purpose, audience, and “does not” boundary; tracker link is relative and points to the new file. No implementation details were added to product behavior.

## Validation and limits

- `git diff --check`: PASS.
- Relative link target existence: PASS.
- No build or runtime test applies to documentation-only work.
- This is a record of user preference and recollection, not evidence that a particular model performs better or that all features should skip planning. Human review may refine the wording or priority later.
- Preserved pre-existing work: modified `docs/architecture/code-ownership/code-ownership.md`, `docs/architecture/vision-mentality/vision.md`, `docs/specs/animations/animations.md`, and existing changes in `docs/specs/open-gameplay-issues.md`; pre-existing untracked `assets/textureshq/sparkv1.png`, `sparkv2.png`, `sparkv3.png`, `docs/changelog/2026-10-27/20261027_142455-architecture-vision-update.md`, and `docs/changelog/2026-10-27/20261027_142957-gameplay-issues-intake.md` were left untouched.
