# Sandbox Friendly-Fire Toggle

- Task: Enable friendly fire for Sandbox with a comment-based off switch.
- Result: `PASS_WITH_HUMAN_REVIEW`
- Time: `2026-10-22T01:09:54Z` (Eastern display: `2026-10-21 21:09:54 EDT`)
- Branch: `20261011zt`
- HEAD: `9ec73c3ae724037da33d032d54843cbf1186f97a` (no commit created)

## Pre-existing changes

- `config/accounts/default.json` was modified before this task and was left untouched.

## Change

### `config/gamemodes/sandbox.json`

- Before, after the description: the NPC behavior-profile comment began; no `friendly_fire` setting was present, so the mode defaulted to false.
- After, lines 5-7:

  ```jsonc
  // Friendly fire: keep one option active to switch Sandbox's team-damage rule.
  // "friendly_fire": false,
  "friendly_fire": true,
  ```

- The user can switch Sandbox back to friendly fire off by uncommenting `false` and commenting out `true`.
- Owner confirmation: `src/gamemode/gamemode.cpp` reads `friendly_fire`; `Gamemode::friendlyFire` defaults to false in `src/gamemode/gamemode.h`.

## Review and validation

- Read `docs/ROUTER.md`, `docs/specs/gamemodes/gamemodes.md`, `docs/specs/networking/networking.md`, `docs/architecture/json-configuration/json-configuration.md`, `docs/architecture/time-and-formatting/time-and-formatting.md`, `docs/skills/spec-behavior-review-v1.md`, and `docs/operations/task-completion/task-completion.md`.
- Review result: the existing gamemode option supports the requested behavior; no code or specification change was needed.
- The repository JSONC parser loaded `config/gamemodes/sandbox.json` and confirmed `id == "sandbox"` and `friendly_fire == true`.
- `git diff --check -- config/gamemodes/sandbox.json` passed.
- No build or live multiplayer scenario was run. Runtime teammate-damage behavior still needs in-game confirmation.
