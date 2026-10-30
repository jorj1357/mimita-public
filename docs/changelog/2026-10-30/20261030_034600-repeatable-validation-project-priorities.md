# Repeatable validation and project-priority intake

- Canonical timestamp: `2026-10-30T03:46:00Z`.
- Display time: `2026-10-29 23:46 EDT` (`America/New_York`).
- Branch: `20261011zt`.
- Base commit: `1b5f3bac`.
- Result: `PASS_WITH_HUMAN_REVIEW` for documentation intake; no runtime changes.

## Change

Added `docs/specs/open-gameplay-issues.md:115-195`, “New intake — 2026-10-29:
repeatable validation and evolving project priorities.” The text records the
existing `devscripts/config_selftest.py` coverage and its lack of
`config/collision.json` validation, distinguishes `--collision-selftest`
algorithm coverage from config parsing/reload coverage, and asks for focused,
repeatable config checks that supplement live runtime scenarios and human
acceptance.

The same section records the player's rough priority order: complete Zombie
Tower; vehicles and destructible-world capability with several possible modes;
then possibly Survive the Disasters; improve the Map Editor and mode authoring;
then promotion, community feedback, and refinement. It connects the desired
fully in-game, extensible, smoother video workflow to the existing Video Editor
spec, updates the issue tracker with the user's report that replay export still
fails, and leaves self-spreading EXE, mobile/controller/console, monetization,
and broader prioritization as open questions. It explicitly does not choose
automatic propagation or present the roadmap as an approved schedule.

Updated the introduction at `docs/specs/open-gameplay-issues.md:2122-2126` for
the older “Longer-range game and project ideas” list: the new 2026-10-29 order
supersedes conflicting sequence suggestions in that older list while retaining
it as scope background. No behavior contract or implementation was changed.

## Source review

- `devscripts/config_selftest.py`: repository-level JSON validation covers
  procedural player config, weapons, animations, avatars, NPC avatar config,
  and gamemode config; no collision-config validation function is present.
- `src/game/game-cli.cpp`: `--collision-selftest` calls
  `collisionStressSelfTest`; separate actor-triangle and collision-subgrid
  self-test entry points exist. This inventory does not claim what all their
  internal assertions cover.
- `src/config/collision-config.cpp`: `CollisionConfig::load()` reads
  `config/collision.json`, applies defaults/clamps, and has a polling reload
  path; those config behaviors are not covered by the repository config script
  identified above.
- `docs/specs/creator-tools/video-editor.md` already covers integrated replay
  and media editing, effects, non-blocking exports, accessibility, and measured
  performance goals. The intake links to that existing spec rather than
  duplicating its contract.
- `docs/specs/replay/replay.md` already records export/performance acceptance
  and the reported exit-code-3 failure with unknown cause. The new intake
  preserves this as unresolved and adds the player's later report that export
  still fails.

## Focused review and validation

- Read `docs/ROUTER.md`, `docs/skills/documentation-checker-v1.md`,
  `docs/skills/spec-behavior-review-v1.md`, relevant issue-tracker sections,
  the video editor, distribution, replay, dev-loop quality-gates, config
  self-test, collision config loader, and executable self-test dispatch.
- The router-listed `docs/doc-review-09-03-2026.md` is absent in this checkout.
- `git diff --check` reports pre-existing trailing whitespace in the
  concurrently edited beginning of `docs/specs/open-gameplay-issues.md` (user
  notes dated `2026 10 29 2343`); the new sections have no trailing whitespace.
  Those notes were preserved.
- No builds, tests, or runtime scenarios were run. This was documentation
  intake, not validation of collision behavior or replay export.

## Human review still needed

Confirm the next priority after Zombie Tower and the scope/order of vehicles,
destructible-world logic, Survive the Disasters, editor work, and promotion.
Clarify whether “self-spreading EXE” means a portable shareable build or
automatic propagation. Define which platforms and controller goals are
practical. The current replay export failure still needs reproduction and an
owner-level diagnosis. Config test behavior and assertions should be inspected
before implementation so new checks reuse existing owners and avoid overlap.
