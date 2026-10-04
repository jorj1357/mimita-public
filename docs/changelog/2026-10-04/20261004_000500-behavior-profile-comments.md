# Behavior profile comments and pursuit explanation

Date: 2026-10-04 00:05 EST
Branch: `afad20a-rebuild`
Result: PASS_WITH_HUMAN_REVIEW

## Request

Explain the proposed direct-pursuit behavior in simple terms and document the
behavior-profile settings inside the JSON file.

## Changes

- Added plain-English comments to `config/behavior-profiles.json` explaining
  aiming, reaction, aggression, preferred range, target selection, weapon
  selection, and pursuit settings.
- Added profile-level comments for the custom `rage2`, `aggressive`, `rage`,
  and `nervous` profiles.
- No C++ pursuit behavior was changed. The future `continue_pursuit` behavior
  remains explanation/design-only in this session.

## Validation

- `git diff --check`: no new whitespace errors in the changed JSON file.
- Existing executable and prior NPC self-tests were not rerun because this
  session only changed comments and did not change runtime code.

## Human review still required

Decide whether `continue_pursuit` should mean only “keep moving to the last
known point” or should also define a local search behavior after reaching that
point.

## Pre-existing work preserved

Existing unrelated worktree edits were preserved.
